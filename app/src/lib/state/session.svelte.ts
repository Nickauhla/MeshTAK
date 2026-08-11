// -----------------------------------------------------------------------------
// État global de l'application (runes Svelte 5).
//
// L'app ne manipule aucune clé : le device déchiffre et ne lui transmet que du
// clair. Elle ne voit donc que les trames de SA propre escouade — les autres sont
// jetées par le firmware faute de clé. Le nombre de nœuds entendus mais illisibles
// remonte par le compteur `peerCount` du diagnostic.
// -----------------------------------------------------------------------------
import { link, type LinkState } from '../ble/link.ts';
import {
  DEFAULT_CONFIG,
  Flags,
  FrameType,
  JoinCmd,
  JoinEvent,
  MarkerKind,
  PlayerStatus,
  SquadState,
  decodeAck,
  decodeConfig,
  decodeJoinEvent,
  decodeMarker,
  decodeNodeInfo,
  decodePosition,
  decodeStatus,
  decodeText,
  encodeConfig,
  encodeJoinCmd,
  encodeMarker,
  encodePosition,
  encodeText,
  fingerprintHex,
  toDegrees,
  type Config,
  type DeviceStatus,
  type Frame,
  type Marker,
} from '../proto/frames.ts';
import { markerSpec } from '../tactical/markers.ts';

export interface NodeEntry {
  /** Adresse dans l'escouade, 1..31. */
  addr: number;
  callsign: string;
  role: number;
  lat: number | null;
  lon: number | null;
  alt: number;
  sats: number;
  hdop: number;
  battPct: number;
  status: number;
  lastHeard: number;
  viaRelay: boolean;
}

export interface Message {
  key: string;
  from: number;
  fromName: string;
  /** undefined = diffusion à toute l'escouade. */
  to?: number;
  text: string;
  at: number;
  outgoing: boolean;
  acked: boolean;
}

export interface Candidate {
  fingerprint: string;
  callsign: string;
  at: number;
}

/** Point tactique partagé — identifié par `(owner, id)`, jamais par l'émetteur. */
export interface MarkerEntry {
  key: string;
  owner: number;
  id: number;
  kind: number;
  /** Degrés décimaux. */
  lat: number;
  lon: number;
  label: string;
  ttlMin: number;
  /** Horodatage LOCAL de pose ou de réception : il n'y a pas d'horloge commune. */
  at: number;
  mine: boolean;
}

/** Seconde émission d'un point tactique — cf. PROTOCOL.md §7.8. */
const MARKER_REPEAT_MS = 6000;

const markerKey = (owner: number, id: number) => `${owner}:${id}`;

function emptyNode(addr: number): NodeEntry {
  return {
    addr,
    callsign: `#${addr}`,
    role: 0,
    lat: null,
    lon: null,
    alt: 0,
    sats: 0,
    hdop: 0,
    battPct: 255,
    status: PlayerStatus.OK,
    lastHeard: 0,
    viaRelay: false,
  };
}

function hexToBytes(hex: string): Uint8Array {
  const out = new Uint8Array(hex.length / 2);
  for (let i = 0; i < out.length; i++) out[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
  return out;
}

class Session {
  // --- Lien ------------------------------------------------------------------
  connected = $state(false);
  deviceName = $state('');
  /** Empreinte du boîtier connecté (8 hex) — annoncée par lui à chaque connexion. */
  deviceFingerprint = $state('');
  lastError = $state('');
  /** État du lien, reconnexion automatique comprise. */
  linkState = $state<LinkState>('off');
  /** Nombre d'essais depuis la dernière coupure — affiché pour ne pas mentir. */
  reconnectAttempt = $state(0);

  // --- Device ----------------------------------------------------------------
  config = $state<Config>({ ...DEFAULT_CONFIG });
  status = $state<DeviceStatus | null>(null);

  // --- Escouade --------------------------------------------------------------
  nodes = $state<Record<number, NodeEntry>>({});
  messages = $state<Message[]>([]);
  /** Candidats en attente de validation — uniquement côté chef. */
  candidates = $state<Candidate[]>([]);
  /** Dernier événement d'adhésion, pour informer le candidat. */
  joinNotice = $state('');
  /** Points tactiques de l'escouade, clés `owner:id`. */
  markers = $state<Record<string, MarkerEntry>>({});

  /** Identifiant du prochain point posé : local au device, boucle sur 1..255. */
  private nextMarkerId = 1;
  private pruneTimer: ReturnType<typeof setInterval> | null = null;

  get isLeader(): boolean {
    return this.config.state === SquadState.LEADER;
  }
  get inSquad(): boolean {
    return this.config.state === SquadState.MEMBER || this.config.state === SquadState.LEADER;
  }
  get isPending(): boolean {
    return this.config.state === SquadState.PENDING;
  }

  get squadMates(): NodeEntry[] {
    return Object.values(this.nodes)
      .filter((n) => n.addr !== this.config.addr)
      .sort((a, b) => a.addr - b.addr);
  }

  /** Points tactiques du plus récent au plus ancien. */
  get squadMarkers(): MarkerEntry[] {
    return Object.values(this.markers).sort((a, b) => b.at - a.at);
  }

  /** Qui a posé ce point — l'indicatif s'il est connu, l'adresse sinon. */
  markerAuthor(m: MarkerEntry): string {
    if (m.mine) return this.config.callsign || 'moi';
    return this.nodes[m.owner]?.callsign ?? `#${m.owner}`;
  }

  get myPosition(): { lat: number; lon: number } | null {
    if (!this.status?.fixValid) return null;
    return { lat: toDegrees(this.status.lat), lon: toDegrees(this.status.lon) };
  }

  // --- Cycle de vie ----------------------------------------------------------
  /** Renvoie vrai si un boîtier connu est en cours de reprise automatique. */
  init(): boolean {
    link.onConnectionChange = (connected, name) => {
      this.connected = connected;
      this.deviceName = name;
      if (connected) {
        this.lastError = '';
        void link.send({ type: FrameType.CFG_GET, flags: Flags.LOCAL });
      } else {
        this.status = null;
        // L'empreinte appartient au boîtier, pas à l'app : sans lien, on n'a
        // plus rien à affirmer sur son identité.
        this.deviceFingerprint = '';
      }
    };
    link.onStateChange = (state, attempt) => {
      this.linkState = state;
      this.reconnectAttempt = attempt;
    };
    link.onFrame = (f) => this.handleFrame(f);

    // Les points tactiques périment tout seuls : un contact ennemi de quinze
    // minutes affiché comme frais est pire que pas de point du tout.
    this.pruneTimer ??= setInterval(() => this.pruneMarkers(), 5000);

    return link.resume();
  }

  async connect(): Promise<void> {
    this.lastError = '';
    try {
      await link.connect();
    } catch (e) {
      this.lastError = e instanceof Error ? e.message : String(e);
    }
  }

  /** Relance immédiatement la reprise en cours, sans attendre le prochain essai. */
  retry(): void {
    this.lastError = '';
    link.retryNow();
  }

  async disconnect(): Promise<void> {
    await link.disconnect();
  }

  // --- Escouade --------------------------------------------------------------
  private async sendJoinCmd(cmd: number, squadName = '', fingerprint?: string): Promise<void> {
    if (!this.connected) return;
    await link.send({
      type: FrameType.JOIN_CMD,
      flags: Flags.LOCAL,
      payload: encodeJoinCmd(cmd, squadName, fingerprint ? hexToBytes(fingerprint) : undefined),
    });
  }

  createSquad(name: string): Promise<void> {
    this.joinNotice = '';
    return this.sendJoinCmd(JoinCmd.CREATE, name.trim().slice(0, 12));
  }

  requestJoin(name: string): Promise<void> {
    this.joinNotice = '';
    return this.sendJoinCmd(JoinCmd.REQUEST, name.trim().slice(0, 12));
  }

  async acceptCandidate(fingerprint: string): Promise<void> {
    await this.sendJoinCmd(JoinCmd.ACCEPT, '', fingerprint);
    this.candidates = this.candidates.filter((c) => c.fingerprint !== fingerprint);
  }

  async refuseCandidate(fingerprint: string): Promise<void> {
    await this.sendJoinCmd(JoinCmd.REFUSE, '', fingerprint);
    this.candidates = this.candidates.filter((c) => c.fingerprint !== fingerprint);
  }

  async leaveSquad(): Promise<void> {
    await this.sendJoinCmd(JoinCmd.LEAVE);
    this.forgetSquad();
  }

  private forgetSquad(): void {
    this.nodes = {};
    this.messages = [];
    this.candidates = [];
    this.markers = {};
  }

  // --- Points tactiques ------------------------------------------------------
  /**
   * Pose un point et le diffuse. L'écho local est immédiat : sur le terrain, le
   * doigt quitte l'écran et le point doit déjà être là, même si la radio met
   * une seconde à partir.
   */
  async placeMarker(kind: number, lat: number, lon: number, label = ''): Promise<void> {
    const id = this.nextMarkerId;
    this.nextMarkerId = (this.nextMarkerId % 255) + 1;

    const m: Marker = {
      owner: this.config.addr,
      id,
      kind,
      lat: Math.round(lat * 1e7),
      lon: Math.round(lon * 1e7),
      ttlMin: markerSpec(kind)?.ttlMin ?? 0,
      label: label.slice(0, 20),
    };
    this.applyMarker(m);
    await this.emitMarker(m);
  }

  /**
   * Renomme un point. Le renvoi vaut réaffirmation : sa durée de validité repart
   * de zéro, ce qui est le comportement voulu — on ne renomme un contact que
   * parce qu'on vient de le revoir.
   */
  async renameMarker(key: string, label: string): Promise<void> {
    const m = this.markers[key];
    if (!m) return;
    const next: Marker = {
      owner: m.owner,
      id: m.id,
      kind: m.kind,
      lat: Math.round(m.lat * 1e7),
      lon: Math.round(m.lon * 1e7),
      ttlMin: m.ttlMin,
      label: label.slice(0, 20),
    };
    this.applyMarker(next);
    await this.emitMarker(next);
  }

  /** Retire un point — le sien comme celui d'un autre membre (§4.7). */
  async removeMarker(key: string): Promise<void> {
    const m = this.markers[key];
    if (!m) return;
    const { [key]: _gone, ...rest } = this.markers;
    this.markers = rest;
    await this.emitMarker({ owner: m.owner, id: m.id, kind: MarkerKind.CLEAR, lat: 0, lon: 0, ttlMin: 0, label: '' });
  }

  private async emitMarker(m: Marker): Promise<void> {
    if (!this.connected) return;
    const payload = encodeMarker(m);
    const frame = { type: FrameType.MARKER, payload };
    try {
      await link.send(frame);
    } catch (e) {
      // Lien coupé entre le geste et l'émission : le point reste sur MA carte,
      // mais personne d'autre ne l'a. Il faut que ça se dise.
      this.lastError = e instanceof Error ? e.message : String(e);
      return;
    }

    // Seconde copie : une position perdue est remplacée dix secondes plus tard,
    // un point tactique perdu ne revient jamais. Les deux copies se refondent à
    // l'arrivée sur `(owner, id)`.
    setTimeout(() => {
      if (this.connected) void link.send(frame).catch(() => undefined);
    }, MARKER_REPEAT_MS);
  }

  private applyMarker(m: Marker): void {
    const key = markerKey(m.owner, m.id);
    if (m.kind === MarkerKind.CLEAR) {
      if (!(key in this.markers)) return;
      const { [key]: _gone, ...rest } = this.markers;
      this.markers = rest;
      return;
    }
    if (!markerSpec(m.kind)) return; // nature inconnue : app plus ancienne que l'émetteur

    this.markers = {
      ...this.markers,
      [key]: {
        key,
        owner: m.owner,
        id: m.id,
        kind: m.kind,
        lat: toDegrees(m.lat),
        lon: toDegrees(m.lon),
        label: m.label,
        ttlMin: m.ttlMin,
        at: Date.now(),
        mine: m.owner === this.config.addr,
      },
    };
  }

  private pruneMarkers(): void {
    const now = Date.now();
    const kept = Object.entries(this.markers).filter(
      ([, m]) => m.ttlMin === 0 || now - m.at < m.ttlMin * 60000,
    );
    if (kept.length !== Object.keys(this.markers).length) {
      this.markers = Object.fromEntries(kept);
    }
  }

  // --- Configuration ---------------------------------------------------------
  async pushConfig(next: Config): Promise<void> {
    this.config = next;
    if (!this.connected) return;
    await link.send({ type: FrameType.CFG_SET, flags: Flags.LOCAL, payload: encodeConfig(next) });
  }

  setCallsign(callsign: string): Promise<void> {
    return this.pushConfig({ ...this.config, callsign: callsign.slice(0, 16) });
  }

  // --- Actions ---------------------------------------------------------------
  /** Envoie un message à toute l'escouade (`to` omis) ou à un membre précis. */
  async sendText(text: string, to?: number): Promise<void> {
    const clean = text.trim();
    if (!clean || !this.connected) return;

    await link.send({
      type: FrameType.TEXT,
      flags: to === undefined ? 0 : Flags.UNICAST | Flags.WANT_ACK,
      dst: to,
      payload: encodeText(clean),
    });

    this.messages = [
      ...this.messages,
      {
        key: `out-${Date.now()}-${this.messages.length}`,
        from: this.config.addr,
        fromName: this.config.callsign || 'moi',
        to,
        text: clean,
        at: Date.now(),
        outgoing: true,
        acked: false,
      },
    ].slice(-300);
  }

  /** Déclare mon statut — le device réémet sa position GNSS avec cette valeur. */
  async setPlayerStatus(status: number): Promise<void> {
    if (!this.connected) return;
    await link.send({ type: FrameType.POSITION, payload: encodePosition({ status }) });
  }

  // --- Réception -------------------------------------------------------------
  private touchNode(addr: number, relayed: boolean): NodeEntry {
    const node = { ...(this.nodes[addr] ?? emptyNode(addr)) };
    node.lastHeard = Date.now();
    node.viaRelay = relayed;
    return node;
  }

  private commit(node: NodeEntry): void {
    this.nodes = { ...this.nodes, [node.addr]: node };
  }

  private handleFrame(f: Frame): void {
    const relayed = (f.flags & Flags.RELAYED) !== 0;

    switch (f.type) {
      case FrameType.POSITION: {
        const p = decodePosition(f.payload);
        if (!p) return;
        const node = this.touchNode(f.src, relayed);
        node.lat = toDegrees(p.lat);
        node.lon = toDegrees(p.lon);
        node.alt = p.alt;
        node.sats = p.sats;
        node.hdop = p.hdop;
        node.battPct = p.battPct;
        node.status = p.status;
        this.commit(node);
        return;
      }

      case FrameType.NODEINFO: {
        const info = decodeNodeInfo(f.payload);
        if (!info) return;
        const node = this.touchNode(f.src, relayed);
        node.callsign = info.callsign || `#${f.src}`;
        node.role = info.role;
        this.commit(node);
        return;
      }

      case FrameType.MARKER: {
        const m = decodeMarker(f.payload);
        if (!m) return;
        // L'émetteur n'est pas forcément le créateur : on note l'un, on applique
        // le point de l'autre.
        this.commit(this.touchNode(f.src, relayed));
        this.applyMarker(m);
        return;
      }

      case FrameType.TEXT: {
        const node = this.touchNode(f.src, relayed);
        this.commit(node);
        this.messages = [
          ...this.messages,
          {
            key: `in-${f.src}-${f.seq}-${Date.now()}`,
            from: f.src,
            fromName: node.callsign,
            to: f.dst,
            text: decodeText(f.payload),
            at: Date.now(),
            outgoing: false,
            acked: true,
          },
        ].slice(-300);
        return;
      }

      case FrameType.ACK: {
        const ack = decodeAck(f.payload);
        if (!ack) return;
        const idx = this.messages.findLastIndex((m) => m.outgoing && !m.acked && m.to === f.src);
        if (idx >= 0) {
          const copy = [...this.messages];
          copy[idx] = { ...copy[idx], acked: true };
          this.messages = copy;
        }
        return;
      }

      case FrameType.CFG_STATE: {
        const c = decodeConfig(f.payload);
        if (!c) return;
        // Sortie d'escouade ou changement d'escouade : la flotte connue n'a plus de sens.
        if (c.squad !== this.config.squad) this.forgetSquad();
        this.config = c;
        return;
      }

      case FrameType.STATUS: {
        const s = decodeStatus(f.payload);
        if (s) this.status = s;
        return;
      }

      case FrameType.JOIN_EVENT: {
        const e = decodeJoinEvent(f.payload);
        if (!e) return;
        const fp = fingerprintHex(e.fingerprint);

        if (e.event === JoinEvent.IDENTITY) {
          // Le device se présente : c'est l'empreinte que le chef verra de son côté.
          this.deviceFingerprint = fp;
        } else if (e.event === JoinEvent.PENDING) {
          // Côté chef : un candidat se manifeste (réémis toutes les 5 s, d'où le filtre).
          if (!this.candidates.some((c) => c.fingerprint === fp)) {
            this.candidates = [
              ...this.candidates,
              { fingerprint: fp, callsign: e.callsign, at: Date.now() },
            ];
          }
        } else if (e.event === JoinEvent.KEY_RECEIVED) {
          this.joinNotice = 'Validé — vous faites partie de l’escouade.';
        } else if (e.event === JoinEvent.REFUSED) {
          this.joinNotice = 'Demande refusée par le chef d’escouade.';
        }
        return;
      }

      case FrameType.LOG:
        console.info('[device]', decodeText(f.payload));
        return;
    }
  }
}

export const session = new Session();

// -----------------------------------------------------------------------------
// État global de l'application (runes Svelte 5).
//
// L'app ne manipule aucune clé : le device déchiffre et ne lui transmet que du
// clair. Elle ne voit donc que les trames de SA propre escouade — les autres sont
// jetées par le firmware faute de clé. Le nombre de nœuds entendus mais illisibles
// remonte par le compteur `peerCount` du diagnostic.
// -----------------------------------------------------------------------------
import { link } from '../ble/link.ts';
import {
  DEFAULT_CONFIG,
  Flags,
  FrameType,
  JoinCmd,
  JoinEvent,
  PlayerStatus,
  SquadState,
  decodeAck,
  decodeConfig,
  decodeJoinEvent,
  decodeNodeInfo,
  decodePosition,
  decodeStatus,
  decodeText,
  encodeConfig,
  encodeJoinCmd,
  encodePosition,
  encodeText,
  fingerprintHex,
  toDegrees,
  type Config,
  type DeviceStatus,
  type Frame,
} from '../proto/frames.ts';

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

  get myPosition(): { lat: number; lon: number } | null {
    if (!this.status?.fixValid) return null;
    return { lat: toDegrees(this.status.lat), lon: toDegrees(this.status.lon) };
  }

  // --- Cycle de vie ----------------------------------------------------------
  init(): void {
    link.onConnectionChange = (connected, name) => {
      this.connected = connected;
      this.deviceName = name;
      if (connected) {
        void link.send({ type: FrameType.CFG_GET, flags: Flags.LOCAL });
      } else {
        this.status = null;
        // L'empreinte appartient au boîtier, pas à l'app : sans lien, on n'a
        // plus rien à affirmer sur son identité.
        this.deviceFingerprint = '';
      }
    };
    link.onFrame = (f) => this.handleFrame(f);
  }

  async connect(): Promise<void> {
    this.lastError = '';
    try {
      await link.connect();
    } catch (e) {
      this.lastError = e instanceof Error ? e.message : String(e);
    }
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
    this.nodes = {};
    this.messages = [];
    this.candidates = [];
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
        if (c.squad !== this.config.squad) {
          this.nodes = {};
          this.messages = [];
          this.candidates = [];
        }
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

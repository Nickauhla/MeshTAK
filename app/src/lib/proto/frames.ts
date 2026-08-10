// -----------------------------------------------------------------------------
// Codec du protocole MeshRadio v2 — implémentation TypeScript.
// Miroir exact de firmware/include/protocol.h.
// Spécification : shared/PROTOCOL.md — toute modification se fait là-bas d'abord.
//
// ⚠️ Ce fichier ne contient AUCUNE cryptographie : le chiffrement vit entièrement
// dans le firmware, les trames qui circulent en BLE sont en clair. On y trouve en
// revanche le calcul du vecteur d'initialisation et des données authentifiées,
// car ils se déduisent de l'en-tête et doivent être identiques des deux côtés.
//
// ⚠️ Syntaxe TypeScript « effaçable » uniquement (pas d'enum, pas de namespace) :
// le fichier doit être exécutable directement par Node pour générer les vecteurs.
// -----------------------------------------------------------------------------

export const VERSION = 2;
export const HEADER_LEN = 8;
export const TAG_LEN = 8;
export const MAX_PAYLOAD = 180;
export const MAX_FRAME = HEADER_LEN + 1 + MAX_PAYLOAD + TAG_LEN;

/** Adresses dans l'escouade : 0 = non attribuée, 1..31 = membres. */
export const ADDR_UNASSIGNED = 0;
export const ADDR_MAX = 31;

export const FrameType = {
  POSITION: 1,
  TEXT: 2,
  NODEINFO: 3,
  ACK: 4,
  JOIN_REQUEST: 5,
  JOIN_GRANT: 6,
  CFG_GET: 8,
  CFG_STATE: 9,
  CFG_SET: 10,
  STATUS: 11,
  LOG: 12,
  JOIN_EVENT: 13,
  JOIN_CMD: 14,
} as const;

export const Flags = {
  ENCRYPTED: 0x01,
  RELAYED: 0x02,
  UNICAST: 0x04,
  WANT_ACK: 0x08,
  LOCAL: 0x10,
  SIGNED: 0x20, // réservé — identité individuelle
} as const;

export const PlayerStatus = { OK: 0, HIT: 1, ELIMINATED: 2, NEED_HELP: 3 } as const;
export const Role = { PLAYER: 0, LEADER: 1, RELAY: 2, COMMAND: 3 } as const;

/** État d'appartenance à une escouade. */
export const SquadState = { ALONE: 0, PENDING: 1, MEMBER: 2, LEADER: 3 } as const;

/** Événements remontés par le device pendant une adhésion. */
/** `IDENTITY` porte l'empreinte du device lui-même, pas celle d'un candidat. */
export const JoinEvent = { PENDING: 0, ACCEPTED: 1, REFUSED: 2, KEY_RECEIVED: 3, IDENTITY: 4 } as const;

/** Commandes envoyées au device par l'app. */
export const JoinCmd = { CREATE: 0, REQUEST: 1, ACCEPT: 2, REFUSE: 3, LEAVE: 4 } as const;

export const DEFAULT_SQUAD_NAMES = [
  'Alpha',
  'Bravo',
  'Charlie',
  'Delta',
  'Echo',
  'Foxtrot',
  'Golf',
  'Hotel',
];

export interface Frame {
  type: number;
  flags: number;
  ttl: number;
  squad: number;
  src: number;
  seq: number;
  epoch: number;
  /** Présent uniquement si le drapeau UNICAST est positionné. */
  dst?: number;
  payload: Uint8Array;
  /** 8 octets si le drapeau ENCRYPTED est positionné. */
  tag?: Uint8Array;
}

// --- Trame -------------------------------------------------------------------
export function encodeFrame(f: Partial<Frame> & { type: number }): Uint8Array {
  const payload = f.payload ?? new Uint8Array(0);
  if (payload.length > MAX_PAYLOAD) throw new Error('charge utile trop longue');

  const flags = (f.flags ?? 0) & 0x3f;
  const unicast = (flags & Flags.UNICAST) !== 0;
  const encrypted = (flags & Flags.ENCRYPTED) !== 0;

  if (unicast && f.dst === undefined) throw new Error('unicast sans destinataire');
  if (encrypted && (f.tag?.length ?? 0) !== TAG_LEN) throw new Error('trame chiffrée sans sceau');

  const dstLen = unicast ? 1 : 0;
  const tagLen = encrypted ? TAG_LEN : 0;
  const out = new Uint8Array(HEADER_LEN + dstLen + payload.length + tagLen);
  const dv = new DataView(out.buffer);

  out[0] = ((VERSION & 0x0f) << 4) | (f.type & 0x0f);
  out[1] = (((f.ttl ?? 0) & 0x03) << 6) | flags;
  out[2] = (f.squad ?? 0) & 0xff;
  out[3] = (f.src ?? 0) & 0xff;
  dv.setUint16(4, (f.seq ?? 0) & 0xffff, true);
  dv.setUint16(6, (f.epoch ?? 0) & 0xffff, true);
  if (unicast) out[8] = f.dst! & 0xff;

  out.set(payload, HEADER_LEN + dstLen);
  if (encrypted) out.set(f.tag!, HEADER_LEN + dstLen + payload.length);

  return out;
}

export function decodeFrame(buf: Uint8Array): Frame | null {
  if (buf.length < HEADER_LEN) return null;
  if (buf[0] >> 4 !== VERSION) return null;

  const flags = buf[1] & 0x3f;
  const unicast = (flags & Flags.UNICAST) !== 0;
  const encrypted = (flags & Flags.ENCRYPTED) !== 0;
  const dstLen = unicast ? 1 : 0;
  const tagLen = encrypted ? TAG_LEN : 0;

  if (buf.length < HEADER_LEN + dstLen + tagLen) return null;
  const payloadLen = buf.length - HEADER_LEN - dstLen - tagLen;
  if (payloadLen > MAX_PAYLOAD) return null;

  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
  const frame: Frame = {
    type: buf[0] & 0x0f,
    flags,
    ttl: buf[1] >> 6,
    squad: buf[2],
    src: buf[3],
    seq: dv.getUint16(4, true),
    epoch: dv.getUint16(6, true),
    payload: buf.slice(HEADER_LEN + dstLen, HEADER_LEN + dstLen + payloadLen),
  };
  if (unicast) frame.dst = buf[8];
  if (encrypted) frame.tag = buf.slice(buf.length - TAG_LEN);
  return frame;
}

// --- Chiffrement : éléments déduits de l'en-tête ------------------------------
// Le vecteur d'initialisation n'est jamais transmis : il se reconstruit des
// champs de l'en-tête. D'où l'importance d'`epoch` — sans lui, `seq` repartirait
// de zéro à chaque démarrage et rejouerait les mêmes vecteurs.
export function nonceFor(f: Pick<Frame, 'src' | 'squad' | 'epoch' | 'seq'>): Uint8Array {
  const n = new Uint8Array(12);
  const dv = new DataView(n.buffer);
  n[0] = f.src & 0xff;
  n[1] = f.squad & 0xff;
  dv.setUint16(2, f.epoch & 0xffff, true);
  dv.setUint16(4, f.seq & 0xffff, true);
  return n;
}

// Données authentifiées : uniquement les champs IMMUABLES de l'en-tête.
// `ttl_flags` en est exclu — un relais le modifie, l'inclure casserait le sceau
// dès la première retransmission.
export function aadFor(f: Partial<Frame> & { type: number }): Uint8Array {
  const unicast = ((f.flags ?? 0) & Flags.UNICAST) !== 0;
  const a = new Uint8Array(unicast ? 8 : 7);
  const dv = new DataView(a.buffer);
  a[0] = ((VERSION & 0x0f) << 4) | (f.type & 0x0f);
  a[1] = (f.squad ?? 0) & 0xff;
  a[2] = (f.src ?? 0) & 0xff;
  dv.setUint16(3, (f.seq ?? 0) & 0xffff, true);
  dv.setUint16(5, (f.epoch ?? 0) & 0xffff, true);
  if (unicast) a[7] = (f.dst ?? 0) & 0xff;
  return a;
}

// --- Tassage des indicateurs de qualité --------------------------------------
const HDOP_BUCKETS = [0.8, 1.0, 1.3, 1.6, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10, 15, 20];
/** Valeur représentative (borne haute) de chaque code, pour l'estimation de précision. */
const HDOP_VALUES = [0.8, 1.0, 1.3, 1.6, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10, 15, 20, 30];

export function encodeHdop(hdop: number): number {
  if (!(hdop > 0)) return 15; // inconnu
  for (let i = 0; i < HDOP_BUCKETS.length; i++) if (hdop < HDOP_BUCKETS[i]) return i;
  return 14;
}

/** Renvoie 0 si le HDOP est inconnu. */
export function decodeHdop(code: number): number {
  return code === 15 ? 0 : HDOP_VALUES[code & 0x0f];
}

/** 0..100 % vers 4 bits ; 255 (inconnu) vers 15. */
export function encodeBattery(pct: number): number {
  if (pct > 100 || pct < 0) return 15;
  return Math.min(14, Math.round((pct * 14) / 100));
}

/** Renvoie 255 si la batterie est inconnue. */
export function decodeBattery(code: number): number {
  return code === 15 ? 255 : Math.round(((code & 0x0f) * 100) / 14);
}

// --- POSITION (12 octets) ----------------------------------------------------
export interface Position {
  lat: number; // degrés × 1e7
  lon: number;
  alt: number; // mètres
  sats: number;
  hdop: number; // valeur réelle, ex. 1.2
  battPct: number; // 0..100, 255 = inconnue
  status: number;
}
export const POSITION_LEN = 12;

export function encodePosition(p: Partial<Position>): Uint8Array {
  const out = new Uint8Array(POSITION_LEN);
  const dv = new DataView(out.buffer);
  dv.setInt32(0, p.lat ?? 0, true);
  dv.setInt32(4, p.lon ?? 0, true);
  dv.setInt16(8, p.alt ?? 0, true);
  out[10] = (Math.min(15, p.sats ?? 0) << 4) | encodeHdop(p.hdop ?? 0);
  out[11] = (encodeBattery(p.battPct ?? 255) << 4) | ((p.status ?? 0) & 0x0f);
  return out;
}

export function decodePosition(b: Uint8Array): Position | null {
  if (b.length < POSITION_LEN) return null;
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  return {
    lat: dv.getInt32(0, true),
    lon: dv.getInt32(4, true),
    alt: dv.getInt16(8, true),
    sats: b[10] >> 4,
    hdop: decodeHdop(b[10] & 0x0f),
    battPct: decodeBattery(b[11] >> 4),
    status: b[11] & 0x0f,
  };
}

// --- NODEINFO ----------------------------------------------------------------
export interface NodeInfo {
  role: number;
  callsign: string;
}

export function encodeNodeInfo(n: NodeInfo): Uint8Array {
  const name = new TextEncoder().encode(n.callsign).slice(0, 16);
  const out = new Uint8Array(1 + name.length);
  out[0] = n.role & 0xff;
  out.set(name, 1);
  return out;
}

export function decodeNodeInfo(b: Uint8Array): NodeInfo | null {
  if (b.length < 1) return null;
  return { role: b[0], callsign: cstr(b.slice(1)) };
}

// --- ACK (3 octets) ----------------------------------------------------------
export function encodeAck(ackSeq: number, ackSrc: number): Uint8Array {
  const out = new Uint8Array(3);
  new DataView(out.buffer).setUint16(0, ackSeq & 0xffff, true);
  out[2] = ackSrc & 0xff;
  return out;
}

export function decodeAck(b: Uint8Array): { ackSeq: number; ackSrc: number } | null {
  if (b.length < 3) return null;
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  return { ackSeq: dv.getUint16(0, true), ackSrc: b[2] };
}

// --- CFG_STATE / CFG_SET (40 octets) -----------------------------------------
export interface Config {
  squad: number;
  addr: number;
  role: number;
  state: number;
  callsign: string;
  squadName: string;
  freqHz: number;
  sf: number;
  txPower: number;
  posIntervalS: number;
  dutyPercent: number;
}
export const CONFIG_LEN = 40;

export const DEFAULT_CONFIG: Config = {
  squad: 0,
  addr: ADDR_UNASSIGNED,
  role: Role.PLAYER,
  state: SquadState.ALONE,
  callsign: '',
  squadName: '',
  freqHz: 869525000,
  sf: 7,
  txPower: 17,
  posIntervalS: 10,
  dutyPercent: 10,
};

export function encodeConfig(c: Config): Uint8Array {
  const out = new Uint8Array(CONFIG_LEN);
  const dv = new DataView(out.buffer);
  out[0] = c.squad & 0xff;
  out[1] = c.addr & 0xff;
  out[2] = c.role & 0xff;
  out[3] = c.state & 0xff;
  out.set(new TextEncoder().encode(c.callsign).slice(0, 16), 4);
  out.set(new TextEncoder().encode(c.squadName).slice(0, 12), 20);
  dv.setUint32(32, c.freqHz >>> 0, true);
  out[36] = c.sf & 0xff;
  dv.setInt8(37, c.txPower);
  out[38] = c.posIntervalS & 0xff;
  out[39] = c.dutyPercent & 0xff;
  return out;
}

export function decodeConfig(b: Uint8Array): Config | null {
  if (b.length < CONFIG_LEN) return null;
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  return {
    squad: b[0],
    addr: b[1],
    role: b[2],
    state: b[3],
    callsign: cstr(b.slice(4, 20)),
    squadName: cstr(b.slice(20, 32)),
    freqHz: dv.getUint32(32, true),
    sf: b[36],
    txPower: dv.getInt8(37),
    posIntervalS: b[38],
    dutyPercent: b[39],
  };
}

// --- STATUS (20 octets) ------------------------------------------------------
export interface DeviceStatus {
  fixValid: boolean;
  lat: number;
  lon: number;
  alt: number;
  sats: number;
  hdop: number;
  battPct: number;
  battMv: number;
  charging: boolean;
  airtimePercent: number;
  lastRssi: number; // dBm, négatif
  lastSnr: number;
  peerCount: number;
}
export const STATUS_LEN = 20;

export function encodeStatus(s: DeviceStatus): Uint8Array {
  const out = new Uint8Array(STATUS_LEN);
  const dv = new DataView(out.buffer);
  out[0] = s.fixValid ? 1 : 0;
  dv.setInt32(1, s.lat, true);
  dv.setInt32(5, s.lon, true);
  dv.setInt16(9, s.alt, true);
  out[11] = (Math.min(15, s.sats) << 4) | encodeHdop(s.hdop);
  out[12] = Math.min(255, Math.max(0, s.battPct));
  dv.setUint16(13, s.battMv & 0xffff, true);
  out[15] = s.charging ? 1 : 0;
  out[16] = Math.min(255, s.airtimePercent);
  out[17] = Math.min(255, Math.abs(s.lastRssi));
  dv.setInt8(18, s.lastSnr);
  out[19] = Math.min(255, s.peerCount);
  return out;
}

export function decodeStatus(b: Uint8Array): DeviceStatus | null {
  if (b.length < STATUS_LEN) return null;
  const dv = new DataView(b.buffer, b.byteOffset, b.byteLength);
  return {
    fixValid: b[0] !== 0,
    lat: dv.getInt32(1, true),
    lon: dv.getInt32(5, true),
    alt: dv.getInt16(9, true),
    sats: b[11] >> 4,
    hdop: decodeHdop(b[11] & 0x0f),
    battPct: b[12],
    battMv: dv.getUint16(13, true),
    charging: b[15] !== 0,
    airtimePercent: b[16],
    lastRssi: -b[17],
    lastSnr: dv.getInt8(18),
    peerCount: b[19],
  };
}

// --- Adhésion à une escouade -------------------------------------------------
export interface JoinEventPayload {
  event: number;
  fingerprint: Uint8Array; // 4 octets
  callsign: string;
}

export function encodeJoinEvent(e: JoinEventPayload): Uint8Array {
  const name = new TextEncoder().encode(e.callsign).slice(0, 16);
  const out = new Uint8Array(6 + name.length);
  out[0] = e.event & 0xff;
  out.set(e.fingerprint.slice(0, 4), 1);
  out[5] = name.length;
  out.set(name, 6);
  return out;
}

export function decodeJoinEvent(b: Uint8Array): JoinEventPayload | null {
  if (b.length < 6) return null;
  const len = Math.min(b[5], b.length - 6);
  return {
    event: b[0],
    fingerprint: b.slice(1, 5),
    callsign: new TextDecoder().decode(b.slice(6, 6 + len)),
  };
}

export function encodeJoinCmd(cmd: number, squadName = '', fingerprint?: Uint8Array): Uint8Array {
  const name = new TextEncoder().encode(squadName).slice(0, 12);
  const out = new Uint8Array(5 + name.length);
  out[0] = cmd & 0xff;
  if (fingerprint) out.set(fingerprint.slice(0, 4), 1);
  out.set(name, 5);
  return out;
}

export function decodeJoinCmd(
  b: Uint8Array,
): { cmd: number; fingerprint: Uint8Array; squadName: string } | null {
  if (b.length < 5) return null;
  return { cmd: b[0], fingerprint: b.slice(1, 5), squadName: cstr(b.slice(5)) };
}

// --- Utilitaires -------------------------------------------------------------
function cstr(b: Uint8Array): string {
  const end = b.indexOf(0);
  return new TextDecoder().decode(end >= 0 ? b.slice(0, end) : b);
}

export function encodeText(text: string): Uint8Array {
  return new TextEncoder().encode(text).slice(0, MAX_PAYLOAD);
}

export function decodeText(b: Uint8Array): string {
  return new TextDecoder().decode(b);
}

/** Identifiant d'escouade déduit du nom : rien à coordonner sur le terrain. */
export function squadIdFromName(name: string, sha256: (d: Uint8Array) => Uint8Array): number {
  return sha256(new TextEncoder().encode(name.toUpperCase()))[0];
}

export function fingerprintHex(fp: Uint8Array): string {
  return Array.from(fp.slice(0, 4), (b) => b.toString(16).toUpperCase().padStart(2, '0')).join('');
}

export function toDegrees(v: number): number {
  return v / 1e7;
}

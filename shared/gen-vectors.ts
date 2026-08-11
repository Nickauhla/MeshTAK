// -----------------------------------------------------------------------------
// Génère les vecteurs de test PARTAGÉS entre les deux implémentations.
//
//   node shared/gen-vectors.ts
//
// Produit :
//   shared/testvectors.json     -> app/src/lib/proto/*.test.ts
//   firmware/include/vectors.h  -> test natif du codec ET autotest embarqué
//
// L'en-tête C++ va dans `include/` et non dans `test/` parce qu'il sert deux fois :
// au test natif du codec sur PC, et à l'autotest cryptographique qui tourne sur la
// carte au démarrage — mbedtls n'existant que sur la cible, c'est le seul endroit
// où l'on peut confronter l'implémentation embarquée à OpenSSL.
//
// Le TypeScript + Node/OpenSSL font foi ; le C++ + mbedtls doivent produire
// exactement les mêmes octets. C'est ce qui garantit que le téléphone, le T-Beam
// et la radio parlent la même langue — y compris pour la cryptographie, où une
// divergence d'un seul octet (ordre du vecteur d'initialisation, champs
// authentifiés, troncature du sceau) rend tout illisible sans rien signaler.
// -----------------------------------------------------------------------------
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import {
  FrameType,
  Flags,
  MarkerKind,
  PlayerStatus,
  Role,
  SquadState,
  aadFor,
  encodeConfig,
  encodeFrame,
  encodeMarker,
  encodeNodeInfo,
  encodePosition,
  encodeStatus,
  encodeText,
  nonceFor,
  squadIdFromName,
} from '../app/src/lib/proto/frames.ts';
import { slipEncode } from '../app/src/lib/proto/slip.ts';
import {
  fingerprint,
  joinWrapKey,
  publicFromPrivate,
  seal,
  sha256,
  x25519,
} from './crypto-ref.ts';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const hex = (b: Uint8Array) => Buffer.from(b).toString('hex');
const unhex = (s: string) => Uint8Array.from(Buffer.from(s, 'hex'));

// --- Matériel cryptographique FIXE (vecteurs déterministes) -------------------
const SQUAD_KEY = unhex('000102030405060708090a0b0c0d0e0f');
const ADMIN_PRIV = unhex('a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf');
const CAND_PRIV = unhex('505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f');
const GRANT_NONCE = unhex('0102030405060708090a0b0c');

const ADMIN_PUB = publicFromPrivate(ADMIN_PRIV);
const CAND_PUB = publicFromPrivate(CAND_PRIV);
const CAND_FP = fingerprint(CAND_PUB);
const WRAP_KEY = joinWrapKey(x25519(ADMIN_PRIV, CAND_PUB));

const SQUAD_ALPHA = squadIdFromName('Alpha', sha256);

interface Case {
  name: string;
  type: number;
  flags: number;
  ttl: number;
  squad: number;
  src: number;
  seq: number;
  epoch: number;
  dst?: number;
  /** Charge utile en clair. Chiffrée à l'encodage si le drapeau ENCRYPTED est posé. */
  plain: Uint8Array;
}

const cases: Case[] = [
  {
    name: 'position_claire',
    type: FrameType.POSITION,
    flags: 0,
    ttl: 3,
    squad: SQUAD_ALPHA,
    src: 1,
    seq: 1,
    epoch: 7,
    plain: encodePosition({
      lat: 488566000,
      lon: 23522000,
      alt: 35,
      sats: 11,
      hdop: 1.2,
      battPct: 87,
      status: PlayerStatus.OK,
    }),
  },
  {
    name: 'position_chiffree',
    type: FrameType.POSITION,
    flags: Flags.ENCRYPTED,
    ttl: 3,
    squad: SQUAD_ALPHA,
    src: 5,
    seq: 4242,
    epoch: 9,
    plain: encodePosition({
      lat: 488566000,
      lon: 23522000,
      alt: 35,
      sats: 11,
      hdop: 1.2,
      battPct: 87,
      status: PlayerStatus.OK,
    }),
  },
  {
    name: 'position_chiffree_relayee',
    // Même trame que ci-dessus mais relayée : TTL décrémenté et bit RELAYED posé.
    // Le sceau doit rester valide — c'est tout l'intérêt d'exclure `ttl_flags`
    // des données authentifiées.
    type: FrameType.POSITION,
    flags: Flags.ENCRYPTED | Flags.RELAYED,
    ttl: 2,
    squad: SQUAD_ALPHA,
    src: 5,
    seq: 4242,
    epoch: 9,
    plain: encodePosition({
      lat: 488566000,
      lon: 23522000,
      alt: 35,
      sats: 11,
      hdop: 1.2,
      battPct: 87,
      status: PlayerStatus.OK,
    }),
  },
  {
    name: 'position_sud_ouest_negatif',
    type: FrameType.POSITION,
    flags: Flags.ENCRYPTED,
    ttl: 1,
    squad: 255,
    src: 31,
    seq: 65535,
    epoch: 65535,
    plain: encodePosition({
      lat: -335678900,
      lon: -704567800,
      alt: -12,
      sats: 4,
      hdop: 25,
      battPct: 0,
      status: PlayerStatus.ELIMINATED,
    }),
  },
  {
    name: 'texte_unicast_accents',
    type: FrameType.TEXT,
    flags: Flags.ENCRYPTED | Flags.UNICAST | Flags.WANT_ACK,
    ttl: 3,
    squad: SQUAD_ALPHA,
    src: 2,
    dst: 7,
    seq: 7,
    epoch: 1,
    plain: encodeText('Contact à 2h — repli sur le point bleu'),
  },
  {
    name: 'texte_octets_slip',
    type: FrameType.TEXT,
    flags: 0,
    ttl: 2,
    squad: 1,
    src: 3,
    seq: 192,
    epoch: 0,
    plain: new Uint8Array([0xc0, 0xdb, 0xc0, 0xdb, 0x41, 0x42]),
  },
  {
    name: 'nodeinfo_chef',
    type: FrameType.NODEINFO,
    flags: Flags.ENCRYPTED,
    ttl: 3,
    squad: SQUAD_ALPHA,
    src: 1,
    seq: 2,
    epoch: 7,
    plain: encodeNodeInfo({ role: Role.LEADER, callsign: 'ALPHA-1' }),
  },
  {
    name: 'cfg_state_local',
    type: FrameType.CFG_STATE,
    flags: Flags.LOCAL,
    ttl: 0,
    squad: SQUAD_ALPHA,
    src: 1,
    seq: 0,
    epoch: 7,
    plain: encodeConfig({
      squad: SQUAD_ALPHA,
      addr: 1,
      role: Role.LEADER,
      state: SquadState.LEADER,
      callsign: 'ALPHA-1',
      squadName: 'Alpha',
      freqHz: 869525000,
      sf: 7,
      txPower: -9,
      posIntervalS: 10,
      dutyPercent: 10,
    }),
  },
  {
    name: 'status_local',
    type: FrameType.STATUS,
    flags: Flags.LOCAL,
    ttl: 0,
    squad: SQUAD_ALPHA,
    src: 1,
    seq: 0,
    epoch: 7,
    plain: encodeStatus({
      fixValid: true,
      lat: 487144000,
      lon: 27397810,
      alt: 97,
      sats: 12,
      hdop: 0.6,
      battPct: 100,
      battMv: 4150,
      charging: true,
      airtimePercent: 3,
      lastRssi: -29,
      lastSnr: 12,
      peerCount: 2,
    }),
  },
  {
    name: 'marqueur_ennemi_chiffre',
    type: FrameType.MARKER,
    flags: Flags.ENCRYPTED,
    ttl: 3,
    squad: SQUAD_ALPHA,
    src: 3,
    seq: 88,
    epoch: 12,
    plain: encodeMarker({
      owner: 3,
      id: 17,
      kind: MarkerKind.HOSTILE,
      lat: 488570000,
      lon: 23530000,
      ttlMin: 10,
      label: 'Contact 2h',
    }),
  },
  {
    name: 'marqueur_suppression',
    // Retrait du point d'un AUTRE membre : `owner` (3) diffère de `src` (7).
    // C'est exactement pourquoi le créateur est dans la charge utile.
    type: FrameType.MARKER,
    flags: Flags.ENCRYPTED,
    ttl: 3,
    squad: SQUAD_ALPHA,
    src: 7,
    seq: 89,
    epoch: 12,
    plain: encodeMarker({ owner: 3, id: 17, kind: MarkerKind.CLEAR }),
  },
  {
    name: 'charge_utile_maximale',
    type: FrameType.TEXT,
    flags: Flags.ENCRYPTED,
    ttl: 3,
    squad: 1,
    src: 31,
    seq: 1000,
    epoch: 3,
    plain: new Uint8Array(180).map((_, i) => i & 0xff),
  },
];

// --- Construction des trames -------------------------------------------------
function build(c: Case): { wire: Uint8Array; cipher: Uint8Array; tag: Uint8Array } {
  const encrypted = (c.flags & Flags.ENCRYPTED) !== 0;
  if (!encrypted) {
    return { wire: encodeFrame({ ...c, payload: c.plain }), cipher: c.plain, tag: new Uint8Array(0) };
  }
  const { ciphertext, tag } = seal(SQUAD_KEY, nonceFor(c), aadFor(c), c.plain);
  return { wire: encodeFrame({ ...c, payload: ciphertext, tag }), cipher: ciphertext, tag };
}

// --- Vecteur JOIN_GRANT ------------------------------------------------------
const grantPlain = (() => {
  const name = new TextEncoder().encode('Alpha');
  const out = new Uint8Array(30);
  out.set(SQUAD_KEY, 0);
  out[16] = 2; // adresse attribuée
  out[17] = name.length;
  out.set(name, 18);
  return out;
})();
const grantSealed = seal(WRAP_KEY, GRANT_NONCE, new Uint8Array(0), grantPlain);

// --- Sorties -----------------------------------------------------------------
const json = {
  _comment: 'Généré par shared/gen-vectors.ts — ne pas éditer. Voir shared/PROTOCOL.md.',
  squadKeyHex: hex(SQUAD_KEY),
  squadAlpha: SQUAD_ALPHA,
  frames: cases.map((c) => {
    const b = build(c);
    return {
      name: c.name,
      type: c.type,
      flags: c.flags,
      ttl: c.ttl,
      squad: c.squad,
      src: c.src,
      dst: c.dst ?? null,
      seq: c.seq,
      epoch: c.epoch,
      plainHex: hex(c.plain),
      nonceHex: hex(nonceFor(c)),
      aadHex: hex(aadFor(c)),
      cipherHex: hex(b.cipher),
      tagHex: hex(b.tag),
      wireHex: hex(b.wire),
    };
  }),
  slip: cases.slice(0, 6).map((c) => {
    const b = build(c);
    return { name: c.name, wireHex: hex(b.wire), slipHex: hex(slipEncode(b.wire)) };
  }),
  join: {
    adminPrivHex: hex(ADMIN_PRIV),
    adminPubHex: hex(ADMIN_PUB),
    candPrivHex: hex(CAND_PRIV),
    candPubHex: hex(CAND_PUB),
    candFingerprintHex: hex(CAND_FP),
    sharedSecretHex: hex(x25519(ADMIN_PRIV, CAND_PUB)),
    wrapKeyHex: hex(WRAP_KEY),
    nonceHex: hex(GRANT_NONCE),
    plainHex: hex(grantPlain),
    cipherHex: hex(grantSealed.ciphertext),
    tagHex: hex(grantSealed.tag),
  },
};

mkdirSync(join(ROOT, 'shared'), { recursive: true });
writeFileSync(join(ROOT, 'shared', 'testvectors.json'), JSON.stringify(json, null, 2) + '\n');

// --- En-tête C++ -------------------------------------------------------------
const cArray = (b: Uint8Array) =>
  b.length === 0
    ? '{0}'
    : '{' + Array.from(b, (v) => '0x' + v.toString(16).padStart(2, '0')).join(', ') + '}';

let h = `#pragma once
// GÉNÉRÉ par shared/gen-vectors.ts — NE PAS ÉDITER.
// Ces octets font foi : produits par le codec TypeScript et OpenSSL (Node).
#include <stddef.h>
#include <stdint.h>

struct TestVector {
    const char *name;
    uint8_t type, flags, ttl, squad, src;
    int16_t dst;  // -1 si diffusion
    uint16_t seq, epoch;
    const uint8_t *plain;
    size_t plainLen;
    const uint8_t *nonce;
    const uint8_t *aad;
    size_t aadLen;
    const uint8_t *cipher;
    size_t cipherLen;
    const uint8_t *tag;
    size_t tagLen;
    const uint8_t *wire;
    size_t wireLen;
};

static const uint8_t SQUAD_KEY[] = ${cArray(SQUAD_KEY)};

`;

const built = cases.map(build);
cases.forEach((c, i) => {
  h += `static const uint8_t v${i}_plain[] = ${cArray(c.plain)};\n`;
  h += `static const uint8_t v${i}_nonce[] = ${cArray(nonceFor(c))};\n`;
  h += `static const uint8_t v${i}_aad[] = ${cArray(aadFor(c))};\n`;
  h += `static const uint8_t v${i}_cipher[] = ${cArray(built[i].cipher)};\n`;
  h += `static const uint8_t v${i}_tag[] = ${cArray(built[i].tag)};\n`;
  h += `static const uint8_t v${i}_wire[] = ${cArray(built[i].wire)};\n`;
});

h += `\nstatic const TestVector VECTORS[] = {\n`;
cases.forEach((c, i) => {
  h +=
    `    {"${c.name}", ${c.type}, 0x${c.flags.toString(16)}, ${c.ttl}, ${c.squad}, ${c.src}, ` +
    `${c.dst ?? -1}, ${c.seq}, ${c.epoch}, ` +
    `v${i}_plain, ${c.plain.length}, v${i}_nonce, v${i}_aad, ${aadFor(c).length}, ` +
    `v${i}_cipher, ${built[i].cipher.length}, v${i}_tag, ${built[i].tag.length}, ` +
    `v${i}_wire, ${built[i].wire.length}},\n`;
});
h += `};\nstatic const size_t VECTOR_COUNT = ${cases.length};\n\n`;

h += `// --- Adhésion à une escouade (X25519 + AES-GCM) ---
static const uint8_t JOIN_ADMIN_PRIV[] = ${cArray(ADMIN_PRIV)};
static const uint8_t JOIN_ADMIN_PUB[] = ${cArray(ADMIN_PUB)};
static const uint8_t JOIN_CAND_PRIV[] = ${cArray(CAND_PRIV)};
static const uint8_t JOIN_CAND_PUB[] = ${cArray(CAND_PUB)};
static const uint8_t JOIN_CAND_FP[] = ${cArray(CAND_FP)};
static const uint8_t JOIN_SHARED[] = ${cArray(x25519(ADMIN_PRIV, CAND_PUB))};
static const uint8_t JOIN_WRAP_KEY[] = ${cArray(WRAP_KEY)};
static const uint8_t JOIN_NONCE[] = ${cArray(GRANT_NONCE)};
static const uint8_t JOIN_PLAIN[] = ${cArray(grantPlain)};
static const uint8_t JOIN_CIPHER[] = ${cArray(grantSealed.ciphertext)};
static const uint8_t JOIN_TAG[] = ${cArray(grantSealed.tag)};
`;

mkdirSync(join(ROOT, 'firmware', 'include'), { recursive: true });
writeFileSync(join(ROOT, 'firmware', 'include', 'vectors.h'), h);

console.log(`✓ ${cases.length} vecteurs de trame + 1 vecteur d'adhésion`);
console.log(`  escouade « Alpha » -> identifiant ${SQUAD_ALPHA}`);
console.log('  shared/testvectors.json');
console.log('  firmware/include/vectors.h');

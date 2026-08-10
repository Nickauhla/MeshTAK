// -----------------------------------------------------------------------------
// Le codec est le cœur du système : si l'app et le firmware divergent d'un seul
// octet, plus rien ne passe. Ces tests le figent contre les vecteurs partagés
// (shared/testvectors.json), les mêmes que ceux du test natif du firmware.
//
// Les vérifications cryptographiques s'appuient sur l'implémentation de référence
// Node/OpenSSL — c'est elle que mbedtls devra reproduire côté firmware.
// -----------------------------------------------------------------------------
import { describe, expect, it } from 'vitest';

import vectors from '../../../../shared/testvectors.json';
import {
  fingerprint,
  joinWrapKey,
  open,
  publicFromPrivate,
  seal,
  sha256,
  x25519,
} from '../../../../shared/crypto-ref.ts';
import {
  ADDR_MAX,
  Flags,
  FrameType,
  JoinEvent,
  MAX_PAYLOAD,
  PlayerStatus,
  Role,
  SquadState,
  aadFor,
  decodeAck,
  decodeConfig,
  decodeFrame,
  decodeHdop,
  decodeJoinCmd,
  decodeJoinEvent,
  decodeNodeInfo,
  decodePosition,
  decodeStatus,
  encodeAck,
  encodeBattery,
  encodeConfig,
  encodeFrame,
  encodeHdop,
  encodeJoinCmd,
  encodeJoinEvent,
  encodeNodeInfo,
  encodePosition,
  encodeStatus,
  encodeText,
  fingerprintHex,
  nonceFor,
  squadIdFromName,
} from './frames.ts';
import { SlipDecoder, slipEncode } from './slip.ts';

const fromHex = (h: string) => Uint8Array.from(Buffer.from(h, 'hex'));
const toHex = (b: Uint8Array) => Buffer.from(b).toString('hex');
const SQUAD_KEY = fromHex(vectors.squadKeyHex);

function frameFrom(v: (typeof vectors.frames)[number]) {
  return {
    type: v.type,
    flags: v.flags,
    ttl: v.ttl,
    squad: v.squad,
    src: v.src,
    seq: v.seq,
    epoch: v.epoch,
    ...(v.dst !== null ? { dst: v.dst } : {}),
  };
}

describe('trames v2 — vecteurs partagés avec le firmware', () => {
  for (const v of vectors.frames) {
    it(`encode « ${v.name} » octet pour octet`, () => {
      const bytes = encodeFrame({
        ...frameFrom(v),
        payload: fromHex(v.cipherHex),
        tag: v.tagHex ? fromHex(v.tagHex) : undefined,
      });
      expect(toHex(bytes)).toBe(v.wireHex);
    });

    it(`décode « ${v.name} »`, () => {
      const f = decodeFrame(fromHex(v.wireHex));
      expect(f).not.toBeNull();
      expect(f!.type).toBe(v.type);
      expect(f!.flags).toBe(v.flags);
      expect(f!.ttl).toBe(v.ttl);
      expect(f!.squad).toBe(v.squad);
      expect(f!.src).toBe(v.src);
      expect(f!.seq).toBe(v.seq);
      expect(f!.epoch).toBe(v.epoch);
      expect(f!.dst ?? null).toBe(v.dst);
      expect(toHex(f!.payload)).toBe(v.cipherHex);
      expect(f!.tag ? toHex(f!.tag) : '').toBe(v.tagHex);
    });

    it(`dérive le même vecteur d'initialisation et AAD pour « ${v.name} »`, () => {
      expect(toHex(nonceFor(frameFrom(v)))).toBe(v.nonceHex);
      expect(toHex(aadFor(frameFrom(v)))).toBe(v.aadHex);
    });
  }

  it('produit une position chiffrée de 28 octets', () => {
    const v = vectors.frames.find((f) => f.name === 'position_chiffree')!;
    expect(v.wireHex.length / 2).toBe(28);
  });
});

describe('chiffrement', () => {
  for (const v of vectors.frames.filter((f) => (f.flags & Flags.ENCRYPTED) !== 0)) {
    it(`déchiffre « ${v.name} » et retrouve le clair`, () => {
      const plain = open(
        SQUAD_KEY,
        fromHex(v.nonceHex),
        fromHex(v.aadHex),
        fromHex(v.cipherHex),
        fromHex(v.tagHex),
      );
      expect(plain).not.toBeNull();
      expect(toHex(plain!)).toBe(v.plainHex);
    });
  }

  // La règle sans laquelle le mesh s'effondre dès le premier relais.
  it('le sceau survit au relais : ttl et flags sont hors des données authentifiées', () => {
    const direct = vectors.frames.find((f) => f.name === 'position_chiffree')!;
    const relayed = vectors.frames.find((f) => f.name === 'position_chiffree_relayee')!;

    expect(relayed.ttl).toBe(direct.ttl - 1);
    expect(relayed.flags & Flags.RELAYED).toBeTruthy();
    // L'octet ttl_flags a bel et bien changé sur le fil…
    expect(relayed.wireHex.slice(2, 4)).not.toBe(direct.wireHex.slice(2, 4));
    // …mais rien de ce qui est authentifié n'a bougé.
    expect(relayed.aadHex).toBe(direct.aadHex);
    expect(relayed.cipherHex).toBe(direct.cipherHex);
    expect(relayed.tagHex).toBe(direct.tagHex);

    const plain = open(
      SQUAD_KEY,
      fromHex(relayed.nonceHex),
      fromHex(relayed.aadHex),
      fromHex(relayed.cipherHex),
      fromHex(relayed.tagHex),
    );
    expect(plain).not.toBeNull();
  });

  it('rejette un chiffré altéré', () => {
    const v = vectors.frames.find((f) => f.name === 'position_chiffree')!;
    const bad = fromHex(v.cipherHex);
    bad[0] ^= 0x01;
    expect(open(SQUAD_KEY, fromHex(v.nonceHex), fromHex(v.aadHex), bad, fromHex(v.tagHex))).toBeNull();
  });

  it('rejette une trame venue d’une autre escouade (mauvaise clé)', () => {
    const v = vectors.frames.find((f) => f.name === 'position_chiffree')!;
    const otherKey = new Uint8Array(16).fill(0xaa);
    expect(
      open(otherKey, fromHex(v.nonceHex), fromHex(v.aadHex), fromHex(v.cipherHex), fromHex(v.tagHex)),
    ).toBeNull();
  });

  it('rejette une usurpation d’adresse : l’AAD lie la trame à son émetteur', () => {
    const v = vectors.frames.find((f) => f.name === 'position_chiffree')!;
    const forged = aadFor({ ...frameFrom(v), src: v.src + 1 });
    expect(
      open(SQUAD_KEY, fromHex(v.nonceHex), forged, fromHex(v.cipherHex), fromHex(v.tagHex)),
    ).toBeNull();
  });

  it('produit un vecteur d’initialisation unique par (src, epoch, seq)', () => {
    const base = { src: 3, squad: 9, epoch: 1, seq: 1 };
    const seen = new Set<string>();
    for (const f of [
      base,
      { ...base, src: 4 },
      { ...base, epoch: 2 },
      { ...base, seq: 2 },
      { ...base, squad: 10 },
    ]) {
      seen.add(toHex(nonceFor(f)));
    }
    expect(seen.size).toBe(5);
  });
});

describe('adhésion à une escouade', () => {
  const j = vectors.join;

  it('reconstitue les clés publiques depuis les scalaires privés', () => {
    expect(toHex(publicFromPrivate(fromHex(j.adminPrivHex)))).toBe(j.adminPubHex);
    expect(toHex(publicFromPrivate(fromHex(j.candPrivHex)))).toBe(j.candPubHex);
  });

  it('les deux côtés calculent le même secret partagé', () => {
    const fromAdmin = x25519(fromHex(j.adminPrivHex), fromHex(j.candPubHex));
    const fromCandidate = x25519(fromHex(j.candPrivHex), fromHex(j.adminPubHex));
    expect(toHex(fromAdmin)).toBe(toHex(fromCandidate));
    expect(toHex(fromAdmin)).toBe(j.sharedSecretHex);
  });

  it('le candidat déchiffre la clé d’escouade, un tiers non', () => {
    const wrap = joinWrapKey(x25519(fromHex(j.candPrivHex), fromHex(j.adminPubHex)));
    expect(toHex(wrap)).toBe(j.wrapKeyHex);

    const plain = open(
      wrap,
      fromHex(j.nonceHex),
      new Uint8Array(0),
      fromHex(j.cipherHex),
      fromHex(j.tagHex),
    );
    expect(plain).not.toBeNull();
    expect(toHex(plain!)).toBe(j.plainHex);
    expect(toHex(plain!.slice(0, 16))).toBe(vectors.squadKeyHex);
    expect(plain![16]).toBe(2); // adresse attribuée

    const intruderKey = joinWrapKey(new Uint8Array(32).fill(7));
    expect(
      open(intruderKey, fromHex(j.nonceHex), new Uint8Array(0), fromHex(j.cipherHex), fromHex(j.tagHex)),
    ).toBeNull();
  });

  it('calcule une empreinte stable et affichable', () => {
    expect(toHex(fingerprint(fromHex(j.candPubHex)))).toBe(j.candFingerprintHex);
    expect(fingerprintHex(fromHex(j.candFingerprintHex))).toHaveLength(8);
  });

  it('déduit l’identifiant d’escouade du nom, insensible à la casse', () => {
    expect(squadIdFromName('Alpha', sha256)).toBe(vectors.squadAlpha);
    expect(squadIdFromName('ALPHA', sha256)).toBe(vectors.squadAlpha);
    expect(squadIdFromName('Bravo', sha256)).not.toBe(vectors.squadAlpha);
  });

  it('code et décode les échanges locaux d’adhésion', () => {
    const fp = fromHex(j.candFingerprintHex);
    const ev = decodeJoinEvent(encodeJoinEvent({ event: 0, fingerprint: fp, callsign: 'TB-E2B2' }));
    expect(ev).toEqual({ event: 0, fingerprint: fp, callsign: 'TB-E2B2' });

    const cmd = decodeJoinCmd(encodeJoinCmd(2, 'Alpha', fp));
    expect(cmd!.cmd).toBe(2);
    expect(cmd!.squadName).toBe('Alpha');
    expect(toHex(cmd!.fingerprint)).toBe(j.candFingerprintHex);
  });

  it('transporte l’identité du device sous le même format', () => {
    const fp = fromHex(j.candFingerprintHex);
    const wire = encodeJoinEvent({ event: JoinEvent.IDENTITY, fingerprint: fp, callsign: 'ROMEO' });
    // Le codec ne change pas : seule la valeur d'événement est nouvelle.
    expect(decodeJoinEvent(wire)).toEqual({
      event: JoinEvent.IDENTITY,
      fingerprint: fp,
      callsign: 'ROMEO',
    });
  });
});

describe('robustesse du décodage', () => {
  const wire = fromHex(vectors.frames[0].wireHex);

  it('rejette une version inconnue', () => {
    const bad = wire.slice();
    bad[0] = (9 << 4) | (bad[0] & 0x0f);
    expect(decodeFrame(bad)).toBeNull();
  });

  it('rejette une trame tronquée', () => {
    expect(decodeFrame(wire.slice(0, 7))).toBeNull();
    expect(decodeFrame(new Uint8Array(0))).toBeNull();
  });

  it('rejette une trame chiffrée trop courte pour porter son sceau', () => {
    const short = new Uint8Array(10);
    short[0] = 2 << 4;
    short[1] = Flags.ENCRYPTED;
    expect(decodeFrame(short)).toBeNull();
  });

  it('refuse une charge utile de plus de 180 octets', () => {
    expect(() =>
      encodeFrame({ type: FrameType.TEXT, payload: new Uint8Array(MAX_PAYLOAD + 1) }),
    ).toThrow();
  });

  it('refuse un unicast sans destinataire et un chiffré sans sceau', () => {
    expect(() => encodeFrame({ type: FrameType.TEXT, flags: Flags.UNICAST })).toThrow();
    expect(() => encodeFrame({ type: FrameType.TEXT, flags: Flags.ENCRYPTED })).toThrow();
  });
});

describe('charges utiles', () => {
  it('conserve les coordonnées et l’altitude négatives', () => {
    const p = {
      lat: -335678900,
      lon: -704567800,
      alt: -12,
      sats: 4,
      hdop: 5.0,
      battPct: 0,
      status: PlayerStatus.ELIMINATED,
    };
    const back = decodePosition(encodePosition(p))!;
    expect(back.lat).toBe(p.lat);
    expect(back.lon).toBe(p.lon);
    expect(back.alt).toBe(p.alt);
    expect(back.sats).toBe(4);
    expect(back.status).toBe(PlayerStatus.ELIMINATED);
  });

  it('sature les satellites à 15 sans déborder sur le HDOP', () => {
    const back = decodePosition(encodePosition({ sats: 27, hdop: 1.2 }))!;
    expect(back.sats).toBe(15);
    expect(back.hdop).toBe(1.3);
  });

  it('code le HDOP par paliers croissants', () => {
    expect(encodeHdop(0.6)).toBe(0);
    expect(encodeHdop(1.2)).toBe(2);
    expect(encodeHdop(25)).toBe(14);
    expect(encodeHdop(0)).toBe(15);
    expect(decodeHdop(15)).toBe(0); // inconnu
    // Monotone : un HDOP plus grand ne doit jamais donner un code plus petit.
    let prev = -1;
    for (const h of [0.5, 0.9, 1.2, 1.9, 3.5, 7, 12, 30]) {
      const c = encodeHdop(h);
      expect(c).toBeGreaterThan(prev);
      prev = c;
    }
  });

  it('code la batterie sur 4 bits avec une erreur bornée', () => {
    for (const pct of [0, 12, 37, 50, 87, 100]) {
      expect(Math.abs(pct - (encodeBattery(pct) * 100) / 14)).toBeLessThanOrEqual(4);
    }
    expect(encodeBattery(255)).toBe(15);
  });

  it('conserve la configuration, indicatif et puissance négative compris', () => {
    const c = {
      squad: 115,
      addr: ADDR_MAX,
      role: Role.LEADER,
      state: SquadState.LEADER,
      callsign: 'ALPHA-1',
      squadName: 'Alpha',
      freqHz: 869525000,
      sf: 7,
      txPower: -9,
      posIntervalS: 10,
      dutyPercent: 10,
    };
    expect(decodeConfig(encodeConfig(c))).toEqual(c);
  });

  it('conserve le diagnostic device, RSSI négatif compris', () => {
    const s = {
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
    };
    const back = decodeStatus(encodeStatus(s))!;
    expect(back.lastRssi).toBe(-29);
    expect(back.lastSnr).toBe(12);
    expect(back.battMv).toBe(4150);
    expect(back.hdop).toBe(0.8); // palier
  });

  it('décode NODEINFO et ACK', () => {
    expect(decodeNodeInfo(encodeNodeInfo({ role: 1, callsign: 'ALPHA-1' }))).toEqual({
      role: 1,
      callsign: 'ALPHA-1',
    });
    expect(decodeAck(encodeAck(4242, 7))).toEqual({ ackSeq: 4242, ackSrc: 7 });
  });
});

describe('cadrage SLIP', () => {
  for (const v of vectors.slip) {
    it(`encode « ${v.name} » conformément aux vecteurs`, () => {
      expect(toHex(slipEncode(fromHex(v.wireHex)))).toBe(v.slipHex);
    });
  }

  it('reconstitue une trame arrivée en fragments de 3 octets', () => {
    const wire = fromHex(vectors.frames.find((f) => f.name === 'texte_octets_slip')!.wireHex);
    const received: Uint8Array[] = [];
    const decoder = new SlipDecoder((f) => received.push(f));
    const framed = slipEncode(wire);
    for (let i = 0; i < framed.length; i += 3) decoder.feed(framed.slice(i, i + 3));

    expect(received).toHaveLength(1);
    expect(toHex(received[0])).toBe(toHex(wire));
    expect(decodeFrame(received[0])).not.toBeNull();
  });

  it('sépare deux trames collées et ignore les délimiteurs vides', () => {
    const a = encodeFrame({ type: FrameType.TEXT, payload: encodeText('un') });
    const b = encodeFrame({ type: FrameType.TEXT, payload: encodeText('deux') });
    const received: Uint8Array[] = [];
    const decoder = new SlipDecoder((f) => received.push(f));

    decoder.feed(new Uint8Array([0xc0, 0xc0]));
    decoder.feed(slipEncode(a));
    decoder.feed(slipEncode(b));

    expect(received).toHaveLength(2);
    expect(toHex(received[0])).toBe(toHex(a));
    expect(toHex(received[1])).toBe(toHex(b));
  });
});

// -----------------------------------------------------------------------------
// Implémentation de RÉFÉRENCE de la cryptographie MeshRadio, en Node.
//
// Elle n'est PAS embarquée dans l'app : le chiffrement vit entièrement dans le
// firmware. Elle sert uniquement à produire les vecteurs de test que le C++ doit
// reproduire à l'octet près — c'est-à-dire à valider mbedtls contre une
// implémentation indépendante (OpenSSL, via Node).
//
// Voir shared/PROTOCOL.md §6 et §5.
// -----------------------------------------------------------------------------
import { createCipheriv, createDecipheriv, createHash, createPrivateKey, createPublicKey, diffieHellman } from 'node:crypto';

export const TAG_LEN = 8;
export const KEY_LEN = 16;
export const JOIN_CONTEXT = 'MESHRADIO-JOIN-v2';

export function sha256(data: Uint8Array): Uint8Array {
  return new Uint8Array(createHash('sha256').update(data).digest());
}

// --- AES-128-GCM, sceau tronqué à 8 octets -----------------------------------
export function seal(
  key: Uint8Array,
  nonce: Uint8Array,
  aad: Uint8Array,
  plaintext: Uint8Array,
): { ciphertext: Uint8Array; tag: Uint8Array } {
  const c = createCipheriv('aes-128-gcm', key, nonce, { authTagLength: TAG_LEN });
  c.setAAD(aad);
  const ciphertext = Buffer.concat([c.update(plaintext), c.final()]);
  return { ciphertext: new Uint8Array(ciphertext), tag: new Uint8Array(c.getAuthTag()) };
}

export function open(
  key: Uint8Array,
  nonce: Uint8Array,
  aad: Uint8Array,
  ciphertext: Uint8Array,
  tag: Uint8Array,
): Uint8Array | null {
  try {
    const d = createDecipheriv('aes-128-gcm', key, nonce, { authTagLength: TAG_LEN });
    d.setAAD(aad);
    d.setAuthTag(tag);
    return new Uint8Array(Buffer.concat([d.update(ciphertext), d.final()]));
  } catch {
    return null; // sceau invalide : la trame est jetée silencieusement
  }
}

// --- X25519 ------------------------------------------------------------------
// Node ne prend pas de clé brute : on l'habille des en-têtes DER standard, ce qui
// permet de partir d'un scalaire fixe et donc d'obtenir des vecteurs déterministes.
const PKCS8_X25519 = Buffer.from('302e020100300506032b656e04220420', 'hex');
const SPKI_X25519 = Buffer.from('302a300506032b656e032100', 'hex');

export function privateKeyFromRaw(raw: Uint8Array) {
  return createPrivateKey({
    key: Buffer.concat([PKCS8_X25519, Buffer.from(raw)]),
    format: 'der',
    type: 'pkcs8',
  });
}

export function publicKeyFromRaw(raw: Uint8Array) {
  return createPublicKey({
    key: Buffer.concat([SPKI_X25519, Buffer.from(raw)]),
    format: 'der',
    type: 'spki',
  });
}

/** Clé publique brute (32 octets) correspondant à un scalaire privé. */
export function publicFromPrivate(rawPrivate: Uint8Array): Uint8Array {
  const der = createPublicKey(privateKeyFromRaw(rawPrivate)).export({
    format: 'der',
    type: 'spki',
  });
  return new Uint8Array(der.subarray(der.length - 32));
}

export function x25519(rawPrivate: Uint8Array, rawPeerPublic: Uint8Array): Uint8Array {
  return new Uint8Array(
    diffieHellman({
      privateKey: privateKeyFromRaw(rawPrivate),
      publicKey: publicKeyFromRaw(rawPeerPublic),
    }),
  );
}

/** Empreinte affichée au chef pour vérifier de visu qui il valide. */
export function fingerprint(rawPublic: Uint8Array): Uint8Array {
  return sha256(rawPublic).slice(0, 4);
}

/** Clé d'enveloppe d'un JOIN_GRANT : seul le candidat visé peut la recalculer. */
export function joinWrapKey(sharedSecret: Uint8Array): Uint8Array {
  const ctx = new TextEncoder().encode(JOIN_CONTEXT);
  const buf = new Uint8Array(sharedSecret.length + ctx.length);
  buf.set(sharedSecret, 0);
  buf.set(ctx, sharedSecret.length);
  return sha256(buf).slice(0, KEY_LEN);
}

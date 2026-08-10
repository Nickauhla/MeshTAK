#pragma once

namespace selftest {

// Confronte le codec et la cryptographie embarqués aux vecteurs générés par
// l'implémentation de référence Node/OpenSSL (shared/gen-vectors.ts).
//
// Cet autotest tourne sur la CARTE, et pas sur le PC, parce que mbedtls n'existe
// que là : c'est le seul endroit où l'on peut vérifier que l'AES-GCM et le X25519
// embarqués produisent exactement les mêmes octets qu'OpenSSL. Une divergence
// silencieuse (ordre du vecteur d'initialisation, champs authentifiés, troncature
// du sceau, « clamping » du scalaire) rendrait le réseau muet sans rien signaler.
//
// Renvoie true si tout passe ; le détail est écrit sur la console série.
bool run();

}  // namespace selftest

#pragma once
#include <stdint.h>

namespace display {

// Détecte l'écran OLED sur le bus I2C 0 et l'initialise. Sans écran soudé,
// toutes les autres fonctions deviennent silencieusement inopérantes.
bool begin();

// À appeler dans la boucle : rafraîchit l'affichage et gère l'extinction auto.
void tick();

// Rallume l'écran et relance le minuteur d'extinction.
void wake();

bool present();

}  // namespace display

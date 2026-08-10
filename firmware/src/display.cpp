// -----------------------------------------------------------------------------
// Afficheur d'état sur l'OLED 1,3" du T-Beam Supreme.
//
// L'écran n'est PAS une interface : la carte, l'escouade et les messages restent
// dans l'app. Il répond à une seule question, celle qu'on se pose sur le terrain
// une fois le téléphone rangé : « est-ce que ce boîtier fonctionne ? »
// -----------------------------------------------------------------------------
#include "display.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "blelink.h"
#include "board.h"
#include "board_pins.h"
#include "gps.h"
#include "mesh.h"
#include "protocol.h"
#include "settings.h"

namespace display {

// L'écran s'éteint tout seul : un OLED tire 10-20 mA, soit une part non
// négligeable du budget d'une 18650 sur une partie longue.
static const uint32_t SCREEN_TIMEOUT_MS = 30000;
static const uint32_t REFRESH_MS = 500;
// Au-delà, un coéquipier n'est plus considéré comme « entendu ».
static const uint32_t PEER_FRESH_MS = 120000;

// SH1106 128x64 en I2C matériel. Wire est initialisé sur les broches du bus 0
// AVANT u8g2.begin() : U8g2 réutilise alors cette instance et ses broches.
static U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static bool s_present = false;
static bool s_on = false;
static uint32_t s_lastActivityMs = 0;
static uint32_t s_nextRefreshMs = 0;
static uint8_t s_lastStatus = proto::ST_OK;
static bool s_lastLinked = false;
// Le boîtier a-t-il vu l'app au moins une fois depuis l'allumage ?
static bool s_everLinked = false;

// --- Détection ----------------------------------------------------------------
static uint8_t scanForDisplay() {
    Serial.println("[i2c] scan du bus 0 (SDA 17 / SCL 18)…");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[i2c]   0x%02X%s\n", addr,
                          (addr == 0x3C || addr == 0x3D) ? "  <- écran OLED" : "");
            if (addr == 0x3C || addr == 0x3D) found = addr;
        }
    }
    if (!found) Serial.println("[i2c] aucun écran détecté");
    return found;
}

bool begin() {
    Wire.begin(I2C0_SDA, I2C0_SCL, 400000);

    const uint8_t addr = scanForDisplay();
    if (!addr) return false;

    u8g2.setI2CAddress(addr << 1);  // U8g2 attend l'adresse décalée
    if (!u8g2.begin()) {
        Serial.println("[oled] initialisation refusée");
        return false;
    }
    u8g2.setBusClock(400000);
    u8g2.enableUTF8Print();

    s_present = true;
    s_on = true;
    s_lastActivityMs = millis();
    Serial.printf("[oled] SH1106 128x64 OK à 0x%02X\n", addr);
    return true;
}

bool present() { return s_present; }

void wake() {
    if (!s_present) return;
    s_lastActivityMs = millis();
    if (!s_on) {
        u8g2.setPowerSave(0);
        s_on = true;
    }
    s_nextRefreshMs = 0;  // redessine immédiatement
}

// --- Rendu --------------------------------------------------------------------
static const char *statusLabel(uint8_t s) {
    switch (s) {
        case proto::ST_HIT: return "TOUCHE";
        case proto::ST_ELIMINATED: return "ELIMINE";
        case proto::ST_NEED_HELP: return "A L'AIDE";
        default: return "";
    }
}

// Bandeau plein écran, lisible à quelques mètres : c'est ce qu'un arbitre ou un
// coéquipier doit pouvoir lire sans manipuler le boîtier.
static void drawAlert(uint8_t status) {
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_helvB18_tr);
    const char *label = statusLabel(status);
    u8g2.drawStr((128 - u8g2.getStrWidth(label)) / 2, 38, label);
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr((128 - u8g2.getStrWidth(settings::cfg.callsign)) / 2, 56, settings::cfg.callsign);
    u8g2.setDrawColor(1);
}

// Tant que le téléphone n'est pas là, l'écran ne sert pas à surveiller la partie
// mais à établir la liaison. Il répond donc à deux autres questions : « laquelle
// de ces lignes Bluetooth est mon boîtier ? » et « quelle empreinte dois-je
// annoncer au chef ? ». L'état vital (GNSS, batterie) reste en pied, condensé :
// on ne remplace pas une réponse par une autre.
static void drawPairing() {
    char line[32];

    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(2, 10, "EN ATTENTE DE L'APP");
    // Pastille clignotante : l'annonce BLE est bien en cours.
    if ((millis() / 500) % 2 == 0) u8g2.drawBox(118, 3, 7, 7);
    u8g2.setDrawColor(1);

    // Le nom à chercher dans la liste Bluetooth, en gros et centré. S'il ne tient
    // pas — nom de produit plus long un jour — on rétrécit plutôt que de tronquer.
    const char *name = blelink::name();
    u8g2.setFont(u8g2_font_7x14B_tr);
    if (u8g2.getStrWidth(name) > 126) u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr((128 - u8g2.getStrWidth(name)) / 2, 30, name);

    // L'empreinte complète : celle que le chef compare avant de valider.
    u8g2.setFont(u8g2_font_6x12_tf);
    snprintf(line, sizeof(line), "EMPREINTE %02X%02X%02X%02X", settings::deviceFp[0],
             settings::deviceFp[1], settings::deviceFp[2], settings::deviceFp[3]);
    u8g2.drawStr((128 - u8g2.getStrWidth(line)) / 2, 45, line);

    u8g2.drawHLine(0, 50, 128);

    const uint8_t pct = board::batteryPercent();
    if (pct == 255) {
        snprintf(line, sizeof(line), "BATT --");
    } else {
        snprintf(line, sizeof(line), "BATT %u%%", pct);
    }
    u8g2.drawStr(2, 62, line);

    if (gps::hasFix()) {
        snprintf(line, sizeof(line), "FIX %u sat", gps::sats());
    } else {
        snprintf(line, sizeof(line), "PAS DE FIX");
    }
    u8g2.drawStr(126 - u8g2.getStrWidth(line), 62, line);
}

static void drawStatus() {
    char line[32];

    // Bandeau d'en-tête inversé : identité + escouade.
    u8g2.drawBox(0, 0, 128, 13);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawStr(2, 10, settings::cfg.callsign);
    if (settings::cfg.state == proto::SQUAD_ALONE) {
        snprintf(line, sizeof(line), "SANS ESC");
    } else {
        snprintf(line, sizeof(line), "%s #%u", settings::cfg.squadName, settings::cfg.addr);
    }
    u8g2.drawStr(126 - u8g2.getStrWidth(line), 10, line);
    u8g2.setDrawColor(1);

    // GNSS.
    if (gps::hasFix()) {
        snprintf(line, sizeof(line), "FIX  %u sat  HDOP %.1f", gps::sats(), gps::hdop());
    } else {
        snprintf(line, sizeof(line), "PAS DE FIX  %u sat", gps::sats());
    }
    u8g2.drawStr(2, 26, line);

    // Batterie.
    const uint8_t pct = board::batteryPercent();
    if (pct == 255) {
        snprintf(line, sizeof(line), "BATT --  %u mV", board::batteryMillivolts());
    } else {
        snprintf(line, sizeof(line), "BATT %u%%%s", pct, board::isCharging() ? "  charge" : "");
    }
    u8g2.drawStr(2, 40, line);

    // Réseau. Le compteur inclut les boîtiers d'autres escouades : ils sont
    // illisibles mais bien présents, et c'est cette présence qui nous relaie.
    const uint8_t peers = mesh::peerCount(PEER_FRESH_MS);
    if (peers == 0) {
        snprintf(line, sizeof(line), "AUCUN CONTACT");
    } else {
        snprintf(line, sizeof(line), "%u boitier%s  %d dBm", peers, peers > 1 ? "s" : "",
                 mesh::lastRssi());
    }
    u8g2.drawStr(2, 54, line);
}

void tick() {
    if (!s_present) return;

    const uint32_t now = millis();

    // Un changement de statut doit se voir, même écran éteint.
    const uint8_t status = mesh::playerStatus();
    if (status != s_lastStatus) {
        s_lastStatus = status;
        wake();
    }

    // Une liaison qui s'établit ou qui tombe est un événement : dans les deux cas
    // l'écran a quelque chose de neuf à dire.
    const bool linked = blelink::connected();
    if (linked != s_lastLinked) {
        s_lastLinked = linked;
        if (linked) s_everLinked = true;
        wake();
    }

    // Avant la toute première connexion, l'écran reste allumé : c'est le moment
    // où l'on cherche le boîtier dans la liste Bluetooth, et un écran éteint
    // rendrait justement cette recherche impossible. Passé cette étape, la
    // temporisation reprend ses droits — l'OLED coûte 10-20 mA.
    const bool pairing = !linked && !s_everLinked;
    if (pairing) s_lastActivityMs = now;

    if (s_on && now - s_lastActivityMs > SCREEN_TIMEOUT_MS) {
        u8g2.setPowerSave(1);
        s_on = false;
        return;
    }
    if (!s_on) return;
    if ((int32_t)(now - s_nextRefreshMs) < 0) return;
    s_nextRefreshMs = now + REFRESH_MS;

    u8g2.clearBuffer();
    if (status != proto::ST_OK) {
        // Un joueur touché prime sur tout le reste, y compris l'appairage.
        drawAlert(status);
    } else if (!linked) {
        drawPairing();
    } else {
        drawStatus();
    }
    u8g2.sendBuffer();
}

}  // namespace display

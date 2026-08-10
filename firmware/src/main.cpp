// -----------------------------------------------------------------------------
// MeshRadio — firmware T-Beam Supreme (ESP32-S3 + SX1262 + GNSS)
//
// Le device est un MODEM : radio mesh LoRa chiffrée, GNSS et lien BLE. Toute
// l'interface (carte, escouade, messages) vit dans l'app Android/iOS, qui parle le
// même protocole binaire (shared/PROTOCOL.md).
//
// La clé d'escouade ne quitte jamais ce firmware : le téléphone ne voit que du clair.
// -----------------------------------------------------------------------------
#include <Arduino.h>

#include "blelink.h"
#include "board.h"
#include "board_pins.h"
#include "display.h"
#include "gps.h"
#include "mesh.h"
#include "protocol.h"
#include "radio.h"
#include "selftest.h"
#include "settings.h"
#include "squad.h"

static uint32_t s_nextPositionMs = 0;
static uint32_t s_nextNodeInfoMs = 0;
static uint32_t s_nextStatusMs = 0;
static uint32_t s_nextGpsLogMs = 0;

static const uint32_t NODEINFO_PERIOD_MS = 60000;
static const uint32_t STATUS_PERIOD_MS = 1000;
static const uint32_t GPS_LOG_PERIOD_MS = 5000;

// --- Bouton BOOT --------------------------------------------------------------
// Appui court  : diffuse immédiatement position + identité, réveille l'écran.
// Appui long   : bascule le statut joueur opérationnel <-> touché.
static void pollButton() {
    static bool wasDown = false;
    static uint32_t downAt = 0;
    static bool longFired = false;

    const bool down = digitalRead(BUTTON_PIN) == LOW;

    if (down && !wasDown) {
        downAt = millis();
        longFired = false;
    } else if (down && !longFired && millis() - downAt > 1500) {
        longFired = true;
        const uint8_t next = (mesh::playerStatus() == proto::ST_OK) ? proto::ST_HIT : proto::ST_OK;
        mesh::setPlayerStatus(next);
        mesh::broadcastPosition();
        display::wake();
        Serial.printf("[main] statut joueur -> %u\n", next);
    } else if (!down && wasDown && !longFired) {
        display::wake();
        mesh::broadcastPosition();
        mesh::broadcastNodeInfo();
        Serial.println("[main] diffusion manuelle");
    }
    wasDown = down;
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== MeshRadio v2 — T-Beam Supreme ===");

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // 1. PMU : indispensable AVANT la radio et le GNSS (rails ALDO3 / ALDO4).
    board::begin();

    // 2. Identité, clés et configuration persistée.
    settings::begin();
    Serial.printf("[main] empreinte %02X%02X%02X%02X  indicatif %s  démarrage n°%u\n",
                  settings::deviceFp[0], settings::deviceFp[1], settings::deviceFp[2],
                  settings::deviceFp[3], settings::cfg.callsign, settings::epoch);
    if (settings::cfg.state == proto::SQUAD_ALONE) {
        Serial.println("[main] aucune escouade — à créer ou rejoindre depuis l'app");
    } else {
        Serial.printf("[main] escouade « %s » (canal %u), je suis #%u%s\n",
                      settings::cfg.squadName, settings::cfg.squad, settings::cfg.addr,
                      settings::cfg.state == proto::SQUAD_LEADER ? " — chef" : "");
    }

    // 3. Autotest : confronte codec et cryptographie aux vecteurs de l'app.
    //    mbedtls n'existe que sur la carte, c'est donc ici — et nulle part
    //    ailleurs — qu'on peut vérifier la compatibilité avec OpenSSL.
    selftest::run();

    // 4. Périphériques.
    display::begin();
    gps::begin();
    if (!radio::begin(mesh::onRadioFrame)) {
        Serial.println("[main] RADIO HORS SERVICE — le device restera muet");
    }

    char bleName[24];
    snprintf(bleName, sizeof(bleName), "MeshRadio-%02X%02X", settings::deviceFp[0],
             settings::deviceFp[1]);
    blelink::begin(bleName, mesh::onAppFrame);

    mesh::begin();
    squad::begin();

    const uint32_t now = millis();
    s_nextPositionMs = now + 5000;
    s_nextNodeInfoMs = now + 3000;
    s_nextStatusMs = now + 1000;
}

void loop() {
    gps::poll();
    radio::poll();
    blelink::poll();
    mesh::tick();
    squad::tick();
    display::tick();
    pollButton();

    const uint32_t now = millis();

    if ((int32_t)(now - s_nextPositionMs) >= 0) {
        s_nextPositionMs = now + (uint32_t)settings::cfg.posIntervalS * 1000;
        mesh::broadcastPosition();
    }

    if ((int32_t)(now - s_nextNodeInfoMs) >= 0) {
        s_nextNodeInfoMs = now + NODEINFO_PERIOD_MS;
        mesh::broadcastNodeInfo();
    }

    // État du GNSS sur la console : sans ça, impossible de savoir depuis le PC si
    // la carte a un fix ou combien de satellites elle voit.
    if ((int32_t)(now - s_nextGpsLogMs) >= 0) {
        s_nextGpsLogMs = now + GPS_LOG_PERIOD_MS;
        if (gps::hasFix()) {
            Serial.printf("[gps] FIX  %.6f, %.6f  alt=%d m  %u sat  HDOP=%.1f\n", gps::lat() / 1e7,
                          gps::lon() / 1e7, gps::alt(), gps::sats(), gps::hdop());
        } else {
            Serial.printf("[gps] pas de fix — %u satellite(s) suivi(s)\n", gps::sats());
        }
    }

    if ((int32_t)(now - s_nextStatusMs) >= 0) {
        s_nextStatusMs = now + STATUS_PERIOD_MS;
        if (blelink::connected()) {
            proto::Status st;
            mesh::fillStatus(st);

            proto::Frame f;
            f.type = proto::T_STATUS;
            f.flags = proto::F_LOCAL;
            f.squad = settings::cfg.squad;
            f.src = settings::cfg.addr;
            f.len = (uint8_t)proto::encodeStatus(st, f.payload);
            blelink::sendFrame(f);
        }
    }

    delay(2);
}

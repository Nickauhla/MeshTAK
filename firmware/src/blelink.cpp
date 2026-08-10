#include "blelink.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

#include "slip.h"

namespace blelink {

// Nordic UART Service — reconnu tel quel par @capacitor-community/bluetooth-le.
static const char *NUS_SERVICE = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // app -> device
static const char *NUS_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // device -> app

static NimBLEServer *s_server = nullptr;
static NimBLECharacteristic *s_txChar = nullptr;
static char s_name[24] = "";
static volatile bool s_connected = false;
static volatile uint16_t s_mtu = 23;
static FrameHandler s_handler = nullptr;

// --- Tampon circulaire SPSC ---------------------------------------------------
// Les écritures BLE arrivent dans la tâche de la pile NimBLE ; on ne traite rien
// là-bas (le SPI radio n'est pas réentrant). On recopie et on traite dans poll().
static const size_t RING_SIZE = 4096;
static uint8_t s_ring[RING_SIZE];
static volatile size_t s_head = 0;  // écrit par la tâche BLE
static volatile size_t s_tail = 0;  // écrit par la boucle principale

static void ringPush(const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const size_t next = (s_head + 1) % RING_SIZE;
        if (next == s_tail) return;  // plein : on jette (le SLIP resynchronisera)
        s_ring[s_head] = data[i];
        s_head = next;
    }
}

// --- Décodage -----------------------------------------------------------------
static void onSlipFrame(const uint8_t *data, size_t len) {
    proto::Frame f;
    if (!proto::decode(data, len, f)) return;
    if (s_handler) s_handler(f);
}
static slip::Decoder s_decoder(onSlipFrame);

// --- Callbacks NimBLE ---------------------------------------------------------
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server) override {
        s_connected = true;
        Serial.println("[ble] téléphone connecté");
    }
    void onDisconnect(NimBLEServer *server) override {
        s_connected = false;
        s_mtu = 23;
        s_decoder.reset();
        Serial.println("[ble] téléphone déconnecté");
        NimBLEDevice::startAdvertising();
    }
    void onMTUChange(uint16_t mtu, ble_gap_conn_desc *desc) override { s_mtu = mtu; }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *chr) override {
        NimBLEAttValue v = chr->getValue();
        ringPush(v.data(), v.length());
    }
};

static ServerCallbacks s_serverCb;
static RxCallbacks s_rxCb;

// --- API ----------------------------------------------------------------------
void begin(const char *deviceName, FrameHandler handler) {
    s_handler = handler;
    snprintf(s_name, sizeof(s_name), "%s", deviceName);

    NimBLEDevice::init(deviceName);
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(&s_serverCb);

    NimBLEService *svc = s_server->createService(NUS_SERVICE);
    s_txChar = svc->createCharacteristic(NUS_TX, NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *rxChar = svc->createCharacteristic(
        NUS_RX, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    rxChar->setCallbacks(&s_rxCb);
    svc->start();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(NUS_SERVICE);
    adv->setScanResponse(true);
    adv->start();

    Serial.printf("[ble] service NUS annoncé sous « %s »\n", deviceName);
}

void poll() {
    while (s_tail != s_head) {
        const uint8_t b = s_ring[s_tail];
        s_tail = (s_tail + 1) % RING_SIZE;
        s_decoder.feed(&b, 1);
    }
}

bool connected() { return s_connected; }

const char *name() { return s_name; }

void sendFrame(const proto::Frame &f) {
    if (!s_connected || !s_txChar) return;

    uint8_t raw[proto::MAX_FRAME];
    const size_t rawLen = proto::encode(f, raw, sizeof(raw));
    if (rawLen == 0) return;

    static uint8_t framed[2 * proto::MAX_FRAME + 2];
    const size_t n = slip::encode(raw, rawLen, framed, sizeof(framed));
    if (n == 0) return;

    const size_t chunk = (s_mtu > 23) ? (size_t)(s_mtu - 3) : 20;
    for (size_t off = 0; off < n; off += chunk) {
        const size_t take = (n - off < chunk) ? (n - off) : chunk;
        s_txChar->setValue(framed + off, take);
        s_txChar->notify();
        delay(2);  // laisse respirer la file de notifications NimBLE
    }
}

void sendLog(const char *text) {
    proto::Frame f;
    f.type = proto::T_LOG;
    f.flags = proto::F_LOCAL;
    f.ttl = 0;
    f.len = (uint8_t)strnlen(text, proto::MAX_PAYLOAD);
    memcpy(f.payload, text, f.len);
    sendFrame(f);
}

}  // namespace blelink

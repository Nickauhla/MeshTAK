#pragma once
// -----------------------------------------------------------------------------
// LilyGO T-Beam Supreme V3 — ESP32-S3 + SX1262 + GNSS (L76K ou u-blox MAX-M10S)
//
// Brochage confirmé par la documentation matérielle officielle LilyGO
// (docs/en/t_beam_supreme/t_beam_supreme_hw.md) et recoupé avec le wiki LilyGO.
// -----------------------------------------------------------------------------

// --- I2C bus 0 : OLED, magnétomètre QMC6310, BME280 --------------------------
#define I2C0_SDA 17
#define I2C0_SCL 18

// --- I2C bus 1 : PMU AXP2101 + RTC PCF8563 -----------------------------------
#define I2C1_SDA 42
#define I2C1_SCL 41

// --- Radio SX1262 (bus SPI dédié) --------------------------------------------
#define RADIO_SCK 12
#define RADIO_MISO 13
#define RADIO_MOSI 11
#define RADIO_CS 10
#define RADIO_DIO1 1
#define RADIO_BUSY 4
#define RADIO_RST 5

// --- GNSS (UART) -------------------------------------------------------------
// ⚠️ Les sources publiques se contredisent sur l'orientation RX/TX de ces deux
// broches (8 et 9). Le firmware sonde donc les DEUX combinaisons au démarrage
// (cf. gps.cpp) et mémorise celle qui produit des trames NMEA valides.
#define GPS_PIN_A 9
#define GPS_PIN_B 8
#define GPS_PPS 6
#define GPS_WAKEUP 7  // réveil, spécifique L76K

// --- Carte microSD (bus SPI partagé avec l'IMU) ------------------------------
#define SD_MOSI 35
#define SD_MISO 37
#define SD_SCK 36
#define SD_CS 47

// --- IMU QMI8658 -------------------------------------------------------------
#define IMU_CS 34
#define IMU_INT 33

// --- Divers ------------------------------------------------------------------
#define RTC_INT 14
#define PMU_IRQ 40
#define BUTTON_PIN 0  // bouton BOOT

// --- Rails d'alimentation AXP2101 --------------------------------------------
// ALDO1 : BME280 + écran + magnétomètre
// ALDO2 : capteurs
// ALDO3 : RADIO SX1262      <- indispensable
// ALDO4 : GNSS              <- indispensable
// BLDO1 : carte microSD
#define RAIL_MV_RADIO 3300
#define RAIL_MV_GNSS 3300
#define RAIL_MV_PERIPH 3300

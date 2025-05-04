#pragma once

// Verze firmwaru
#define FIRMWARE_VERSION "1.0.2"

// Konfigurace hardwaru
// Definice pinů pro RFM95W
#define LORA_CS 5        // NSS pin
#define LORA_RST 14      // RESET pin
#define LORA_DIO0 36     // DIO0 pin (přerušení)
#define SPI_MISO_PIN 35  // MISO pin
#define SPI_MOSI_PIN 37  // MOSI pin
#define SPI_SCK_PIN 18   // SCK pin

// LoRa registry
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_OCP 0x0B
#define REG_LNA 0x0C
#define REG_FIFO_ADDR_PTR 0x0D
#define REG_FIFO_TX_BASE_ADDR 0x0E
#define REG_FIFO_RX_BASE_ADDR 0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1A
#define REG_MODEM_CONFIG_1 0x1D
#define REG_MODEM_CONFIG_2 0x1E
#define REG_SYMB_TIMEOUT_LSB 0x1F
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_FREQ_ERROR_MSB 0x28
#define REG_FREQ_ERROR_MID 0x29
#define REG_FREQ_ERROR_LSB 0x2A
#define REG_RSSI_WIDEBAND 0x2C
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_INVERTIQ 0x33
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_INVERTIQ2 0x3B
#define REG_DIO_MAPPING_1 0x40
#define REG_VERSION 0x42
#define REG_PA_DAC 0x4D

// LoRa módy
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06
#define MODE_CAD 0x07

// Konfigurace aplikace
#define MAX_SENSORS 20                // Maximální počet senzorů
#define LOG_BUFFER_SIZE 200           // Velikost bufferu pro logy
#define AP_TIMEOUT 300000             // 5 minut v milisekundách pro timeout AP módu
#define WIFI_RECONNECT_INTERVAL 60000 // 1 minuta v milisekundách pro pokus o reconnect
#define WDT_TIMEOUT 10                // Watchdog timeout v sekundách

// Konfigurace souborového systému
#define CONFIG_FILE "/config.json"    // Soubor s konfigurací
#define SENSORS_FILE "/sensors.json"  // Soubor se senzory

// NTP konfigurace
#define NTP_SERVER "pool.ntp.org"
#define DEFAULT_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"  // Central European Time with auto DST


// CORS hlavičky pro API
#define CORS_HEADER_NAME "Access-Control-Allow-Origin"
#define CORS_HEADER_VALUE "*"

// Webserver porty
#define HTTP_PORT 80           // Port pro HTTP server
#define DNS_PORT 53            // Port pro DNS server

// Definice typů senzorů - snadno rozšiřitelné pro další typy
#define SENSOR_TYPE_UNKNOWN 0x00
#define SENSOR_TYPE_BME280 0x01    // Teplota, vlhkost, tlak
#define SENSOR_TYPE_SCD40 0x02     // Teplota, vlhkost, CO2
#define SENSOR_TYPE_METEO 0x03  // Meteorologická stanice
#define SENSOR_TYPE_VEML7700 0x04  // Světelný senzor (LUX)
// Přidejte další typy senzorů zde...

// Konfigurace PSRAM pro web server
#define WEB_BUFFER_SIZE 16384   // Reduced to 16KB


// Nastavení OTA updatu
#define OTA_PASSWORD "admin"    // Heslo pro OTA update
#define OTA_PORT 3232           // Port pro OTA update
#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "../config.h"
#include "../Data/Logging.h"
#include "SPI_Manager.h"

/**
 * Třída pro správu LoRa modulu RFM95W
 * 
 * Zajišťuje inicializaci, konfiguraci a komunikaci s LoRa modulem RFM95W.
 * Abstrahuje SPI komunikaci a obsluhu přerušení.
 */
class LoRaModule {
private:
    int csPin;           // Chip select pin
    int rstPin;          // Reset pin
    int dio0Pin;         // DIO0 pin pro přerušení
    SPIManager* spiManager; // Správce SPI komunikace
    
    Logger& logger;      // Reference na logger
    
    static volatile bool interruptOccurred;   // Příznak přerušení (statický pro použití v ISR)
    static void IRAM_ATTR handleInterrupt();  // Obsluha přerušení
    
    // Nastavení pinů a SPI
    bool setupPins();
    // Reset modulu
    void resetModule();
    
public:
    // Konstruktor
    LoRaModule(Logger& log, SPIManager* spiMgr = nullptr, int cs = LORA_CS, int rst = LORA_RST, int dio0 = LORA_DIO0);
    
    // Destruktor
    ~LoRaModule();
    
    // Inicializace LoRa modulu
    bool init();
    
    // Reset modulu
    bool reset();
    
    // Zápis do registru
    void writeRegister(uint8_t reg, uint8_t value);
    
    // Čtení z registru
    uint8_t readRegister(uint8_t reg);
    
    // Přijímání paketu
    bool receivePacket(uint8_t *buffer, uint8_t *length);
    
    // Získání RSSI
    int getRSSI();
    
    // Vrátí SNR posledního paketu
    float getSNR();
    
    // Kontrola, zda došlo k přerušení
    static bool hasInterrupt();
    
    // Vynulování příznaku přerušení
    static void clearInterrupt();
    
    // Kontrola, zda je modul připojený
    bool isConnected();
    
    // Získání verze čipu
    uint8_t getVersion();
};
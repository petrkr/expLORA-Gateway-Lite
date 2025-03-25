#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "../Data/Logging.h"
#include "../config.h"

/**
 * Třída pro správu SPI komunikace
 * 
 * Zajišťuje inicializaci a správu SPI rozhraní pro komunikaci s LoRa modulem a dalšími periferiemi.
 */
class SPIManager {
private:
    SPIClass* spi;      // Instance SPI
    Logger& logger;     // Reference na logger
    
    int sckPin;         // SCK pin
    int misoPin;        // MISO pin
    int mosiPin;        // MOSI pin
    bool initialized;   // Příznak inicializace
    
public:
    // Konstruktor
    SPIManager(Logger& log, int sck = SPI_SCK_PIN, int miso = SPI_MISO_PIN, int mosi = SPI_MOSI_PIN);
    
    // Destruktor
    ~SPIManager();
    
    // Inicializace SPI
    bool init();
    
    // Reset SPI
    bool reset();
    
    // Získání instance SPI
    SPIClass* getSPI();
    
    // Začátek transakce
    void beginTransaction(SPISettings settings = SPISettings(1000000, MSBFIRST, SPI_MODE0));
    
    // Konec transakce
    void endTransaction();
    
    // Přenos dat
    uint8_t transfer(uint8_t data);
    void transfer(uint8_t* data, size_t length);
    
    // Kontrola, zda je SPI inicializováno
    bool isInitialized() const;
};
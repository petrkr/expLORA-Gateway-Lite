#include "SPI_Manager.h"

// Konstruktor
SPIManager::SPIManager(Logger& log, int sck, int miso, int mosi)
    : spi(nullptr), logger(log), sckPin(sck), misoPin(miso), mosiPin(mosi), initialized(false) {
}

// Destruktor
SPIManager::~SPIManager() {
    // Neuvolňujeme instanci SPI, protože může být sdílená s jinými komponentami
}

// Inicializace SPI
bool SPIManager::init() {
    logger.debug("Initializing SPI interface: SCK=" + String(sckPin) + 
                ", MISO=" + String(misoPin) + ", MOSI=" + String(mosiPin));
    
    spi = new SPIClass(HSPI);
    
    if (spi == nullptr) {
        logger.error("Failed to create SPI instance");
        return false;
    }
    
    // Inicializace SPI
    spi->begin(sckPin, misoPin, mosiPin);
    
    initialized = true;
    logger.info("SPI interface initialized successfully");
    return true;
}

// Reset SPI
bool SPIManager::reset() {
    if (!initialized) {
        return init();
    }
    
    logger.debug("Resetting SPI interface");
    
    // Ukončení a znovu inicializace SPI
    spi->end();
    delay(100);
    spi->begin(sckPin, misoPin, mosiPin);
    
    logger.info("SPI interface reset successfully");
    return true;
}

// Získání instance SPI
SPIClass* SPIManager::getSPI() {
    if (!initialized) {
        init();
    }
    
    return spi;
}

// Začátek transakce
void SPIManager::beginTransaction(SPISettings settings) {
    if (!initialized) {
        init();
    }
    
    spi->beginTransaction(settings);
}

// Konec transakce
void SPIManager::endTransaction() {
    if (initialized) {
        spi->endTransaction();
    }
}

// Přenos dat
uint8_t SPIManager::transfer(uint8_t data) {
    if (!initialized) {
        init();
    }
    
    return spi->transfer(data);
}

// Přenos dat - pole
void SPIManager::transfer(uint8_t* data, size_t length) {
    if (!initialized) {
        init();
    }
    
    for (size_t i = 0; i < length; i++) {
        data[i] = spi->transfer(data[i]);
    }
}

// Kontrola, zda je SPI inicializováno
bool SPIManager::isInitialized() const {
    return initialized;
}
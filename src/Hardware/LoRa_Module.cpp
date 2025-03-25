#include "LoRa_Module.h"
#include <Arduino.h>

// Inicializace statických proměnných
volatile bool LoRaModule::interruptOccurred = false;

// Obsluha přerušení
void IRAM_ATTR LoRaModule::handleInterrupt() {
    interruptOccurred = true;
}

// Konstruktor
LoRaModule::LoRaModule(Logger& log, SPIManager* spiMgr, int cs, int rst, int dio0)
    : csPin(cs), rstPin(rst), dio0Pin(dio0), spiManager(spiMgr), logger(log) {
    
    // Pokud nebyla předána instance SPIManager, vytvoříme novou
    if (spiManager == nullptr) {
        spiManager = new SPIManager(logger);
    }
}

// Destruktor
LoRaModule::~LoRaModule() {
    // Neuvolňujeme instanci SPIManager, pokud byla vytvořena externě
}

// Nastavení pinů a SPI
bool LoRaModule::setupPins() {
    // Nastavení CS pinu
    pinMode(csPin, OUTPUT);
    digitalWrite(csPin, HIGH);
    
    // Nastavení RST pinu
    pinMode(rstPin, OUTPUT);
    digitalWrite(rstPin, HIGH);
    
    // Nastavení DIO0 pinu
    pinMode(dio0Pin, INPUT);
    
    // Inicializace SPI
    if (!spiManager->isInitialized()) {
        if (!spiManager->init()) {
            logger.error("Failed to initialize SPI manager");
            return false;
        }
    }
    
    // Připojení přerušení na DIO0 pin
    attachInterrupt(digitalPinToInterrupt(dio0Pin), handleInterrupt, RISING);
    
    logger.debug("LoRa module pins configured: CS=" + String(csPin) + 
                ", RST=" + String(rstPin) + ", DIO0=" + String(dio0Pin));
    
    return true;
}

// Reset modulu
void LoRaModule::resetModule() {
    logger.debug("Resetting LoRa module...");
    
    digitalWrite(rstPin, LOW);
    delay(10);
    digitalWrite(rstPin, HIGH);
    delay(10);
}

// Inicializace LoRa modulu
bool LoRaModule::init() {
    logger.info("Initializing LoRa module...");
    
    // Nastavení pinů a SPI
    if (!setupPins()) {
        logger.error("Failed to setup pins for LoRa module");
        return false;
    }
    
    // Reset modulu
    resetModule();
    
    // Kontrola komunikace s modulem s opakováním
    uint8_t version = 0;
    uint8_t retries = 3;
    bool success = false;

    while (retries > 0 && !success) {
        version = getVersion();
        logger.debug("LoRa chip version: 0x" + String(version, HEX));

        if (version == 0x12) {
            success = true;
            break;
        }

        delay(100);
        retries--;

        // Necháme běžet další úkoly mezi pokusy
        yield();

        // Zkusíme resetovat SPI a modul znovu
        if (retries == 1) {
            logger.warning("Trying to restore SPI connection...");
            spiManager->reset();
            resetModule();
        }
    }

    if (!success) {
        logger.error("LoRa module not found after multiple attempts!");
        // Zkontrolujeme stavy pinů pro diagnostiku
        logger.debug("MISO pin state: " + String(digitalRead(SPI_MISO_PIN)));
        return false;
    }
    
    logger.info("Configuring LoRa module...");
    
    // Přepnutí do spánkového režimu pro konfiguraci
    writeRegister(REG_OP_MODE, MODE_SLEEP);
    delay(10);
    
    // Nastavení LoRa módu
    writeRegister(REG_OP_MODE, MODE_SLEEP | MODE_LONG_RANGE_MODE);
    delay(10);
    
    // Nastavení frekvence na 868 MHz pro EU
    uint64_t frf = ((uint64_t)868000000 << 19) / 32000000;
    writeRegister(REG_FRF_MSB, (uint8_t)(frf >> 16));
    writeRegister(REG_FRF_MID, (uint8_t)(frf >> 8));
    writeRegister(REG_FRF_LSB, (uint8_t)(frf >> 0));
    
    // Nastavení výkonu
    writeRegister(REG_PA_CONFIG, 0x8F);  // PA_BOOST, max power
    
    // Nastavení přijímače
    writeRegister(REG_LNA, 0x23);  // Vysoký zisk, automatický AGC
    
    // Optimalizace pro SF7–SF10 
    writeRegister(REG_DETECTION_OPTIMIZE,  0xC5);
    writeRegister(REG_DETECTION_THRESHOLD, 0x0C);
    
    // Over-current protection
    writeRegister(REG_OCP, 0x2F);  // 150mA
    
    // Nastavení základních FIFO adres
    writeRegister(REG_FIFO_TX_BASE_ADDR, 0);
    writeRegister(REG_FIFO_RX_BASE_ADDR, 0);
    
    // Nastavení šířky pásma, korekce chyb, spreading faktoru
    writeRegister(REG_MODEM_CONFIG_1, 0x72);  // BW=125kHz, CR=4/5, explicit header
    writeRegister(REG_MODEM_CONFIG_2, 0x94);  // SF=9, CRC enabled (0x90 | 0x04)
    writeRegister(REG_MODEM_CONFIG_3, 0x04);  // LNA AGC on  
    
    // Nastavení preamble
    writeRegister(REG_PREAMBLE_MSB, 0x00);
    writeRegister(REG_PREAMBLE_LSB, 0x10);
    
    // Nastavení sync word
    writeRegister(REG_SYNC_WORD, 0x12);
    
    // Přepnutí do režimu kontinuálního příjmu
    writeRegister(REG_OP_MODE, MODE_RX_CONTINUOUS | MODE_LONG_RANGE_MODE);
    delay(10);
    
    logger.info("LoRa module initialized and in receive mode");
    return true;
}

// Reset modulu
bool LoRaModule::reset() {
    resetModule();
    
    // Kontrola komunikace s modulem po resetu
    uint8_t version = getVersion();
    if (version != 0x12) {
        logger.error("LoRa module not responding after reset");
        return false;
    }
    
    // Přepnutí do spánkového režimu pro konfiguraci
    writeRegister(REG_OP_MODE, MODE_SLEEP);
    delay(10);
    
    // Přepnutí do režimu kontinuálního příjmu
    writeRegister(REG_OP_MODE, MODE_RX_CONTINUOUS | MODE_LONG_RANGE_MODE);
    delay(10);
    
    logger.info("LoRa module reset successfully");
    return true;
}

// Zápis do registru
void LoRaModule::writeRegister(uint8_t reg, uint8_t value) {
    digitalWrite(csPin, LOW);
    
    spiManager->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    spiManager->transfer(reg | 0x80);  // 0x80 indikuje zápis
    spiManager->transfer(value);
    spiManager->endTransaction();
    
    digitalWrite(csPin, HIGH);
}

// Čtení z registru
uint8_t LoRaModule::readRegister(uint8_t reg) {
    digitalWrite(csPin, LOW);
    
    spiManager->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    spiManager->transfer(reg & 0x7F);  // 0x7F indikuje čtení
    uint8_t value = spiManager->transfer(0x00);
    spiManager->endTransaction();
    
    digitalWrite(csPin, HIGH);
    return value;
}

// Přijímání paketu
bool LoRaModule::receivePacket(uint8_t *buffer, uint8_t *length) {
    // Kontrola, zda byl dokončen příjem
    uint8_t irqFlags = readRegister(REG_IRQ_FLAGS);
    
    // Kontrola příznaku dokončení příjmu
    if (!(irqFlags & 0x40)) {
        return false;
    }
    
    // Čtení počtu přijatých bytů
    uint8_t packetLength = readRegister(REG_RX_NB_BYTES);
    
    // Základní kontrola délky dat
    if (packetLength == 0 || packetLength > 255) {
        logger.warning("Invalid packet length: " + String(packetLength));
        // Vyčištění příznaků přerušení
        writeRegister(REG_IRQ_FLAGS, 0xFF);
        return false;
    }
    
    *length = packetLength;
    
    // Získání aktuální adresy v FIFO
    uint8_t currentAddr = readRegister(REG_FIFO_RX_CURRENT_ADDR);
    
    // Kontrola, že adresa dává smysl
    if (currentAddr > 255) {
        logger.warning("Invalid FIFO address: " + String(currentAddr));
        // Vyčištění příznaků přerušení
        writeRegister(REG_IRQ_FLAGS, 0xFF);
        return false;
    }
    
    // Nastavení FIFO adresy
    writeRegister(REG_FIFO_ADDR_PTR, currentAddr);
    
    // Čtení dat z FIFO
    digitalWrite(csPin, LOW);
    
    spiManager->beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
    spiManager->transfer(REG_FIFO & 0x7F);  // 0x7F indikuje čtení
    
    // Přenos dat
    for (int i = 0; i < packetLength; i++) {
        buffer[i] = spiManager->transfer(0x00);
    }
    
    spiManager->endTransaction();
    digitalWrite(csPin, HIGH);
    
    // Vyčištění příznaků přerušení
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    
    return true;
}

// Získání RSSI
int LoRaModule::getRSSI() {
    int16_t rssi = readRegister(REG_PKT_RSSI_VALUE) - 137;
    return rssi;
}

// Získání SNR posledního paketu
float LoRaModule::getSNR() {
    int8_t snr = readRegister(REG_PKT_SNR_VALUE);
    if (snr & 0x80) {
        snr = ((~snr + 1) & 0xFF) >> 2;
        snr = -snr;
    } else {
        snr = snr >> 2;
    }
    return snr * 0.25;
}

// Kontrola, zda došlo k přerušení
bool LoRaModule::hasInterrupt() {
    return interruptOccurred;
}

// Vynulování příznaku přerušení
void LoRaModule::clearInterrupt() {
    interruptOccurred = false;
}

// Kontrola, zda je modul připojený
bool LoRaModule::isConnected() {
    return (getVersion() == 0x12);
}

// Získání verze čipu
uint8_t LoRaModule::getVersion() {
    return readRegister(REG_VERSION);
}
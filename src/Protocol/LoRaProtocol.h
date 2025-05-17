#pragma once

#include <Arduino.h>
#include "../Hardware/LoRa_Module.h"
#include "../Data/SensorManager.h"
#include "../Data/Logging.h"

/**
 * Třída pro zpracování LoRa protokolu
 * 
 * Zajišťuje příjem a dekódování LoRa paketů, jejich dešifrování
 * a zpracování podle typu senzoru.
 */
class LoRaProtocol {
private:
    LoRaModule& loraModule;     // Reference na LoRa modul
    SensorManager& sensorManager; // Reference na správce senzorů
    Logger& logger;            // Reference na logger
    
    // Maximální délka paketu
    static const size_t MAX_PACKET_LENGTH = 256;
    
    // Buffer pro přijatý paket
    uint8_t packetBuffer[MAX_PACKET_LENGTH];
    uint8_t decryptedBuffer[MAX_PACKET_LENGTH];
    
    // Zpracování paketu podle typu senzoru
    bool processBME280Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processSCD40Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processVEML7700Packet(uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    bool processMeteoPacket(uint8_t *data, uint8_t len, int sensorIndex, int rssi);

    int lastProcessedSensorIndex; // Index posledně zpracovaného senzoru

    // Validace kontrolního součtu
    bool validateChecksum(uint8_t *buf, uint8_t len);
    
    // Výpočet kontrolního součtu
    uint8_t calculateChecksum(const uint8_t *data, uint8_t length);
    
    // Kontrola platnosti paketu
    bool isValidPacket(uint8_t *buf, uint8_t len);
    
    // Factory metoda pro zpracování paketu podle typu senzoru
    bool processPacketByType(SensorType type, uint8_t *data, uint8_t len, int sensorIndex, int rssi);
    
public:
    // Konstruktor
    LoRaProtocol(LoRaModule& module, SensorManager& manager, Logger& log);
    
    // Destruktor
    ~LoRaProtocol();
    
    // Zpracování přijatého paketu
    bool processReceivedPacket();
    
    // Dešifrování dat s klíčem
    void decryptData(uint8_t *data, uint8_t data_len, uint32_t key);
    
    // Zkouška dešifrování se všemi známými klíči
    int tryDecryptWithAllKeys(uint8_t *encData, uint8_t len, uint8_t *decData);

    // Getter pro lastProcessedSensorIndex
    int getLastProcessedSensorIndex() const { return lastProcessedSensorIndex; }

    // Převod relativního tlaku na absolutní tlak
    double relativeToAbsolutePressure(double p0_hpa, int altitude_m, double temp_c);
};
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <vector>
#include "Data/SensorManager.h"
#include "Data/Logging.h"
#include "Storage/ConfigManager.h"

/**
 * Třída pro správu MQTT komunikace s Home Assistant
 * 
 * Zajišťuje automatickou detekci senzorů v Home Assistant pomocí MQTT discovery
 * a pravidelné publikování dat ze senzorů.
 */
class MQTTManager {
private:
    WiFiClient wifiClient;       // WiFi klient pro MQTT
    PubSubClient mqttClient;     // MQTT klient
    SensorManager& sensorManager; // Reference na správce senzorů
    ConfigManager& configManager; // Reference na konfiguraci
    Logger& logger;               // Reference na logger
    
    String clientId;              // MQTT Client ID
    unsigned long lastReconnectAttempt; // Čas posledního pokusu o připojení
    unsigned long lastDiscoveryUpdate;  // Čas poslední aktualizace discovery
    
    // Připojení k MQTT brokeru
    bool connect();
    
    // Vytvoření discovery tématu pro senzor
    String buildDiscoveryTopic(const SensorData& sensor, const String& valueType);
    
    // Vytvoření konfiguračního JSON pro Home Assistant discovery
    String buildDiscoveryJson(const SensorData& sensor, const String& valueType, const String& stateTopic);

    // Helper function to capitalize first letter
    String capitalizeFirst(const String& input);
    
public:
    // Konstruktor
    MQTTManager(SensorManager& sensors, ConfigManager& config, Logger& log);
    
    // Inicializace
    bool init();
    
    // Zpracování MQTT komunikace (volat v hlavní smyčce)
    void process();
    
    // Publikování discovery konfigurace pro senzory
    void publishDiscovery();
    
    // Publikování dat senzorů
    void publishSensorData(int sensorIndex);

    // Publikování discovery pro konkrétní senzor
    void publishDiscoveryForSensor(int sensorIndex);

    // Odstranění discovery pro smazaný senzor
    void removeDiscoveryForSensor(uint32_t serialNumber);
    
    // Kontrola připojení
    bool isConnected();
    
    // Odpojení od MQTT brokeru
    void disconnect();
};
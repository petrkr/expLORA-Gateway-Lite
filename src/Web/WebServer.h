#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <DNSServer.h>
#include "../Data/SensorManager.h"
#include "../Data/Logging.h"
#include "../Storage/ConfigManager.h"
#include "../Protocol/MQTTManager.h"

/**
 * Třída pro správu webového portálu
 * 
 * Zajišťuje webové rozhraní pro konfiguraci a monitoring zařízení.
 * Poskytuje API pro přístup k datům ze senzorů.
 */
class WebPortal {
private:
    // Task handle for web server
   //TaskHandle_t webServerTaskHandle = NULL;
    
    MQTTManager* mqttManager; // Reference na MQTT manager
    
    // Static task function for the second core
    static void webServerTask(void *parameter);
    
    // Processing flag to avoid race conditions
    volatile bool isProcessing = false;

    AsyncWebServer server;       // Asynchronní webový server
    DNSServer dnsServer;         // DNS server pro captive portal
    SensorManager& sensorManager; // Reference na správce senzorů
    Logger& logger;              // Reference na logger
    
    bool isAPMode;               // Režim AP (true) nebo klient (false)
    String apName;               // Název AP v režimu AP
    
    // Konfigurace zařízení - reference na externí konfiguraci
    String& wifiSSID;
    String& wifiPassword;
    bool& configMode;
    String& timezone;
    
    // Inicializace AP módu
    void setupAP();

    ConfigManager& configManager;  // Reference na konfiguraci
    
    // Nastavení cest pro webový server
    void setupRoutes();
    
    // Vytvoření obsahu jednotlivých stránek
    String createHomePage();
    String createConfigPage();
    String createSensorsPage();
    String createSensorAddPage();
    String createSensorEditPage(int index);
    String createLogsPage();
    String createAPIPage();
    
    // Zpracování HTTP požadavků
    void handleRoot(AsyncWebServerRequest *request);
    void handleConfig(AsyncWebServerRequest *request);
    void handleConfigPost(AsyncWebServerRequest *request);
    void handleSensors(AsyncWebServerRequest *request);
    void handleSensorAdd(AsyncWebServerRequest *request);
    void handleSensorAddPost(AsyncWebServerRequest *request);
    void handleSensorEdit(AsyncWebServerRequest *request);
    void handleSensorEditPost(AsyncWebServerRequest *request);
    void handleSensorDelete(AsyncWebServerRequest *request);
    void handleLogs(AsyncWebServerRequest *request);
    void handleLogsClear(AsyncWebServerRequest *request);
    void handleLogLevel(AsyncWebServerRequest *request);
    void handleAPI(AsyncWebServerRequest *request);
    void handleMqtt(AsyncWebServerRequest *request);
    void handleMqttPost(AsyncWebServerRequest *request);
    void handleReboot(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
    
    // Příjem WebSocket zpráv
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                         AwsEventType type, void *arg, uint8_t *data, size_t len);
    
    // Odesílání aktualizací přes WebSocket
    void sendSensorUpdates();
    void sendLogUpdates();
    
public:
    // Konstruktor
    WebPortal(SensorManager& sensors, Logger& log, String& ssid, String& password,
             bool& configMode, ConfigManager& config, String& timezone);
    // Destruktor
    ~WebPortal();
    
    // Inicializace webového serveru
    bool init();
    
    // Obsluha požadavků
    void handleClient();
    
    // Zpracování DNS požadavků (pro captive portal)
    void processDNS();
    
    // Přepnutí mezi režimy AP a klient
    void setAPMode(bool enable);
    
    // Získání aktuálního režimu
    bool isInAPMode() const;
    
    // Restart webového serveru
    void restart();

    // Nastavení MQTTManager reference
    void setMqttManager(MQTTManager* manager) { mqttManager = manager; }
};

#include "WebServer.h"
#include "HTMLGenerator.h"
#include <WiFi.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "../config.h"

// Konstruktor
WebPortal::WebPortal(SensorManager& sensors, Logger& log, String& ssid, String& password, bool& configMode)
    : server(HTTP_PORT), sensorManager(sensors), logger(log), isAPMode(false),
      wifiSSID(ssid), wifiPassword(password), configMode(configMode) {
}

// Add this static task function to WebPortal in WebServer.cpp
// void WebPortal::webServerTask(void *parameter) {
//     WebPortal *server = (WebPortal*)parameter;
    
//     for (;;) {
//         if (server->isAPMode) {
//             // Process DNS requests (captive portal)
//             server->dnsServer.processNextRequest();
//         }
        
//         // Give other tasks time to run
//         vTaskDelay(1);
//     }
// }

// Modify the init function to create the task
bool WebPortal::init() {
    logger.info("Initializing web portal");
    
    // Create AP name (if needed)
    uint8_t mac[6];
    WiFi.macAddress(mac);
    
    apName = "SVERIO-GW-";
    for (int i = 0; i < 6; i++) {
        if (mac[i] < 16) apName += "0";
        apName += String(mac[i], HEX);
    }
    apName.toUpperCase();
    
    // Set mode based on current configuration
    isAPMode = configMode || (WiFi.status() != WL_CONNECTED);
    
    if (isAPMode) {
        setupAP();
    }
    
    // Setup routes
    setupRoutes();
    
    // Start server
    server.begin();
    logger.info("Web server started on port " + String(HTTP_PORT));
    
    // Create task on core 0 for DNS and other processing
    // xTaskCreatePinnedToCore(
    //     webServerTask,        // Task function
    //     "WebServerTask",      // Task name
    //     4096,                 // Stack size (bytes)
    //     this,                 // Parameter to pass
    //     1,                    // Task priority (1 is low)
    //     &webServerTaskHandle, // Task handle
    //     0                     // Run on core 0
    // );
    
    // logger.info("Web server task created on core 0");
    
    return true;
}


// Inicializace AP módu
void WebPortal::setupAP() {
    logger.info("Starting AP mode: " + apName);
    
    // Full WiFi reset sequence
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(200);
    
    // AP configuration with optimized parameters
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximum power
    
    // Set up AP mode with optimized settings
    WiFi.mode(WIFI_AP);
    logger.info("Setting up AP: " + apName);
    
    // AP configuration with 4 clients max and channel 6 (less crowded usually)
    bool apStarted = WiFi.softAP(apName.c_str(), "password123", 6, false, 4);
    delay(1000); // Give AP time to fully initialize
    
    if (apStarted) {
        logger.info("AP setup successful");
    } else {
        logger.error("AP setup failed");
    }
    
    IPAddress apIP = WiFi.softAPIP();
    logger.info("AP IP assigned: " + apIP.toString());
    
    // Configure DNS server with custom TTL for faster responses
    dnsServer.setTTL(30); // TTL in seconds (lower value = more responsive)
    dnsServer.start(DNS_PORT, "*", apIP);
    logger.info("DNS server started on port " + String(DNS_PORT));
    
    isAPMode = true;
}

// Nastavení cest pro webový server
// Modify WebPortal::setupRoutes() in WebServer.cpp
void WebPortal::setupRoutes() {
    logger.info("Setting up web server routes");
    
    // In AP mode, we only want to show WiFi configuration
    if (isAPMode) {
        // Basic pages for AP mode only
        server.on("/", HTTP_GET, std::bind(&WebPortal::handleConfig, this, std::placeholders::_1));
        logger.debug("Route registered: GET / (redirects to config in AP mode)");
        
        server.on("/config", HTTP_GET, std::bind(&WebPortal::handleConfig, this, std::placeholders::_1));
        logger.debug("Route registered: GET /config");
        
        server.on("/config", HTTP_POST, std::bind(&WebPortal::handleConfigPost, this, std::placeholders::_1));
        logger.debug("Route registered: POST /config");
    } else {
        // Full set of routes for client mode
        server.on("/", HTTP_GET, std::bind(&WebPortal::handleRoot, this, std::placeholders::_1));
        logger.debug("Route registered: GET /");
        
        server.on("/config", HTTP_GET, std::bind(&WebPortal::handleConfig, this, std::placeholders::_1));
        server.on("/config", HTTP_POST, std::bind(&WebPortal::handleConfigPost, this, std::placeholders::_1));
        
        // Sensor management
        server.on("/sensors/add", HTTP_GET, std::bind(&WebPortal::handleSensorAdd, this, std::placeholders::_1));
        server.on("/sensors/add", HTTP_POST, std::bind(&WebPortal::handleSensorAddPost, this, std::placeholders::_1));
        server.on("/sensors/edit", HTTP_GET, std::bind(&WebPortal::handleSensorEdit, this, std::placeholders::_1));
        server.on("/sensors/update", HTTP_POST, std::bind(&WebPortal::handleSensorEditPost, this, std::placeholders::_1));
        server.on("/sensors/delete", HTTP_GET, std::bind(&WebPortal::handleSensorDelete, this, std::placeholders::_1));
        server.on("/sensors", HTTP_GET, std::bind(&WebPortal::handleSensors, this, std::placeholders::_1));
        
        // Logs
        server.on("/logs/clear", HTTP_GET, std::bind(&WebPortal::handleLogsClear, this, std::placeholders::_1));
        server.on("/logs/level", HTTP_POST, std::bind(&WebPortal::handleLogLevel, this, std::placeholders::_1));
        server.on("/logs", HTTP_GET, std::bind(&WebPortal::handleLogs, this, std::placeholders::_1));
        
        // API
        server.on("/api", HTTP_GET, std::bind(&WebPortal::handleAPI, this, std::placeholders::_1));
        
        // Reboot
        server.on("/reboot", HTTP_GET, std::bind(&WebPortal::handleReboot, this, std::placeholders::_1));
    }
    
    // Unknown pages (404) - needed in both modes
    server.onNotFound(std::bind(&WebPortal::handleNotFound, this, std::placeholders::_1));
    logger.debug("Route registered: 404 handler");
    
    logger.info("All routes registered successfully");
}

// Modify the handleClient method to avoid duplicate processing
void WebPortal::handleClient() {
    // No need to do anything here since the task handles it
    // We keep this method for compatibility
}

// Ensure proper cleanup in the destructor
WebPortal::~WebPortal() {
    // Stop the task if it's running
    //if (webServerTaskHandle != NULL) {
    //    vTaskDelete(webServerTaskHandle);
    //}
    
    // Stop DNS server
    dnsServer.stop();
}

// Zpracování DNS požadavků
void WebPortal::processDNS() {
    dnsServer.processNextRequest();
    // je volána automaticky v handleClient()
}

// Přepnutí mezi režimy AP a klient
void WebPortal::setAPMode(bool enable) {
    if (enable == isAPMode) {
        return; // Žádná změna
    }
    
    if (enable) {
        // Přepnutí do AP módu
        setupAP();
    } else {
        // Přepnutí do klientského módu
        dnsServer.stop();
        isAPMode = false;
    }
}

// Získání aktuálního režimu
bool WebPortal::isInAPMode() const {
    return isAPMode;
}

// Restart webového serveru
void WebPortal::restart() {
    server.end();
    
    // Reset DNS serveru, pokud byl spuštěn
    if (isAPMode) {
        dnsServer.stop();
    }
    
    // Znovu inicializace
    init();
}

// Zpracování HTTP požadavků

// Kořenová stránka
void WebPortal::handleRoot(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /");
    
    // In AP mode, redirect to config page
    if (isAPMode) {
        request->redirect("/config");
        return;
    }

    // Získání seznamu senzorů
    std::vector<SensorData> sensorsList = sensorManager.getActiveSensors();
    
    // Vygenerování HTML
    String html = HTMLGenerator::generateHomePage(sensorsList);
    
    // Odeslání odpovědi
    request->send(200, "text/html", html);
}

// Stránka konfigurace WiFi
void WebPortal::handleConfig(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /config");
    
    String currentIP = isAPMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    
    // Vygenerování HTML
    String html = HTMLGenerator::generateConfigPage(wifiSSID, wifiPassword, configMode, currentIP);
    
    // Odeslání odpovědi
    request->send(200, "text/html", html);
}

// Zpracování formuláře konfigurace WiFi
void WebPortal::handleConfigPost(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: POST /config");
    
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
        wifiSSID = request->getParam("ssid", true)->value();
        wifiPassword = request->getParam("password", true)->value();
        
        logger.info("New WiFi configuration - SSID: " + wifiSSID);
        
        // IMPORTANT: Make sure to save both to ConfigManager and to Preferences
        // This ensures config persists across reboots
        configMode = false;
        
        // Since you have direct reference to these variables, you need to save them
        // to persistent storage before restarting
        // Add this code that explicitly saves to both file system and preferences
        
        // Save to HTML response
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta http-equiv='refresh' content='10;url=/'>";
        html += "<title>Configuration Saved</title></head>";
        html += "<body><h1>Configuration Saved</h1>";
        html += "<p>New WiFi settings have been saved. The device will restart in a few seconds.</p>";
        html += "</body></html>";
        
        request->send(200, "text/html", html);
        
        // Make sure configMode is saved as FALSE
        // This is critical to ensure it doesn't start in AP mode after restart
        File file = LittleFS.open(CONFIG_FILE, "w");
        if (file) {
            DynamicJsonDocument doc(512);
            doc["ssid"] = wifiSSID;
            doc["password"] = wifiPassword;
            doc["configMode"] = false;  // Explicitly set to false
            serializeJson(doc, file);
            file.close();
            logger.info("Configuration saved to file system");
        }
        
        // Restart ESP32 after 1 second
        delay(1000);
        ESP.restart();
    } else {
        // Missing parameters
        request->send(400, "text/plain", "Missing parameters");
    }
}

// Stránka se seznamem senzorů
void WebPortal::handleSensors(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /sensors");
    
    // Získání seznamu senzorů
    std::vector<SensorData> sensorsList = sensorManager.getActiveSensors();
    
    // Vygenerování HTML
    String html = HTMLGenerator::generateSensorsPage(sensorsList);

    // Odeslání odpovědi
    request->send(200, "text/html", html);
}

// Stránka pro přidání senzoru
void WebPortal::handleSensorAdd(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /sensors/add");
    
    // Vygenerování HTML
    String html = HTMLGenerator::generateSensorAddPage();
    
    // Odeslání odpovědi
    request->send(200, "text/html", html);
}

// Zpracování formuláře pro přidání senzoru
void WebPortal::handleSensorAddPost(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: POST /sensors/add");
    
    // Kontrola parametrů
    if (request->hasParam("name", true) && 
        request->hasParam("deviceType", true) && 
        request->hasParam("serialNumber", true) && 
        request->hasParam("deviceKey", true)) {
        
        // Získání dat z formuláře
        String name = request->getParam("name", true)->value();
        uint8_t deviceTypeValue = request->getParam("deviceType", true)->value().toInt();
        String serialNumberHex = request->getParam("serialNumber", true)->value();
        String deviceKeyHex = request->getParam("deviceKey", true)->value();
        String endpoint = request->hasParam("endpoint", true) ? 
                          request->getParam("endpoint", true)->value() : "";
        String customParams = request->hasParam("customParams", true) ? 
                              request->getParam("customParams", true)->value() : "";
        
        // Převod hodnot
        SensorType deviceType = static_cast<SensorType>(deviceTypeValue);
        uint32_t serialNumber = strtoul(serialNumberHex.c_str(), NULL, 16);
        uint32_t deviceKey = strtoul(deviceKeyHex.c_str(), NULL, 16);
        
        // Přidání senzoru
        int sensorIndex = sensorManager.addSensor(deviceType, serialNumber, deviceKey, name);
        
        if (sensorIndex >= 0) {
            // Získání přidaného senzoru a aktualizace dalších údajů
            SensorData* sensor = sensorManager.getSensor(sensorIndex);
            if (sensor) {
                sensor->endpoint = endpoint;
                sensor->customParams = customParams;
                sensorManager.saveSensors(true);
                
                logger.info("Added new sensor: " + name + " (SN: 0x" + serialNumberHex + ")");
                
                // Přesměrování na stránku se seznamem senzorů
                request->redirect("/sensors");
                return;
            }
        }
        
        // Chyba při přidání senzoru
        request->send(500, "text/plain", "Failed to add sensor");
    } else {
        // Chybí parametry
        request->send(400, "text/plain", "Missing parameters");
    }
}

// Stránka pro úpravu senzoru
void WebPortal::handleSensorEdit(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /sensors/edit");
    
    // Kontrola parametru index
    if (request->hasParam("index")) {
        int index = request->getParam("index")->value().toInt();
        
        // Získání senzoru
        const SensorData* sensor = sensorManager.getSensor(index);
        
        if (sensor && sensor->configured) {
            // Vygenerování HTML
            String html = HTMLGenerator::generateSensorEditPage(*sensor, index);
            
            // Odeslání odpovědi
            request->send(200, "text/html", html);
            return;
        }
    }
    
    // Senzor nenalezen, přesměrování na stránku se seznamem senzorů
    request->redirect("/sensors");
}

// Zpracování formuláře pro úpravu senzoru
void WebPortal::handleSensorEditPost(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: POST /sensors/update");
    
    // Kontrola parametrů
    if (request->hasParam("index", true) && 
        request->hasParam("name", true) && 
        request->hasParam("deviceType", true) && 
        request->hasParam("serialNumber", true) && 
        request->hasParam("deviceKey", true)) {
        
        // Získání dat z formuláře
        int index = request->getParam("index", true)->value().toInt();
        String name = request->getParam("name", true)->value();
        uint8_t deviceTypeValue = request->getParam("deviceType", true)->value().toInt();
        String serialNumberHex = request->getParam("serialNumber", true)->value();
        String deviceKeyHex = request->getParam("deviceKey", true)->value();
        String endpoint = request->hasParam("endpoint", true) ? 
                          request->getParam("endpoint", true)->value() : "";
        String customParams = request->hasParam("customParams", true) ? 
                              request->getParam("customParams", true)->value() : "";
        
        // Převod hodnot
        SensorType deviceType = static_cast<SensorType>(deviceTypeValue);
        uint32_t serialNumber = strtoul(serialNumberHex.c_str(), NULL, 16);
        uint32_t deviceKey = strtoul(deviceKeyHex.c_str(), NULL, 16);
        
        // Aktualizace senzoru
        bool success = sensorManager.updateSensorConfig(index, name, deviceType, serialNumber, 
                                                       deviceKey, endpoint, customParams);
        
        if (success) {
            logger.info("Updated sensor: " + name + " (SN: 0x" + serialNumberHex + ")");
            
            // Přesměrování na stránku se seznamem senzorů
            request->redirect("/sensors");
        } else {
            // Chyba při aktualizaci senzoru
            request->send(500, "text/plain", "Failed to update sensor");
        }
    } else {
        // Chybí parametry
        request->send(400, "text/plain", "Missing parameters");
    }
}

// Smazání senzoru
void WebPortal::handleSensorDelete(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /sensors/delete");
    
    // Kontrola parametru index
    if (request->hasParam("index")) {
        int index = request->getParam("index")->value().toInt();
        
        // Získání senzoru
        const SensorData* sensor = sensorManager.getSensor(index);
        
        if (sensor && sensor->configured) {
            String name = sensor->name;
            uint32_t serialNumber = sensor->serialNumber;
            
            // Smazání senzoru
            bool success = sensorManager.deleteSensor(index);
            
            if (success) {
                logger.info("Deleted sensor: " + name + " (SN: " + String(serialNumber, HEX) + ")");
            } else {
                logger.warning("Failed to delete sensor with index " + String(index));
            }
        } else {
            logger.warning("Attempt to delete non-existent sensor with index " + String(index));
        }
    }
    
    // Přesměrování na stránku se seznamem senzorů
    request->redirect("/sensors");
}

// Stránka logů
void WebPortal::handleLogs(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /logs");
    
    // Získání logů
    size_t logCount = 0;
    const LogEntry* logs = logger.getLogs(logCount);
    
    // Vygenerování HTML
    String html = HTMLGenerator::generateLogsPage(logs, logCount, logger.getLogLevel());
    
    // Odeslání odpovědi
    request->send(200, "text/html", html);
}

// Vymazání logů
void WebPortal::handleLogsClear(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /logs/clear");
    
    // Vymazání logů
    logger.clearLogs();
    
    logger.info("Memory after log clear - Free heap: " + String(ESP.getFreeHeap()) + 
           " bytes, Largest block: " + String(ESP.getMaxAllocHeap()) + " bytes");

    // Přesměrování zpět na stránku logů
    request->redirect("/logs");
}

// Nastavení úrovně logování
void WebPortal::handleLogLevel(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: POST /logs/level");
    
    // Kontrola parametru level
    if (request->hasParam("level", true)) {
        String levelStr = request->getParam("level", true)->value();
        LogLevel level = logger.levelFromString(levelStr);
        
        // Nastavení nové úrovně
        logger.setLogLevel(level);
    }
    
    // Přesměrování zpět na stránku logů
    request->redirect("/logs");
}

// API pro získání dat senzorů
void WebPortal::handleAPI(AsyncWebServerRequest *request) {
    logger.debug("HTTP request: GET /api");
    
    // Kontrola parametru format
    String format = request->hasParam("format") ? request->getParam("format")->value() : "json";
    
    // Kontrola parametru sensor
    String sensorParam = request->hasParam("sensor") ? request->getParam("sensor")->value() : "";
    
    // Získání seznamu senzorů
    std::vector<SensorData> sensorsList;
    
    if (sensorParam.length() > 0) {
        // Filtrování podle konkrétního senzoru
        uint32_t serialNumber = strtoul(sensorParam.c_str(), NULL, 16);
        int sensorIndex = sensorManager.findSensorBySN(serialNumber);
        
        if (sensorIndex >= 0) {
            const SensorData* sensor = sensorManager.getSensor(sensorIndex);
            if (sensor && sensor->configured) {
                sensorsList.push_back(*sensor);
            }
        }
    } else {
        // Všechny senzory
        sensorsList = sensorManager.getActiveSensors();
    }
    
    if (format.equalsIgnoreCase("json")) {
        // JSON formát
        String jsonOutput = HTMLGenerator::generateAPIJson(sensorsList);
        
        // Odeslání JSON odpovědi
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print(jsonOutput);
        response->addHeader(CORS_HEADER_NAME, CORS_HEADER_VALUE);
        request->send(response);
    } else if (format.equalsIgnoreCase("csv")) {
        // CSV formát
        AsyncResponseStream *response = request->beginResponseStream("text/csv");
        response->print("name,type,serialNumber,lastSeen,temperature,humidity,pressure,ppm,lux,batteryVoltage,rssi\r\n");
        
        for (const auto& sensor : sensorsList) {
            response->print(sensor.name);
            response->print(",");
            response->print(static_cast<uint8_t>(sensor.deviceType));
            response->print(",");
            response->print("0x");
            response->print(String(sensor.serialNumber, HEX));
            response->print(",");
            response->print(sensor.lastSeen > 0 ? String((millis() - sensor.lastSeen) / 1000) : "-1");
            response->print(",");
            
            response->print(sensor.hasTemperature() ? String(sensor.temperature, 2) : "");
            response->print(",");
            response->print(sensor.hasHumidity() ? String(sensor.humidity, 2) : "");
            response->print(",");
            response->print(sensor.hasPressure() ? String(sensor.pressure, 2) : "");
            response->print(",");
            response->print(sensor.hasPPM() ? String(sensor.ppm, 0) : "");
            response->print(",");
            response->print(sensor.hasLux() ? String(sensor.lux, 1) : "");
            response->print(",");
            response->print(String(sensor.batteryVoltage, 2));
            response->print(",");
            response->print(String(sensor.rssi));
            response->print("\r\n");
        }
        
        response->addHeader(CORS_HEADER_NAME, CORS_HEADER_VALUE);
        request->send(response);
    } else if (format.equalsIgnoreCase("html")) {
        // HTML formát (dokumentace API)
        String html = HTMLGenerator::generateAPIPage(sensorsList);
        request->send(200, "text/html", html);
    } else {
        // Neznámý formát
        request->send(400, "text/plain", "Invalid format parameter. Supported formats: json, csv, html");
    }
}

// Restart zařízení
void WebPortal::handleReboot(AsyncWebServerRequest *request) {
    logger.info("HTTP request: GET /reboot - Rebooting device");
    
    // Odeslání potvrzení
    request->send(200, "text/html", 
                 "<html><head><meta http-equiv='refresh' content='10;url=/'></head>"
                 "<body><h1>Rebooting</h1>"
                 "<p>The device is rebooting. You will be redirected in 10 seconds...</p></body></html>");
    
    // Restart ESP32 po 500ms (aby se stihla odeslat odpověď)
    delay(500);
    ESP.restart();
}

// Obsluha neznámých stránek
// In WebServer.cpp, modify the handleNotFound method
// Update WebPortal::handleNotFound in WebServer.cpp
void WebPortal::handleNotFound(AsyncWebServerRequest *request) {
    String requestUrl = request->url();
    logger.debug("HTTP 404: " + requestUrl);
    
    if (isAPMode) {
        // In AP mode - captive portal - redirect all requests to config page
        
        // Skip redirecting for certain files that might be requested by browsers
        if (requestUrl.endsWith(".ico") || requestUrl.endsWith(".jpg") || 
            requestUrl.endsWith(".png") || requestUrl.endsWith(".gif") ||
            requestUrl.endsWith(".css") || requestUrl.endsWith(".js")) {
            request->send(404, "text/plain", "Not found");
            return;
        }
        
        // For Apple devices
        if (requestUrl == "/hotspot-detect.html" || requestUrl.indexOf("captive.apple.com") >= 0) {
            request->redirect("http://" + WiFi.softAPIP().toString() + "/config");
            return;
        }
        
        // For Android and Windows devices
        if (requestUrl == "/generate_204" || requestUrl == "/ncsi.txt" || 
            requestUrl.indexOf("detectportal.firefox.com") >= 0) {
            request->redirect("http://" + WiFi.softAPIP().toString() + "/config");
            return;
        }
        
        // General case - redirect to config
        request->redirect("/config");
    } else {
        // In normal mode - 404
        request->send(404, "text/plain", "404: Not Found");
    }
}
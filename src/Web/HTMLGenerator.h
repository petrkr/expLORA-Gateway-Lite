#pragma once

#include <Arduino.h>
#include <vector>
#include "../Data/SensorData.h"
#include "../Data/Logging.h"

/**
 * Třída pro generování HTML obsahu
 * 
 * Zajišťuje vytváření HTML stránek pro webové rozhraní, optimalizované
 * pro použití s PSRAM (pokud je k dispozici).
 */
class HTMLGenerator {
private:
    // Buffer pro generování HTML obsahu
    static char* htmlBuffer;
    static size_t htmlBufferSize;
    static bool usePSRAM;
    
    // Inicializace bufferu
    static bool initBuffer();
    
    // Uvolnění bufferu
    static void freeBuffer();
    
    // Přidání HTML úvodního kódu
    static void addHtmlHeader(String& html, const String& title, bool isAPMode);
    
    // Přidání HTML koncového kódu
    static void addHtmlFooter(String& html);
    
    // Přidání CSS pro stránky
    static void addStyles(String& html);
    
    // Přidání JavaScript pro interaktivní prvky
    static void addJavaScript(String& html);
    
    // Přidání navigace
    static void addNavigation(String& html, const String& activePage);
    
public:
    // Inicializace generátoru
    static bool init(bool usePsram = true, size_t bufferSize = 32768);
    
    // Uvolnění zdrojů
    static void deinit();
    
    // Generování domovské stránky
    static String generateHomePage(const std::vector<SensorData>& sensors);
    
    // Generování stránky konfigurace
    static String generateConfigPage(const String& ssid, const String& password, 
                                    bool configMode, const String& ip);
    
    // Generování stránky se seznamem senzorů
    static String generateSensorsPage(const std::vector<SensorData>& sensors);
    
    // Generování stránky pro přidání senzoru
    static String generateSensorAddPage();
    
    // Generování stránky pro úpravu senzoru
    static String generateSensorEditPage(const SensorData& sensor, int index);
    
    // Generování stránky logů
    static String generateLogsPage(const LogEntry* logs, size_t logCount, LogLevel currentLevel);
    
    // Generování stránky API
    static String generateAPIPage(const std::vector<SensorData>& sensors);
    
    // Generování JSON pro API
    static String generateAPIJson(const std::vector<SensorData>& sensors);
    
    // Optimalizované verze pomocí bufferu
    static void generateSensorTable(char* buffer, size_t& maxLen, const std::vector<SensorData>& sensors);
    static void generateLogTable(char* buffer, size_t& maxLen, const LogEntry* logs, size_t logCount);
    
    // Další pomocné metody
    static String getWifiNetworkOptions(const String& currentSSID);
    static String getSensorTypeOptions(SensorType currentType);
    static String getLogLevelOptions(LogLevel currentLevel);
};
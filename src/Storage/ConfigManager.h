#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../Data/Logging.h"
#include "../config.h"

/**
 * Třída pro správu konfigurace zařízení
 * 
 * Zajišťuje ukládání a načítání konfigurace ze dvou zdrojů:
 * 1. LittleFS - pro běžnou konfiguraci, která se resetuje při aktualizaci firmwaru
 * 2. Preferences - pro persistentní konfiguraci, která přežije aktualizaci firmwaru
 */
class ConfigManager {
private:
    Logger& logger;              // Reference na logger
    Preferences preferences;     // Pro persistentní konfiguraci
    bool preferencesInitialized; // Příznak inicializace Preferences
    
    // Konfigurační soubory
    const char* configFile;      // Soubor s konfigurací
    
    // Načítání a ukládání z/do LittleFS
    bool loadFromFS();
    bool saveToFS();
    
    // Načítání a ukládání z/do Preferences
    bool loadFromPreferences();
    bool saveToPreferences();
    
public:
    // Konfigurace zařízení
    String wifiSSID;             // SSID WiFi sítě
    String wifiPassword;         // Heslo WiFi sítě
    bool configMode;             // Režim konfigurace (AP mód)
    unsigned long lastWifiAttempt; // Čas posledního pokusu o připojení k WiFi
    LogLevel logLevel;           // Úroveň logování
    
    // Konstruktor
    ConfigManager(Logger& log, const char* file = CONFIG_FILE);
    
    // Destruktor
    ~ConfigManager();
    
    // Inicializace
    bool init();
    
    // Načtení konfigurace z obou zdrojů
    bool load();
    
    // Uložení konfigurace do obou zdrojů
    bool save();
    
    // Resetování konfigurace na výchozí hodnoty
    void resetToDefaults();
    
    // Získání verze firmwaru
    String getFirmwareVersion() const;
    
    // Získání MAC adresy zařízení
    String getMacAddress() const;
    
    // Získání názvu zařízení pro AP mód
    String getDeviceName() const;
    
    // Nastavení WiFi konfigurace
    bool setWiFiConfig(const String& ssid, const String& password, bool saveConfig = true);
    
    // Přepnutí do režimu konfigurace (AP)
    void enableConfigMode(bool enable = true, bool saveConfig = true);
    
    // Nastavení úrovně logování
    void setLogLevel(LogLevel level, bool saveConfig = true);
};
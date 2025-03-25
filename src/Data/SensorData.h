#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "SensorTypes.h"

/**
 * Struktura pro ukládání dat senzorů
 * 
 * Tato struktura ukládá jak konfigurační data, tak aktuální hodnoty
 * měření ze senzorů. Díky propojení se SensorTypes umožňuje flexibilní
 * práci s různými typy senzorů a jejich specifickými daty.
 */
struct SensorData {
    // Základní identifikace senzoru
    SensorType deviceType;     // Typ zařízení
    uint32_t serialNumber;     // Sériové číslo
    uint32_t deviceKey;        // Klíč pro dešifrování dat
    String name;               // Uživatelský název senzoru
    
    // Rozšířená konfigurace
    String endpoint;           // Vlastní endpoint pro odesílání dat
    String customParams;       // Vlastní parametry pro URL
    
    // Stav senzoru
    unsigned long lastSeen;    // Čas posledního viděného paketu
    bool configured;           // Zda je senzor nakonfigurován
    
    // Obecná data senzorů - hodnoty jsou platné podle typu zařízení
    float temperature;         // Teplota (°C) - BME280, SCD40
    float humidity;            // Vlhkost (%) - BME280, SCD40
    float pressure;            // Tlak (hPa) - BME280
    float ppm;                 // CO2 koncentrace (ppm) - SCD40
    float lux;                 // Intenzita osvětlení (lux) - VEML7700
    
    // Obecná data zařízení
    float batteryVoltage;      // Napětí baterie (V)
    int rssi;                  // Síla signálu (dBm)
    
    // Konstruktor pro inicializaci výchozími hodnotami
    SensorData() :
        deviceType(SensorType::UNKNOWN),
        serialNumber(0),
        deviceKey(0),
        name(""),
        endpoint(""),
        customParams(""),
        lastSeen(0),
        configured(false),
        temperature(0.0f),
        humidity(0.0f),
        pressure(0.0f),
        ppm(0.0f),
        lux(0.0f),
        batteryVoltage(0.0f),
        rssi(0)
    {}
    
    // Pomocné metody pro ověření, zda senzor poskytuje určitý typ dat
    bool hasTemperature() const {
        return getSensorTypeInfo(deviceType).hasTemperature;
    }
    
    bool hasHumidity() const {
        return getSensorTypeInfo(deviceType).hasHumidity;
    }
    
    bool hasPressure() const {
        return getSensorTypeInfo(deviceType).hasPressure;
    }
    
    bool hasPPM() const {
        return getSensorTypeInfo(deviceType).hasPPM;
    }
    
    bool hasLux() const {
        return getSensorTypeInfo(deviceType).hasLux;
    }
    
    // Získání informací o typu senzoru
    const SensorTypeInfo& getTypeInfo() const {
        return getSensorTypeInfo(deviceType);
    }

    // Převod na JSON pro webové API a ukládání
    void toJson(JsonObject& json) const {
        json["deviceType"] = static_cast<uint8_t>(deviceType);
        json["typeName"] = getTypeInfo().name;
        json["serialNumber"] = String(serialNumber, HEX);
        json["name"] = name;
        
        // Přidání dat specifických pro typ senzoru s kontrolou platnosti
       //if (lastSeen > 0) {
            if (hasTemperature()) {
                json["temperature"] = round(temperature * 100) / 100.0;
            }
            
            if (hasHumidity()) {
                json["humidity"] = round(humidity * 100) / 100.0;
            }
            
            if (hasPressure()) {
                json["pressure"] = round(pressure * 100) / 100.0;
            }
            
            if (hasPPM()) {
                json["ppm"] = round(ppm);
            }
            
            if (hasLux()) {
                json["lux"] = round(lux * 10) / 10.0;
            }
            
            json["batteryVoltage"] = round(batteryVoltage * 100) / 100.0;
            json["rssi"] = rssi;
            
            if (lastSeen > 0) {
                json["lastSeen"] = (millis() - lastSeen) / 1000; // sekundy od posledního vidění
            } else{
                json["lastSeen"] = -1;
            }
    }
    
    // Formátování dat pro zobrazení na webu
    String getDataString() const {
        String dataStr = "";
        bool first = true;
        
        if (lastSeen == 0) {
            return "-";
        }
        
        if (hasTemperature()) {
            if (!first) dataStr += ", ";
            dataStr += String(temperature, 2) + " °C";
            first = false;
        }
        
        if (hasHumidity()) {
            if (!first) dataStr += ", ";
            dataStr += String(humidity, 2) + " %";
            first = false;
        }
        
        if (hasPressure()) {
            if (!first) dataStr += ", ";
            dataStr += String(pressure, 2) + " hPa";
            first = false;
        }
        
        if (hasPPM()) {
            if (!first) dataStr += ", ";
            dataStr += String(int(ppm)) + " ppm CO2";
            first = false;
        }
        
        if (hasLux()) {
            if (!first) dataStr += ", ";
            dataStr += String(lux, 1) + " lux";
            first = false;
        }
        
        // Přidání napětí baterie
        if (!first) dataStr += ", ";
        dataStr += String(batteryVoltage, 2) + " V";
        
        return dataStr;
    }
    
    // Formátování času posledního viděného paketu
    String getLastSeenString() const {
        if (lastSeen == 0) {
            return "Never";
        }
        
        unsigned long seconds = (millis() - lastSeen) / 1000;
        if (seconds < 60) {
            return String(seconds) + " seconds ago";
        } else if (seconds < 3600) {
            return String(seconds / 60) + " minutes ago";
        } else {
            return String(seconds / 3600) + " hours ago";
        }
    }
};
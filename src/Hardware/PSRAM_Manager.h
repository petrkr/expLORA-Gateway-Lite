#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>

/**
 * Třída pro správu PSRAM paměti
 * 
 * Poskytuje metody pro alokaci a uvolnění paměti v PSRAM,
 * s automatickým fallbackem na běžnou RAM, pokud není PSRAM k dispozici.
 */
class PSRAMManager {
private:
    static bool psramAvailable;  // Příznak dostupnosti PSRAM
    
public:
    // Inicializace
    static bool init();
    
    // Kontrola dostupnosti PSRAM
    static bool isPSRAMAvailable();
    
    // Získání informací o PSRAM
    static size_t getTotalPSRAM();
    static size_t getFreePSRAM();
    static size_t getLargestFreePSRAMBlock();
    
    // Alokace paměti v PSRAM s fallbackem na heap
    static void* allocateMemory(size_t size);
    
    // Uvolnění paměti
    static void freeMemory(void* ptr);
    
    // Vrátí procentuální využití PSRAM
    static int getPSRAMUsagePercent();
    
    // Vytvoření bufferu v PSRAM s fallbackem na heap
    template<typename T>
    static T* createBuffer(size_t size) {
        if (isPSRAMAvailable()) {
            // Alokace v PSRAM
            T* buffer = (T*)heap_caps_malloc(sizeof(T) * size, MALLOC_CAP_SPIRAM);
            if (buffer != nullptr) {
                return buffer;
            }
        }
        
        // Fallback na běžnou RAM
        return new T[size];
    }
    
    // Uvolnění bufferu
    template<typename T>
    static void deleteBuffer(T* buffer) {
        if (buffer == nullptr) {
            return;
        }
        
        if (isPSRAMAvailable() && heap_caps_check_integrity_addr((intptr_t)buffer, false)) {
            // Buffer je v PSRAM
            heap_caps_free(buffer);
        } else {
            // Buffer je v běžné RAM
            delete[] buffer;
        }
    }
};
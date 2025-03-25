#include "PSRAM_Manager.h"
#include <Arduino.h>

// Inicializace statických proměnných
bool PSRAMManager::psramAvailable = false;

// Inicializace
bool PSRAMManager::init() {
    psramAvailable = esp_spiram_is_initialized();
    
    if (psramAvailable) {
        Serial.printf("PSRAM initialized: %u bytes total, %u bytes free\n", 
                     getTotalPSRAM(), getFreePSRAM());
    } else {
        Serial.println("PSRAM not available");
    }
    
    return psramAvailable;
}

// Kontrola dostupnosti PSRAM
bool PSRAMManager::isPSRAMAvailable() {
    return psramAvailable;
}

// Získání informací o PSRAM
size_t PSRAMManager::getTotalPSRAM() {
    if (!psramAvailable) return 0;
    return ESP.getPsramSize();
}

size_t PSRAMManager::getFreePSRAM() {
    if (!psramAvailable) return 0;
    return ESP.getFreePsram();
}

size_t PSRAMManager::getLargestFreePSRAMBlock() {
    if (!psramAvailable) return 0;
    return heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}

// Alokace paměti v PSRAM s fallbackem na heap
void* PSRAMManager::allocateMemory(size_t size) {
    void* ptr = nullptr;
    
    if (psramAvailable) {
        // Pokus o alokaci v PSRAM
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    }
    
    if (ptr == nullptr) {
        // Fallback na běžnou RAM
        ptr = malloc(size);
    }
    
    return ptr;
}

// Uvolnění paměti
void PSRAMManager::freeMemory(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    
    if (psramAvailable && heap_caps_check_integrity_addr((intptr_t)ptr, false)) {
        // Paměť je v PSRAM
        heap_caps_free(ptr);
    } else {
        // Paměť je v běžné RAM
        free(ptr);
    }
}

// Vrátí procentuální využití PSRAM
int PSRAMManager::getPSRAMUsagePercent() {
    if (!psramAvailable) return 0;
    
    size_t total = getTotalPSRAM();
    size_t free = getFreePSRAM();
    
    if (total == 0) return 0;
    
    return 100 - (free * 100 / total);
}
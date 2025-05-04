#pragma once

#include <Arduino.h>
#include <mutex>
#include "../config.h"

/**
 * Definice systému logování s podporou různých úrovní
 * 
 * Systém podporuje ukládání logů do PSRAM (pokud je k dispozici) a umožňuje
 * nastavit úroveň logování přes webové rozhraní. Logy jsou ukládány v kruhovém
 * bufferu pro efektivní využití paměti.
 */

// Úrovně logování
enum class LogLevel {
    ERROR = 0,    // Pouze kritické chyby
    WARNING = 1,  // Chyby a varování
    INFO = 2,     // Základní informace o činnosti
    DEBUG = 3,    // Podrobné informace pro debugování
    VERBOSE = 4   // Všechny dostupné informace
};

// Struktura pro uložení záznamu logu
struct LogEntry {
    unsigned long timestamp;  // Časová značka v milisekundách od startu
    LogLevel level;           // Úroveň logu
    String message;           // Textová zpráva
    String formattedTime;     // Formátovaný čas (pokud je k dispozici)
    
    // Převod úrovně logu na textovou reprezentaci
    String getLevelString() const {
        switch (level) {
            case LogLevel::ERROR:   return "ERROR";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::VERBOSE: return "VERBOSE";
            default:                return "UNKNOWN";
        }
    }
    
    // Získání barvy pro danou úroveň logu (pro webové rozhraní)
    String getLevelColor() const {
        switch (level) {
            case LogLevel::ERROR:   return "#ff5555"; // Červená
            case LogLevel::WARNING: return "#ffaa00"; // Oranžová
            case LogLevel::INFO:    return "#2196F3"; // Modrá
            case LogLevel::DEBUG:   return "#4CAF50"; // Zelená
            case LogLevel::VERBOSE: return "#9E9E9E"; // Šedá
            default:                return "#000000"; // Černá
        }
    }
    
    // Formátování logu pro zobrazení v UI
    String getFormattedLog() const {
        return formattedTime + " [" + getLevelString() + "] " + message;
    }
};

class Logger {
private:
    static LogLevel currentLevel;
    static LogEntry* logBuffer;
    static size_t logBufferSize;
    static size_t logIndex;
    static size_t logCount;
    static bool usePSRAM;
    static std::mutex logMutex;
    static bool initialized;
    static bool timeInitialized;
    
    // Získání časové značky (pokud je k dispozici)
    static String getTimeStamp();
    
public:
    // Inicializace loggeru
    static bool init(size_t bufferSize = LOG_BUFFER_SIZE);
    
    // Uvolnění paměti při skončení
    static void deinit();
    
    // Nastavení úrovně logování
    static void setLogLevel(LogLevel level);
    
    // Získání aktuální úrovně logování
    static LogLevel getLogLevel();
    
    // Přidání logu s určenou úrovní
    static void log(LogLevel level, const String& message);
    
    // Pomocné metody pro různé úrovně logů
    static void error(const String& message);
    static void warning(const String& message);
    static void info(const String& message);
    static void debug(const String& message);
    static void verbose(const String& message);
    
    // Získání všech logů (pro webové rozhraní)
    static const LogEntry* getLogs(size_t &count);
    
    // Vymazání všech logů
    static void clearLogs();
    
    // Převod úrovně logu z řetězce
    static LogLevel levelFromString(const String& levelStr);
    
    // Převod úrovně logu na řetězec
    static String levelToString(LogLevel level);

    // Nastavení inicializace času
    static void setTimeInitialized(bool initialized);

    static bool isTimeInitialized() { return timeInitialized; }

    static size_t getLogIndex() { return logIndex; }
    static size_t getLogBufferSize() { return logBufferSize; }
};
#include "Logging.h"
#include <time.h>
#include <esp_heap_caps.h>
#include <Arduino.h>

// Inicializace statických proměnných
LogLevel Logger::currentLevel = LogLevel::INFO;
LogEntry* Logger::logBuffer = nullptr;
size_t Logger::logBufferSize = LOG_BUFFER_SIZE;
size_t Logger::logIndex = 0;
size_t Logger::logCount = 0;
std::mutex Logger::logMutex;
bool Logger::initialized = false;

bool Logger::timeInitialized = false;

void Logger::setTimeInitialized(bool initialized) {
    timeInitialized = initialized;
}

// Inicializace loggeru
bool Logger::init(size_t bufferSize) {
    // Pokud je již inicializován, nejprve uvolníme paměť
    if (initialized) {
        deinit();
    }
    
    logBufferSize = bufferSize;
    

    logBuffer = new LogEntry[logBufferSize];
    Serial.printf("Logger: Allocated %u bytes in RAM for logs\n", sizeof(LogEntry) * logBufferSize);
    
    // Kontrola, zda se povedla alokace paměti
    if (logBuffer == nullptr) {
        Serial.println("Logger: Failed to allocate memory for logs");
        return false;
    }
    
    // Reset indexů a počtu logů
    logIndex = 0;
    logCount = 0;
    initialized = true;
    
    // První log po inicializaci - použijte char[] místo String concat
    char initMsg[64];
    snprintf(initMsg, sizeof(initMsg), "Logging system initialized with %u entries", 
             logBufferSize);
    info(initMsg);
    
    return true;
}

// Uvolnění paměti loggeru
void Logger::deinit() {
    if (!initialized) return;
    
    std::lock_guard<std::mutex> lock(logMutex);
    
    if (logBuffer != nullptr) {
        delete[] logBuffer;
        logBuffer = nullptr;
    }
    
    initialized = false;
}

// Nastavení úrovně logování
void Logger::setLogLevel(LogLevel level) {
    currentLevel = level;
    info("Log level set to: " + levelToString(level));
}

// Získání aktuální úrovně logování
LogLevel Logger::getLogLevel() {
    return currentLevel;
}

// Získání časové značky
String Logger::getTimeStamp() {
    static char timeBuffer[32];
    
    if (timeInitialized) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
            return String(timeBuffer);
        }
    }
    
    // Time not set - use simple static string
    return String("[Time not set]");
}

// Přidání logu s určenou úrovní
void Logger::log(LogLevel level, const String& message) {
    if (!initialized || level > currentLevel) {
        return;
    }
    
    // Získáme časovou značku jen jednou
    String timeStamp = getTimeStamp();
    //String timeStamp = "[Time not set]";

    // Výpis na sériový port
    Serial.print(timeStamp);
    Serial.print(F(" ["));
    Serial.print(levelToString(level));
    Serial.print(F("] "));
    Serial.println(message);

    std::lock_guard<std::mutex> lock(logMutex);
    
    if (logBuffer != nullptr) {
        logBuffer[logIndex].timestamp = millis();
        logBuffer[logIndex].level = level;
        logBuffer[logIndex].message = message;
        logBuffer[logIndex].formattedTime = timeStamp;
      
        logIndex = (logIndex + 1) % logBufferSize;
        if (logCount < logBufferSize) {
            logCount++;
        }
    }
}

// Pomocné metody pro různé úrovně logů
void Logger::error(const String& message) {
    log(LogLevel::ERROR, message);
}

void Logger::warning(const String& message) {
    log(LogLevel::WARNING, message);
}

void Logger::info(const String& message) {
    log(LogLevel::INFO, message);
}

void Logger::debug(const String& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::verbose(const String& message) {
    log(LogLevel::VERBOSE, message);
}

// Získání všech logů (pro webové rozhraní)
const LogEntry* Logger::getLogs(size_t &count) {
    std::lock_guard<std::mutex> lock(logMutex);
    count = logCount;
    return logBuffer;
}

// Vymazání všech logů
void Logger::clearLogs() {
    std::lock_guard<std::mutex> lock(logMutex);
    
    // Explicitly clear all String objects
    for (size_t i = 0; i < logBufferSize; i++) {
        // Force String deallocation by assigning empty strings
        logBuffer[i].message = String();
        logBuffer[i].formattedTime = String();
    }
    
    logIndex = 0;
    logCount = 0;
}

// Převod úrovně logu z řetězce
LogLevel Logger::levelFromString(const String& levelStr) {
    if (levelStr.equalsIgnoreCase("ERROR")) return LogLevel::ERROR;
    if (levelStr.equalsIgnoreCase("WARNING")) return LogLevel::WARNING;
    if (levelStr.equalsIgnoreCase("INFO")) return LogLevel::INFO;
    if (levelStr.equalsIgnoreCase("DEBUG")) return LogLevel::DEBUG;
    if (levelStr.equalsIgnoreCase("VERBOSE")) return LogLevel::VERBOSE;
    
    return LogLevel::INFO;
}

// Převod úrovně logu na řetězec
String Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::INFO: return "INFO";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::VERBOSE: return "VERBOSE";
        default: return "UNKNOWN";
    }
}

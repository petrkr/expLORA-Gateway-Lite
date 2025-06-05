
/**
 * expLORA Gateway Lite
 *
 * LoRa module manager implementation file
 *
 * Copyright Pajenicko s.r.o., Igor Sverma (C) 2025
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>

#include "Network_Manager.h"
 
 // Constructor
NetworkManager::NetworkManager(Logger &log)
    : logger(log)
{
 
}

// Destructor
NetworkManager::~NetworkManager()
{

}

// Initialize LoRa module
bool NetworkManager::init()
{
    logger.info("Initializing Networking...");
    return true;
}

// WiFi methods
String NetworkManager::getWiFiSSID() const
{
    return WiFi.SSID();
}

String NetworkManager::getWiFiAPSSID() const
{
    return WiFi.softAPSSID();
}

IPAddress NetworkManager::getWiFiIP() const
{
    return WiFi.localIP();
}

IPAddress NetworkManager::getWiFiAPIP() const
{
    return WiFi.softAPIP();
}

wifi_mode_t NetworkManager::getWiFiMode() const
{
    return WiFi.getMode();
}

uint8_t* NetworkManager::getWiFimacAddress(uint8_t* mac)
{
    return WiFi.macAddress(mac);
}

// Check if network is available
bool NetworkManager::isConnected() const
{
    logger.debug("Checking if any network is connected...");
    return isWiFiConnected() || isEthernetConnected() || isModemConnected();
}

// Check if WiFi is connected
bool NetworkManager::isWiFiConnected() const
{
    logger.debug("Checking WiFi connection...");

    return WiFi.status() == WL_CONNECTED;
}

// Check if Ethernet is connected
bool NetworkManager::isEthernetConnected() const
{
    logger.debug("Checking Ethernet connection...");
    // Implement Ethernet connection check logic here
    return false;
}

// Check if Modem is connected
bool NetworkManager::isModemConnected() const
{
    logger.debug("Checking Modem connection...");
    // Implement Modem connection check logic here
    return false;
}
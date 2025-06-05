
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
#include <WiFi.h>

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


// Check if network is available
bool NetworkManager::isConnected()
{
    logger.debug("Checking if any network is connected...");
    return isWiFiConnected() || isEthernetConnected() || isModemConnected();
}

// Check if WiFi is connected
bool NetworkManager::isWiFiConnected()
{
    logger.debug("Checking WiFi connection...");

    return WiFi.status() == WL_CONNECTED;
}

// Check if Ethernet is connected
bool NetworkManager::isEthernetConnected()
{
    logger.debug("Checking Ethernet connection...");
    // Implement Ethernet connection check logic here
    return false;
}

// Check if Modem is connected
bool NetworkManager::isModemConnected()
{
    logger.debug("Checking Modem connection...");
    // Implement Modem connection check logic here
    return false;
}
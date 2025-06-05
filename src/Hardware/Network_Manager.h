
/**
 * expLORA Gateway Lite
 *
 * LoRa module manager header file
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

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "../config.h"
#include "../Data/Logging.h"
#include "SPI_Manager.h"
#include "board_config.h"

/**
 * Class for managing RFM95W LoRa module
 *
 * Handles initialization, configuration, and communication with RFM95W LoRa module.
 * Abstracts SPI communication and interrupt handling.
 */
class NetworkManager
{
private:
    Logger &logger; // Reference to logger

public:
    // Constructor
    NetworkManager(Logger &log);

    // Destructor
    ~NetworkManager();

    // Initialize network module
    bool init();

    // Check if any network is available
    bool isConnected() const;

    // Check if specific network is connected
    bool isWiFiConnected() const;
    bool isEthernetConnected() const;
    bool isModemConnected() const;
};

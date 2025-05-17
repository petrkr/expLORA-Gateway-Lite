#include "HTMLGenerator.h"
#include <esp_heap_caps.h>
#include <algorithm>
#include <Arduino.h>
#include <WiFi.h>
#include "../config.h"

// Inicializace statických proměnných
char *HTMLGenerator::htmlBuffer = nullptr;
size_t HTMLGenerator::htmlBufferSize = WEB_BUFFER_SIZE;
bool HTMLGenerator::usePSRAM = false;

// Inicializace generátoru
// Inicializace generátoru
bool HTMLGenerator::init(bool usePsram, size_t bufferSize)
{
    // Pokud už je buffer inicializován, nejprve ho uvolníme
    if (htmlBuffer != nullptr)
    {
        deinit();
    }

    htmlBufferSize = bufferSize;

#ifdef BOARD_HAS_PSRAM
    usePSRAM = true;
    htmlBuffer = (char *)heap_caps_malloc(htmlBufferSize, MALLOC_CAP_SPIRAM);

    if (htmlBuffer != nullptr)
    {
        Serial.printf("HTMLGenerator: Allocated %u bytes in PSRAM for HTML content\n", htmlBufferSize);
    }
    else
    {
        // Fallback na běžnou RAM
        usePSRAM = false;
        htmlBuffer = new char[htmlBufferSize];
        Serial.printf("HTMLGenerator: Failed to allocate PSRAM, using %u bytes in RAM\n", htmlBufferSize);
    }
#else
    usePSRAM = false;
    htmlBuffer = new char[htmlBufferSize];
    Serial.printf("HTMLGenerator: Using %u bytes in RAM for HTML content\n", htmlBufferSize);
#endif

    // Kontrola, zda se povedla alokace paměti
    if (htmlBuffer == nullptr)
    {
        Serial.println("HTMLGenerator: Failed to allocate memory for HTML buffer");
        return false;
    }

    return true;
}

// Uvolnění zdrojů
void HTMLGenerator::deinit()
{
    if (htmlBuffer != nullptr)
    {
#ifdef BOARD_HAS_PSRAM
        if (usePSRAM)
        {
            heap_caps_free(htmlBuffer);
        }
        else
        {
#endif
            delete[] htmlBuffer;
#ifdef BOARD_HAS_PSRAM
        }
#endif
        htmlBuffer = nullptr;
    }
}

// Přidání HTML úvodního kódu
// Update HTMLGenerator::addHtmlHeader in HTMLGenerator.cpp
void HTMLGenerator::addHtmlHeader(String &html, const String &title, bool isAPMode = false)
{
    html += "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>" + title + " - expLORA Gateway Lite</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";

    // Add CSS styles
    addStyles(html);

    html += "</head><body>";
    html += "<header><h1>expLORA Gateway Lite</h1><h2>" + title + "</h2></header>";

    // Add navigation only if not in AP mode
    if (!isAPMode)
    {
        addNavigation(html, title);
    }
    else
    {
        // In AP mode, just add the container div without navigation
        html += "<div class='container'>";
    }
}

// Přidání HTML koncového kódu
void HTMLGenerator::addHtmlFooter(String &html)
{
    html += "<footer>";
    html += "<p>expLORA Gateway Lite v" + String(FIRMWARE_VERSION) + " &copy; 2025</p>";
    html += "</footer>";

    // Přidání JavaScriptu
    addJavaScript(html);

    html += "</body></html>";
}

// Přidání CSS pro stránky
void HTMLGenerator::addStyles(String &html)
{
    html += "<style>";
    html += "* { box-sizing: border-box; }";
    html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; line-height: 1.6; }";
    html += "header { background: #0066cc; color: white; padding: 20px; text-align: center; }";
    html += "header h1 { margin: 0; }";
    html += "header h2 { margin: 5px 0 0 0; font-weight: normal; }";

    // Navigace
    html += "nav { background: #333; overflow: hidden; }";
    html += "nav a { float: left; display: block; color: white; text-align: center; padding: 14px 16px; text-decoration: none; }";
    html += "nav a:hover { background: #0066cc; }";
    html += "nav a.active { background: #0066cc; }";
    html += "nav .icon { display: none; }";

    // Karty
    html += ".container { padding: 20px; }";
    html += ".card { background: white; border-radius: 5px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";

    // Tabulky
    html += "table { width: 100%; border-collapse: collapse; margin-bottom: 20px; }";
    html += "th, td { text-align: left; padding: 12px; border-bottom: 1px solid #ddd; }";
    html += "th { background-color: #f2f2f2; }";
    html += "tr:hover { background-color: #f5f5f5; }";

    // Formuláře
    html += "form { margin-top: 20px; }";
    html += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
    html += "input[type='text'], input[type='password'], input[type='number'], select, textarea { ";
    html += "  width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; }";
    html += "input[type='submit'], button, .btn { ";
    html += "  background: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; ";
    html += "  text-decoration: none; display: inline-block; font-size: 14px; margin-right: 10px; }";
    html += "input[type='submit']:hover, button:hover, .btn:hover { background: #0055aa; }";
    html += ".btn-delete { background: #cc0000; }";
    html += ".btn-delete:hover { background: #aa0000; }";

    // Responzivní design
    html += "@media screen and (max-width: 600px) {";
    html += "  nav a:not(:first-child) { display: none; }";
    html += "  nav a.icon { float: right; display: block; }";
    html += "  nav.responsive { position: relative; }";
    html += "  nav.responsive a.icon { position: absolute; right: 0; top: 0; }";
    html += "  nav.responsive a { float: none; display: block; text-align: left; }";
    html += "}";

    // Speciální styly pro logy
    html += ".log-container { background: #f8f8f8; padding: 10px; border-radius: 4px; max-height: 70vh; overflow-y: auto; }";
    html += ".log-entry { padding: 5px; border-bottom: 1px solid #ddd; font-family: monospace; white-space: pre-wrap; }";
    html += ".log-error { color: #ff5555; }";
    html += ".log-warning { color: #ffaa00; }";
    html += ".log-info { color: #2196F3; }";
    html += ".log-debug { color: #4CAF50; }";
    html += ".log-verbose { color: #9E9E9E; }";

    // Patička
    html += "footer { background: #f2f2f2; padding: 10px; text-align: center; font-size: 12px; color: #666; }";

    html += "</style>";
}

// Přidání JavaScript pro interaktivní prvky
void HTMLGenerator::addJavaScript(String &html)
{
    html += "<script>";

    // Responzivní menu
    html += "function toggleMenu() {";
    html += "  var x = document.getElementsByTagName('nav')[0];";
    html += "  if (x.className === '') {";
    html += "    x.className = 'responsive';";
    html += "  } else {";
    html += "    x.className = '';";
    html += "  }";
    html += "}";

    // Automatické obnovení stránky
    html += "function startAutoRefresh(interval) {";
    html += "  setTimeout(function(){ location.reload(); }, interval);";
    html += "}";

    // WebSocket pro real-time aktualizace
    // Implementace bude přidána později

    html += "</script>";
}

// Přidání navigace
void HTMLGenerator::addNavigation(String &html, const String &activePage)
{
    html += "<nav>";
    html += "<a href='/' class='" + String(activePage == "Home" ? "active" : "") + "'>Home</a>";
    html += "<a href='/config' class='" + String(activePage == "Configuration" ? "active" : "") + "'>WiFi Setup</a>";
    html += "<a href='/sensors' class='" + String(activePage == "Sensors" ? "active" : "") + "'>Sensors</a>";
    html += "<a href='/mqtt' class='" + String(activePage == "MQTT" ? "active" : "") + "'>MQTT</a>";
    html += "<a href='/logs' class='" + String(activePage == "Logs" ? "active" : "") + "'>Logs</a>";
    html += "<a href='/api' class='" + String(activePage == "API" ? "active" : "") + "'>API</a>";
    html += "<a href='/reboot' onclick=\"return confirm('Are you sure you want to reboot the device?');\">Reboot</a>";
    html += "<a href='javascript:void(0);' class='icon' onclick='toggleMenu()'>&#9776;</a>";
    html += "</nav>";

    html += "<div class='container'>";
}

// In HTMLGenerator.cpp, modify the generateHomePage method
String HTMLGenerator::generateHomePage(const std::vector<SensorData> &sensors)
{
    String html;

    // Add header with reduced content
    addHtmlHeader(html, "Home");

    // Status card
    html += "<div class='card'>";
    html += "<h2>System Status</h2>";

    html += "<p><strong>Mode:</strong> " + String(WiFi.status() == WL_CONNECTED ? "Client" : "Access Point") + "</p>";

    if (WiFi.status() == WL_CONNECTED)
    {
        html += "<p><strong>WiFi:</strong> Connected to " + WiFi.SSID() + "</p>";
        html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
    }
    else
    {
        html += "<p><strong>WiFi:</strong> Disconnected</p>";
        if (WiFi.getMode() == WIFI_AP)
        {
            html += "<p><strong>AP IP:</strong> " + WiFi.softAPIP().toString() + "</p>";
        }
    }

    // Aktuální čas
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        html += "<p><strong>Time:</strong> " + String(timeStr) + "</p>";
    }
    else
    {
        html += "<p><strong>Time:</strong> Not set</p>";
    }

    unsigned long seconds = millis() / 1000;
    unsigned int days    = seconds / 86400;           // 24*60*60
    unsigned int hours   = (seconds % 86400) / 3600;  // rest of the day
    unsigned int minutes = (seconds % 3600) / 60;     // rest of the hour
    unsigned int secs    = seconds % 60;
    
    html  += "<p><strong>Uptime:</strong> ";
    if (days)               html += String(days)    + " d ";
    if (days || hours)      html += String(hours)   + " h ";
    if (days || hours || minutes)
                           html += String(minutes) + " m ";
    html += String(secs)    + " s</p>";

    html += "</div>";

    // Active sensors - optimize by checking vector size first
    if (!sensors.empty())
    {
        html += "<div class='card'>";
        html += "<h2>Active Sensors</h2>";

        // Generate table with minimal content
        html += "<table>";
        html += "<tr><th>Name</th><th>Type</th><th>Last Seen</th><th>Data</th></tr>";

        for (const auto &sensor : sensors)
        {
            if (sensor.configured)
            {
                html += "<tr>";
                html += "<td>" + sensor.name + "</td>";
                html += "<td>" + sensorTypeToString(sensor.deviceType) + "</td>";
                html += "<td>" + sensor.getLastSeenString() + "</td>";

                // Add sensor data - use the getDataString() method
                html += "<td>" + sensor.getDataString() + "</td>";

                html += "</tr>";

                // Yield to other tasks periodically during table generation
                if (&sensor != &sensors.back())
                {
                    yield();
                }
            }
        }

        html += "</table>";
        html += "</div>";
    }

    // Auto-refresh with longer interval in AP mode
    html += "<script>startAutoRefresh(60000);</script>"; // 60 seconds instead of 30

    // Add footer
    addHtmlFooter(html);

    return html;
}

// Generování tabulky senzorů (optimalizovaná verze pro buffer)
void HTMLGenerator::generateSensorTable(char *buffer, size_t &maxLen, const std::vector<SensorData> &sensors)
{
    size_t contentLen = 0;

    // Začátek tabulky
    contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                           "<table>"
                           "<tr><th>Name</th><th>Type</th><th>Serial Number</th><th>Last Seen</th><th>Sensor Data</th></tr>");

    // Průchod všemi senzory
    for (const auto &sensor : sensors)
    {
        if (sensor.configured)
        {
            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<tr>"
                                   "<td>%s</td>"
                                   "<td>%s</td>"
                                   "<td>%X</td>",
                                   sensor.name.c_str(),
                                   sensor.getTypeInfo().name,
                                   sensor.serialNumber);

            // Last Seen
            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<td>%s</td>",
                                   sensor.getLastSeenString().c_str());

            // Sensor Data
            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<td>%s</td>"
                                   "</tr>",
                                   sensor.getDataString().c_str());
        }
    }

    // Konec tabulky
    contentLen += snprintf(buffer + contentLen, maxLen - contentLen, "</table>");
}

// Generování stránky konfigurace
// Modify HTMLGenerator::generateConfigPage to use more reliable AJAX for WiFi scanning
// Update HTMLGenerator::generateConfigPage in HTMLGenerator.cpp
String HTMLGenerator::generateConfigPage(const String &ssid, const String &password, bool configMode, const String &ip, const String &timezone)
{
    String html;

    // Add header - pass configMode to suppress navigation in AP mode
    addHtmlHeader(html, "Configuration", configMode);

    // Current status
    html += "<div class='card'>";
    html += "<h2>Current Status</h2>";
    if (configMode)
    {
        html += "<p><strong>Mode:</strong> Access Point</p>";
        html += "<p><strong>AP Name:</strong> " + WiFi.softAPSSID() + "</p>";
        html += "<p><strong>AP IP:</strong> " + WiFi.softAPIP().toString() + "</p>";
    }
    else
    {
        html += "<p><strong>Mode:</strong> Client</p>";
        html += "<p><strong>SSID:</strong> " + ssid + "</p>";
        html += "<p><strong>Status:</strong> " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "</p>";
        if (WiFi.status() == WL_CONNECTED)
        {
            html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
        }
    }
    html += "</div>";

    // WiFi configuration form
    html += "<div class='card'>";
    html += "<h2>WiFi Settings</h2>";
    html += "<form method='post' action='/config'>";
    html += "<label for='ssid'>SSID:</label>";
    html += "<input type='text' id='ssid' name='ssid' value='" + ssid + "' required>";
    html += "<label for='password'>Password:</label>";
    html += "<input type='password' id='password' name='password' value='" + password + "'>";
    html += "<label for='timezone'>Timezone:</label>";
    html += "<select id='timezone' name='timezone'>";

    html += "<option value='GMT0'";
    if (timezone == "GMT0") html += " selected";
    html += ">GMT/UTC (00:00)</option>";
     
    html += "<option value='WET0WEST,M3.5.0/1,M10.5.0'";
    if (timezone == "WET0WEST,M3.5.0/1,M10.5.0") html += " selected";
    html += ">Western European (WET/WEST)</option>";
     
    html += "<option value='CET-1CEST,M3.5.0,M10.5.0/3'";
    if (timezone == "CET-1CEST,M3.5.0,M10.5.0/3") html += " selected";
    html += ">Central European (CET/CEST)</option>";
     
    html += "<option value='EET-2EEST,M3.5.0/3,M10.5.0/4'";
    if (timezone == "EET-2EEST,M3.5.0/3,M10.5.0/4") html += " selected";
    html += ">Eastern European (EET/EEST)</option>";
     
    html += "<option value='MSK-3'";
    if (timezone == "MSK-3") html += " selected";
    html += ">Moscow Time (MSK)</option>";
     
    html += "<input type='submit' value='Save and Restart'>";
    html += "</form>";
    html += "</div>";

    // Add footer
    addHtmlFooter(html);

    return html;
}

// Generate MQTT Configuration Page
String HTMLGenerator::generateMqttPage(const String& host, int port, const String& user, const String& password, bool enabled) {
    String html;

    // Add header
    addHtmlHeader(html, "MQTT Configuration");

    // MQTT configuration form
    html += "<div class='card'>";
    html += "<h2>MQTT Settings</h2>";
    html += "<p>Configure connection to Home Assistant MQTT broker for automatic sensor discovery.</p>";
    html += "<form method='post' action='/mqtt'>";
    
    // Enable MQTT checkbox
    html += "<div class='form-group'>";
    html += "<label for='enabled'>Enable MQTT:</label>";
    html += "<input type='checkbox' id='enabled' name='enabled' value='1'" + String(enabled ? " checked" : "") + ">";
    html += "</div>";
    
    // MQTT Broker Host
    html += "<div class='form-group'>";
    html += "<label for='host'>MQTT Broker Host:</label>";
    html += "<input type='text' id='host' name='host' value='" + host + "' required>";
    html += "</div>";
    
    // MQTT Port
    html += "<div class='form-group'>";
    html += "<label for='port'>MQTT Port:</label>";
    html += "<input type='number' id='port' name='port' value='" + String(port) + "' required min='1' max='65535'>";
    html += "</div>";
    
    // Username
    html += "<div class='form-group'>";
    html += "<label for='user'>Username (optional):</label>";
    html += "<input type='text' id='user' name='user' value='" + user + "'>";
    html += "</div>";
    
    // Password
    html += "<div class='form-group'>";
    html += "<label for='password'>Password (optional):</label>";
    html += "<input type='password' id='password' name='password' value='" + password + "'>";
    html += "</div>";
    
    html += "<input type='submit' value='Save MQTT Settings'>";
    html += "</form></div>";

    // Add footer
    addHtmlFooter(html);
    return html;
}

// Generování stránky se seznamem senzorů
String HTMLGenerator::generateSensorsPage(const std::vector<SensorData> &sensors)
{
    String html;
    // Přidání hlavičky
    addHtmlHeader(html, "Sensors");

    // Seznam senzorů
    html += "<div class='card'>";
    html += "<h2>Configured Sensors</h2>";

    if (sensors.empty())
    {
        html += "<p>No sensors configured yet.</p>";
    }
    else
    {
        html += "<table>";
        html += "<tr><th>Name</th><th>Type</th><th>Serial Number</th><th>Last Seen</th><th>Actions</th></tr>";

        for (size_t i = 0; i < sensors.size(); i++)
        {
            const auto &sensor = sensors[i];
            if (sensor.configured)
            {
                html += "<tr>";
                html += "<td>" + sensor.name + "</td>";
                html += "<td>" + sensorTypeToString(sensor.deviceType) + "</td>";
                html += "<td>" + String(sensor.serialNumber, HEX) + "</td>";
                html += "<td>" + sensor.getLastSeenString() + "</td>";
                html += "<td>";
                html += "<a href='/sensors/edit?index=" + String(i) + "' class='btn'>Edit</a> ";
                html += "<a href='/sensors/delete?index=" + String(i) + "' class='btn btn-delete' onclick='return confirm(\"Are you sure you want to delete this sensor?\")'>Delete</a>";
                html += "</td>";
                html += "</tr>";
            }
        }

        html += "</table>";
    }

    html += "<p><a href='/sensors/add' class='btn'>Add New Sensor</a></p>";
    html += "</div>";

    // Přidání patičky
    addHtmlFooter(html);

    return html;
}

// Generování stránky pro přidání senzoru
String HTMLGenerator::generateSensorAddPage()
{
    String html;

    // Přidání hlavičky
    addHtmlHeader(html, "Add Sensor");

    // Formulář pro přidání senzoru
    html += "<div class='card'>";
    html += "<h2>Add New Sensor</h2>";
    html += "<form method='post' action='/sensors/add'>";

    // Jméno senzoru
    html += "<label for='name'>Sensor Name:</label>";
    html += "<input type='text' id='name' name='name' required>";

    // Typ senzoru
    html += "<label for='deviceType'>Device Type:</label>";
    html += "<select id='deviceType' name='deviceType'>";

    // Dynamicky generujeme typy senzorů z definice
    for (const auto &type : SENSOR_TYPE_DEFINITIONS)
    {
        if (type.type != SensorType::UNKNOWN)
        {
            html += "<option value='" + String(static_cast<uint8_t>(type.type)) + "'>" +
                    String(type.name);

            // Přidání informace o schopnostech senzoru
            html += " - ";
            bool first = true;

            if (type.hasTemperature)
            {
                html += "Temperature";
                first = false;
            }

            if (type.hasHumidity)
            {
                html += (first ? "" : ", ") + String("Humidity");
                first = false;
            }

            if (type.hasPressure)
            {
                html += (first ? "" : ", ") + String("Pressure");
                first = false;
            }

            if (type.hasPPM)
            {
                html += (first ? "" : ", ") + String("CO2");
                first = false;
            }

            if (type.hasLux)
            {
                html += (first ? "" : ", ") + String("Light");
            }


            if (type.hasRainAmount)
            {
                html += (first ? "" : ", ") + String("Rain");
            }
            if (type.hasWindSpeed)
            {
                html += (first ? "" : ", ") + String("Wind");
            }

            html += "</option>";
        }
    }

    html += "</select>";

    // Sériové číslo
    html += "<label for='serialNumber'>Serial Number:</label>";
    html += "<input type='text' id='serialNumber' name='serialNumber' placeholder='e.g. 1234567A' required>";

    // Klíč zařízení
    html += "<label for='deviceKey'>Device Key:</label>";
    html += "<input type='text' id='deviceKey' name='deviceKey' placeholder='e.g. DEADBEEF' required>";

    // New code with a single field
    html += "<label for='customUrl'>Custom URL with Placeholders (Optional):</label>";
    html += "<div id='urlHelp' style='margin-bottom: 10px; font-size: 0.9em;'>";
    html += "<p>Available placeholders:</p>";
    html += "<style>";
    html += ".placeholder-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 15px; }";
    html += ".placeholder-item { background: #f5f5f5; padding: 8px; border-radius: 4px; }";
    html += ".placeholder-item code { background: #e0e0e0; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #0066cc; }";
    html += "</style>";
    html += "<div class='placeholder-grid' style='display: grid; grid-template-columns: repeat(3, 1fr); gap: 5px;'>";
    // Environment placeholders
    html += "<div id='tempPlaceholder' class='placeholder-item'>Temperature <code>*TEMP*</code></div>";
    html += "<div id='humPlaceholder' class='placeholder-item'>Humidity <code>*HUM*</code></div>";
    html += "<div id='pressPlaceholder' class='placeholder-item'>Pressure <code>*PRESS*</code></div>";
    html += "<div id='ppmPlaceholder' class='placeholder-item'>CO2 <code>*PPM*</code></div>";
    html += "<div id='luxPlaceholder' class='placeholder-item'>Light <code>*LUX*</code></div>";
    // Weather specific
    html += "<div id='windSpeedPlaceholder' class='placeholder-item'>Wind Speed <code>*WIND_SPEED*</code></div>";
    html += "<div id='windDirPlaceholder' class='placeholder-item'>Wind Direction <code>*WIND_DIR*</code></div>";
    html += "<div id='rainPlaceholder' class='placeholder-item'>Rain Amount <code>*RAIN*</code></div>";
    html += "<div id='dailyRainPlaceholder' class='placeholder-item'>Rain since midnight <code>*DAILY_RAIN*</code></div>";
    html += "<div id='rainRatePlaceholder' class='placeholder-item'>Rain Rate <code>*RAIN_RATE*</code></div>";
    // Device info
    html += "<div class='placeholder-item'>Battery <code>*BAT*</code></div>";
    html += "<div class='placeholder-item'>Signal <code>*RSSI*</code></div>";
    html += "<div class='placeholder-item'>Serial Number <code>*SN*</code></div>";
    html += "<div class='placeholder-item'>Device Type <code>*TYPE*</code></div>";
    html += "</div>"; // end of grid
    html += "</div>"; // end of urlHelp
    html += "<input type='text' id='customUrl' name='customUrl' placeholder='https://example.com/api?temp=*TEMP*&hum=*HUM*'>";

    // Altitude field pro BME280 (zobrazí se jen když je vybraný BME280)
    html += "<div id='altitudeDiv' style='display: none;'>";
    html += "<label for='altitude'>Altitude (m) - Optional for pressure adjustment:</label>";
    html += "<input type='number' id='altitude' name='altitude' placeholder='e.g. 320' min='0' max='8848'>";
    html += "<p style='font-size: 0.9em; color: #666;'>If provided, relative pressure will be converted to absolute pressure</p>";
    html += "</div>";

    // JavaScript pro aktualizaci placeholderů podle typu senzoru
    html += "<script>";
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  var deviceTypeSelect = document.getElementById('deviceType');";
    html += "  var altitudeDiv = document.getElementById('altitudeDiv');";
    html += "  function updatePlaceholderVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    console.log('Selected device type:', type);"; // Debugging

    // Definice typů zařízení pro jednotlivé placeholdery
    html += "    var tempHumDevices = [1, 2, 3];";
    html += "    var pressureDevices = [1, 3];";
    html += "    var co2Devices = [2];";
    html += "    var luxDevices = [4];";
    html += "    var weatherDevices = [3];";

    // Aktualizace zobrazení
    html += "    document.getElementById('tempPlaceholder').style.display = tempHumDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('humPlaceholder').style.display = tempHumDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('pressPlaceholder').style.display = pressureDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('ppmPlaceholder').style.display = co2Devices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('luxPlaceholder').style.display = luxDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windSpeedPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windDirPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('dailyRainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainRatePlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "  }";

    html += "  function updateFieldVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    altitudeDiv.style.display = (type === 1) ? 'block' : 'none';";
    html += "    updatePlaceholderVisibility();"; 
    html += "  }";

    // Spustíme funkci hned po načtení stránky
    html += "  updateFieldVisibility();";  // ZMĚNA: volat updateFieldVisibility místo updatePlaceholderVisibility

    // A také při změně výběru
    html += "  deviceTypeSelect.addEventListener('change', updateFieldVisibility);"; // ZMĚNA: volat updateFieldVisibility
    html += "});";
    html += "</script>";

    // Tlačítka
    html += "<input type='submit' value='Add Sensor'>";
    html += "<a href='/sensors' class='btn' style='background: #999;'>Cancel</a>";
    html += "</form>";
    html += "</div>";

    // Přidání patičky
    addHtmlFooter(html);

    return html;
}

// Generování stránky pro úpravu senzoru
String HTMLGenerator::generateSensorEditPage(const SensorData &sensor, int index)
{
    String html;

    // Přidání hlavičky
    addHtmlHeader(html, "Edit Sensor");

    // Formulář pro úpravu senzoru
    html += "<div class='card'>";
    html += "<h2>Edit Sensor</h2>";
    html += "<form method='post' action='/sensors/update'>";

    // Skrytý index
    html += "<input type='hidden' name='index' value='" + String(index) + "'>";

    // Jméno senzoru
    html += "<label for='name'>Sensor Name:</label>";
    html += "<input type='text' id='name' name='name' value='" + sensor.name + "' required>";

    // Typ senzoru
    html += "<label for='deviceType'>Device Type:</label>";
    html += "<select id='deviceType' name='deviceType'>";

    // Dynamicky generujeme typy senzorů z definice
    for (const auto &type : SENSOR_TYPE_DEFINITIONS)
    {
        if (type.type != SensorType::UNKNOWN)
        {
            bool isSelected = (type.type == sensor.deviceType);
            html += "<option value='" + String(static_cast<uint8_t>(type.type)) + "'";
            if (isSelected) {
                html += " selected";
            }
            html += ">" + String(type.name);

            // Přidání informace o schopnostech senzoru
            html += " - ";
            bool first = true;

            if (type.hasTemperature)
            {
                html += "Temperature";
                first = false;
            }

            if (type.hasHumidity)
            {
                html += (first ? "" : ", ") + String("Humidity");
                first = false;
            }

            if (type.hasPressure)
            {
                html += (first ? "" : ", ") + String("Pressure");
                first = false;
            }

            if (type.hasPPM)
            {
                html += (first ? "" : ", ") + String("CO2");
                first = false;
            }

            if (type.hasLux)
            {
                html += (first ? "" : ", ") + String("Light");
            }

            if (type.hasRainAmount)
            {
                html += (first ? "" : ", ") + String("Rain");
            }
            if (type.hasWindSpeed)
            {
                html += (first ? "" : ", ") + String("Wind");
            }

            html += "</option>";
        }
    }

    html += "</select>";

    // Sériové číslo
    html += "<label for='serialNumber'>Serial Number:</label>";
    html += "<input type='text' id='serialNumber' name='serialNumber' value='" + String(sensor.serialNumber, HEX) + "' required>";

    // Klíč zařízení
    html += "<label for='deviceKey'>Device Key:</label>";
    html += "<input type='text' id='deviceKey' name='deviceKey' value='" + String(sensor.deviceKey, HEX) + "' required>";

    // New code with a single field
    html += "<label for='customUrl'>Custom URL with Placeholders (Optional):</label>";
    html += "<div id='urlHelp' style='margin-bottom: 10px; font-size: 0.9em;'>";
    html += "<p>Available placeholders:</p>";

    html += "<style>";
    html += ".placeholder-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 15px; }";
    html += ".placeholder-item { background: #f5f5f5; padding: 8px; border-radius: 4px; }";
    html += ".placeholder-item code { background: #e0e0e0; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #0066cc; }";
    html += "</style>";
    html += "<div class='placeholder-grid' style='display: grid; grid-template-columns: repeat(3, 1fr); gap: 5px;'>";
    // Environment placeholders
    html += "<div id='tempPlaceholder' class='placeholder-item'>Temperature <code>*TEMP*</code></div>";
    html += "<div id='humPlaceholder' class='placeholder-item'>Humidity <code>*HUM*</code></div>";
    html += "<div id='pressPlaceholder' class='placeholder-item'>Pressure <code>*PRESS*</code></div>";
    html += "<div id='ppmPlaceholder' class='placeholder-item'>CO2 <code>*PPM*</code></div>";
    html += "<div id='luxPlaceholder' class='placeholder-item'>Light <code>*LUX*</code></div>";
    // Weather specific
    html += "<div id='windSpeedPlaceholder' class='placeholder-item'>Wind Speed <code>*WIND_SPEED*</code></div>";
    html += "<div id='windDirPlaceholder' class='placeholder-item'>Wind Direction <code>*WIND_DIR*</code></div>";
    html += "<div id='rainPlaceholder' class='placeholder-item'>Rain Amount <code>*RAIN*</code></div>";
    html += "<div id='dailyRainPlaceholder' class='placeholder-item'>Rain since midnight <code>*DAILY_RAIN*</code></div>";
    html += "<div id='rainRatePlaceholder' class='placeholder-item'>Rain Rate <code>*RAIN_RATE*</code></div>";
    // Device info
    html += "<div class='placeholder-item'>Battery <code>*BAT*</code></div>";
    html += "<div class='placeholder-item'>Signal <code>*RSSI*</code></div>";
    html += "<div class='placeholder-item'>Serial Number <code>*SN*</code></div>";
    html += "<div class='placeholder-item'>Device Type <code>*TYPE*</code></div>";
    html += "</div>"; // end of grid
    html += "</div>"; // end of urlHelp
    html += "<input type='text' id='customUrl' name='customUrl' placeholder='https://example.com/api?temp=*TEMP*&hum=*HUM*' value='" + sensor.customUrl + "'>";

    html += "<div id='altitudeDiv' style='display: " + 
            String(sensor.deviceType == SensorType::BME280 ? "block" : "none") + ";'>";
    html += "<label for='altitude'>Altitude (m) - Optional for pressure adjustment:</label>";
    html += "<input type='number' id='altitude' name='altitude' value='" + 
        String(sensor.altitude) + "' min='0' max='8848'>";
    html += "<p style='font-size: 0.9em; color: #666;'>If provided, relative pressure will be converted to absolute pressure</p>";
    html += "</div>";

    // JavaScript pro aktualizaci placeholderů podle typu senzoru
    // JavaScript pro aktualizaci placeholderů podle typu senzoru
    html += "<script>";
    html += "document.addEventListener('DOMContentLoaded', function() {";
    html += "  var deviceTypeSelect = document.getElementById('deviceType');";
    html += "  var altitudeDiv = document.getElementById('altitudeDiv');";
    html += "  function updatePlaceholderVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    console.log('Selected device type:', type);"; // Debugging

    // Definice typů zařízení pro jednotlivé placeholdery
    html += "    var tempHumDevices = [1, 2, 3];";
    html += "    var pressureDevices = [1, 3];";
    html += "    var co2Devices = [2];";
    html += "    var luxDevices = [4];";
    html += "    var weatherDevices = [3];";

    // Aktualizace zobrazení
    html += "    document.getElementById('tempPlaceholder').style.display = tempHumDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('humPlaceholder').style.display = tempHumDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('pressPlaceholder').style.display = pressureDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('ppmPlaceholder').style.display = co2Devices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('luxPlaceholder').style.display = luxDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windSpeedPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('windDirPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('dailyRainPlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "    document.getElementById('rainRatePlaceholder').style.display = weatherDevices.includes(type) ? 'block' : 'none';";
    html += "  }";

    html += "  function updateFieldVisibility() {";
    html += "    var type = parseInt(deviceTypeSelect.value);";
    html += "    altitudeDiv.style.display = (type === 1) ? 'block' : 'none';";
    html += "    updatePlaceholderVisibility();"; 
    html += "  }";

    // Nastavit správně vybranou hodnotu pro typ senzoru
    html += "  deviceTypeSelect.value = '" + String(static_cast<uint8_t>(sensor.deviceType)) + "';";

    // Spustíme funkci hned po načtení stránky
    html += "  updateFieldVisibility();";  // ZMĚNA: volat updateFieldVisibility místo updatePlaceholderVisibility

    // A také při změně výběru
    html += "  deviceTypeSelect.addEventListener('change', updateFieldVisibility);"; // ZMĚNA: volat updateFieldVisibility
    html += "});";
    html += "</script>";

    // Tlačítka
    html += "<input type='submit' value='Update Sensor'>";
    html += "<a href='/sensors' class='btn' style='background: #999;'>Cancel</a>";
    html += "</form>";
    html += "</div>";

    // Přidání patičky
    addHtmlFooter(html);

    return html;
}

// Generování stránky logů
String HTMLGenerator::generateLogsPage(const LogEntry *logs, size_t logCount, LogLevel currentLevel)
{
    String html;

    // Přidání hlavičky
    addHtmlHeader(html, "Logs");

    // Ovládací panel logů
    html += "<div class='card'>";
    html += "<h2>Log Settings</h2>";

    // Log level selector
    html += "<form method='post' action='/logs/level'>";
    html += "<label for='level'>Log Level:</label>";
    html += "<select id='level' name='level'>";
    html += "<option value='ERROR'" + String(currentLevel == LogLevel::ERROR ? " selected" : "") + ">ERROR</option>";
    html += "<option value='WARNING'" + String(currentLevel == LogLevel::WARNING ? " selected" : "") + ">WARNING</option>";
    html += "<option value='INFO'" + String(currentLevel == LogLevel::INFO ? " selected" : "") + ">INFO</option>";
    html += "<option value='DEBUG'" + String(currentLevel == LogLevel::DEBUG ? " selected" : "") + ">DEBUG</option>";
    html += "<option value='VERBOSE'" + String(currentLevel == LogLevel::VERBOSE ? " selected" : "") + ">VERBOSE</option>";
    html += "</select>";
    html += "<input type='submit' value='Set Level'>";
    html += "</form>";

    // Tlačítka pro práci s logy
    html += "<div style='margin-top: 20px;'>";
    html += "<a href='/logs' class='btn'>Refresh</a> ";
    html += "<a href='/logs/clear' class='btn btn-delete' onclick='return confirm(\"Are you sure you want to clear all logs?\")'>Clear Logs</a>";
    html += "</div>";
    html += "</div>";

    // Obsah logů
    html += "<div class='card'>";
    html += "<h2>System Logs</h2>";
    html += "<div class='log-container'>";

    // Použití optimalizované metody pro generování logů
    if (htmlBuffer != nullptr)
    {
        memset(htmlBuffer, 0, htmlBufferSize);
        size_t maxLen = htmlBufferSize;
        generateLogTable(htmlBuffer, maxLen, logs, logCount);
        html += htmlBuffer;
    }
    else
    {
        // Fallback pokud není k dispozici buffer
        if (logCount > 0)
        {
            for (size_t i = 0; i < logCount; i++)
            {
                size_t index = (logCount - 1 - i); // Zobrazení od nejnovějšího
                String logClass = "log-";

                switch (logs[index].level)
                {
                case LogLevel::ERROR:
                    logClass += "error";
                    break;
                case LogLevel::WARNING:
                    logClass += "warning";
                    break;
                case LogLevel::INFO:
                    logClass += "info";
                    break;
                case LogLevel::DEBUG:
                    logClass += "debug";
                    break;
                case LogLevel::VERBOSE:
                    logClass += "verbose";
                    break;
                default:
                    logClass += "info";
                    break;
                }

                html += "<div class='log-entry " + logClass + "'>" + logs[index].getFormattedLog() + "</div>";
            }
        }
        else
        {
            html += "<div class='log-entry'>No logs to display</div>";
        }
    }

    html += "</div>"; // log-container
    html += "</div>"; // card

    // Auto-refresh pro logy
    html += "<script>startAutoRefresh(30000);</script>";

    // Přidání patičky
    addHtmlFooter(html);

    return html;
}

// Generování tabulky logů
// In HTMLGenerator.cpp
void HTMLGenerator::generateLogTable(char *buffer, size_t &maxLen, const LogEntry *logs, size_t logCount)
{
    size_t contentLen = 0;

    if (logCount > 0)
    {
        // Loop through from newest to oldest
        for (size_t i = 0; i < logCount; i++)
        {
            // Calculate correct index in the circular buffer
            // This is critical to display logs correctly
            size_t logIdx = (Logger::getLogIndex() - 1 - i + Logger::getLogBufferSize()) % Logger::getLogBufferSize();

            const char *logClass = "";
            switch (logs[logIdx].level)
            {
            case LogLevel::ERROR:
                logClass = "log-error";
                break;
            case LogLevel::WARNING:
                logClass = "log-warning";
                break;
            case LogLevel::INFO:
                logClass = "log-info";
                break;
            case LogLevel::DEBUG:
                logClass = "log-debug";
                break;
            case LogLevel::VERBOSE:
                logClass = "log-verbose";
                break;
            default:
                logClass = "log-info";
                break;
            }

            contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                   "<div class='log-entry %s'>%s</div>",
                                   logClass,
                                   logs[logIdx].getFormattedLog().c_str());

            // Safety check to avoid buffer overflow
            if (contentLen >= maxLen - 100)
            {
                contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                                       "<div class='log-entry log-warning'>Log output truncated due to buffer size limitations</div>");
                break;
            }
        }
    }
    else
    {
        contentLen += snprintf(buffer + contentLen, maxLen - contentLen,
                               "<div class='log-entry'>No logs to display</div>");
    }
}

// Generování JSON pro API
String HTMLGenerator::generateAPIJson(const std::vector<SensorData> &sensors)
{
    // Odhad velikosti JSON dokumentu
    const size_t capacity = JSON_OBJECT_SIZE(5) + JSON_ARRAY_SIZE(sensors.size()) +
                            sensors.size() * JSON_OBJECT_SIZE(15) + 1024; // Extra prostor pro jistotu

    DynamicJsonDocument doc(capacity);

    // Základní informace
    doc["version"] = FIRMWARE_VERSION;

    // Aktuální čas
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        doc["time"] = timeStr;
    }
    else
    {
        doc["time"] = "Time not set";
    }

    doc["status"] = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";

    // Pole senzorů
    JsonArray sensorsArray = doc.createNestedArray("sensors");

    for (const auto &sensor : sensors)
    {
        if (sensor.configured)
        {
            JsonObject sensorObj = sensorsArray.createNestedObject();
            sensor.toJson(sensorObj);
        }
    }

    // Serializace do řetězce
    String result;
    serializeJson(doc, result);
    return result;
}

// Generování stránky API
String HTMLGenerator::generateAPIPage(const std::vector<SensorData> &sensors)
{
    String html;

    // Přidání hlavičky
    addHtmlHeader(html, "API");

    // API dokumentace
    html += "<div class='card'>";
    html += "<h2>API Documentation</h2>";
    html += "<p>This gateway provides a JSON API for accessing sensor data.</p>";

    html += "<h3>Endpoints</h3>";
    html += "<table>";
    html += "<tr><th>URL</th><th>Description</th></tr>";
    html += "<tr><td><code>/api?format=json</code></td><td>Returns all sensor data in JSON format</td></tr>";
    html += "<tr><td><code>/api?format=csv</code></td><td>Returns sensor data in CSV format</td></tr>";
    html += "<tr><td><code>/api?sensor=XXXX</code></td><td>Returns data for a specific sensor by serial number</td></tr>";
    html += "</table>";

    html += "<h3>Example JSON Response</h3>";
    html += "<pre style='background: #f5f5f5; padding: 10px; overflow-x: auto;'>";
    html += "{\n";
    html += "  \"version\": \"" + String(FIRMWARE_VERSION) + "\",\n";
    html += "  \"time\": \"2023-08-01 12:34:56\",\n";
    html += "  \"status\": \"connected\",\n";
    html += "  \"sensors\": [\n";

    // Generování příkladu dat senzoru
    if (!sensors.empty())
    {
        const auto &sensor = sensors[0];
        html += "    {\n";
        html += "      \"name\": \"" + sensor.name + "\",\n";
        html += "      \"type\": " + String(static_cast<uint8_t>(sensor.deviceType)) + ",\n";
        html += "      \"typeName\": \"" + String(sensor.getTypeInfo().name) + "\",\n";
        html += "      \"serialNumber\": \"" + String(sensor.serialNumber, HEX) + "\",\n";
        html += "      \"lastSeen\": " + (sensor.lastSeen > 0 ? String((millis() - sensor.lastSeen) / 1000) : "-1") + ",\n";

        if (sensor.hasTemperature())
        {
            html += "      \"temperature\": " + String(sensor.temperature, 2) + ",\n";
        }

        if (sensor.hasHumidity())
        {
            html += "      \"humidity\": " + String(sensor.humidity, 2) + ",\n";
        }

        if (sensor.hasPressure())
        {
            html += "      \"pressure\": " + String(sensor.pressure, 2) + ",\n";
        }

        if (sensor.hasPPM())
        {
            html += "      \"ppm\": " + String(sensor.ppm, 0) + ",\n";
        }

        if (sensor.hasLux())
        {
            html += "      \"lux\": " + String(sensor.lux, 1) + ",\n";
        }

        html += "      \"batteryVoltage\": " + String(sensor.batteryVoltage, 2) + ",\n";
        html += "      \"rssi\": " + String(sensor.rssi) + "\n";
        html += "    }\n";
    }
    else
    {
        html += "    {\n";
        html += "      \"name\": \"Example Sensor\",\n";
        html += "      \"type\": 1,\n";
        html += "      \"typeName\": \"CLIMA\",\n";
        html += "      \"serialNumber\": \"123456\",\n";
        html += "      \"lastSeen\": 300,\n";
        html += "      \"temperature\": 21.50,\n";
        html += "      \"humidity\": 45.20,\n";
        html += "      \"pressure\": 1013.20,\n";
        html += "      \"batteryVoltage\": 3.82,\n";
        html += "      \"rssi\": -72\n";
        html += "    }\n";
    }

    html += "  ]\n";
    html += "}\n";
    html += "</pre>";

    html += "<h3>Live API</h3>";
    html += "<p>Access the live API here: <a href='/api?format=json' target='_blank'>/api?format=json</a></p>";
    html += "</div>";

    // Přidání patičky
    addHtmlFooter(html);

    return html;
}
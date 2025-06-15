/*
  ESP32 LCD Time Zone & Configuration Project
  --------------------------------------------
  
  DESCRIPTION:
  This project uses an ESP32 with a 20×4 I2C LCD to display:
    - A custom sun icon and the current local time (synchronized by NTP)
    - A scrolling text message 
    - The connected WiFi SSID and device IP address

  CONFIGURATION:
  - WiFi is set up via WiFiManager (captive portal if no credentials exist).
  - A custom web interface is served (at /config) to update:
      • Time Zone (selectable from a dropdown of offset‑based TZ strings)
      • Scrolling Speed (adjustable via a range slider between 30–500 ms)
      • Temperature Unit (°C or F)
  - When the time zone is updated, configTzTime() is called to force an NTP sync.
    The code waits for valid local time before updating the LCD.

  DEPENDENCIES:
  - Wire (for I2C communication)
  - LiquidCrystal_I2C (for LCD control)
  - WiFi (for network connectivity)
  - WebServer (for hosting the HTTP server)
  - ArduinoJson (for processing JSON payloads)
  - WiFiManager (for handling WiFi & captive portal configuration)
  - Preferences (for persistent storage of configuration settings)
  - ESPmDNS (for resolving the device name on the LAN)
  - time.h (for time-related functions, including configTzTime())

  Author: Your Name
  License: MIT License
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>    // Captive portal configuration library
#include <Preferences.h>    // Persistent storage
#include <ESPmDNS.h>        // mDNS for friendly host names
#include <time.h>

// ----- LCD Configuration -----
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// ----- Custom Character: Sun Symbol -----
byte sunSymbol[8] = {
  0b00100,
  0b10001,
  0b01110,
  0b11111,
  0b01110,
  0b10001,
  0b00100,
  0b00000
};

// ----- Global Variables -----
String displayMessage = "Scrolling Text Demo ";
int scrollIndex = 0;
const String blankPadding = "                    "; // 20 spaces for scrolling effect

// Timing variables for scrolling text
unsigned long scrollDelay = 300;          // Default scroll speed (ms)
unsigned long previousScrollMillis = 0;

// ----- Web Server Instance -----
WebServer server(80);

/*
  handleDisplay (POST /display)
  ------------------------------
  Accepts a JSON payload:
      { "text": "Your message here" }
  (The text must be 40 characters or fewer.)
  This endpoint updates the scrolling message.
*/
void handleDisplay() {
  if (server.method() == HTTP_POST) {
    if (!server.hasArg("plain")) {
      server.send(400, "text/plain", "No message received");
      return;
    }
    String reqBody = server.arg("plain");
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, reqBody);
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }
    if (!doc.containsKey("text")) {
      server.send(400, "text/plain", "Missing 'text' field");
      return;
    }
    String newText = doc["text"].as<String>();
    if (newText.length() > 40) {
      server.send(400, "application/json", "{\"error\":\"Text length exceeds maximum allowed of 40 characters.\"}");
      return;
    }
    displayMessage = newText;
    scrollIndex = 0; // Reset scrolling index
    server.send(200, "application/json", "{\"status\":\"OK\"}");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

/*
  handleConfigGet (GET /config)
  ------------------------------
  Serves an HTML configuration page that features:
    - A dropdown to select the Time Zone (using offset-based strings)
    - A range slider to adjust the Scroll Speed (30–500 ms)
    - A dropdown to select the Temperature Unit (°C vs F)
*/
void handleConfigGet() {
  Preferences preferences;
  preferences.begin("config", true);
  // Default time zone is set to Central European Time using an offset-based string.
  String tz = preferences.getString("tzOffset", "CET-1CEST,M3.5.0/2,M10.5.0/3");
  String scrollSpeedStr = preferences.getString("scrollSpeed", "300");
  String tempUnit = preferences.getString("tempUnit", "C");
  preferences.end();
  
  String html = "<!DOCTYPE html><html><head><title>ESP32 Config</title></head><body>";
  html += "<h1>ESP32 Configuration</h1>";
  html += "<form action=\"/updateConfig\" method=\"POST\">";
  
  // Time Zone Dropdown Options (offset-based)
  html += "Time Zone (TZ): <select name=\"tzOffset\">";
  html += "<option value=\"CET-1CEST,M3.5.0/2,M10.5.0/3\"" + String(tz == "CET-1CEST,M3.5.0/2,M10.5.0/3" ? " selected" : "") + ">Central European Time (CET)</option>";
  html += "<option value=\"EST5EDT,M3.2.0/2,M11.1.0/2\"" + String(tz == "EST5EDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Eastern Standard Time (US)</option>";
  html += "<option value=\"CST6CDT,M3.2.0/2,M11.1.0/2\"" + String(tz == "CST6CDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Central Standard Time (US)</option>";
  html += "<option value=\"MST7MDT,M3.2.0/2,M11.1.0/2\"" + String(tz == "MST7MDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Mountain Standard Time (US)</option>";
  html += "<option value=\"PST8PDT,M3.2.0/2,M11.1.0/2\"" + String(tz == "PST8PDT,M3.2.0/2,M11.1.0/2" ? " selected" : "") + ">Pacific Standard Time (US)</option>";
  html += "<option value=\"UTC0\"" + String(tz == "UTC0" ? " selected" : "") + ">UTC</option>";
  html += "<option value=\"JST-9\"" + String(tz == "JST-9" ? " selected" : "") + ">Japan Standard Time (JST)</option>";
  html += "</select><br><br>";
  
  // Scroll Speed Range Slider
  html += "Scroll Speed (ms): <input type=\"range\" name=\"scrollSpeed\" min=\"30\" max=\"500\" value=\"" + scrollSpeedStr + "\" id=\"scrollSpeedRange\">";
  html += " <span id=\"scrollSpeedValue\">" + scrollSpeedStr + "</span><br><br>";
  
  // Temperature Unit Dropdown
  html += "Temperature Unit: <select name=\"tempUnit\">";
  html += "<option value=\"C\"" + String(tempUnit == "C" ? " selected" : "") + ">&deg;C</option>";
  html += "<option value=\"F\"" + String(tempUnit == "F" ? " selected" : "") + ">F</option>";
  html += "</select><br><br>";
  
  html += "<input type=\"submit\" value=\"Update Configuration\">";
  html += "</form>";
  
  // JavaScript to show live slider updates.
  html += "<script>document.getElementById('scrollSpeedRange').oninput = function(){document.getElementById('scrollSpeedValue').innerText = this.value;};</script>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

/*
  handleUpdateConfig (POST /updateConfig)
  -----------------------------------------
  Processes the configuration form submission.
  Validates:
    - Scroll Speed is within 30–500 ms.
    - Temperature Unit is either "C" or "F".
    - Time Zone is non-empty.
  On success:
    1. Stores settings in persistent storage.
    2. Forces an NTP sync using configTzTime() with the provided offset-based TZ string.
    3. Waits until a valid local time is received and updates the LCD's displayed time.
*/
void handleUpdateConfig() {
  if (server.hasArg("tzOffset") && server.hasArg("scrollSpeed") && server.hasArg("tempUnit")) {
    String newTz = server.arg("tzOffset");
    String newScrollSpeed = server.arg("scrollSpeed");
    String newTempUnit = server.arg("tempUnit");
    
    int speed = newScrollSpeed.toInt();
    if (speed < 30 || speed > 500) {
      server.send(400, "text/plain", "Scroll Speed must be between 30 and 500 ms");
      return;
    }
    if (newTempUnit != "C" && newTempUnit != "F") {
      server.send(400, "text/plain", "Temperature Unit must be either C or F");
      return;
    }
    if (newTz.length() == 0) {
      server.send(400, "text/plain", "Time zone must not be empty");
      return;
    }
    
    Preferences preferences;
    preferences.begin("config", false);
    preferences.putString("tzOffset", newTz);
    preferences.putString("scrollSpeed", newScrollSpeed);
    preferences.putString("tempUnit", newTempUnit);
    preferences.end();
    
    scrollDelay = atol(newScrollSpeed.c_str());
    
    // Force NTP sync using the new offset-based time zone.
    configTzTime(newTz.c_str(), "pool.ntp.org");
    
    // Wait up to 10 * 500ms for a valid local time.
    struct tm updatedTime;
    int retries = 0;
    while (!getLocalTime(&updatedTime) && retries < 10) {
      delay(500);
      retries++;
    }
    
    char updatedTimeStr[6];
    if (getLocalTime(&updatedTime)) {
      sprintf(updatedTimeStr, "%02d:%02d", updatedTime.tm_hour, updatedTime.tm_min);
      lcd.setCursor(LCD_COLUMNS - 5, 0);
      lcd.print("     "); // Clear time area.
      lcd.setCursor(LCD_COLUMNS - 5, 0);
      lcd.print(updatedTimeStr);
    }
    
    String message = "Configuration updated successfully. (Local time updated.) <a href=\"/config\">Go Back</a>";
    server.send(200, "text/html", message);
    
  } else {
    server.send(400, "text/plain", "Missing fields");
  }
}

/*
  setupWiFi
  ---------
  Uses WiFiManager to configure WiFi and additional settings (time zone, temperature unit, scroll speed).
  After a successful connection, it forces an NTP sync using configTzTime() with the stored time zone.
*/
void setupWiFi() {
  Preferences preferences;
  preferences.begin("config", false);
  String storedTz = preferences.getString("tzOffset", "CET-1CEST,M3.5.0/2,M10.5.0/3");
  String storedScrollSpeed = preferences.getString("scrollSpeed", "300");
  String storedTempUnit = preferences.getString("tempUnit", "C");
  
  WiFiManager wifiManager;
  WiFiManagerParameter tzParam("tzOffset", "TZ", storedTz.c_str(), 64);
  WiFiManagerParameter tempParam("tempUnit", "Temp Unit", storedTempUnit.c_str(), 2);
  WiFiManagerParameter scrollParam("scrollSpeed", "Scroll Speed", storedScrollSpeed.c_str(), 6);
  wifiManager.addParameter(&tzParam);
  wifiManager.addParameter(&tempParam);
  wifiManager.addParameter(&scrollParam);
  
  if (!wifiManager.autoConnect("ESP32-AP")) {
    Serial.println("Failed to connect, restarting...");
    ESP.restart();
  }
  
  WiFi.mode(WIFI_STA);
  
  String newTz = String(tzParam.getValue());
  String newScrollSpeed = String(scrollParam.getValue());
  String newTempUnit = String(tempParam.getValue());
  if (newTz != storedTz) preferences.putString("tzOffset", newTz);
  if (newScrollSpeed != storedScrollSpeed) preferences.putString("scrollSpeed", newScrollSpeed);
  if (newTempUnit != storedTempUnit) preferences.putString("tempUnit", newTempUnit);
  preferences.end();
  
  scrollDelay = atol(newScrollSpeed.c_str());
  
  Serial.println("Connected to WiFi: " + String(WiFi.SSID()));
  
  // Force NTP sync using the stored offset-based time zone.
  configTzTime(newTz.c_str(), "pool.ntp.org");
  
  if (!MDNS.begin("esp32portal")) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started at http://esp32portal.local/");
  }
}

/*
  setup
  -----
  Initializes Serial, LCD, WiFi, and web server endpoints.
*/
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  
  lcd.createChar(0, sunSymbol);
  // Initialize the top row: Print sun icon and clear space.
  lcd.setCursor(0, 0);
  lcd.write(byte(0));
  lcd.setCursor(1, 0);
  lcd.print("              ");
  
  setupWiFi();
  
  // Set up web server end-points.
  server.on("/display", HTTP_POST, handleDisplay);
  server.on("/config", HTTP_GET, handleConfigGet);
  server.on("/updateConfig", HTTP_POST, handleUpdateConfig);
  server.on("/", HTTP_GET, [](){
    server.send(200, "application/json", "{\"display\":\"" + displayMessage + "\"}");
  });
  server.begin();
  
  // Initialize scrolling text on row 1.
  lcd.setCursor(0, 1);
  lcd.print((displayMessage + blankPadding).substring(0, LCD_COLUMNS));
}

/*
  loop
  ----
  Continuously updates the LCD display (sun icon and current time on row 0,
  scrolling text on row 1, WiFi SSID on row 2, and IP address on row 3)
  and processes incoming web server requests.
*/
void loop() {
  server.handleClient();
  
  // Update Top Row: Display sun icon and current local time (HH:mm).
  char timeStr[6];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    strcpy(timeStr, "00:00");
  }
  lcd.setCursor(0, 0);
  lcd.write(byte(0));
  lcd.setCursor(1, 0);
  lcd.print("              ");
  lcd.setCursor(LCD_COLUMNS - 5, 0);
  lcd.print(timeStr);
  
  // Update Row 1: Scrolling text.
  unsigned long currentMillis = millis();
  if (currentMillis - previousScrollMillis >= scrollDelay) {
    previousScrollMillis = currentMillis;
    lcd.setCursor(0, 1);
    String padded = displayMessage + blankPadding;
    if (scrollIndex > padded.length() - LCD_COLUMNS) {
      scrollIndex = 0;
    }
    lcd.print(padded.substring(scrollIndex, scrollIndex + LCD_COLUMNS));
    scrollIndex++;
  }
  
  // Update Row 2: Display connected WiFi SSID.
  String ssidDisplay = "SSID:" + WiFi.SSID();
  if (ssidDisplay.length() < LCD_COLUMNS) {
    ssidDisplay += String("                    ").substring(0, LCD_COLUMNS - ssidDisplay.length());
  } else {
    ssidDisplay = ssidDisplay.substring(0, LCD_COLUMNS);
  }
  lcd.setCursor(0, 2);
  lcd.print(ssidDisplay);
  
  // Update Row 3: Display device IP address.
  String ipDisplay = "IP:" + WiFi.localIP().toString();
  if (ipDisplay.length() < LCD_COLUMNS) {
    ipDisplay += String("                    ").substring(0, LCD_COLUMNS - ipDisplay.length());
  } else {
    ipDisplay = ipDisplay.substring(0, LCD_COLUMNS);
  }
  lcd.setCursor(0, 3);
  lcd.print(ipDisplay);
}

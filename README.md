# ESP32 based dashboard/control unit for my home automation

Under contruction...

The device is initially configured via a captive portal (using WiFiManager) and then managed via a custom web configuration interface. You can update settings such as:
- **Time Zone:** Select from several offset-based TZ strings (see list below)
- **Scrolling Speed:** Adjust using a range slider (30–500 ms)
- **Temperature Unit:** Choose between °C and Fahrenheit

After updating these settings, the ESP32 forces an NTP synchronization (using `configTzTime()`) according to the selected time zone so that the LCD displays the correct local time.

---

## Features

- **WiFi Setup via Captive Portal:**
  - Automatically starts a captive portal if no WiFi credentials are stored.
  
- **LCD Display:**
  - **Row 0:** Displays a custom sun icon and the current local time in HH:mm format.
  - **Row 1:** Shows a scrolling text message.
  - **Row 2:** Shows the connected WiFi SSID.
  - **Row 3:** Shows the device's IP address.

- **Custom Web Configuration Interface:**
  - Access via `http://<ESP32_IP>/config` or via mDNS at `http://esp32portal.local/config`
  - Update settings:
    - **Time Zone:** Choose from several offset-based TZ strings:
      - Central European Time (CET): `"CET-1CEST,M3.5.0/2,M10.5.0/3"`
      - Eastern Standard Time (US): `"EST5EDT,M3.2.0/2,M11.1.0/2"`
      - Central Standard Time (US): `"CST6CDT,M3.2.0/2,M11.1.0/2"`
      - Mountain Standard Time (US): `"MST7MDT,M3.2.0/2,M11.1.0/2"`
      - Pacific Standard Time (US): `"PST8PDT,M3.2.0/2,M11.1.0/2"`
      - UTC: `"UTC0"`
      - Japan Standard Time (JST): `"JST-9"`
    - **Scrolling Speed:** Set the speed (30–500 ms) for scrolling text.
    - **Temperature Unit:** Choose between °C and F.
  - Upon submission, the device forces an NTP sync using the selected time zone.

- **REST Endpoint:**
  - **POST `/display`:**  
    Update the scrolling message using a JSON payload (example: `{ "text": "Hello ESP32!" }`, with a maximum of 40 characters).

- **Accurate Time Zone Synchronization:**
  - Uses `configTzTime()` in both WiFi setup and configuration update to force an NTP sync with the selected, offset-based time zone.
  - The project waits until a valid local time is obtained before updating the LCD.

---

## Requirements

### Hardware
- **ESP32 Development Board**
- **20×4 I2C LCD Display**  
  (Ensure your LCD is compatible with the LiquidCrystal_I2C library. Typical wiring: SDA → GPIO21, SCL → GPIO22 on the ESP32.)

### Software / Libraries
- **ESP32 Arduino Core:**  
  Version **3.2.0 or later** is recommended.
- **Libraries:**
  - [Wire](https://www.arduino.cc/en/Reference/Wire) (Built-in for I2C)
  - [LiquidCrystal_I2C](https://github.com/makernotaku/LiquidCrystal_I2C) (for LCD control)
  - [WiFi](https://www.arduino.cc/en/Reference/WiFi) (Built-in)
  - [WebServer](https://www.arduino.cc/en/Reference/WebServer) (Built-in)
  - [ArduinoJson](https://arduinojson.org/) (for JSON parsing)
  - [WiFiManager](https://github.com/tzapu/WiFiManager) (for captive portal setup)
  - [Preferences](https://www.arduino.cc/en/Reference/Preferences) (Built-in for persistent storage)
  - [ESPmDNS](https://github.com/espressif/arduino-esp32/tree/master/libraries/ESPmDNS) (for mDNS)
  - [time.h](https://www.gnu.org/software/libc/manual/html_node/Time-Functions.html) (for time functions, including `configTzTime()`)

---

## Installation & Setup

1. **Install the ESP32 Core:**
   - Open Arduino IDE, go to **File > Preferences**, and add the following URL to “Additional Boards Manager URLs”:
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
   - Then open **Tools > Board > Boards Manager**, search for "esp32," and install version 3.2.0 or a later release.

2. **Install Required Libraries:**
   - Use the Arduino Library Manager to install:
     - LiquidCrystal_I2C
     - ArduinoJson
     - WiFiManager

3. **Hardware Connections:**
   - Connect the I2C LCD to the ESP32 (typically SDA to GPIO21 and SCL to GPIO22). Adjust if necessary.

4. **Upload the Code:**
   - Copy the complete code (provided in this repository) into a new Arduino sketch and upload it to your ESP32.

---

## Usage

- **Initial WiFi Setup:**
  - On first boot (or if no credentials exist), the ESP32 launches a captive portal with the SSID `ESP32-AP`.
  - Connect with your smartphone or computer and provide your WiFi credentials along with the default custom settings.

- **Configuration Interface:**
  - Once connected, open a web browser and go to:
    - `http://<ESP32_IP>/config` or
    - `http://esp32portal.local/config`
  - Use the form to choose your desired time zone (from the offset-based strings), adjust the scroll speed, and select the temperature unit.
  - When you submit the form, the ESP32 forces an NTP sync using `configTzTime()` and updates the LCD’s local time display accordingly.

- **Updating the Scrolling Text:**
  - To update the scrolling text via REST:
    ```sh
    curl -X POST "http://<ESP32_IP>/display" -H "Content-Type: application/json" -d '{"text":"Hello ESP32!"}'
    ```
  - The message must be 40 characters or fewer.

---

## Troubleshooting

- **Time Zone Not Updating:**
  - Ensure you select one of the provided offset-based TZ strings (e.g., `"CET-1CEST,M3.5.0/2,M10.5.0/3"`).
  - Check network connectivity and NTP server status (`pool.ntp.org`).
  - If the time still remains in UTC, try power cycling your board.

- **mDNS Issues:**
  - If `http://esp32portal.local` is inaccessible, use the ESP32's assigned IP address.

---

## License

This project is released under the MIT License.

---

## Acknowledgments

- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [LiquidCrystal_I2C Library](https://github.com/makernotaku/LiquidCrystal_I2C)
- [ArduinoJson Library](https://arduinojson.org/)
- ESPmDNS, Preferences, and time.h (provided in the ESP32 core)

# MQTT_TEST Project

This project is an ESP32-based application developed using PlatformIO that creates a configurable MQTT client. It features Wi-Fi management, persistent configuration storage, and a web-based setup interface.

## 🗂️ Project Structure

```
MQTT_TEST/
├── src/
│   ├── main.cpp               # Application entry point
│   └── util/                  # Utility classes
│       ├── ConfigHelper.*     # Configuration management
│       └── WifiManagerHelper.*# Wi-Fi setup handling
├── include/                   # Header files
├── lib/                      # Project-specific libraries
├── test/                     # Unit tests
└── platformio.ini            # PlatformIO configuration
```

## ✨ Features

- **Wi-Fi Management**: 
  - Web-based configuration portal
  - Persistent Wi-Fi credentials storage
  - Automatic reconnection handling
- **MQTT Configuration**:
  - Configurable broker URL and path
  - Custom client hostname support
  - Connection state management
- **System Features**:
  - SPIFFS-based configuration storage
  - Web-based setup interface
  - Status LED indicators
  - Serial debug output

## 🔧 Requirements

### Hardware
- ESP32 development board
- Micro USB cable
- LED (optional, for status indication)

### Software
- Visual Studio Code with PlatformIO extension
- ESP32 Arduino framework
- Required Libraries:
  - WiFiManager
  - PubSubClient
  - ArduinoJson

## 📦 Installation

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/yourusername/MQTT_TEST.git
   cd MQTT_TEST
   ```

2. **Open in VS Code**:
   ```bash
   code .
   ```

3. **Install Dependencies**:
   - Open PlatformIO extension
   - Click "Build" to install required libraries

## ⚙️ Configuration

### Initial Setup
1. Power up the ESP32
2. Connect to the "DMD_CONFIG_WIFI" Wi-Fi network
3. Open the configuration portal (usually at 192.168.4.1)
4. Configure:
   - Wi-Fi credentials
   - MQTT broker URL
   - MQTT base path
   - Device hostname

### Configuration Parameters
- **MQTT URL**: Broker address (e.g., `mqtt://broker.example.com`)
- **MQTT Path**: Base topic path (e.g., `/home/sensors/`)
- **Hostname**: Device identifier on network

## 🚀 Building and Flashing

### Using VS Code + PlatformIO
1. **Build Project**:
   ```bash
   pio run
   ```

2. **Upload to ESP32**:
   ```bash
   pio run --target upload
   ```

3. **Monitor Serial Output**:
   ```bash
   pio device monitor
   ```

### Using PlatformIO CLI
```bash
# Build
pio run

# Upload
pio run -t upload

# Monitor
pio device monitor
```

## 🔍 Debugging

### Serial Monitor Output
- Baud Rate: 115200
- Debug messages include:
  - Wi-Fi connection status
  - MQTT connection state
  - Configuration changes
  - System events

### LED Status Indicators
- **Solid**: Connected to Wi-Fi and MQTT
- **Slow Blink**: Wi-Fi connected, MQTT disconnected
- **Fast Blink**: Configuration mode active
- **Off**: System offline

## ⚙️ How It Works

1.  **Initialization**:
    *   The ESP32 initializes and attempts to connect to Wi-Fi using stored credentials. If no credentials are found, it starts in configuration mode, hosting a web portal for setup.
2.  **Wi-Fi Connection**:
    *   If Wi-Fi credentials are valid, the ESP32 connects to the specified network.
    *   If the connection fails or no credentials are saved, the ESP32 enters Access Point (AP) mode, allowing configuration via a web browser.
3.  **MQTT Connection**:
    *   After a successful Wi-Fi connection, the ESP32 attempts to connect to the MQTT broker using the configured URL, path, and hostname.
    *   It subscribes to the specified MQTT topic to receive messages.
4.  **Message Handling**:
    *   When an MQTT message is received, the ESP32 processes the message and updates the DMD (Dot Matrix Display) accordingly.
    *   The message content is parsed, and the display is updated in real-time.
5.  **DMD Display**:
    *   The ESP32 controls the DMD to display the received messages.
    *   The display logic handles scrolling, formatting, and any other visual effects.
6.  **Configuration Updates**:
    *   Users can update the Wi-Fi and MQTT settings via the web configuration portal.
    *   The updated configuration is saved to SPIFFS for persistent storage.
7.  **Error Handling**:
    *   The system includes error handling for Wi-Fi and MQTT connections, providing status updates via serial output and LED indicators.
    *   If the MQTT connection is lost, the system attempts to reconnect automatically.

## 🧪 Testing

```bash
# Run all tests
pio test

# Run specific test
pio test -f test_wifi
```

## 📝 Contributing

1. Fork the repository
2. Create feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit changes (`git commit -m 'Add AmazingFeature'`)
4. Push to branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.

## 📚 Resources

- [PlatformIO Documentation](https://docs.platformio.org/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [WiFiManager Library](https://github.com/tzapu/WiFiManager)
- [PubSubClient Library](https://github.com/knolleary/pubsubclient)

## 🤝 Support

For support and questions:
- Open an issue
- Contact: your.email@example.com

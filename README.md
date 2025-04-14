# ESP32 6-Channel NTC Sensor with LCD and WiFi

This project implements a 6-channel NTC temperature sensor system using an ESP32 microcontroller. It features a 20x4 HD44780 LCD for real-time temperature display and WiFi capabilities for configuration and data feedback. The system supports both Access Point (AP) and Station (STA) WiFi modes, allowing users to configure settings and view temperature data through a web interface.

## Features

- **6-Channel NTC Temperature Sensing**: Reads temperature data from up to 6 NTC thermistors.
- **HD44780 20x4 LCD Display**: Displays real-time temperature readings and system status.
- **WiFi AP and STA Modes**:
  - Both AP and STA modes provide configuration and access to the web interface. The difference lies in the connection mode:
    - **AP Mode**: The ESP32 creates a local WiFi network for direct connection.
    - **STA Mode**: The ESP32 connects to an existing WiFi network for remote access.
- **Web Interface**:
  - Configure WiFi settings (AP/STA SSID and password).
  - Enable or disable individual sensor channels.
  - View temperature data and graphs.
- **Frontend Integration**: The web interface is built using the `frontend` folder and is included in the device firmware during flashing.

## How It Works

1. **Temperature Sensing**:
   - The ESP32 reads raw ADC values from the connected NTC thermistors.
   - The raw values are converted to temperatures using the Steinhart-Hart equation.

2. **LCD Display**:
   - The 20x4 HD44780 LCD displays the temperature readings for all active channels.
   - Additional system information, such as WiFi status, is also shown.

3. **WiFi Capabilities**:
   - In **AP Mode**, the ESP32 creates a local WiFi network for direct connection.
   - In **STA Mode**, the ESP32 connects to a user-specified WiFi network for remote access.

4. **Web Interface**:
   - The web interface allows users to configure WiFi settings, enable/disable sensors, and view temperature data.
   - The interface is built using modern web technologies and is served directly from the ESP32 internal FATFS.

## Building and Flashing

1. **Frontend Build**:
   - Navigate to the `frontend/sta-settings` folder.
   - Run the following commands to build the frontend:
     ```sh
     npm install
     npm run build
     ```
   - The built files will be included in the firmware during the flashing process.

2. **Flashing the Device**:
   - Use the ESP-IDF build system to compile and flash the firmware:
     ```sh
     idf.py build
     idf.py flash
     ```

3. **Accessing the Web Interface**:
   - In AP Mode, connect to the ESP32's WiFi network (default SSID: `ESP32-AP`, password: `12345678`).
   - Open a browser and navigate to `http://192.168.4.1`.

## Troubleshooting

- **Program Upload Failure**:
  - Ensure the hardware connection is correct. Use `idf.py -p PORT monitor` to check for logs.
  - Lower the baud rate in the `menuconfig` menu if the upload fails.

- **WiFi Connection Issues**:
  - Verify the SSID and password for STA mode.
  - Check the signal strength of the target WiFi network.

We hope you enjoy using this project!

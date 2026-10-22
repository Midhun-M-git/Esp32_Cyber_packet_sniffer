# ESP32 Cyber Packet Sniffer

<div align="center">

[![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![ESP32](https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://espressif.com)
[![Hardware](https://img.shields.io/badge/Display-3.5%22%20Parallel%20TFT-4E73DF?style=for-the-badge)](https://github.com/Bodmer/TFT_eSPI)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)

</div>

A standalone embedded wireless analysis dashboard built for the ESP32 and 3.5-inch 8-bit parallel TFT shields. Captures raw IEEE 802.11 frames in promiscuous mode and renders real-time wireless metrics and channel telemetry.

---

## Hardware Architecture & Signal Flow

```mermaid
graph LR
    RF[2.4GHz Wi-Fi Signals] --> ANT[ESP32 Integrated Antenna]
    ANT --> PROMISC[ESP-IDF Promiscuous Core]
    PROMISC --> PARSER[Frame Filter & Header Parser]
    
    subgraph ESP32 Processing Core
        PARSER --> CHANNELS[Channel Hopping Engine: 1-14]
        PARSER --> METRICS[Packet Counter: Beacon / Data / Mgmt]
        PARSER --> RSSI[Signal Strength & MAC Extractor]
    end

    METRICS --> TFT[TFT_eSPI Parallel 8-Bit Bus]
    RSSI --> TFT
    CHANNELS --> TFT
    TFT --> LCD[3.5 Inch ILI9486 Parallel LCD Shield]
```

---

## System Pipeline

```mermaid
sequenceDiagram
    participant Air as 802.11 Airwaves
    participant ESP as ESP32 Wi-Fi Baseband
    participant Core as Sniffer Parser Callback
    participant Display as 3.5" TFT LCD

    ESP->>Air: Hop to Channel N
    Air-->>ESP: Raw IEEE 802.11 Frame
    ESP->>Core: wifi_promiscuous_rx_cb_t
    Core->>Core: Decode Frame Type & Subtype
    Core->>Core: Extract Source MAC & Signal RSSI
    Core->>Display: Draw Live Waveform & Packet Counters
```

---

## Technical Specifications

- **Microcontroller**: ESP32 Dual-Core Tensilica LX6 (240MHz)
- **Supported Display**: 3.5" Arduino Uno Parallel TFT Shield (ILI9486 / ST7796)
- **Data Bus**: 8-bit parallel interface utilizing GPIOs `12, 13, 26, 25, 17, 16, 27, 14`
- **Capture Capabilities**:
  - IEEE 802.11b/g/n raw frame capture
  - Management frames (Beacons, Probe Requests/Responses, Deauth frames)
  - Data frames and payload packet volume
  - Dynamic RSSI decibel telemetry

---

## Hardware Pinout Reference

| TFT Shield Pin | ESP32 GPIO | Function |
| :--- | :--- | :--- |
| **LCD_CS** | GPIO 33 | Chip Select |
| **LCD_RS (CD)** | GPIO 15 | Register Select / Command-Data |
| **LCD_WR** | GPIO 4 | Write Strobe |
| **LCD_RD** | GPIO 2 | Read Strobe |
| **LCD_RST** | GPIO 32 | Hardware Reset |
| **LCD_D0 - D7** | `12, 13, 26, 25, 17, 16, 27, 14` | 8-Bit Data Bus |

---

## Installation & Setup

1. Clone this repository:
   ```bash
   git clone https://github.com/Midhun-M-git/Esp32_Cyber_packet_sniffer.git
   cd Esp32_Cyber_packet_sniffer
   ```

2. Configure TFT_eSPI:
   - Copy `User_Setup.h` from this repository to your Arduino libraries folder:
     `Arduino/libraries/TFT_eSPI/User_Setup.h`

3. Flash Firmware:
   - Open `esp32_sniffer.ino` in Arduino IDE or PlatformIO.
   - Select Board: `ESP32 Dev Module`.
   - Set CPU Frequency: `240MHz (WiFi/BT)`.
   - Click **Upload**.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

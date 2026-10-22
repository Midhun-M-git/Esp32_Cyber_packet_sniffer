<div align="center">
  <h1> ESP32 Cyber Packet Sniffer</h1>
  <p><strong>A futuristic, cyberpunk-themed Wi-Fi analyzer built for the ESP32 and 3.5" Parallel TFT Shields.</strong></p>
</div>

---

Welcome to the **ESP32 Cyber Packet Sniffer**! This project turns a standard ESP32 development board and a 3.5" Arduino Uno TFT LCD Shield into a real-time wireless traffic dashboard with a stunning Cyberpunk aesthetic.

Using the ESP32's native **Promiscuous Mode**, the board scans all 2.4GHz Wi-Fi channels, sniffs airborne 802.11 frames, extracts network names (SSIDs), and displays live traffic and raw hexadecimal payload data!

## Key Features

* **Real-Time Sniffing**: Uses ESP32 hardware-level promiscuous mode to capture Wi-Fi frames mid-air.
* **Cinematic Boot Animation**: Features a retro-terminal typewriter boot sequence, glowing progress bar, and visual screen glitches on startup.
* **Intelligent SSID Extraction**: Parses raw Management Frames (Beacons & Probe Requests) to extract human-readable Network Names instead of just showing MAC addresses.
* **Raw Payload Inspector**: A dedicated Hex-Dump box that intercepts the first 16 bytes of encrypted packet data and streams it live to the screen in a classic matrix style!
* **Cyberpunk Dashboard**: A premium dark-mode UI with smooth, flicker-free micro-updates and color-coded signal strengths.
* **LovyanGFX Powered**: Built on the blazing-fast LovyanGFX library. All hardware pin configurations are embedded directly in the sketch—**no external library config files needed!**

---

## Hardware Connections & Pinout

This code is heavily optimized for **3.5" 8-Bit Parallel TFT Shields** (commonly designed for the Arduino Uno) using drivers like **ILI9486**, **ILI9481**, or **ST7796**.

** CRITICAL POWER WARNING:** If you are using an Arduino Uno Shield, you **MUST** connect the shield's `5V` pin to the ESP32's `VIN` (5V) pin. Powering it with 3.3V will result in corrupted, glitchy visual lines.

### Wiring Table (ESP32 to Uno TFT Shield)

| TFT Shield Pin | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **5V** | **VIN / 5V** | Power Supply (Mandatory for Uno Shields) |
| **GND** | **GND** | Common ground reference |
| **LCD_CS (A3)** | **GPIO 33** | Chip Select |
| **LCD_RS (A2)** | **GPIO 15** | Register Select / Command-Data |
| **LCD_WR (A1)** | **GPIO 4** | Write Strobe |
| **LCD_RD (A0)** | **GPIO 2** | Read Strobe |
| **LCD_RST (A4)**| **GPIO 32** | Display Reset |
| **LCD_D0 (8)** | **GPIO 12** | 8-bit parallel data line 0 |
| **LCD_D1 (9)** | **GPIO 13** | 8-bit parallel data line 1 |
| **LCD_D2 (2)** | **GPIO 26** | 8-bit parallel data line 2 |
| **LCD_D3 (3)** | **GPIO 25** | 8-bit parallel data line 3 |
| **LCD_D4 (4)** | **GPIO 17** | 8-bit parallel data line 4 |
| **LCD_D5 (5)** | **GPIO 16** | 8-bit parallel data line 5 |
| **LCD_D6 (6)** | **GPIO 27** | 8-bit parallel data line 6 |
| **LCD_D7 (7)** | **GPIO 14** | 8-bit parallel data line 7 |

---

## Installation & Setup

1. **Install Arduino IDE** and the [ESP32 Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).
2. Open the **Arduino Library Manager** (`Sketch > Include Library > Manage Libraries`).
3. Search for and install **`LovyanGFX`** by lovyan03.
4. Open `esp32_sniffer.ino`.
5. **Select your Display Driver:** Around line 34 in the code, uncomment the driver that matches your screen (e.g., `lgfx::Panel_ILI9481`, `lgfx::Panel_ILI9486`, or `lgfx::Panel_ST7796`).
6. Select your ESP32 board under **Tools > Board** and hit **Upload**!

---

## Deep-Dive Architecture

To build a high-performance network sniffer that actually works reliably on cheap hardware, we addressed critical bottlenecks:

### Thread-Safety & Queueing
The Wi-Fi promiscuous callback fires directly inside a high-priority background network thread running on FreeRTOS. Drawing onto an LCD takes time. If we draw directly inside the callback, the ESP32 Wi-Fi thread blocks and crashes the Watchdog Timer (WDT).
**Solution:** We implemented a lock-free circular queue. Captured packets are instantly dumped into RAM. The main `loop()` on Core 1 pops packets from this queue and renders the Cyberpunk dashboard at its own safe pace.

### Analog Touch Screen Limitations
Why doesn't the touch screen work? Analog resistive touch screens (like those on Uno shields) require the ESP32's ADC (Analog-to-Digital Converter) to measure touch voltages. Because the pins we use (like GPIO 4 and 15) belong to **ADC2**, and the ESP32 physically disables ADC2 when the Wi-Fi radio is active, it is mathematically impossible to use this specific touch screen simultaneously with a Wi-Fi sniffer!

---
*Created by [Midhun-M-git](https://github.com/Midhun-M-git)*

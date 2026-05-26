/**
 * ==================================================================================
 *               TFT_eSPI LIBRARY CUSTOM CONFIGURATION (User_Setup.h)
 *             --- 8-BIT PARALLEL INTERFACE VERSION FOR ESP32 ---
 * ==================================================================================
 * 
 * Instructions:
 *   1. Locate your Arduino libraries folder (usually Documents/Arduino/libraries).
 *   2. Find the "TFT_eSPI" folder.
 *   3. Replace the existing "User_Setup.h" file with the content of this file.
 * 
 * Description:
 *   This setup file is pre-configured specifically for:
 *   - ESP32 Development Board (38-pin version)
 *   - 3.5" TFT LCD Display with 8-bit Parallel Interface
 *     (Pins: lcdd0-lcdd7, lcd cs, lcd rs, lcd wr, lcd rd, lcd rst)
 * ==================================================================================
 */

// ----------------------------------------------------------------------------------
// SECTION 1: SELECT DISPLAY DRIVER
// ----------------------------------------------------------------------------------
// Uncomment the line corresponding to your display's controller chip.
// Most parallel 3.5" Arduino Mega/Uno shields use ILI9488 or ST7796.

#define ILI9488_DRIVER     // Default driver for 3.5" ILI9488 panels
// #define ST7796_DRIVER   // Uncomment this and comment out ILI9488 if you have ST7796S

// ----------------------------------------------------------------------------------
// SECTION 2: ENABLE PARALLEL INTERFACE MODE
// ----------------------------------------------------------------------------------
// Defining ESP32_PARALLEL tells TFT_eSPI to use an 8-bit parallel bus instead of SPI.
#define ESP32_PARALLEL

// ----------------------------------------------------------------------------------
// SECTION 3: DEFINE 8-BIT PARALLEL PIN CONNECTIONS
// ----------------------------------------------------------------------------------
// This pinout is highly optimized for ESP32 parallel performance.
// Connect your parallel TFT display pins to the ESP32 pins as defined below:

// --- Control Pins ---
#define TFT_CS   33  // Connect to LCD CS (Chip Select)
#define TFT_DC   15  // Connect to LCD RS / LCD REG (Register Select / Data-Command)
#define TFT_WR    4  // Connect to LCD WR (Write strobe)
#define TFT_RD    2  // Connect to LCD RD (Read strobe)
#define TFT_RST  32  // Connect to LCD RST (Reset)

// --- 8-Bit Data Bus Pins (D0 to D7) ---
#define TFT_D0   12  // Connect to LCD D0 (lcdd0)
#define TFT_D1   13  // Connect to LCD D1 (lcdd1)
#define TFT_D2   26  // Connect to LCD D2 (lcdd2)
#define TFT_D3   25  // Connect to LCD D3 (lcdd3)
#define TFT_D4   17  // Connect to LCD D4 (lcdd4)
#define TFT_D5   16  // Connect to LCD D5 (lcdd5)
#define TFT_D6   27  // Connect to LCD D6 (lcdd6)
#define TFT_D7   14  // Connect to LCD D7 (lcdd7)

// ----------------------------------------------------------------------------------
// SECTION 4: FONTS TO INCLUDE
// ----------------------------------------------------------------------------------
#define LOAD_GLCD   // Font 1. Original Adafruit 8-pixel font
#define LOAD_FONT2  // Font 2. Small 16-pixel font
#define LOAD_FONT4  // Font 4. Medium 26-pixel font (Used for metrics/headers)
#define LOAD_FONT6  // Font 6. Large 48-pixel font
#define LOAD_FONT7  // Font 7. 7-segment 48-pixel font
#define LOAD_FONT8  // Font 8. Large 75-pixel font (Digit only)
#define LOAD_GFXFF  // FreeFonts. Include access to standard GFX Free Fonts

#define SMOOTH_FONT // Enable smooth anti-aliased font support (highly recommended)

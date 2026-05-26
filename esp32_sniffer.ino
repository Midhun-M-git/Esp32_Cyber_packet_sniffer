/**
 * ==================================================================================
 *           ESP32 CYBER PACKET SNIFFER (3.5" PARALLEL TFT - LOVYANGFX VERSION)
 * ==================================================================================
 * 
 * Description:
 *   This sketch configures the ESP32 in Promiscuous Mode to sniff 802.11 Wi-Fi packets,
 *   parses management & data frames to extract source MACs, RSSI, and channels, and
 *   renders a gorgeous cyberpunk-style dashboard on an 8-bit Parallel 3.5" TFT LCD.
 * 
 * Library Dependency:
 *   - LovyanGFX by lovyan03 (Install via Arduino Library Manager)
 *   
 * 💡 ADVANTAGE OF LOVYANGFX:
 *   Unlike other libraries, LovyanGFX does NOT require you to find or modify any 
 *   hidden library configuration files (like 'User_Setup.h'). All display pins, 
 *   parallel bus settings, and driver selections are configured DIRECTLY below 
 *   inside this sketch!
 * 
 * ==================================================================================
 */

#include <WiFi.h>
#include "esp_wifi.h"

// ----------------------------------------------------------------------------------
// LOVYANGFX HARDWARE INITIALIZATION (Pins Configured Directly In-Code)
// ----------------------------------------------------------------------------------
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  // Uncomment ONLY ONE of the panels below depending on your display:
  // lgfx::Panel_ILI9488     _panel_instance; 
  // lgfx::Panel_ST7796       _panel_instance; 
  // lgfx::Panel_ILI9486     _panel_instance; // Try ILI9486 (Most common for Uno Shields)
  lgfx::Panel_ILI9481     _panel_instance;
  // lgfx::Panel_HX8357D     _panel_instance;
  lgfx::Bus_Parallel8      _bus_instance;   // 8-bit parallel bus

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      // LOWERED FREQUENCY: Jumper wires often can't handle 20MHz cleanly.
      // This fixes "stranded lines" and graphical glitches.
      cfg.freq_write = 10000000; // 10MHz write clock
      
      // Control Pin Configurations
      cfg.pin_wr = 4;            // WR Write pin
      cfg.pin_rd = 2;            // RD Read pin
      cfg.pin_rs = 15;           // RS/DC Command pin
      
      // 8-Bit Data Bus Pins (lcdd0 - lcdd7)
      cfg.pin_d0 = 12;           // D0
      cfg.pin_d1 = 13;           // D1
      cfg.pin_d2 = 26;           // D2
      cfg.pin_d3 = 25;           // D3
      cfg.pin_d4 = 17;           // D4
      cfg.pin_d5 = 16;           // D5
      cfg.pin_d6 = 27;           // D6
      cfg.pin_d7 = 14;           // D7
      
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 33; // CS Chip Select pin
      cfg.pin_rst          = 32; // RST Reset pin
      cfg.panel_width      = 320;
      cfg.panel_height     = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      
      _panel_instance.config(cfg);
    }
    
    setPanel(&_panel_instance);
  }
};

// Create display instance
LGFX tft;

// ----------------------------------------------------------------------------------
// CONFIGURATION DEFINITIONS
// ----------------------------------------------------------------------------------
#define MAX_UNIQUE_DEVICES 250   // Max unique MAC addresses to track
#define CHANNEL_HOP_INTERVAL 300 // Time (ms) spent on each channel
#define LIVE_FEED_ROWS 5         // Number of scrolling rows in the live packet feed

// ----------------------------------------------------------------------------------
// QUEUE FOR THREAD-SAFE RENDER HANDLING
// ----------------------------------------------------------------------------------
struct LoggedPacket {
    char type[8];
    char mac[18];
    int rssi;
    uint8_t channel;
    uint8_t payloadPreview[16]; // 16 bytes of raw packet data for the hex dump
};

#define QUEUE_SIZE 32
LoggedPacket packetQueue[QUEUE_SIZE];
volatile int queueHead = 0;
volatile int queueTail = 0;
portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

// Enqueue a packet safely from the Wi-Fi background task (ISR-like context)
bool enqueuePacket(const char* type, const char* mac, int rssi, uint8_t channel, const uint8_t* rawData, size_t rawLen) {
    bool success = false;
    portENTER_CRITICAL_ISR(&queueMux);
    int nextHead = (queueHead + 1) % QUEUE_SIZE;
    if (nextHead != queueTail) { // Check if queue is not full
        strncpy(packetQueue[queueHead].type, type, sizeof(packetQueue[queueHead].type) - 1);
        packetQueue[queueHead].type[sizeof(packetQueue[queueHead].type) - 1] = '\0';
        
        strncpy(packetQueue[queueHead].mac, mac, sizeof(packetQueue[queueHead].mac) - 1);
        packetQueue[queueHead].mac[sizeof(packetQueue[queueHead].mac) - 1] = '\0';
        
        packetQueue[queueHead].rssi = rssi;
        packetQueue[queueHead].channel = channel;

        // Copy up to 16 bytes for the Hex Dump preview
        size_t copyLen = (rawLen > 16) ? 16 : rawLen;
        memset(packetQueue[queueHead].payloadPreview, 0, 16);
        if (rawData != nullptr && copyLen > 0) {
            memcpy(packetQueue[queueHead].payloadPreview, rawData, copyLen);
        }

        queueHead = nextHead;
        success = true;
    }
    portEXIT_CRITICAL_ISR(&queueMux);
    return success;
}

// Dequeue a packet safely from the main loop thread
bool dequeuePacket(LoggedPacket &item) {
    bool success = false;
    portENTER_CRITICAL(&queueMux);
    if (queueHead != queueTail) {
        item = packetQueue[queueTail];
        queueTail = (queueTail + 1) % QUEUE_SIZE;
        success = true;
    }
    portEXIT_CRITICAL(&queueMux);
    return success;
}

// ----------------------------------------------------------------------------------
// GLOBAL SNIFFER VARIABLES
// ----------------------------------------------------------------------------------
volatile uint32_t totalPackets = 0;
volatile uint32_t mgmtPackets = 0;
volatile uint32_t dataPackets = 0;
volatile uint32_t ctrlPackets = 0;

uint8_t uniqueDevices[MAX_UNIQUE_DEVICES][6];
volatile int uniqueDeviceCount = 0;

uint8_t currentChannel = 1;
unsigned long lastChannelHopTime = 0;

// Scrolling history buffer for display
LoggedPacket feedHistory[LIVE_FEED_ROWS];
int feedHistoryCount = 0;

// ----------------------------------------------------------------------------------
// HELPER FUNCTIONS: PACKET PARSING & MAC REGISTRATION
// ----------------------------------------------------------------------------------

// Register unique MAC addresses, ignoring broadcasts and multicasts
void registerUniqueDevice(const uint8_t *mac) {
    // 1. Skip Broadcast (FF:FF:FF:FF:FF:FF)
    if (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF && 
        mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF) {
        return;
    }
    
    // 2. Skip Multicast/Group addresses (LSB of first byte is 1)
    if (mac[0] & 0x01) {
        return;
    }

    // 3. Check if we already registered this MAC
    for (int i = 0; i < uniqueDeviceCount; i++) {
        if (memcmp(uniqueDevices[i], mac, 6) == 0) {
            return; // Already exists
        }
    }

    // 4. Add MAC to list if there's space
    if (uniqueDeviceCount < MAX_UNIQUE_DEVICES) {
        memcpy(uniqueDevices[uniqueDeviceCount], mac, 6);
        uniqueDeviceCount++;
    }
}

// ----------------------------------------------------------------------------------
// ESP32 PROMISCUOUS MODE CALLBACK (Wi-Fi Core Task Context)
// ----------------------------------------------------------------------------------
void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t*)buf;
    int rssi = packet->rx_ctrl.rssi;
    uint32_t len = packet->rx_ctrl.sig_len;
    uint8_t ch = packet->rx_ctrl.channel;

    totalPackets++;

    if (type == WIFI_PKT_MGMT) {
        mgmtPackets++;
    } else if (type == WIFI_PKT_DATA) {
        dataPackets++;
    } else if (type == WIFI_PKT_CTRL) {
        ctrlPackets++;
        return; // Control frames are small & frequent, we skip parsing for display
    } else {
        return; // Skip miscellaneous frames
    }

    // Parse the 802.11 Frame (requires a minimum length of 24 bytes for MAC header)
    if (len >= 24) {
        uint8_t *payload = packet->payload;
        
        // Byte 0: Frame Control
        uint8_t frameType = (payload[0] >> 2) & 0x03;
        uint8_t frameSubtype = (payload[0] >> 4) & 0x0F;

        // Management (0) or Data (2) frame types
        if (frameType == 0 || frameType == 2) {
            uint8_t sa[6]; // Source Address (SA) is at byte index 10-15
            memcpy(sa, &payload[10], 6);

            // Register device
            registerUniqueDevice(sa);

            // Format MAC address to standard string
            char infoStr[18];
            snprintf(infoStr, sizeof(infoStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
                     sa[0], sa[1], sa[2], sa[3], sa[4], sa[5]);

            // Set type label
            const char* typeLbl = "MGMT";
            if (frameType == 2) {
                typeLbl = "DATA";
            } else if (frameType == 0 && frameSubtype == 8) {
                typeLbl = "BEACN"; // Highlight Beacon frames specifically
            } else if (frameType == 0 && frameSubtype == 4) {
                typeLbl = "PROBE"; // Highlight Probe Requests
            }

            // Try to extract SSID for Beacons and Probes to show device/network name
            if (frameType == 0 && (frameSubtype == 8 || frameSubtype == 4)) {
                int tagOffset = (frameSubtype == 8) ? 36 : 24;
                if (len >= (tagOffset + 2) && payload[tagOffset] == 0) { // Tag 0 is SSID
                    uint8_t ssidLen = payload[tagOffset + 1];
                    int ssidDataOffset = tagOffset + 2;
                    if (ssidLen > 0 && (ssidDataOffset + ssidLen) <= len) {
                        uint8_t copyLen = (ssidLen > 17) ? 17 : ssidLen;
                        
                        // Check if SSID is printable characters
                        bool isPrintable = true;
                        for(int i=0; i<copyLen; i++) {
                            char c = payload[ssidDataOffset + i];
                            if (c < 32 || c > 126) isPrintable = false;
                        }
                        
                        if (isPrintable) {
                            memset(infoStr, 0, sizeof(infoStr));
                            memcpy(infoStr, &payload[ssidDataOffset], copyLen);
                            infoStr[copyLen] = '\0';
                        }
                    }
                }
            }

            // Enqueue packet data to be rendered by the main thread
            enqueuePacket(typeLbl, infoStr, rssi, ch, payload, len);
        }
    }
}

// ----------------------------------------------------------------------------------
// DRAWING & RENDERING FUNCTIONS (Main Thread Context)
// ----------------------------------------------------------------------------------

// Draws the static cyberpunk dashboard boxes and header once at boot
void drawStaticUI() {
    tft.fillScreen(TFT_BLACK);

    // 1. Header Bar
    tft.fillRect(0, 0, 480, 35, 0x10A2); // Dark techno grey-blue
    tft.drawFastHLine(0, 35, 480, TFT_CYAN); // Glowing neon underline
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, 0x10A2);
    tft.drawString("ESP32 CYBER PACKET SNIFFER", 15, 10);

    // Pulsing LIVE indicator dot
    tft.fillCircle(430, 17, 6, TFT_RED);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, 0x10A2);
    tft.drawString("LIVE", 445, 13);

    // 2. Left Box: Metrics Console
    tft.drawRoundRect(5, 42, 185, 273, 8, TFT_CYAN);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString(" SYSTEM METRICS ", 18, 38);

    // 3. Right Box: Live Packet Feed Console
    tft.drawRoundRect(198, 42, 277, 180, 8, 0x5E8B); // Glowing green-gray border
    tft.setTextColor(0x5E8B, TFT_BLACK);
    tft.drawString(" LIVE TRAFFIC MONITOR ", 210, 38);

    // 4. Bottom Right Box: Raw Payload Hex Dump
    tft.drawRoundRect(198, 227, 277, 88, 8, TFT_MAGENTA); // Magenta border for raw data
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.drawString(" RAW PAYLOAD INSPECTOR ", 210, 223);
}

// Render current active scanning channel (flicker-free)
void drawChannelInfo() {
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("SCANNING CHANNEL:", 15, 55);

    tft.setTextSize(2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    char buf[12];
    snprintf(buf, sizeof(buf), "CH %02d [2.4G]", currentChannel);
    tft.drawString(buf, 15, 70);
}

// Renders dynamic system metric counters (flicker-free)
void drawStatsUI() {
    static uint32_t lastTotal = 0;
    static int lastDevices = 0;

    // Redraw only when stats actually change to prevent CPU/SPI overhead
    if (totalPackets == lastTotal && uniqueDeviceCount == lastDevices) {
        return;
    }
    lastTotal = totalPackets;
    lastDevices = uniqueDeviceCount;

    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("TOTAL PACKETS:", 15, 98);

    tft.setTextSize(2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", totalPackets);
    // Print padding spaces to clear previous wider digits
    tft.drawString(strcat(buf, "    "), 15, 112);

    // Sub-packet Type counters
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "MGMT PKTS: %u    ", mgmtPackets);
    tft.drawString(buf, 15, 142);

    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    snprintf(buf, sizeof(buf), "DATA PKTS: %u    ", dataPackets);
    tft.drawString(buf, 15, 165);

    tft.setTextColor(0xB512, TFT_BLACK); // Nice steel-blue/grey color
    snprintf(buf, sizeof(buf), "CTRL PKTS: %u    ", ctrlPackets);
    tft.drawString(buf, 15, 188);

    // Separator line
    tft.drawFastHLine(15, 222, 165, TFT_DARKGREY);

    // Devices metric
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("DEVICES DETECTED:", 15, 232);

    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%d / %d", uniqueDeviceCount, MAX_UNIQUE_DEVICES);
    tft.drawString(strcat(buf, "    "), 15, 248);

    // Device storage capacity warning
    tft.setTextSize(1);
    if (uniqueDeviceCount >= MAX_UNIQUE_DEVICES) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("STORAGE FULL (LIMIT) ", 15, 280);
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("CACHE STATUS: OK    ", 15, 280);
    }
}

// Add a newly popped packet to the feed history and shift old rows down
void addPacketToFeed(const LoggedPacket &pkt) {
    // Shift old records down to make space at index 0
    for (int i = LIVE_FEED_ROWS - 1; i > 0; i--) {
        feedHistory[i] = feedHistory[i - 1];
    }
    feedHistory[0] = pkt;

    if (feedHistoryCount < LIVE_FEED_ROWS) {
        feedHistoryCount++;
    }
}

// Renders the scrolling packet history screen (highly optimized & flicker-free)
void drawFeedUI() {
    tft.setTextSize(1);
    
    for (int i = 0; i < feedHistoryCount; i++) {
        int y_pos = 65 + (i * 32); // 32 pixels height per row
        
        // 1. Draw Packet Tag
        if (strcmp(feedHistory[i].type, "BEACN") == 0) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else if (strcmp(feedHistory[i].type, "PROBE") == 0) {
            tft.setTextColor(0xFBE0, TFT_BLACK); // Orange accent for Probe Request
        } else if (strcmp(feedHistory[i].type, "MGMT") == 0) {
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
        } else if (strcmp(feedHistory[i].type, "DATA") == 0) {
            tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        
        char tagBuf[12];
        snprintf(tagBuf, sizeof(tagBuf), "[%s]", feedHistory[i].type);
        // Overwrite the background to clear previous strings efficiently
        tft.fillRect(205, y_pos - 2, 52, 18, TFT_BLACK);
        tft.drawString(tagBuf, 205, y_pos);

        // 2. Draw MAC address (Source)
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.fillRect(260, y_pos - 2, 114, 18, TFT_BLACK);
        tft.drawString(feedHistory[i].mac, 260, y_pos);

        // 3. Draw Channel
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        char chBuf[6];
        snprintf(chBuf, sizeof(chBuf), "C%02d", feedHistory[i].channel);
        tft.fillRect(380, y_pos - 2, 28, 18, TFT_BLACK);
        tft.drawString(chBuf, 380, y_pos);

        // 4. Draw RSSI (color based on signal strength)
        int rssi = feedHistory[i].rssi;
        if (rssi >= -60) {
            tft.setTextColor(TFT_GREEN, TFT_BLACK); // Strong Signal
        } else if (rssi >= -80) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK); // Medium Signal
        } else {
            tft.setTextColor(TFT_RED, TFT_BLACK); // Weak Signal
        }
        
        char rssiBuf[12];
        snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", rssi);
        tft.fillRect(415, y_pos - 2, 53, 18, TFT_BLACK);
        tft.drawString(rssiBuf, 415, y_pos);
    }
}

// ----------------------------------------------------------------------------------
// BOOT SEQUENCE ANIMATION
// ----------------------------------------------------------------------------------
void typeText(const char* text, int x, int y, int delayMs, uint16_t color) {
    tft.setTextColor(color, TFT_BLACK);
    tft.setCursor(x, y);
    for (int i = 0; i < strlen(text); i++) {
        tft.print(text[i]);
        delay(delayMs);
    }
}

void playBootAnimation() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    
    // 1. Typewriter console logs
    int y = 40;
    typeText("> INIT KERNEL V4.2.9...", 20, y, 20, TFT_GREEN);
    delay(200);
    typeText(" [OK]", tft.getCursorX(), y, 10, TFT_CYAN);
    
    y += 30;
    typeText("> MOUNTING SECURE FS...", 20, y, 15, TFT_GREEN);
    delay(300);
    typeText(" [OK]", tft.getCursorX(), y, 10, TFT_CYAN);
    
    y += 30;
    typeText("> BYPASSING FIRMWARE LOCK...", 20, y, 30, TFT_GREEN);
    delay(400);
    typeText(" [OK]", tft.getCursorX(), y, 10, TFT_CYAN);
    
    y += 30;
    typeText("> ENGAGING PROMISCUOUS RADIO...", 20, y, 25, TFT_GREEN);
    delay(300);
    typeText(" [OK]", tft.getCursorX(), y, 10, TFT_CYAN);
    
    delay(500);

    // 2. Loading Bar
    tft.drawRect(50, 220, 380, 20, 0x5E8B);
    for (int i = 0; i <= 376; i += 8) {
        tft.fillRect(52, 222, i, 16, TFT_MAGENTA);
        delay(15);
    }
    
    delay(300);

    // 3. Glitch Transition
    tft.setTextSize(4);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("SYSTEM READY", 90, 100);
    delay(100);
    
    // Glitch effect: random horizontal lines and inversions
    for (int i = 0; i < 15; i++) {
        tft.invertDisplay(true);
        tft.fillRect(random(0, 480), random(0, 320), random(50, 200), random(5, 20), TFT_WHITE);
        delay(random(10, 40));
        tft.invertDisplay(false);
        delay(random(10, 40));
    }
    
    tft.fillScreen(TFT_BLACK);
}

// ----------------------------------------------------------------------------------
// MAIN SETUP
// ----------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[SYSTEM] Initializing ESP32 Cyber Sniffer...");

    // 1. Initialize TFT Display (LovyanGFX self-contained setup)
    tft.init();
    tft.setRotation(1); // Set display to Landscape Mode (480x320)
    
    // Play the awesome Cyberpunk boot animation
    playBootAnimation();
    
    // Draw initial static screens
    drawStaticUI();
    drawChannelInfo();
    drawStatsUI();

    Serial.println("[SYSTEM] TFT display successfully initialized.");

    // 2. Initialize ESP32 Wi-Fi in Promiscuous Mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&snifferCallback);
    
    // Start sniffing on channel 1
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    lastChannelHopTime = millis();
    
    Serial.println("[SYSTEM] Wi-Fi promiscuous mode active. Sniffing started.");
}

// ----------------------------------------------------------------------------------
// MAIN LOOP
// ----------------------------------------------------------------------------------
void loop() {
    unsigned long currentMillis = millis();

    // 1. Channel Hopping handler
    if (currentMillis - lastChannelHopTime >= CHANNEL_HOP_INTERVAL) {
        lastChannelHopTime = currentMillis;
        
        currentChannel++;
        if (currentChannel > 13) {
            currentChannel = 1;
        }
        
        // Instruct Wi-Fi radio to hop to the next 2.4GHz channel
        esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
        
        // Render updated channel value
        drawChannelInfo();
    }

    // 2. Queue processing loop
    LoggedPacket pkt;
    bool feedUpdated = false;
    
    // Process up to 5 packets per loop iteration to maintain smooth UI & responsive channel hopping
    int processedCount = 0;
    while (dequeuePacket(pkt) && processedCount < 5) {
        addPacketToFeed(pkt);
        feedUpdated = true;
        processedCount++;
    }

    // 3. Render updates if new packets were dequeued
    if (feedUpdated) {
        drawFeedUI();
        
        // Render the raw hex dump of the most recent packet (index 0)
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK); // Classic Matrix Green
        
        for (int row = 0; row < 2; row++) {
            for (int col = 0; col < 8; col++) {
                int byteIdx = (row * 8) + col;
                char hexBuf[4];
                snprintf(hexBuf, sizeof(hexBuf), "%02X ", feedHistory[0].payloadPreview[byteIdx]);
                
                int x_pos = 208 + (col * 32);
                int y_pos = 242 + (row * 24);
                
                tft.fillRect(x_pos, y_pos, 25, 15, TFT_BLACK);
                tft.drawString(hexBuf, x_pos, y_pos);
            }
        }
        
        // Add some "cyber" visual flair at the bottom
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("ENC_LVL: WPA2/3    STATUS: INTERCEPTED", 208, 296);
    }
    
    // 4. Update the system metric display counters
    drawStatsUI();

    // Small delay to prevent running the loop completely open (reduces thermal footprint)
    delay(2);
}

#include "WiFiSniffer.h"

#include <Arduino.h>
#include <WiFi.h>

#include "esp_wifi.h"

struct PktMeta {
    uint8_t mac[6];
    uint16_t len;
    uint32_t ts;
    uint8_t type;
};

constexpr int PKT_BUF = 64;
volatile PktMeta pktBuf[PKT_BUF];
volatile int pktWrite = 0;
volatile int pktCount = 0;

portMUX_TYPE pktMux = portMUX_INITIALIZER_UNLOCKED;

int viewIndex = 0;
int scrollOffset = 0;
const int visibleItems = 6;
bool scanning = true;
bool inDetail = false;
unsigned long lastDraw = 0;
const unsigned long redrawMs = 60;

int currentChannel = 0;

static inline void copyMac(const uint8_t* src, uint8_t* dst) {
    for (int i = 0; i < 6; i++) dst[i] = src[i];
}

static const char* hex2str(uint8_t b, char* out) {
    const char* hex = "0123456789ABCDEF";
    out[0] = hex[(b >> 4) & 0x0F];
    out[1] = hex[b & 0xF];
    out[2] = 0;
    return out;
}

void hopChannel() {
    static uint8_t channels[] = {1, 6, 11, 2, 7, 3, 8, 4, 9, 5, 10};
    static int channelIndex = 0;

    channelIndex = (channelIndex + 1) % (sizeof(channels) / sizeof(channels[0]));
    currentChannel = channels[channelIndex];

    esp_err_t ret = esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    if (ret != ESP_OK) {
        Serial.printf("Error setting channel %d: %d\n", currentChannel, ret);
    }
}

void IRAM_ATTR wifiPromiscCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    wifi_promiscuous_pkt_t* p = (wifi_promiscuous_pkt_t*)buf;
    if (!p || p->rx_ctrl.sig_len == 0) return;

    const uint8_t* payload = p->payload;
    int len = p->rx_ctrl.sig_len;

    // Minimum length check untuk frame WiFi
    if (len < 24) return;

    // Parse frame control field
    uint8_t frame_control = payload[0];
    uint8_t frame_type = (frame_control & 0x0C) >> 2;     // Bits 2-3: Type
    uint8_t frame_subtype = (frame_control & 0xF0) >> 4;  // Bits 4-7: Subtype

    const uint8_t* mac_addr = NULL;

    // Deteksi MAC address berdasarkan frame type
    switch (frame_type) {
        case 0x00:  // Management frames
            if (len >= 24) {
                mac_addr = payload + 10;  // Source Address (SA) di management frames
            }
            break;

        case 0x01:  // Control frames - biasanya tidak ada MAC source
            // Skip control frames atau handle khusus
            return;

        case 0x02:  // Data frames
            if (len >= 24) {
                // Cek ToDS/FromDS flags untuk menentukan posisi MAC
                uint8_t flags = payload[1];
                bool toDS = (flags & 0x01) != 0;
                bool fromDS = (flags & 0x02) != 0;

                if (!toDS && !fromDS) {
                    mac_addr = payload + 10;  // SA untuk IBSS
                } else if (toDS && !fromDS) {
                    mac_addr = payload + 10;  // SA
                } else if (!toDS && fromDS) {
                    mac_addr = payload + 4;   // SA
                } else {                      // toDS && fromDS
                    mac_addr = payload + 16;  // SA di WDS frames
                }
            }
            break;
    }

    if (!mac_addr) return;

    // Copy MAC address dan data packet
    portENTER_CRITICAL_ISR(&pktMux);
    int w = pktWrite;

    // Copy MAC address
    copyMac(mac_addr, (uint8_t*)pktBuf[w].mac);

    // Simpan data packet
    pktBuf[w].len = len;
    pktBuf[w].ts = millis();
    pktBuf[w].type = frame_control;  // Simpan seluruh frame control field

    // Update buffer pointers
    pktWrite = (w + 1) % PKT_BUF;
    if (pktCount < PKT_BUF) {
        pktCount++;
    }

    portEXIT_CRITICAL_ISR(&pktMux);
}

void drawList() {
    display.clearBuffer();

    // Header dengan informasi channel dan status
    display.setFont(u8g2_font_6x10_tf);
    char header[32];
    snprintf(header, sizeof(header), "CH:%d %s", currentChannel, scanning ? "SNIFF" : "PAUSE");
    display.drawStr(0, 10, header);

    // Packet count info
    int localCount;
    portENTER_CRITICAL(&pktMux);
    localCount = pktCount;
    portEXIT_CRITICAL(&pktMux);

    char countStr[16];
    snprintf(countStr, sizeof(countStr), "PKTS:%d", localCount);
    display.drawStr(70, 10, countStr);

    // Separator line
    display.drawHLine(0, 12, 128);

    if (localCount == 0) {
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(10, 35, "No packets");
        display.setFont(u8g2_font_5x8_tr);
        display.drawStr(10, 50, "Scanning...");
    } else {
        // Compute start index of oldest stored packet
        int localWrite;
        portENTER_CRITICAL(&pktMux);
        localWrite = pktWrite;
        portEXIT_CRITICAL(&pktMux);

        int start = (localWrite - localCount + PKT_BUF) % PKT_BUF;

        // Show visible items with better formatting
        for (int i = 0; i < visibleItems; i++) {
            int idx = scrollOffset + i;
            if (idx >= localCount) break;

            int bufIndex = (start + idx) % PKT_BUF;

            // Format MAC dengan warna berbeda untuk jenis frame
            char macStr[18];
            snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X",
                     pktBuf[bufIndex].mac[0], pktBuf[bufIndex].mac[1], pktBuf[bufIndex].mac[2],
                     pktBuf[bufIndex].mac[3], pktBuf[bufIndex].mac[4]);

            char line[32];
            snprintf(line, sizeof(line), "%s %3u", macStr, (unsigned)pktBuf[bufIndex].len);

            int y = 24 + i * 9;

            if (idx == viewIndex) {
                display.drawBox(0, y - 8, 128, 9);
                display.setDrawColor(0);
            } else {
                display.setDrawColor(1);
            }

            // Tampilkan icon berdasarkan packet type
            char typeIcon[2] = "?";
            uint8_t frame_type = (pktBuf[bufIndex].type & 0x0C) >> 2;
            switch (frame_type) {
                case 0:
                    typeIcon[0] = 'M';
                    break;  // Management
                case 1:
                    typeIcon[0] = 'C';
                    break;  // Control
                case 2:
                    typeIcon[0] = 'D';
                    break;  // Data
            }

            display.setFont(u8g2_font_5x8_tr);
            display.drawStr(2, y, typeIcon);
            display.drawStr(10, y, line);
            display.setDrawColor(1);
        }
    }
    display.sendBuffer();
}

void drawDetail(int absoluteIndex) {
    display.clearBuffer();

    // Header
    display.setFont(u8g2_font_6x10_tf);
    display.drawStr(0, 10, "Packet Details");

    // Packet counter
    char counter[20];
    snprintf(counter, sizeof(counter), "[%d/%d]", absoluteIndex + 1, pktCount);
    display.drawStr(90, 10, counter);

    display.drawHLine(0, 12, 128);

    int localWrite, localCount;
    portENTER_CRITICAL(&pktMux);
    localWrite = pktWrite;
    localCount = pktCount;
    portEXIT_CRITICAL(&pktMux);

    if (localCount == 0) {
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(10, 35, "No data");
        display.sendBuffer();
        return;
    }

    int start = (localWrite - localCount + PKT_BUF) % PKT_BUF;
    int bufIndex = (start + absoluteIndex) % PKT_BUF;

    char info[40];
    display.setFont(u8g2_font_5x8_tr);

    // MAC Address
    snprintf(info, sizeof(info), "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             pktBuf[bufIndex].mac[0], pktBuf[bufIndex].mac[1], pktBuf[bufIndex].mac[2],
             pktBuf[bufIndex].mac[3], pktBuf[bufIndex].mac[4], pktBuf[bufIndex].mac[5]);
    display.drawStr(0, 24, info);

    // Packet Info
    snprintf(info, sizeof(info), "Size: %d bytes", pktBuf[bufIndex].len);
    display.drawStr(0, 34, info);

    // Frame Type
    uint8_t frame_type = (pktBuf[bufIndex].type & 0x0C) >> 2;
    uint8_t frame_subtype = (pktBuf[bufIndex].type & 0xF0) >> 4;
    const char* typeStr = "Unknown";
    switch (frame_type) {
        case 0:
            typeStr = "Management";
            break;
        case 1:
            typeStr = "Control";
            break;
        case 2:
            typeStr = "Data";
            break;
    }
    snprintf(info, sizeof(info), "Type: %s", typeStr);
    display.drawStr(0, 44, info);

    // Timestamp
    unsigned long timeAgo = millis() - pktBuf[bufIndex].ts;
    snprintf(info, sizeof(info), "Age: %lu ms", timeAgo);
    display.drawStr(0, 54, info);

    // Footer
    display.drawStr(0, 63, "OK:Back  Up/Down:Navigate");

    display.sendBuffer();
}

void onUp() {
    if (inDetail) {
        // move detail selection up
        if (viewIndex > 0) viewIndex--;
    } else {
        if (viewIndex > 0) viewIndex--;
        if (viewIndex < scrollOffset) scrollOffset = viewIndex;
    }
}

void onDown() {
    int localCount;
    portENTER_CRITICAL(&pktMux);
    localCount = pktCount;
    portEXIT_CRITICAL(&pktMux);

    if (inDetail) {
        if (viewIndex < localCount - 1) viewIndex++;
    } else {
        if (viewIndex < localCount - 1) viewIndex++;
        if (viewIndex >= scrollOffset + visibleItems) scrollOffset = viewIndex - visibleItems + 1;
    }
}

void onOK() {
    // toggle scanning <-> detail mode
    if (scanning) {
        // stop scanning and enter detail mode at current selection
        esp_wifi_set_promiscuous(false);
        scanning = false;
        inDetail = true;
        // viewIndex already points to selection (relative oldest=0)
    } else {
        // if in detail: resume scanning
        if (inDetail) {
            inDetail = false;
            // resume sniffing
            esp_wifi_set_promiscuous(true);
            scanning = true;
        } else {
            // if not scanning, start scanning
            esp_wifi_set_promiscuous(true);
            scanning = true;
        }
    }
}

unsigned long lastHop = 0;
const unsigned long hopInterval = 150;  // ms

void doUpdate(void* pvParameter) {
    for (;;) {
        appHeartBeat();

        if (!scanning && !inDetail) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        unsigned long now = millis();

        // channel hopping tiap hopInterval
        if (now - lastHop > hopInterval) {
            hopChannel();
            lastHop = now;
        }

        if (now - lastDraw >= redrawMs) {
            if (!inDetail)
                drawList();
            else
                drawDetail(viewIndex);
            lastDraw = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

TaskHandle_t sniffTaskHandle = NULL;

void stopSniffing() {
    // 1. Hentikan promiscuous mode pertama
    esp_wifi_set_promiscuous(false);

    // 2. Hapus callback
    esp_wifi_set_promiscuous_rx_cb(NULL);

    // 3. Reset state variables
    scanning = false;
    inDetail = false;

    // 4. Clear packet buffer
    portENTER_CRITICAL(&pktMux);
    pktCount = 0;
    pktWrite = 0;
    viewIndex = 0;
    scrollOffset = 0;
    portEXIT_CRITICAL(&pktMux);

    // 5. Beri waktu untuk proses cleanup
    vTaskDelay(pdMS_TO_TICKS(100));

    // 6. Reset WiFi mode
    WiFi.mode(WIFI_OFF);
}

void onOKLongPress() {
    stopSniffing();
    if (sniffTaskHandle != NULL) {
        vTaskDelete(sniffTaskHandle);
        sniffTaskHandle = NULL;
    }

    drawMenu();
}

void startSniffing() {
    // Initialize WiFi pertama
    WiFi.mode(WIFI_MODE_NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);

    // Set configuration
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&wifiPromiscCb);
    esp_wifi_set_promiscuous(true);

    scanning = true;

    // Setup buttons dan task
    btnUp.attachClick(onUp);
    btnDown.attachClick(onDown);
    btnOK.attachClick(onOK);
    btnOK.attachLongPressStart(onOKLongPress);

    xTaskCreate(doUpdate, "SniffTask", 4096, NULL, 1, &sniffTaskHandle);
}
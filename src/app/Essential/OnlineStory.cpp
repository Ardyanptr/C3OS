#include "OnlineStory.h"

#include <vector>

#include "system16/wrapper.h"

static String raw_OnlineStory;
static std::vector<String> wrapped_OnlineStory;
static int scrollY_OnlineStory = 0;
static const int lineHeight_OnlineStory = 12;
static const int visibleLines_OnlineStory = 5;
static bool exitOnlineStory = false;

static int homeCurrentStorySelected_OnlineStory = 1;
static int homeCursorStory_OnlineStory = 22;

// Database cerita lokal (pengganti fetchStory)
static const char* getLocalContent(int id) {
    switch (id) {
        case 1:
            return "JEJAK DI LANTAI\n\n"
                   "Setiap pagi, petugas kebersihan memastikan lantai koridor gedung kantor saya bersih. "
                   "Namun, selama beberapa hari terakhir, selalu muncul jejak kaki basah tepat di depan pintu ruangan saya.\n\n"
                   "Jejak itu tidak pernah mengarah ke mana pun, hanya muncul lalu berhenti. Ukurannya tidak wajar, "
                   "dan jarak langkahnya terlalu panjang untuk manusia.\n\n"
                   "Suatu malam, saat saya lembur sendirian, suara tetesan air terdengar. "
                   "Ketika pintu dibuka, lantai kering—kecuali jejak kaki itu kini berada di dalam ruangan, "
                   "tepat di belakang kursi kerja saya.";

        case 2:
            return "REKAMAN SUARA\n\n"
                   "Saya membeli perekam suara lama dari pasar loak untuk keperluan dokumentasi. "
                   "Tidak ada yang aneh hingga saya memutar ulang hasil rekaman.\n\n"
                   "Di setiap rekaman, setelah suara saya berhenti, terdengar napas lain yang berat dan tidak beraturan. "
                   "Napas itu selalu muncul tepat tiga detik setelah keheningan.\n\n"
                   "Pada rekaman terakhir, napas tersebut berubah menjadi bisikan yang menyebut nama saya, "
                   "dengan pengucapan yang salah, seolah baru belajar mengenal saya.";

        case 3:
            return "PANTULAN\n\n"
                   "Cermin kamar mandi saya mulai menunjukkan keanehan. "
                   "Pantulan saya selalu bergerak setengah detik lebih lambat dari gerakan asli.\n\n"
                   "Awalnya saya mengabaikannya, hingga suatu hari pantulan itu menatap saya terlebih dahulu "
                   "sebelum mengikuti gerakan saya.\n\n"
                   "Ketika saya menutup mata, pantulan itu masih terlihat. "
                   "Ia tersenyum, sementara wajah saya tetap diam tanpa ekspresi.";

        case 4:
            return "KAMAR TERAKHIR\n\n"
                   "Di ujung koridor rumah sakit tempat saya dirawat, terdapat satu kamar tanpa nomor yang selalu terkunci. "
                   "Tidak ada perawat yang mau menjelaskan keberadaannya.\n\n"
                   "Setiap pukul tiga pagi, bel dari kamar itu selalu berbunyi satu kali, meskipun tidak terhubung ke sistem.\n\n"
                   "Pada malam terakhir perawatan saya, pintu kamar itu terbuka. "
                   "Di atasnya tertempel label dengan nama lengkap saya.";

        default:
            return "No story available.";
    }
}

static void drawStory(const std::vector<String>& lines) {
    display.clearBuffer();
    display.setFont(u8g2_font_5x8_tr);
    for (int i = 0; i < visibleLines_OnlineStory; i++) {
        int idx = scrollY_OnlineStory + i;
        if (idx < 0 || idx >= (int)lines.size()) break;
        display.drawStr(0, (i + 1) * lineHeight_OnlineStory, lines[idx].c_str());
    }

    if (!lines.empty()) {
        int total = (int)lines.size();
        int barH = std::max(4, (visibleLines_OnlineStory * 60) / std::max(total, visibleLines_OnlineStory));
        int barY = map(scrollY_OnlineStory, 0, std::max(0, total - visibleLines_OnlineStory), 0, 60 - barH);
        display.drawFrame(124, 0, 4, 64);
        display.drawBox(125, barY, 2, barH);
    }
    display.sendBuffer();
}

static void onUp_OnlineStory() {
    if (scrollY_OnlineStory > 0) scrollY_OnlineStory--;
}

static void onDown_OnlineStory() {
    if (!wrapped_OnlineStory.empty()) {
        int maxScroll = (int)wrapped_OnlineStory.size() - visibleLines_OnlineStory;
        if (scrollY_OnlineStory < std::max(0, maxScroll)) scrollY_OnlineStory++;
    }
}

static void onOK_OnlineStory() {
    exitOnlineStory = true;
    drawMenu();
}

static void handleOnHome();
static void runMainStory();

static void handleHomeUp_OnlineStory() {
    if (homeCurrentStorySelected_OnlineStory > 1) {
        homeCurrentStorySelected_OnlineStory--;
        homeCursorStory_OnlineStory -= 10;
    }
    handleOnHome();
}

static void handleHomeDown_OnlineStory() {
    if (homeCurrentStorySelected_OnlineStory < 4) {
        homeCurrentStorySelected_OnlineStory++;
        homeCursorStory_OnlineStory += 10;
    }
    handleOnHome();
}

static void handleHomeOK_OnlineStory() { runMainStory(); }

static void handleOnHome() {
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tr);
    display.drawStr(1, 9, "Library Story");
    display.drawLine(1, 12, 126, 12);

    display.setFont(u8g2_font_5x8_tr);
    display.drawStr(8, 22, "The Red Dot");
    display.drawStr(8, 32, "The Late Night Bus");
    display.drawStr(8, 42, "Smile");
    display.drawStr(8, 52, "Alone");

    display.drawStr(0, homeCursorStory_OnlineStory, ">");
    display.sendBuffer();

    btnUp.attachClick(handleHomeUp_OnlineStory);
    btnDown.attachClick(handleHomeDown_OnlineStory);
    btnOK.attachClick(handleHomeOK_OnlineStory);
    btnOK.attachLongPressStart([]() { drawMenu(); });
}

void runOnlineStory() {
    handleOnHome();
}

static void detachAllOnlineStoryButtons() {
    btnUp.reset();
    btnDown.reset();
    btnOK.reset();
}

static void runMainStory() {
    exitOnlineStory = false;
    scrollY_OnlineStory = 0;

    detachAllOnlineStoryButtons();
    btnUp.attachClick(onUp_OnlineStory);
    btnDown.attachClick(onDown_OnlineStory);
    btnOK.attachClick(onOK_OnlineStory);

    // Ambil dari variabel lokal, bukan WiFi
    raw_OnlineStory = getLocalContent(homeCurrentStorySelected_OnlineStory);
    wrapped_OnlineStory = wrapText(display, raw_OnlineStory, 120);

    uint32_t lastDraw = 0;
    while (!exitOnlineStory) {
        appHeartBeat();

        btnUp.tick();
        btnDown.tick();
        btnOK.tick();

        uint32_t now = millis();
        if (now - lastDraw >= 33) {
            drawStory(wrapped_OnlineStory);
            lastDraw = now;
        }
        delay(1);
    }

    detachAllOnlineStoryButtons();
    handleOnHome();
}
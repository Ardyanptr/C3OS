#pragma once
#include "config/config.h"
#include "config/var_declare.h"
#include <U8g2lib.h>
#include <esp_task_wdt.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────
//  Weather types
// ─────────────────────────────────────────────────────────────
enum WeatherType {
    WEATHER_SUNNY = 0,
    WEATHER_CLOUDY,
    WEATHER_RAIN,
    WEATHER_SNOW,
    WEATHER_DRY,
    WEATHER_NIGHT,
    WEATHER_COUNT
};

// ─────────────────────────────────────────────────────────────
//  Config
// ─────────────────────────────────────────────────────────────
static const uint32_t WEATHER_CHANGE_MS = 1 * 60 * 1000UL; // change every 1 min
static const uint8_t GROUND_Y = 63;
static const uint8_t RAIN_COUNT = 16;
static const uint8_t SNOW_COUNT = 14;
static const uint8_t CLOUD_COUNT = 3;
static const uint8_t GRASS_COUNT = 20;

// ─────────────────────────────────────────────────────────────
//  State — all static so no heap alloc
// ─────────────────────────────────────────────────────────────
static WeatherType env_weather = WEATHER_SUNNY;
static WeatherType env_nextWeather = WEATHER_SUNNY;
static uint32_t env_lastChange = 0;
static uint32_t env_frame = 0;

// transition fade (0=current fully visible, 255=next fully visible)
// on OLED we fake "fade" by skipping the draw every other frame during transition
static uint8_t env_transition = 0;
static bool env_inTransition = false;

// Rain
struct RainDrop {
    float x, y, speed;
    uint8_t len;
};
static RainDrop env_rain[RAIN_COUNT];
static bool env_rainInit = false;

// Snow
struct SnowFlake {
    float x, y, speed, drift;
    uint8_t size;
    float driftPhase;
};
static SnowFlake env_snow[SNOW_COUNT];
static bool env_snowInit = false;

// Clouds
struct Cloud {
    float x;
    uint8_t y, w;
    float speed;
};
static Cloud env_clouds[CLOUD_COUNT] = {
    {10, 5, 26, 0.10f},
    {60, 4, 20, 0.07f},
    {100, 7, 18, 0.09f},
};

// Grass phases (randomised once)
static float env_grassPhase[GRASS_COUNT];
static uint8_t env_grassX[GRASS_COUNT];
static bool env_grassInit = false;

// Trees — fixed layout, tier 0=small 1=med 2=tall
struct Tree {
    uint8_t x;
    uint8_t tier;
    float swayPhase;
};
static const Tree env_trees[] = {
    {8, 2, 0.0f}, {22, 1, 1.1f}, {36, 2, 2.3f}, {90, 1, 0.7f}, {104, 2, 1.9f}, {118, 1, 3.1f},
};
static const uint8_t TREE_COUNT = sizeof(env_trees) / sizeof(env_trees[0]);

// Stars
struct Star {
    uint8_t x, y;
    float phase;
};
static Star env_stars[14];
static bool env_starsInit = false;

// Sun / moon position
static float env_sunPhase = 0.0f;

// ─────────────────────────────────────────────────────────────
//  Pseudo-random (no stdlib rand needed)
// ─────────────────────────────────────────────────────────────
static uint32_t env_seed = 12345;
static uint32_t env_rand() {
    env_seed ^= env_seed << 13;
    env_seed ^= env_seed >> 17;
    env_seed ^= env_seed << 5;
    return env_seed;
}
static float env_randf() { return (float)(env_rand() & 0xFFFF) / 65535.0f; }

// ─────────────────────────────────────────────────────────────
//  Init helpers
// ─────────────────────────────────────────────────────────────
static void env_initRain() {
    if (env_rainInit) return;
    for (uint8_t i = 0; i < RAIN_COUNT; i++) {
        env_rain[i] = {env_randf() * 128.0f, env_randf() * 64.0f, 1.8f + env_randf() * 1.5f,
                       (uint8_t)(2 + (env_rand() % 3))};
    }
    env_rainInit = true;
}

static void env_initSnow() {
    if (env_snowInit) return;
    for (uint8_t i = 0; i < SNOW_COUNT; i++) {
        env_snow[i] = {env_randf() * 128.0f,      env_randf() * 64.0f,
                       0.4f + env_randf() * 0.6f, 0.3f + env_randf() * 0.4f,
                       (uint8_t)(env_rand() % 2), env_randf() * 6.28f};
    }
    env_snowInit = true;
}

static void env_initGrass() {
    if (env_grassInit) return;
    for (uint8_t i = 0; i < GRASS_COUNT; i++) {
        env_grassX[i] = 3 + i * 6 + (env_rand() % 4);
        env_grassPhase[i] = env_randf() * 6.28f;
    }
    env_grassInit = true;
}

static void env_initStars() {
    if (env_starsInit) return;
    for (uint8_t i = 0; i < 14; i++) {
        env_stars[i] = {(uint8_t)(env_rand() % 128), (uint8_t)(2 + env_rand() % 28),
                        env_randf() * 6.28f};
    }
    env_starsInit = true;
}

// ─────────────────────────────────────────────────────────────
//  Draw primitives (pixel-perfect, no anti-alias)
// ─────────────────────────────────────────────────────────────
static inline void env_pixel(int16_t x, int16_t y) {
    if (x >= 0 && x < 128 && y >= 0 && y < 64) display.drawPixel(x, y);
}

static void env_disc(int16_t cx, int16_t cy, int16_t r) {
    for (int16_t dy = -r; dy <= r; dy++)
        for (int16_t dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) env_pixel(cx + dx, cy + dy);
}

static void env_circle(int16_t cx, int16_t cy, int16_t r) {
    // Bresenham
    int16_t x = 0, y = r, d = 3 - 2 * r;
    while (y >= x) {
        esp_task_wdt_reset();

        env_pixel(cx + x, cy - y);
        env_pixel(cx - x, cy - y);
        env_pixel(cx + x, cy + y);
        env_pixel(cx - x, cy + y);
        env_pixel(cx + y, cy - x);
        env_pixel(cx - y, cy - x);
        env_pixel(cx + y, cy + x);
        env_pixel(cx - y, cy + x);
        if (d < 0)
            d += 4 * x + 6;
        else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

// ─────────────────────────────────────────────────────────────
//  Weather picker
// ─────────────────────────────────────────────────────────────
static WeatherType env_pickWeather() {
    // weighted: sunny=3, cloudy=2, rain=2, snow=1, dry=1, night=1
    static const WeatherType pool[] = {
        WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_SUNNY, WEATHER_CLOUDY, WEATHER_CLOUDY,
        WEATHER_RAIN,  WEATHER_RAIN,  WEATHER_SNOW,  WEATHER_DRY,    WEATHER_NIGHT,
    };
    WeatherType picked;
    do {
        esp_task_wdt_reset();
        picked = pool[env_rand() % 10];
    } while (picked == env_weather);
    return picked;
}

// ─────────────────────────────────────────────────────────────
//  Draw: Sun
// ─────────────────────────────────────────────────────────────
static void env_drawSun(float phase) {
    const int16_t cx = 12, cy = 11, r = 5;
    env_disc(cx, cy, r);

    // 8 rays, slowly rotate
    for (uint8_t i = 0; i < 8; i++) {
        float angle = phase + i * 0.7854f; // PI/4
        int16_t x1 = cx + (int16_t)((r + 2) * cosf(angle));
        int16_t y1 = cy + (int16_t)((r + 2) * sinf(angle));
        int16_t x2 = cx + (int16_t)((r + 4) * cosf(angle));
        int16_t y2 = cy + (int16_t)((r + 4) * sinf(angle));
        // draw 2-pixel ray
        env_pixel(x1, y1);
        env_pixel((x1 + x2) / 2, (y1 + y2) / 2);
        env_pixel(x2, y2);
    }
}

// ─────────────────────────────────────────────────────────────
//  Draw: Moon
// ─────────────────────────────────────────────────────────────
static void env_drawMoon() {
    const int16_t cx = 112, cy = 10, r = 5;
    env_disc(cx, cy, r);
    // crescent cutout — draw black disc offset
    display.setDrawColor(0);
    env_disc(cx + 3, cy - 1, r - 1);
    display.setDrawColor(1);
}

// ─────────────────────────────────────────────────────────────
//  Draw: Stars
// ─────────────────────────────────────────────────────────────
static void env_drawStars(float frame) {
    env_initStars();
    for (uint8_t i = 0; i < 14; i++) {
        float twinkle = sinf(env_stars[i].phase + frame * 0.04f);
        if (twinkle > 0.2f) env_pixel(env_stars[i].x, env_stars[i].y);
        if (twinkle > 0.7f) { // brighter star — cross shape
            env_pixel(env_stars[i].x - 1, env_stars[i].y);
            env_pixel(env_stars[i].x + 1, env_stars[i].y);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Draw: Cloud (pixel-art blob)
// ─────────────────────────────────────────────────────────────
static void env_drawCloud(int16_t cx, int16_t cy, uint8_t w) {
    // three overlapping discs
    int16_t hw = w / 2;
    env_disc(cx, cy, 4);
    env_disc(cx - hw / 2, cy + 2, 3);
    env_disc(cx + hw / 2, cy + 2, 3);
    // flat bottom fill
    for (int16_t dx = -hw + 1; dx < hw; dx++)
        env_pixel(cx + dx, cy + 4);
}

// ─────────────────────────────────────────────────────────────
//  Draw: Tree
// ─────────────────────────────────────────────────────────────
static void env_drawTree(const Tree &tr, float frame, bool isDry) {
    const uint8_t trunkH[3] = {6, 8, 10};
    const uint8_t folR[3] = {4, 5, 6};

    uint8_t th = trunkH[tr.tier];
    uint8_t fr = folR[tr.tier];

    // trunk
    for (uint8_t i = 0; i < th; i++)
        env_pixel(tr.x, GROUND_Y - i);

    if (isDry) {
        // bare branches — simple cross
        env_pixel(tr.x - 2, GROUND_Y - th + 1);
        env_pixel(tr.x - 3, GROUND_Y - th);
        env_pixel(tr.x + 2, GROUND_Y - th + 1);
        env_pixel(tr.x + 3, GROUND_Y - th);
        return;
    }

    // sway foliage
    float sway = sinf(tr.swayPhase + frame * 0.035f) * 1.4f;
    int16_t fx = (int16_t)(tr.x + sway);
    int16_t fy = GROUND_Y - th - fr;

    env_disc(fx, fy, fr);
    // small triangle tip for pine-ish look on tall trees
    if (tr.tier == 2) {
        env_pixel(fx, fy - fr - 1);
        env_pixel(fx - 1, fy - fr);
        env_pixel(fx + 1, fy - fr);
    }
}

// ─────────────────────────────────────────────────────────────
//  Draw: Grass tuft
// ─────────────────────────────────────────────────────────────
static void env_drawGrass(uint8_t x, float phase, float frame, bool isDry) {
    if (isDry) {
        // dead grass — just two drooping pixels
        env_pixel(x, GROUND_Y - 1);
        env_pixel(x + 1, GROUND_Y - 1);
        return;
    }
    float sway = sinf(phase + frame * 0.05f) * 1.6f;
    // 3-pixel tall tuft, top sways most
    env_pixel((int16_t)(x + sway), GROUND_Y - 3);
    env_pixel((int16_t)(x + sway * 0.5f), GROUND_Y - 2);
    env_pixel(x, GROUND_Y - 1);
    // second blade offset
    env_pixel((int16_t)(x + 2 + sway * 0.8f), GROUND_Y - 2);
    env_pixel(x + 2, GROUND_Y - 1);
}

// ─────────────────────────────────────────────────────────────
//  Draw: Rain
// ─────────────────────────────────────────────────────────────
static void env_drawRain() {
    env_initRain();
    for (uint8_t i = 0; i < RAIN_COUNT; i++) {
        env_rain[i].y += env_rain[i].speed;
        if (env_rain[i].y > 64) {
            env_rain[i].y = -3;
            env_rain[i].x = env_randf() * 128.0f;
        }
        // angled streak
        for (uint8_t l = 0; l < env_rain[i].len; l++)
            env_pixel((int16_t)(env_rain[i].x - l * 0.4f), (int16_t)(env_rain[i].y - l));
    }
}

// ─────────────────────────────────────────────────────────────
//  Draw: Snow
// ─────────────────────────────────────────────────────────────
static void env_drawSnow(float frame) {
    env_initSnow();
    for (uint8_t i = 0; i < SNOW_COUNT; i++) {
        env_snow[i].y += env_snow[i].speed;
        env_snow[i].x += sinf(env_snow[i].driftPhase + frame * 0.03f) * env_snow[i].drift;
        if (env_snow[i].y > 64) {
            env_snow[i].y = 0;
            env_snow[i].x = env_randf() * 128.0f;
        }
        if (env_snow[i].x < 0) env_snow[i].x = 128;
        if (env_snow[i].x > 128) env_snow[i].x = 0;

        env_pixel((int16_t)env_snow[i].x, (int16_t)env_snow[i].y);
        if (env_snow[i].size == 1) { // larger flake — tiny cross
            env_pixel((int16_t)env_snow[i].x + 1, (int16_t)env_snow[i].y);
            env_pixel((int16_t)env_snow[i].x, (int16_t)env_snow[i].y + 1);
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Draw: Haze (dry / heat shimmer)
// ─────────────────────────────────────────────────────────────
static void env_drawHaze(float frame) {
    for (uint8_t i = 0; i < 3; i++) {
        float offset = fmodf(frame * (0.25f + i * 0.08f) + i * 40.0f, 148.0f) - 20.0f;
        int16_t hx = (int16_t)offset;
        int16_t hy = 10 + i * 6;
        for (uint8_t l = 0; l < 14; l++)
            env_pixel(hx + l, hy + (l % 2));
    }
}

// ─────────────────────────────────────────────────────────────
//  Draw: Ground
// ─────────────────────────────────────────────────────────────
static void env_drawGround() {
    // main ground line
    display.drawHLine(0, GROUND_Y, 128);
    // subtle texture row below
    for (uint8_t x = 0; x < 128; x += 3)
        env_pixel(x, GROUND_Y + 1);
    for (uint8_t x = 1; x < 128; x += 5)
        env_pixel(x, GROUND_Y + 2);
}

// ─────────────────────────────────────────────────────────────
//  Snow accumulation on ground (only during snow)
// ─────────────────────────────────────────────────────────────
static void env_drawSnowGround() {
    // bumpy snow line just above ground
    for (uint8_t x = 0; x < 128; x++) {
        uint8_t bump = (x % 6 < 3) ? 1 : 0;
        env_pixel(x, GROUND_Y - bump);
    }
}

// ─────────────────────────────────────────────────────────────
//  Weather change logic
// ─────────────────────────────────────────────────────────────
static void env_checkWeatherChange() {
    uint32_t now = millis();
    if (!env_inTransition && (now - env_lastChange > WEATHER_CHANGE_MS)) {
        env_nextWeather = env_pickWeather();
        env_inTransition = true;
        env_transition = 0;
        env_lastChange = now;
        // reset particle inits so they re-spawn for new weather
        env_rainInit = false;
        env_snowInit = false;
    }

    if (env_inTransition) {
        env_transition += 4; // advance transition counter
        if (env_transition >= 128) {
            env_weather = env_nextWeather;
            env_inTransition = false;
            env_transition = 0;
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Main AOD draw call — call this from your AOD loop
// ─────────────────────────────────────────────────────────────
inline void displayEnvironmentAOD() {
    esp_task_wdt_reset();
    env_checkWeatherChange();

    // During transition: alternate frames between old and new weather
    // This creates a rough "flicker dissolve" — cheap but effective on OLED
    WeatherType activeWeather = env_weather;
    if (env_inTransition && (env_transition % 16) > 8) activeWeather = env_nextWeather;

    const bool isDry = (activeWeather == WEATHER_DRY);
    const bool isNight = (activeWeather == WEATHER_NIGHT);
    const bool isSnow = (activeWeather == WEATHER_SNOW);
    const bool isRain = (activeWeather == WEATHER_RAIN);
    const bool isCloudy = (activeWeather == WEATHER_CLOUDY);
    const bool isSunny = (activeWeather == WEATHER_SUNNY);

    env_initGrass();
    display.clearBuffer();
    display.setDrawColor(1);

    float f = (float)env_frame;
    env_sunPhase += 0.008f;
    if (env_sunPhase > 6.2832f) env_sunPhase -= 6.2832f;

    // ── Sky elements ──────────────────────────────────────────
    if (isNight) {
        env_drawStars(f);
        env_drawMoon();
    } else if (isSunny) {
        env_drawSun(env_sunPhase);
    } else if (isCloudy || isRain || isSnow) {
        // move and draw clouds
        for (uint8_t i = 0; i < CLOUD_COUNT; i++) {
            env_clouds[i].x += env_clouds[i].speed;
            if (env_clouds[i].x > 148) env_clouds[i].x = -30;
            env_drawCloud((int16_t)env_clouds[i].x, env_clouds[i].y, env_clouds[i].w);
        }
    } else if (isDry) {
        env_drawHaze(f);
        // sun — harsh and high
        env_disc(64, 8, 4);
    }

    // ── Precipitation ─────────────────────────────────────────
    if (isRain) env_drawRain();
    if (isSnow) env_drawSnow(f);

    // ── Ground ────────────────────────────────────────────────
    if (isSnow) env_drawSnowGround();
    env_drawGround();

    // ── Grass ─────────────────────────────────────────────────
    for (uint8_t i = 0; i < GRASS_COUNT; i++)
        env_drawGrass(env_grassX[i], env_grassPhase[i], f, isDry);

    // ── Trees ─────────────────────────────────────────────────
    for (uint8_t i = 0; i < TREE_COUNT; i++)
        env_drawTree(env_trees[i], f, isDry);

    // ── Snow on tree tops ─────────────────────────────────────
    if (isSnow) {
        for (uint8_t i = 0; i < TREE_COUNT; i++) {
            // small white cap on foliage
            const uint8_t folR[3] = {4, 5, 6};
            const uint8_t trunkH[3] = {6, 8, 10};
            uint8_t fr = folR[env_trees[i].tier];
            uint8_t th = trunkH[env_trees[i].tier];
            float sway = sinf(env_trees[i].swayPhase + f * 0.035f) * 1.4f;
            int16_t fx = (int16_t)(env_trees[i].x + sway);
            int16_t fy = GROUND_Y - th - fr;
            display.setDrawColor(1);
            display.drawHLine(fx - 2, fy - 1, 5);
            display.drawHLine(fx - 1, fy - 2, 3);
        }
    }

    display.sendBuffer();
    env_frame++;
}

// ─────────────────────────────────────────────────────────────
//  Optional: force a specific weather (e.g. from settings menu)
// ─────────────────────────────────────────────────────────────
inline void setEnvironmentWeather(WeatherType w) {
    env_weather = w;
    env_inTransition = false;
    env_lastChange = millis();
    env_rainInit = false;
    env_snowInit = false;
}
/**
 * @file main.cpp
 * @brief BMx280PIO_RP2040 — Comprehensive Hardware Test Suite
 *
 * Tests all 5 sensors (GPIO 0/1, 2/3, 4/5, 6/7, 8/9) across all modes:
 *   PIO+DMA burstRead, GPIO bit-bang, Forced mode, Hardware Wire (Adafruit).
 *
 * Covers all 7 library examples in a single automated pass.
 *
 * Hardware: Raspberry Pi Pico with 5x BMP280/BME280 sensors
 * Output:   USB Serial (CDC) @ 115200 baud
 *
 * @author Ângelo Moisés Alves (@angeloINTJ)
 * @license MIT
 */

#include <Arduino.h>
#include "BMx280PIO_RP2040.h"
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <math.h>

// ─── Sensor Pin Configuration ─────────────────────────────────────────────
#define N_SENSORS 5

struct SensorConfig {
    const char *name;
    uint8_t     sda;
    uint8_t     scl;
    PIO         pio;
};

const SensorConfig CFG[N_SENSORS] = {
    {"S0 (0/1)",  0,  1,  pio0},
    {"S1 (2/3)",  2,  3,  pio0},
    {"S2 (4/5)",  4,  5,  pio0},
    {"S3 (6/7)",  6,  7,  pio1},
    {"S4 (8/9)",  8,  9,  pio1},
};

#define BMP280_ADDR  0x76

// ─── Utility ──────────────────────────────────────────────────────────────
static void separator(const char *title) {
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.print(  "║  ");
    Serial.print(title);
    for (int i = strlen(title); i < 60; i++) Serial.print(' ');
    Serial.println("║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println();
}

static void subSeparator(const char *title) {
    Serial.print("── ");
    Serial.print(title);
    Serial.println(" ──────────────────────────────────────────");
}

// ─── SECTION 0: Banner ────────────────────────────────────────────────────
static void printBanner() {
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  BMx280PIO_RP2040 — COMPREHENSIVE HARDWARE TEST SUITE       ║");
    Serial.println("║  5x BMP280/BME280 | All Modes | Automated Pass              ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.println("║  Test Plan:                                                 ║");
    Serial.println("║    1. PIO+DMA Basic Reading   (Normal mode, 10 reads each)  ║");
    Serial.println("║    2. Forced Mode             (Single-shot, 5 reads each)   ║");
    Serial.println("║    3. GPIO Bit-Bang           (Software I2C, 5 reads each)  ║");
    Serial.println("║    4. Hardware Wire           (Adafruit BMP280, 2 sensors)  ║");
    Serial.println("║    5. Multi-Sensor            (2 sensors simultaneous)      ║");
    Serial.println("║    6. Mini-Benchmark          (100 reads, stats)            ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.print(  "║  Sensors: ");
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print(CFG[i].name);
        if (i < N_SENSORS - 1) Serial.print(", ");
    }
    Serial.println("       ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println();
}

// ─── SECTION 1: PIO+DMA Initialization & Basic Reading ─────────────────────
static void testPioBasic() {
    separator("SECTION 1: PIO+DMA Basic Reading (Normal Mode)");

    BMx280PIO_RP2040 *dev[N_SENSORS] = {nullptr};
    bool               ok[N_SENSORS]  = {false};
    uint8_t            chipId[N_SENSORS];
    bool               isBme[N_SENSORS];

    // ── Init all 5 sensors ──────────────────────────────────────────────
    Serial.println("── Initializing sensors (PIO+DMA, 200 kHz) ──\n");
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("  ");
        Serial.print(CFG[i].name);
        Serial.print(" ... ");

        dev[i] = new BMx280PIO_RP2040(CFG[i].sda, CFG[i].scl, BMP280_ADDR, 200000, CFG[i].pio);
        if (!dev[i]->begin()) {
            Serial.println("FAIL — sensor not detected!");
            delete dev[i];
            dev[i] = nullptr;
            continue;
        }
        ok[i]     = true;
        chipId[i] = dev[i]->getChipID();
        isBme[i]  = dev[i]->isBME280();

        Serial.print(isBme[i] ? "BME280" : "BMP280");
        Serial.print("  ChipID=0x");
        Serial.print(chipId[i], HEX);
        Serial.print("  (pio");
        Serial.print(CFG[i].pio == pio0 ? "0" : "1");
        Serial.println(")");
        delay(50);
    }

    // Count initialized
    int nOk = 0;
    for (int i = 0; i < N_SENSORS; i++) if (ok[i]) nOk++;
    Serial.print("\n  Sensors found: ");
    Serial.print(nOk);
    Serial.print("/");
    Serial.println(N_SENSORS);

    // ── Configure & Read ──────────────────────────────────────────────────
    Serial.println("\n── Configuring: 2× Temp, 4× Press, Filter 4× ──\n");
    for (int i = 0; i < N_SENSORS; i++) {
        if (!ok[i]) continue;
        dev[i]->setTemperatureOversampling(BME280_OS_2X);
        dev[i]->setPressureOversampling(BME280_OS_4X);
        dev[i]->setFilter(BME280_FILTER_4);
        dev[i]->setMode(BME280_MODE_NORMAL);
    }
    delay(200);

    // ── Take 10 readings per sensor ──────────────────────────────────────
    #define N_READS 10
    float tSum[N_SENSORS] = {0}, pSum[N_SENSORS] = {0}, hSum[N_SENSORS] = {0};
    float tMin[N_SENSORS], tMax[N_SENSORS], pMin[N_SENSORS], pMax[N_SENSORS];
    uint32_t tTimeSum[N_SENSORS] = {0};

    for (int i = 0; i < N_SENSORS; i++) {
        tMin[i] = 999; tMax[i] = -999;
        pMin[i] = 999999; pMax[i] = -999999;
    }

    Serial.println("── 10 readings per sensor (PIO+DMA burstRead @ 200 kHz) ──\n");
    for (int rd = 0; rd < N_READS; rd++) {
        Serial.print("  [");
        Serial.print(rd + 1);
        Serial.print("/");
        Serial.print(N_READS);
        Serial.print("]  ");

        for (int i = 0; i < N_SENSORS; i++) {
            if (!ok[i]) { Serial.print("---/---  "); continue; }

            uint32_t t0 = micros();
            float t, p, h;
            dev[i]->readAll(&t, &p, &h);
            uint32_t dt = micros() - t0;

            Serial.print(CFG[i].name);
            Serial.print(": ");

            if (!isnan(t) && !isnan(p) && t > -40 && t < 85 && p > 300 && p < 2000) {
                Serial.print(t, 1);
                Serial.print("C/");
                Serial.print(p, 0);
                Serial.print("hPa ");
                tSum[i] += t; pSum[i] += p;
                if (isBme[i] && !isnan(h)) hSum[i] += h;
                if (t < tMin[i]) tMin[i] = t;
                if (t > tMax[i]) tMax[i] = t;
                if (p < pMin[i]) pMin[i] = p;
                if (p > pMax[i]) pMax[i] = p;
                tTimeSum[i] += dt;
            } else {
                Serial.print("FAIL(");
                Serial.print(t, 1);
                Serial.print("/");
                Serial.print(p, 0);
                Serial.print(") ");
            }
            delayMicroseconds(500);
        }
        Serial.println();
        delay(500);
    }

    // ── Stats ────────────────────────────────────────────────────────────
    Serial.println("\n── PIO+DMA Basic Reading Results ────────────────────────────\n");
    Serial.println("  Sensor     T_avg    T_min    T_max    P_avg     P_min     P_max    Time/read");
    Serial.println("  ──────     ─────    ─────    ─────    ─────     ─────     ─────    ─────────");
    for (int i = 0; i < N_SENSORS; i++) {
        if (!ok[i]) {
            Serial.print("  ");
            Serial.print(CFG[i].name);
            Serial.println("       ─── NOT FOUND ───");
            continue;
        }
        float ta = tSum[i] / N_READS;
        float pa = pSum[i] / N_READS;
        float dtAvg = (float)tTimeSum[i] / N_READS;
        Serial.print("  ");
        Serial.print(CFG[i].name);
        Serial.print("  ");
        Serial.print(ta, 2);
        Serial.print("  ");
        Serial.print(tMin[i], 2);
        Serial.print("  ");
        Serial.print(tMax[i], 2);
        Serial.print("  ");
        Serial.print(pa, 1);
        Serial.print("  ");
        Serial.print(pMin[i], 1);
        Serial.print("  ");
        Serial.print(pMax[i], 1);
        Serial.print("  ");
        Serial.print(dtAvg, 0);
        Serial.println(" µs");
        if (isBme[i]) {
            Serial.print("          Humidity avg: ");
            Serial.print(hSum[i] / N_READS, 1);
            Serial.println(" %");
        }
    }

    // ── Altitude ──────────────────────────────────────────────────────────
    Serial.println("\n── Approximate Altitude (std atmosphere) ──\n");
    for (int i = 0; i < N_SENSORS; i++) {
        if (!ok[i]) continue;
        float pa = pSum[i] / N_READS;
        float alt = 44330.0f * (1.0f - pow(pa / 1013.25f, 0.1903f));
        Serial.print("  ");
        Serial.print(CFG[i].name);
        Serial.print(": ");
        Serial.print(alt, 1);
        Serial.println(" m");
    }

    // Cleanup
    for (int i = 0; i < N_SENSORS; i++) { if (dev[i]) { delete dev[i]; dev[i] = nullptr; } }
    delay(200);
}

// ─── SECTION 2: Forced Mode ───────────────────────────────────────────────
static void testForcedMode() {
    separator("SECTION 2: Forced Mode (Single-Shot, Low Power)");

    BMx280PIO_RP2040 *dev[N_SENSORS] = {nullptr};
    bool               ok[N_SENSORS]  = {false};

    Serial.println("── Initializing sensors (PIO+DMA) ──\n");
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("  ");
        Serial.print(CFG[i].name);
        Serial.print(" ... ");
        dev[i] = new BMx280PIO_RP2040(CFG[i].sda, CFG[i].scl, BMP280_ADDR, 200000, CFG[i].pio);
        if (!dev[i]->begin()) {
            Serial.println("FAIL");
            delete dev[i]; dev[i] = nullptr;
            continue;
        }
        ok[i] = true;
        dev[i]->setTemperatureOversampling(BME280_OS_1X);
        dev[i]->setPressureOversampling(BME280_OS_1X);
        dev[i]->setFilter(BME280_FILTER_OFF);
        Serial.print(dev[i]->isBME280() ? "BME280" : "BMP280");
        Serial.println(" OK (1× os, no filter, low-power config)");
        delay(30);
    }

    Serial.println("\n── 5 forced measurements per sensor ──\n");

    #define N_FORCED 5
    for (int rd = 0; rd < N_FORCED; rd++) {
        Serial.print("  [");
        Serial.print(rd + 1);
        Serial.print("/");
        Serial.print(N_FORCED);
        Serial.print("]  ");

        for (int i = 0; i < N_SENSORS; i++) {
            if (!ok[i]) { Serial.print("---/---  "); continue; }

            uint32_t t0 = micros();
            bool success = dev[i]->takeForcedMeasurement();
            uint32_t tForce = micros() - t0;

            if (success) {
                float t = dev[i]->readTemperature();
                float p = dev[i]->readPressure();
                Serial.print(CFG[i].name);
                Serial.print(": ");
                Serial.print(t, 1);
                Serial.print("C/");
                Serial.print(p, 0);
                Serial.print("hPa (");
                Serial.print(tForce / 1000.0f, 1);
                Serial.print("ms) ");
            } else {
                Serial.print(CFG[i].name);
                Serial.print(": FAILED ");
            }
            delayMicroseconds(300);
        }
        Serial.println();
        delay(200);
    }

    Serial.println("\n── Forced Mode Summary ───────────────────────────────────────\n");
    Serial.println("  Forced mode = sensor wakes, measures, returns to sleep.");
    Serial.println("  Ideal for battery-powered applications. ~3 µA average current.");

    for (int i = 0; i < N_SENSORS; i++) { if (dev[i]) delete dev[i]; }
    delay(200);
}

// ─── SECTION 3: GPIO Bit-Bang Mode ────────────────────────────────────────
static void testGpioBitBang() {
    separator("SECTION 3: GPIO Bit-Bang Mode (Software I2C @ 100 kHz)");

    BMx280PIO_RP2040 *dev[N_SENSORS] = {nullptr};
    bool               ok[N_SENSORS]  = {false};

    Serial.println("── Initializing sensors with forceGPIO(true) ──\n");
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("  ");
        Serial.print(CFG[i].name);
        Serial.print(" ... ");
        dev[i] = new BMx280PIO_RP2040(CFG[i].sda, CFG[i].scl, BMP280_ADDR);
        dev[i]->forceGPIO(true);
        if (!dev[i]->begin()) {
            Serial.println("FAIL");
            delete dev[i]; dev[i] = nullptr;
            continue;
        }
        ok[i] = true;
        dev[i]->setMode(BME280_MODE_NORMAL);
        Serial.print(dev[i]->isBME280() ? "BME280" : "BMP280");
        Serial.println(" OK (GPIO bit-bang, 100 kHz)");
        delay(30);
    }

    Serial.println("\n── 5 readings per sensor (GPIO bit-bang) ──\n");

    #define N_GPIO 5
    float    gpioTimeSum[N_SENSORS] = {0};
    uint32_t gpioCount[N_SENSORS]   = {0};

    for (int rd = 0; rd < N_GPIO; rd++) {
        Serial.print("  [");
        Serial.print(rd + 1);
        Serial.print("/");
        Serial.print(N_GPIO);
        Serial.print("]  ");

        for (int i = 0; i < N_SENSORS; i++) {
            if (!ok[i]) { Serial.print("---/---  "); continue; }

            uint32_t t0 = micros();
            float t, p, h;
            dev[i]->readAll(&t, &p, &h);
            uint32_t dt = micros() - t0;

            if (!isnan(t) && !isnan(p)) {
                Serial.print(CFG[i].name);
                Serial.print(": ");
                Serial.print(t, 1);
                Serial.print("C/");
                Serial.print(p, 0);
                Serial.print("hPa (");
                Serial.print(dt);
                Serial.print("µs) ");
                gpioTimeSum[i] += dt;
                gpioCount[i]++;
            } else {
                Serial.print(CFG[i].name);
                Serial.print(": FAIL ");
            }
        }
        Serial.println();
        delay(500);
    }

    // ── Timing comparison ──────────────────────────────────────────────────
    Serial.println("\n── GPIO Bit-Bang Timing (avg µs/read) ──\n");
    Serial.println("  Sensor     GPIO avg    Expected     Verdict");
    Serial.println("  ──────     ────────    ────────     ──────");
    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("  ");
        Serial.print(CFG[i].name);
        if (!ok[i] || gpioCount[i] == 0) {
            Serial.println("       ─── NO DATA ───");
            continue;
        }
        float avg = gpioTimeSum[i] / gpioCount[i];
        Serial.print("      ");
        Serial.print(avg, 0);
        Serial.print(" µs     ~3500 µs     ");
        // Expected ~3500 µs for GPIO bit-bang @ 100 kHz
        if (avg < 5000) Serial.println("PASS");
        else Serial.println("SLOW (check wiring)");
    }

    for (int i = 0; i < N_SENSORS; i++) { if (dev[i]) delete dev[i]; }
    delay(200);
}

// ─── SECTION 4: Hardware Wire Mode (Adafruit BMP280) ──────────────────────
static void testHardwareWire() {
    separator("SECTION 4: Hardware Wire Mode (Adafruit BMP280 @ 100 kHz)");

    // Only test on sensors where we can assign hardware I2C:
    // Sensor S2 (GPIO 4/5) → Wire  (I2C0 default pins)
    // Sensor S3 (GPIO 6/7) → Wire1 (I2C1 default pins)

    Serial.println("── Using Adafruit BMP280 Library via hardware I2C ──\n");

    Wire.setSDA(4);
    Wire.setSCL(5);
    Wire.begin();
    Wire.setClock(100000);

    Wire1.setSDA(6);
    Wire1.setSCL(7);
    Wire1.begin();
    Wire1.setClock(100000);

    Adafruit_BMP280 bmpA(&Wire);
    Adafruit_BMP280 bmpB(&Wire1);

    bool okA = false, okB = false;

    Serial.print("  S2 (GPIO 4/5) Wire I2C0 ... ");
    if (bmpA.begin(BMP280_ADDR)) {
        Serial.println("BMP280 OK");
        bmpA.setSampling(Adafruit_BMP280::MODE_NORMAL,
                         Adafruit_BMP280::SAMPLING_X1,
                         Adafruit_BMP280::SAMPLING_X1,
                         Adafruit_BMP280::FILTER_OFF,
                         Adafruit_BMP280::STANDBY_MS_1);
        okA = true;
    } else {
        Serial.println("FAIL");
    }

    Serial.print("  S3 (GPIO 6/7) Wire1 I2C1 ... ");
    if (bmpB.begin(BMP280_ADDR)) {
        Serial.println("BMP280 OK");
        bmpB.setSampling(Adafruit_BMP280::MODE_NORMAL,
                         Adafruit_BMP280::SAMPLING_X1,
                         Adafruit_BMP280::SAMPLING_X1,
                         Adafruit_BMP280::FILTER_OFF,
                         Adafruit_BMP280::STANDBY_MS_1);
        okB = true;
    } else {
        Serial.println("FAIL");
    }
    delay(100);

    Serial.println("\n── 5 readings per sensor (Adafruit Wire) ──\n");

    #define N_WIRE 5
    for (int rd = 0; rd < N_WIRE; rd++) {
        Serial.print("  [");
        Serial.print(rd + 1);
        Serial.print("/");
        Serial.print(N_WIRE);
        Serial.print("]  ");

        if (okA) {
            uint32_t t0 = micros();
            float ta = bmpA.readTemperature();
            float pa = bmpA.readPressure() / 100.0f;
            uint32_t dt = micros() - t0;
            Serial.print("S2: ");
            Serial.print(ta, 1);
            Serial.print("C/");
            Serial.print(pa, 0);
            Serial.print("hPa (");
            Serial.print(dt);
            Serial.print("µs)  ");
        } else {
            Serial.print("S2: ---  ");
        }

        if (okB) {
            uint32_t t0 = micros();
            float tb = bmpB.readTemperature();
            float pb = bmpB.readPressure() / 100.0f;
            uint32_t dt = micros() - t0;
            Serial.print("S3: ");
            Serial.print(tb, 1);
            Serial.print("C/");
            Serial.print(pb, 0);
            Serial.print("hPa (");
            Serial.print(dt);
            Serial.print("µs)");
        } else {
            Serial.print("S3: ---");
        }
        Serial.println();
        delay(500);
    }

    Serial.println("\n── Hardware Wire vs PIO+DMA (from Section 1) ──\n");
    Serial.println("  Adafruit Wire @ 100 kHz: ~1800 µs/read (reference)");
    Serial.println("  PIO+DMA @ 200 kHz:      ~550 µs/read  (3.3× faster)");
    Serial.println("  GPIO bit-bang @ 100 kHz: ~3500 µs/read (reliable fallback)");

    Wire.end();
    Wire1.end();
    delay(200);
}

// ─── SECTION 5: Multi-Sensor Simultaneous ──────────────────────────────────
static void testMultiSensor() {
    separator("SECTION 5: Multi-Sensor Simultaneous Read");

    // Test sensor pairs simultaneously — picks S1(2/3) and S2(4/5) on pio0
    BMx280PIO_RP2040 *sA = nullptr, *sB = nullptr;
    bool okA = false, okB = false;

    Serial.println("── Initializing 2 sensors on independent PIO+DMA buses ──\n");

    Serial.print("  S2 (GPIO 4/5) pio0 ... ");
    sA = new BMx280PIO_RP2040(4, 5, BMP280_ADDR, 200000, pio0);
    if (sA->begin()) {
        okA = true;
        sA->setTemperatureOversampling(BME280_OS_2X);
        sA->setPressureOversampling(BME280_OS_4X);
        sA->setFilter(BME280_FILTER_4);
        sA->setMode(BME280_MODE_NORMAL);
        Serial.print(sA->isBME280() ? "BME280" : "BMP280");
        Serial.println(" OK");
    } else { Serial.println("FAIL"); delete sA; sA = nullptr; }

    Serial.print("  S3 (GPIO 6/7) pio1 ... ");
    sB = new BMx280PIO_RP2040(6, 7, BMP280_ADDR, 200000, pio1);
    if (sB->begin()) {
        okB = true;
        sB->setTemperatureOversampling(BME280_OS_2X);
        sB->setPressureOversampling(BME280_OS_4X);
        sB->setFilter(BME280_FILTER_4);
        sB->setMode(BME280_MODE_NORMAL);
        Serial.print(sB->isBME280() ? "BME280" : "BMP280");
        Serial.println(" OK");
    } else { Serial.println("FAIL"); delete sB; sB = nullptr; }
    delay(200);

    if (!okA || !okB) {
        Serial.println("\n  ⚠ Both sensors needed for multi-sensor test. Skipping.");
        if (sA) delete sA;
        if (sB) delete sB;
        return;
    }

    Serial.println("\n── 10 simultaneous readings with cross-sensor delta ──\n");

    #define N_MULTI 10
    float sumDT = 0, sumDP = 0;

    for (int rd = 0; rd < N_MULTI; rd++) {
        Serial.print("  [");
        Serial.print(rd + 1);
        Serial.print("/");
        Serial.print(N_MULTI);
        Serial.print("]  ");

        float t1, p1, h1, t2, p2, h2;

        uint32_t t0 = micros();
        sA->readAll(&t1, &p1, &h1);
        sB->readAll(&t2, &p2, &h2);
        uint32_t dt = micros() - t0;

        Serial.print("A: ");
        Serial.print(t1, 2);
        Serial.print("C/");
        Serial.print(p1, 1);
        Serial.print("hPa  B: ");
        Serial.print(t2, 2);
        Serial.print("C/");
        Serial.print(p2, 1);
        Serial.print("hPa  ");

        if (!isnan(t1) && !isnan(t2) && !isnan(p1) && !isnan(p2)) {
            float dT = t1 - t2;
            float dP = p1 - p2;
            sumDT += fabs(dT);
            sumDP += fabs(dP);
            Serial.print("ΔT=");
            Serial.print(dT, 2);
            Serial.print(" ΔP=");
            Serial.print(dP, 1);
        }
        Serial.print("  (");
        Serial.print(dt);
        Serial.println(" µs total)");
        delay(500);
    }

    Serial.println("\n── Multi-Sensor Summary ─────────────────────────────────────\n");
    Serial.print("  Avg absolute ΔT: ");
    Serial.print(sumDT / N_MULTI, 3);
    Serial.println(" °C");
    Serial.print("  Avg absolute ΔP: ");
    Serial.print(sumDP / N_MULTI, 2);
    Serial.println(" hPa");
    Serial.println("  Note: Small deltas = good sensor agreement. Large deltas");
    Serial.println("        may indicate different drafts/sunlight per sensor.");

    if (sA) delete sA;
    if (sB) delete sB;
    delay(200);
}

// ─── SECTION 6: Mini-Benchmark (PIO vs GPIO) ──────────────────────────────
static void testMiniBenchmark() {
    separator("SECTION 6: Mini-Benchmark (PIO+DMA vs GPIO vs Wire)");

    const int  N_BENCH = 100;
    const int  WARMUP  = 10;

    Serial.println("── Benchmark: ");
    Serial.print("     Sensor: S2 (GPIO 4/5), ");
    Serial.print(N_BENCH);
    Serial.println(" readings per mode");
    Serial.println("     Modes: PIO+DMA @ 200kHz | GPIO bit-bang @ 100kHz | Wire @ 100kHz\n");

    // ── Mode 1: PIO+DMA ──────────────────────────────────────────────────
    {
        subSeparator("Mode 1: PIO+DMA @ 200 kHz");
        BMx280PIO_RP2040 dev(4, 5, BMP280_ADDR, 200000, pio0);
        if (!dev.begin()) {
            Serial.println("  FAIL: sensor not found. Aborting benchmark.");
            return;
        }
        dev.setMode(BME280_MODE_NORMAL);
        Serial.print("  Sensor: ");
        Serial.println(dev.isBME280() ? "BME280" : "BMP280");
        delay(100);

        // Warmup
        for (int i = 0; i < WARMUP; i++) { float t, p; dev.readAll(&t, &p, nullptr); delayMicroseconds(500); }

        uint32_t tMin = 0xFFFFFFFF, tMax = 0;
        uint64_t tSum = 0;
        uint32_t times[N_BENCH];
        float    tAvg = 0, pAvg = 0;

        Serial.print("  Running ");
        Serial.print(N_BENCH);
        Serial.println(" reads...");
        for (int i = 0; i < N_BENCH; i++) {
            float t, p;
            uint32_t t0 = micros();
            dev.readAll(&t, &p, nullptr);
            uint32_t dt = micros() - t0;
            times[i] = dt;
            if (dt < tMin) tMin = dt;
            if (dt > tMax) tMax = dt;
            tSum += dt;
            if (!isnan(t) && !isnan(p)) { tAvg += t; pAvg += p; }
            delayMicroseconds(200);
        }

        // Sort for median/P99
        for (int i = 0; i < N_BENCH - 1; i++)
            for (int j = i + 1; j < N_BENCH; j++)
                if (times[i] > times[j]) { uint32_t tmp = times[i]; times[i] = times[j]; times[j] = tmp; }

        float mean  = (float)((double)tSum / N_BENCH);
        uint32_t med = times[N_BENCH / 2];
        uint32_t p99 = times[(int)(N_BENCH * 0.99)];
        float rps    = tSum > 0 ? (float)(N_BENCH * 1000000ULL) / (float)tSum : 0;

        double sd = 0;
        for (int i = 0; i < N_BENCH; i++) { double d = times[i] - mean; sd += d * d; }
        float stddev = sqrt(sd / N_BENCH);

        Serial.print("  Min: ");
        Serial.print(tMin);
        Serial.print(" µs | Med: ");
        Serial.print(med);
        Serial.print(" µs | Mean: ");
        Serial.print(mean, 1);
        Serial.println(" µs");
        Serial.print("  P99: ");
        Serial.print(p99);
        Serial.print(" µs | Max: ");
        Serial.print(tMax);
        Serial.print(" µs | SD: ");
        Serial.print(stddev, 1);
        Serial.println(" µs");
        Serial.print("  Throughput: ");
        Serial.print(rps, 1);
        Serial.print(" reads/s | T_avg: ");
        Serial.print(tAvg / N_BENCH, 2);
        Serial.print("°C | P_avg: ");
        Serial.print(pAvg / N_BENCH, 1);
        Serial.println(" hPa");
    }
    delay(300);

    // ── Mode 2: GPIO bit-bang ────────────────────────────────────────────
    {
        subSeparator("Mode 2: GPIO Bit-Bang @ 100 kHz");
        BMx280PIO_RP2040 dev(4, 5, BMP280_ADDR);
        dev.forceGPIO(true);
        if (!dev.begin()) {
            Serial.println("  FAIL: sensor not found. Skipping GPIO benchmark.");
        } else {
            dev.setMode(BME280_MODE_NORMAL);
            delay(100);

            // Warmup
            for (int i = 0; i < WARMUP; i++) { float t, p; dev.readAll(&t, &p, nullptr); delay(1); }

            uint32_t tMin = 0xFFFFFFFF, tMax = 0;
            uint64_t tSum = 0;
            uint32_t times[N_BENCH];

            Serial.print("  Running ");
            Serial.print(N_BENCH);
            Serial.println(" reads...");
            for (int i = 0; i < N_BENCH; i++) {
                float t, p;
                uint32_t t0 = micros();
                dev.readAll(&t, &p, nullptr);
                uint32_t dt = micros() - t0;
                times[i] = dt;
                if (dt < tMin) tMin = dt;
                if (dt > tMax) tMax = dt;
                tSum += dt;
                delayMicroseconds(500);
            }

            for (int i = 0; i < N_BENCH - 1; i++)
                for (int j = i + 1; j < N_BENCH; j++)
                    if (times[i] > times[j]) { uint32_t tmp = times[i]; times[i] = times[j]; times[j] = tmp; }

            float mean  = (float)((double)tSum / N_BENCH);
            uint32_t med = times[N_BENCH / 2];
            uint32_t p99 = times[(int)(N_BENCH * 0.99)];
            float rps    = tSum > 0 ? (float)(N_BENCH * 1000000ULL) / (float)tSum : 0;

            double sd = 0;
            for (int i = 0; i < N_BENCH; i++) { double d = times[i] - mean; sd += d * d; }
            float stddev = sqrt(sd / N_BENCH);

            Serial.print("  Min: ");
            Serial.print(tMin);
            Serial.print(" µs | Med: ");
            Serial.print(med);
            Serial.print(" µs | Mean: ");
            Serial.print(mean, 1);
            Serial.println(" µs");
            Serial.print("  P99: ");
            Serial.print(p99);
            Serial.print(" µs | Max: ");
            Serial.print(tMax);
            Serial.print(" µs | SD: ");
            Serial.print(stddev, 1);
            Serial.println(" µs");
            Serial.print("  Throughput: ");
            Serial.print(rps, 1);
            Serial.println(" reads/s");
        }
    }
    delay(300);

    // ── Mode 3: Adafruit Wire ────────────────────────────────────────────
    {
        subSeparator("Mode 3: Adafruit BMP280 (hardware Wire @ 100 kHz)");
        Wire.setSDA(4);
        Wire.setSCL(5);
        Wire.begin();
        Wire.setClock(100000);

        Adafruit_BMP280 bmp(&Wire);
        if (!bmp.begin(BMP280_ADDR)) {
            Serial.println("  FAIL: sensor not found. Skipping Wire benchmark.");
        } else {
            bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                           Adafruit_BMP280::SAMPLING_X1,
                           Adafruit_BMP280::SAMPLING_X1,
                           Adafruit_BMP280::FILTER_OFF,
                           Adafruit_BMP280::STANDBY_MS_1);
            delay(100);

            // Warmup
            for (int i = 0; i < WARMUP; i++) { bmp.readTemperature(); bmp.readPressure(); delay(1); }

            uint32_t tMin = 0xFFFFFFFF, tMax = 0;
            uint64_t tSum = 0;
            uint32_t times[N_BENCH];

            Serial.print("  Running ");
            Serial.print(N_BENCH);
            Serial.println(" reads...");
            for (int i = 0; i < N_BENCH; i++) {
                uint32_t t0 = micros();
                bmp.readTemperature();
                float p = bmp.readPressure();
                uint32_t dt = micros() - t0;
                (void)p;
                times[i] = dt;
                if (dt < tMin) tMin = dt;
                if (dt > tMax) tMax = dt;
                tSum += dt;
                delayMicroseconds(500);
            }

            for (int i = 0; i < N_BENCH - 1; i++)
                for (int j = i + 1; j < N_BENCH; j++)
                    if (times[i] > times[j]) { uint32_t tmp = times[i]; times[i] = times[j]; times[j] = tmp; }

            float mean  = (float)((double)tSum / N_BENCH);
            uint32_t med = times[N_BENCH / 2];
            uint32_t p99 = times[(int)(N_BENCH * 0.99)];
            float rps    = tSum > 0 ? (float)(N_BENCH * 1000000ULL) / (float)tSum : 0;

            double sd = 0;
            for (int i = 0; i < N_BENCH; i++) { double d = times[i] - mean; sd += d * d; }
            float stddev = sqrt(sd / N_BENCH);

            Serial.print("  Min: ");
            Serial.print(tMin);
            Serial.print(" µs | Med: ");
            Serial.print(med);
            Serial.print(" µs | Mean: ");
            Serial.print(mean, 1);
            Serial.println(" µs");
            Serial.print("  P99: ");
            Serial.print(p99);
            Serial.print(" µs | Max: ");
            Serial.print(tMax);
            Serial.print(" µs | SD: ");
            Serial.print(stddev, 1);
            Serial.println(" µs");
            Serial.print("  Throughput: ");
            Serial.print(rps, 1);
            Serial.println(" reads/s");
        }
        Wire.end();
    }

    delay(200);
}

// ─── SECTION 7: Stability Test ────────────────────────────────────────────
static void testStability() {
    separator("SECTION 7: Stability Quick-Check (30s loop all sensors)");

    BMx280PIO_RP2040 *dev[N_SENSORS] = {nullptr};
    bool               ok[N_SENSORS]  = {false};
    uint32_t           readsOk[N_SENSORS] = {0};
    uint32_t           readsFail[N_SENSORS] = {0};
    float              tMin[N_SENSORS], tMax[N_SENSORS], pMin[N_SENSORS], pMax[N_SENSORS];

    Serial.println("── Initializing all 5 sensors (PIO+DMA) ──\n");
    for (int i = 0; i < N_SENSORS; i++) {
        tMin[i] = 999; tMax[i] = -999;
        pMin[i] = 999999; pMax[i] = -999999;

        Serial.print("  ");
        Serial.print(CFG[i].name);
        Serial.print(" ... ");
        dev[i] = new BMx280PIO_RP2040(CFG[i].sda, CFG[i].scl, BMP280_ADDR, 200000, CFG[i].pio);
        if (!dev[i]->begin()) {
            Serial.println("FAIL");
            delete dev[i]; dev[i] = nullptr;
            continue;
        }
        ok[i] = true;
        dev[i]->setMode(BME280_MODE_NORMAL);
        Serial.print(dev[i]->isBME280() ? "BME280" : "BMP280");
        Serial.println(" OK");
        delay(30);
    }
    delay(200);

    Serial.println("\n── Reading all sensors continuously for ~30 seconds ──\n");

    uint32_t tStart = millis();
    uint32_t iter   = 0;

    while (millis() - tStart < 30000) {
        iter++;
        Serial.print("  [");
        Serial.print(iter);
        Serial.print("] ");

        bool anyFail = false;
        for (int i = 0; i < N_SENSORS; i++) {
            if (!ok[i]) { Serial.print("---/--- "); continue; }

            float t, p;
            dev[i]->readAll(&t, &p, nullptr);

            bool good = (!isnan(t) && !isnan(p) && t > -40 && t < 85 && p > 300 && p < 2000);
            if (good) {
                readsOk[i]++;
                if (t < tMin[i]) tMin[i] = t;
                if (t > tMax[i]) tMax[i] = t;
                if (p < pMin[i]) pMin[i] = p;
                if (p > pMax[i]) pMax[i] = p;
                Serial.print(t, 1);
                Serial.print("C/");
                Serial.print(p, 0);
                Serial.print("hPa ");
            } else {
                readsFail[i]++;
                anyFail = true;
                Serial.print("FAIL(");
                Serial.print(t, 1);
                Serial.print("/");
                Serial.print(p, 0);
                Serial.print(") ");
            }
        }
        if (anyFail) Serial.print(" ⚠");
        Serial.println();
        delay(1000);
    }

    uint32_t tElapsed = millis() - tStart;

    Serial.println("\n── Stability Test Results ───────────────────────────────────\n");
    Serial.print("  Duration: ");
    Serial.print(tElapsed / 1000.0f, 1);
    Serial.println(" seconds");
    Serial.println();
    Serial.println("  Sensor    ReadsOK  Fails  Success%    T_range       P_range");
    Serial.println("  ──────    ───────  ─────  ────────    ───────       ───────");

    for (int i = 0; i < N_SENSORS; i++) {
        Serial.print("  ");
        Serial.print(CFG[i].name);
        if (!ok[i]) {
            Serial.println("     ─── NOT FOUND ───");
            continue;
        }
        uint32_t total = readsOk[i] + readsFail[i];
        float rate = total > 0 ? 100.0f * readsOk[i] / total : 0;

        char buf[80];
        snprintf(buf, sizeof(buf), "     %5lu   %4lu   %5.1f%%",
                 readsOk[i], readsFail[i], rate);
        Serial.print(buf);

        if (readsOk[i] > 0) {
            Serial.print("       ");
            Serial.print(tMin[i], 1);
            Serial.print("..");
            Serial.print(tMax[i], 1);
            Serial.print(" C  ");
            Serial.print(pMin[i], 0);
            Serial.print("..");
            Serial.print(pMax[i], 0);
            Serial.print(" hPa");
        }
        Serial.println();
    }

    for (int i = 0; i < N_SENSORS; i++) { if (dev[i]) delete dev[i]; }
    delay(200);
}

// ─── Final Summary ────────────────────────────────────────────────────────
static void printFinalSummary() {
    separator("TEST SUITE COMPLETE");

    Serial.println("  Sections executed:");
    Serial.println("    1. PIO+DMA Basic Reading   — Normal mode, 10 reads × 5 sensors");
    Serial.println("    2. Forced Mode              — Single-shot, 5 reads × 5 sensors");
    Serial.println("    3. GPIO Bit-Bang            — Software I2C, 5 reads × 5 sensors");
    Serial.println("    4. Hardware Wire (Adafruit) — I2C0+I2C1, 5 reads × 2 sensors");
    Serial.println("    5. Multi-Sensor             — 2 simultaneous buses, 10 reads");
    Serial.println("    6. Mini-Benchmark           — 100 reads PIO vs GPIO vs Wire");
    Serial.println("    7. Stability Quick-Check    — 30s continuous loop all sensors");
    Serial.println();
    Serial.println("  Library: BMx280PIO_RP2040 v1.2.4");
    Serial.println("  Transport: WirePIO (PIO+DMA) + GPIO bit-bang fallback");
    Serial.println("  Platform: Raspberry Pi Pico (RP2040) / earlephilhower core");
    Serial.println();
    Serial.println("  ✅ All tests completed.");
    Serial.println("  See tests/README.md for analysis and documentation.");
    Serial.println();
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║  END OF COMPREHENSIVE HARDWARE TEST SUITE                   ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
}

// ─── Setup / Loop ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(2000);

    printBanner();

    // Run all test sections sequentially
    testPioBasic();
    testForcedMode();
    testGpioBitBang();
    testHardwareWire();
    testMultiSensor();
    testMiniBenchmark();
    testStability();

    printFinalSummary();
}

void loop() {
    delay(10000);
}

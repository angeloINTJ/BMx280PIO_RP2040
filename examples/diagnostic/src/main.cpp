/**
 * I2C Logic Analyzer — Dual-Core RP2040
 *
 * Core 1: fast GPIO sampling of SDA+SCL into a ring buffer
 * Core 0: runs PIO+DMA I2C transactions, then dumps captured waveform
 *
 * Compile with: pio run -d .../diagnostic
 * Requires: Pico with BMP280 on GPIO4(SDA)/GPIO5(SCL)
 */
#include <Arduino.h>
#include "BMx280PIO_RP2040.h"

#define SDA 4
#define SCL 5
#define ADDR 0x76

// ─── Logic Analyzer on Core 1 ─────────────────────────────────────────
#define CAP_BITS 16000  // capture this many samples
volatile uint32_t g_cap[CAP_BITS];  // each word stores SDA(bit0)+SCL(bit1)
volatile uint32_t g_cap_idx = 0;
volatile bool g_cap_done = false;
volatile bool g_cap_arm = false;

void setup1() {}
void loop1() {
    if (!g_cap_arm) { delay(1); return; }

    // Fast capture loop: sample both pins, store in buffer
    uint32_t mask = (1u << SDA) | (1u << SCL);
    uint32_t *buf = (uint32_t *)g_cap;
    uint32_t sio_base = SIO_BASE;

    for (int i = 0; i < CAP_BITS; i++) {
        // Read GPIO IN register directly (fastest possible read)
        buf[i] = (*(volatile uint32_t *)(sio_base + 0x004)) & mask;
    }
    g_cap_done = true;
    g_cap_arm = false;
}

// ─── Trigger capture from Core 0 ───────────────────────────────────────
static void capStart() {
    g_cap_idx = 0;
    g_cap_done = false;
    g_cap_arm = true;
    delayMicroseconds(50); // let Core 1 start capturing before transaction
}

static void capWait() {
    while (!g_cap_done) { /* wait */ }
    delayMicroseconds(10); // let last samples flush
}

// ─── Analyze and print captured waveform ───────────────────────────────
static void capAnalyze(const char *label) {
    uint32_t mask = (1u << SDA) | (1u << SCL);
    uint32_t *buf = (uint32_t *)g_cap;

    // Find first SCL HIGH → LOW transition (start of I2C activity)
    int start = 0;
    for (int i = 1; i < CAP_BITS - 1; i++) {
        // SCL was HIGH, now LOW
        if ((buf[i-1] & (1u << SCL)) && !(buf[i] & (1u << SCL))) {
            start = i;
            break;
        }
    }

    Serial.print("\n--- "); Serial.print(label); Serial.println(" ---");
    Serial.print("  First SCL edge at sample: "); Serial.println(start);

    // Measure SCL period (HIGH pulse width + LOW pulse width)
    int sclHighTotal = 0, sclLowTotal = 0;
    int sclHighCount = 0, sclLowCount = 0;
    int sclHighMin = 99999, sclHighMax = 0;
    int sclLowMin = 99999, sclLowMax = 0;

    int state = 0; // 0=waiting, 1=SCL_HIGH, 2=SCL_LOW
    int count = 0;
    int transitions = 0;

    for (int i = start; i < CAP_BITS - 1 && transitions < 500; i++) {
        bool scl_high = buf[i] & (1u << SCL);
        bool scl_prev = buf[i-1] & (1u << SCL);

        if (scl_high != scl_prev) {
            transitions++;
            if (scl_high) {
                // LOW → HIGH: measure previous LOW duration
                if (count > 0 && count < 5000 && sclLowCount > 3) {
                    sclLowTotal += count;
                    sclLowCount++;
                    if (count < sclLowMin) sclLowMin = count;
                    if (count > sclLowMax) sclLowMax = count;
                }
            } else {
                // HIGH → LOW: measure previous HIGH duration
                if (count > 0 && count < 5000 && sclHighCount > 3) {
                    sclHighTotal += count;
                    sclHighCount++;
                    if (count < sclHighMin) sclHighMin = count;
                    if (count > sclHighMax) sclHighMax = count;
                }
            }
            count = 0;
        }
        count++;
    }

    if (sclHighCount > 0 && sclLowCount > 0) {
        float avgHigh = (float)sclHighTotal / sclHighCount;
        float avgLow = (float)sclLowTotal / sclLowCount;
        float period = avgHigh + avgLow;
        float freq_khz = 133000.0f / period;  // 133 MHz CPU clock

        Serial.print("  SCL HIGH: avg="); Serial.print(avgHigh, 1);
        Serial.print(" samples (min="); Serial.print(sclHighMin);
        Serial.print(" max="); Serial.print(sclHighMax);
        Serial.print(") = "); Serial.print(avgHigh / 133.0f, 1); Serial.println(" µs");

        Serial.print("  SCL LOW:  avg="); Serial.print(avgLow, 1);
        Serial.print(" samples (min="); Serial.print(sclLowMin);
        Serial.print(" max="); Serial.print(sclLowMax);
        Serial.print(") = "); Serial.print(avgLow / 133.0f, 1); Serial.println(" µs");

        Serial.print("  SCL period: "); Serial.print(period, 1);
        Serial.print(" samples = "); Serial.print(period / 133.0f, 2);
        Serial.print(" µs → I2C freq = "); Serial.print(freq_khz, 1);
        Serial.println(" kHz");
        Serial.print("  Samples: HIGH="); Serial.print(sclHighCount);
        Serial.print(" LOW="); Serial.println(sclLowCount);
    }

    // Print first 50 samples around first edge for visual inspection
    Serial.print("  Waveform (first 100 samples from edge): ");
    for (int i = start; i < start + 100 && i < CAP_BITS; i++) {
        bool sda = buf[i] & (1u << SDA);
        bool scl = buf[i] & (1u << SCL);
        Serial.print(sda ? "H" : "L");
        Serial.print(scl ? "H" : "L");
        Serial.print(" ");
        if ((i - start) % 20 == 19) Serial.println();
    }
    Serial.println();
}

// ─── Core 0 ────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(2000);

    Serial.println("\n=== I2C Logic Analyzer — Dual-Core ===\n");
    Serial.print("Sampling rate: ~133 MHz / ~6 cycles = ~22 Msps\n");
    Serial.print("Capture depth: "); Serial.print(CAP_BITS); Serial.println(" samples\n");

    // ─── Test 1: GPIO bit-bang I2C (known good) ────────────────────
    Serial.println("--- Test 1: GPIO bit-bang (reference) ---");
    {
        BMx280PIO_RP2040 s(SDA, SCL, ADDR);
        s.forceGPIO(true);
        if (!s.begin()) { Serial.println("GPIO begin FAIL!"); return; }
        s.setMode(BME280_MODE_NORMAL);
        delay(200);

        capStart();
        float t, p;
        s.readAll(&t, &p, nullptr);
        capWait();

        capAnalyze("GPIO bit-bang readAll");
        Serial.print("  Result: T="); Serial.print(t,1);
        Serial.print("C P="); Serial.print(p,0); Serial.println("hPa");
    }
    delay(500);

    // ─── Test 2: Capture PIO+DMA begin (includes CTRL_MEAS write) ─
    Serial.println("\n--- Test 2: PIO+DMA begin() capture ---");
    {
        capStart();  // arm BEFORE creating sensor (captures begin writes)
        BMx280PIO_RP2040 s(SDA, SCL, ADDR, 200000, pio0);
        bool ok = s.begin();
        capWait();

        Serial.print("  begin: "); Serial.println(ok ? "OK" : "FAIL");
        if (ok) {
            Serial.print("  CTRL_MEAS: 0x"); Serial.println(s.readRegister(0xF4), HEX);
            capAnalyze("PIO+DMA begin() transaction");

            s.setMode(BME280_MODE_NORMAL);
            delay(200);
            capStart();
            float t, p;
            s.readAll(&t, &p, nullptr);
            capWait();
            capAnalyze("PIO+DMA readAll");
            Serial.print("  Result: T="); Serial.print(t,1);
            Serial.print("C P="); Serial.print(p,0); Serial.println("hPa");
        }
    }
    delay(500);

    // ─── Test 3: PIO+DMA setMode write ────────────────────────────
    Serial.println("\n--- Test 3: PIO+DMA setMode write ---");
    {
        BMx280PIO_RP2040 s(SDA, SCL, ADDR, 200000, pio0);
        if (!s.begin()) { Serial.println("PIO begin FAIL!"); return; }
        Serial.print("  CTRL_MEAS before: 0x"); Serial.println(s.readRegister(0xF4), HEX);

        capStart();
        s.setMode(BME280_MODE_NORMAL);
        capWait();

        capAnalyze("PIO+DMA setMode write");
        uint8_t ctrl = s.readRegister(0xF4);
        Serial.print("  CTRL_MEAS after: 0x"); Serial.print(ctrl, HEX);
        Serial.print(" (expected 0x27) ");
        Serial.println(ctrl == 0x27 ? "OK" : "FAIL!");
    }

    Serial.println("\n=== Done ===");
}

void loop() { delay(1000); }

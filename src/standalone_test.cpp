/*
 * BMx280_PIO — Standalone Test (PlatformIO development only)
 *
 * Compiled only when -DBMX280_STANDALONE_TEST is defined (platformio.ini).
 * When installed as an Arduino/PlatformIO library dependency, this flag
 * is absent — the file compiles to an empty translation unit with no
 * conflicting setup()/loop() symbols.
 */
#ifdef BMX280_STANDALONE_TEST

#include <Arduino.h>
#include "BMx280_PIO.h"

BMx280_PIO bme(2, 3);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);
    delay(500);
    Serial.println("BMx280_PIO Quick Test");

    if (!bme.begin()) {
        Serial.println("Sensor not found! Check wiring.");
        while (1) delay(1000);
    }
    Serial.print("Detected: ");
    Serial.print(bme.isBME280() ? "BME280" : "BMP280");
    Serial.print(" (chip ID 0x");
    Serial.print(bme.getChipID(), HEX);
    Serial.println(")");
}

void loop() {
    bme.takeForcedMeasurement();
    float t, p, h;
    bme.readAll(&t, &p, &h);
    Serial.print("T="); Serial.print(t, 2); Serial.print("°C  P="); Serial.print(p, 2); Serial.print("hPa");
    if (bme.isBME280()) { Serial.print("  H="); Serial.print(h, 2); Serial.print("%"); }
    Serial.println();
    delay(2000);
}

#endif /* BMX280_STANDALONE_TEST */

#ifdef BMX280PIO_RP2040_STANDALONE_TEST
#include <Arduino.h>
#include "BMx280PIO_RP2040.h"

BMx280PIO_RP2040 bme(4, 5);

void setup() {
    Serial.begin(115200); while (!Serial) delay(100); delay(500);
    Serial.println("BMx280PIO_RP2040 Test");
    if (!bme.begin()) { Serial.println("Sensor fail!"); while (1); }
    Serial.print("Sensor: "); Serial.println(bme.isBME280() ? "BME280" : "BMP280");
    bme.setTemperatureOversampling(BME280_OS_2X);
    bme.setPressureOversampling(BME280_OS_4X);
    bme.setFilter(BME280_FILTER_4);
    bme.setMode(BME280_MODE_NORMAL);
}

void loop() {
    float t, p, h;
    bme.readAll(&t, &p, &h);
    Serial.print("T="); Serial.print(t, 2); Serial.print("C P="); Serial.print(p, 2);
    Serial.print("hPa Alt="); Serial.print(44330.0f*(1.0f-pow(p/1013.25f,0.1903f)),2); Serial.println("m");
    delay(2000);
}
#endif

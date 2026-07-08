/*
 * PIO+DMA Auto-Scan Example for BMx280PIO_RP2040
 *
 * ⚠️ EXPERIMENTAL — The continuous DMA burst mode is not yet fully
 * validated on hardware. Use basic_reading or forced_mode for
 * production. This example demonstrates the API and compiles
 * correctly, but readAllAsync() may return incorrect data.
 *
 * For a working PIO+DMA demonstration, see the pio_dma_hybrid example.
 *
 * Wiring: Sensor VCC→3.3V, GND→GND, SDA→GPIO2, SCL→GPIO3
 */

#include <Arduino.h>
#include "BMx280PIO_RP2040.h"

BMx280PIO_RP2040 bme(2, 3);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(100);
    delay(500);
    Serial.println("BMx280 PIO+DMA Auto-Scan (experimental)");

    if (!bme.begin()) { Serial.println("Sensor fail!"); while(1); }
    if (!bme.beginPIO(pio0)) { Serial.println("PIO fail!"); while(1); }
    if (!bme.beginAutoScan(1000)) { Serial.println("DMA fail!"); while(1); }
    Serial.println("Auto-scan started");
    delay(1500);
}

void loop() {
    float t, p, h;
    bme.readAllAsync(&t, &p, &h);
    Serial.print("T="); Serial.print(t,2);
    Serial.print(" P="); Serial.println(p,2);
    delay(1000);
}

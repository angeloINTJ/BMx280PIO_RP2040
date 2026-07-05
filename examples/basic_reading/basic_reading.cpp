/*
 * Basic Reading Example for BME280_PIO
 *
 * Reads temperature, pressure, and humidity from a BME280 sensor
 * using PIO-based I2C on the RP2040 and prints the values to Serial.
 *
 * Wiring:
 *   BME280 VCC → 3.3V
 *   BME280 GND → GND
 *   BME280 SDA → GPIO4 (default, can be changed)
 *   BME280 SCL → GPIO5 (default, can be changed)
 *
 * Expected output (Serial Monitor, 9600 baud):
 *   BME280 PIO Test
 *   Temperature: 23.45 °C
 *   Pressure: 1013.25 hPa
 *   Humidity: 45.67 %
 */

#include <Arduino.h>
#include "BME280_PIO.h"

// Define I2C pins (can be any GPIO pins)
#define SDA_PIN 4
#define SCL_PIN 5

// Create sensor instance
// Parameters: SDA pin, SCL pin, I2C address (optional, default 0x76)
BME280_PIO bme(SDA_PIN, SCL_PIN);

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ; // Wait for Serial connection (native USB)
    }

    Serial.println("BME280 PIO Test");
    Serial.println("================");

    // Initialize the sensor
    if (!bme.begin()) {
        Serial.println("ERROR: Could not find BME280 sensor!");
        Serial.println("Check your wiring:");
        Serial.println("  VCC -> 3.3V");
        Serial.println("  GND -> GND");
        Serial.println("  SDA -> GPIO4");
        Serial.println("  SCL -> GPIO5");
        while (1) {
            delay(1000);
        }
    }

    Serial.print("Chip ID: 0x");
    Serial.println(bme.getChipID(), HEX);

    // Configure for maximum accuracy (optional)
    bme.setTemperatureOversampling(BME280_OS_2X);
    bme.setPressureOversampling(BME280_OS_4X);
    bme.setHumidityOversampling(BME280_OS_1X);
    bme.setFilter(BME280_FILTER_4);

    // Set normal mode for continuous measurement
    bme.setMode(BME280_MODE_NORMAL);

    Serial.println("Sensor initialized successfully!");
    Serial.println();
}

void loop() {
    // Read all sensor values in one efficient I2C burst
    float temperature, pressure, humidity;
    bme.readAll(&temperature, &pressure, &humidity);

    // Print with 2 decimal places
    Serial.print("Temperature: ");
    Serial.print(temperature, 2);
    Serial.println(" °C");

    Serial.print("Pressure:    ");
    Serial.print(pressure, 2);
    Serial.println(" hPa");

    Serial.print("Humidity:    ");
    Serial.print(humidity, 2);
    Serial.println(" %");

    // Calculate approximate altitude (standard atmosphere)
    // h = 44330 * (1 - (P / P0)^(1/5.255))
    float altitude = 44330.0f * (1.0f - pow(pressure / 1013.25f, 0.1903f));
    Serial.print("Altitude:    ");
    Serial.print(altitude, 2);
    Serial.println(" m");

    Serial.println("------------------------");

    delay(2000); // Read every 2 seconds
}

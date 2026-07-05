/*
 * Forced Mode Example for BME280_PIO
 *
 * Demonstrates the use of Forced Mode for low-power operation.
 * In Forced Mode, the sensor takes a single measurement and then
 * returns to Sleep mode, consuming minimal power.
 *
 * This is ideal for battery-powered weather stations and IoT sensors
 * that only need periodic readings.
 *
 * Wiring:
 *   BME280 VCC → 3.3V
 *   BME280 GND → GND
 *   BME280 SDA → GPIO4
 *   BME280 SCL → GPIO5
 */

#include <Arduino.h>
#include "BME280_PIO.h"

// Define I2C pins
#define SDA_PIN 4
#define SCL_PIN 5

// Measurement interval in milliseconds
#define INTERVAL_MS 60000  // 1 minute

BME280_PIO bme(SDA_PIN, SCL_PIN);

void setup() {
    Serial.begin(9600);
    while (!Serial) {
        ; // Wait for Serial
    }

    Serial.println("BME280 PIO - Forced Mode Example");
    Serial.println("================================");

    if (!bme.begin()) {
        Serial.println("ERROR: Could not find BME280!");
        while (1) delay(1000);
    }

    // Configure oversampling
    // Higher oversampling = better accuracy but longer measurement time
    bme.setTemperatureOversampling(BME280_OS_1X);
    bme.setPressureOversampling(BME280_OS_1X);
    bme.setHumidityOversampling(BME280_OS_1X);
    bme.setFilter(BME280_FILTER_OFF); // No filtering in forced mode

    // Sensor starts in SLEEP mode by default
    Serial.println("Sensor ready. Taking measurements every 60 seconds...");
    Serial.println();
}

void loop() {
    Serial.print("Taking measurement... ");

    // Trigger a single forced measurement
    // This method blocks until the measurement completes
    if (bme.takeForcedMeasurement()) {
        Serial.println("done!");

        // Read the values (sensor is now back in sleep mode)
        float temperature = bme.readTemperature();
        float pressure    = bme.readPressure();
        float humidity    = bme.readHumidity();

        Serial.print("Temperature: ");
        Serial.print(temperature, 2);
        Serial.println(" °C");

        Serial.print("Pressure:    ");
        Serial.print(pressure, 2);
        Serial.println(" hPa");

        Serial.print("Humidity:    ");
        Serial.print(humidity, 2);
        Serial.println(" %");

        // Dew point approximation (Magnus formula)
        float a = 17.27f;
        float b = 237.7f;
        float gamma = (a * temperature) / (b + temperature) + log(humidity / 100.0f);
        float dewPoint = (b * gamma) / (a - gamma);
        Serial.print("Dew Point:   ");
        Serial.print(dewPoint, 2);
        Serial.println(" °C");

    } else {
        Serial.println("FAILED!");
    }

    Serial.print("Sleeping for ");
    Serial.print(INTERVAL_MS / 1000);
    Serial.println(" seconds...");
    Serial.println("------------------------");

    // The sensor is already in sleep mode (automatic after forced measurement)
    // We just need to wait for the next measurement interval
    delay(INTERVAL_MS);
}

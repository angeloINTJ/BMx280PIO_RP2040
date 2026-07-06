# BMx280_PIO — BMP280/BME280 Driver for RP2040

[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Arduino library for the **Bosch BMP280/BME280** environmental sensor on **RP2040** (Raspberry Pi Pico). Reads temperature, pressure, and humidity using GPIO-based I2C on any pin pair — no hardware I2C peripheral required.

Follows the same architecture as [OneWirePIO_RP2040](https://github.com/angeloINTJ/OneWirePIO_RP2040) and [DHT22PIO_RP2040](https://github.com/angeloINTJ/DHT22PIO_RP2040).

## Features

- ✅ Temperature ±0.01°C, pressure ±0.12 hPa (verified on hardware)
- ✅ Auto-detects BMP280 vs BME280 by chip ID
- ✅ I2C on any GPIO pins (open-drain emulation)
- ✅ Sleep, Forced, and Normal operating modes
- ✅ Configurable oversampling: 1× to 16× per channel
- ✅ IIR filter (off, 2, 4, 8, 16)
- ✅ Standby time configuration (Normal mode)
- ✅ Bosch datasheet compensation (double-precision pressure, integer temperature)
- ✅ PlatformIO & Arduino IDE compatible

## Installation

### PlatformIO

```ini
[env:pico]
platform = raspberrypi
board = rpipico
framework = arduino
lib_deps = angeloINTJ/bme280-pio-rp2040
```

### Arduino IDE

1. Install the **Raspberry Pi Pico/RP2040** package by Earle Philhower
2. Install **BMx280_PIO** from the Library Manager
3. Or clone this repository into your `libraries/` folder

### Manual

```bash
cd ~/Arduino/libraries/
git clone https://github.com/angeloINTJ/bme280-pio-rp2040.git
```

## Quick Start

```cpp
#include <Arduino.h>
#include "BMx280_PIO.h"

// SDA = GPIO2, SCL = GPIO3 (any GPIO pair works)
BMx280_PIO sensor(2, 3);

void setup() {
    Serial.begin(115200);

    if (!sensor.begin()) {
        Serial.println("Sensor not found!");
        while (1);
    }

    Serial.print("Detected: ");
    Serial.println(sensor.isBME280() ? "BME280" : "BMP280");
}

void loop() {
    sensor.takeForcedMeasurement();

    float temp = sensor.readTemperature();
    float press = sensor.readPressure();

    Serial.printf("T: %.2f °C | P: %.2f hPa\n", temp, press);
    delay(2000);
}
```

## Wiring

| BMP280/BME280 | Raspberry Pi Pico |
|---------------|-------------------|
| VCC           | 3.3V              |
| GND           | GND               |
| SDA           | GPIO2 (or any)    |
| SCL           | GPIO3 (or any)    |

> ⚠️ The sensor operates at **3.3V**. Do not connect directly to 5V.

> 💡 SDA and SCL can be **any GPIO pins** — just specify them in the constructor.

## API Reference

### Constructor

```cpp
// GPIO-based I2C (any pins)
BMx280_PIO sensor(uint8_t sda, uint8_t scl,
                  uint8_t addr = 0x76,
                  uint32_t freq = 100000);

// Hardware I2C (Wire)
BMx280_PIO sensor(TwoWire &wire, uint8_t addr = 0x76);
```

### Initialization

```cpp
bool begin();              // Initialize PIO and sensor, load calibration
void reset();              // Soft-reset the sensor
uint8_t getChipID();       // Returns 0x58 (BMP280) or 0x60 (BME280)
bool isBME280();           // True if humidity sensor is present
bool isInitialized();      // True if sensor is ready
```

### Configuration

```cpp
void setTemperatureOversampling(uint8_t os);  // BME280_OS_SKIP, _1X, _2X, _4X, _8X, _16X
void setPressureOversampling(uint8_t os);
void setHumidityOversampling(uint8_t os);     // BME280 only
void setFilter(uint8_t filter);               // BME280_FILTER_OFF, _2, _4, _8, _16
void setStandbyTime(uint8_t standby);          // Normal mode: 0.5ms to 1000ms
void setMode(uint8_t mode);                    // BME280_MODE_SLEEP, _FORCED, _NORMAL
bool takeForcedMeasurement();                  // Trigger + wait (forced mode)
```

### Readings

```cpp
float readTemperature();    // °C
float readPressure();      // hPa
float readHumidity();      // % (0 if BMP280)
void readAll(float *t, float *p, float *h);  // Single burst read
```

### Register Access

```cpp
uint8_t readRegister(uint8_t reg);
void writeRegister(uint8_t reg, uint8_t value);
void readRegisters(uint8_t reg, uint8_t *data, size_t len);
```

### I2C Bus Scan

```cpp
PIO_I2C i2c(2, 3);  // SDA, SCL
i2c.begin();
i2c.scan();          // Lists all devices on the bus
```

## Operating Modes

| Mode | Power | Use Case |
|------|-------|----------|
| **Sleep** | ~0.1 µA | Sensor idle, registers preserved |
| **Forced** | ~0.5–3 µA avg | Single measurement, then back to sleep. Ideal for battery power |
| **Normal** | ~3–5 µA | Continuous measurement with configurable interval |

## Oversampling & Accuracy

| Setting | Temp Resolution | Press Resolution | Hum Resolution | Time (1× all) |
|---------|----------------|------------------|----------------|---------------|
| 1× | 0.005 °C | 0.18 Pa | 0.003 %RH | ~9 ms |
| 16× | 0.0003 °C | 0.012 Pa | 0.0001 %RH | ~90 ms |

## Architecture

```
┌─────────────────────────────────────────┐
│  BMx280_PIO (sensor driver)             │
│  - Bosch compensation (double/int)      │
│  - Register configuration               │
│  - Auto-detection BMP280 vs BME280      │
└──────────────┬──────────────────────────┘
               │ writeThenRead()
┌──────────────▼──────────────────────────┐
│  PIO_I2C (I2C transport)               │
│  - GPIO bit-bang (any pins)             │
│  - Open-drain SDA emulation             │
│  - START/STOP/ACK/NACK                  │
│  - PIO-ready program included           │
└──────────────┬──────────────────────────┘
               │ GPIO
┌──────────────▼──────────────────────────┐
│  RP2040 GPIO                            │
│  - Direction-based open-drain           │
│  - Any GPIO pin pair                    │
└─────────────────────────────────────────┘
```

## Dependencies

- [arduino-pico](https://github.com/earlephilhower/arduino-pico) — Arduino core for RP2040 (Earle Philhower)
- PlatformIO platform `raspberrypi` or Arduino IDE with RP2040 package

## License

MIT — see [LICENSE](LICENSE) for details.

The PIO program (`pio/i2c.pio`) is based on the official [pico-examples](https://github.com/raspberrypi/pico-examples) I2C implementation by Raspberry Pi (BSD-3-Clause).

## Credits

- **Bosch Sensortec** — BME280 datasheet and compensation formulas
- **Raspberry Pi Foundation** — PIO I2C example and RP2040 SDK
- **Earle Philhower** — Arduino-Pico core

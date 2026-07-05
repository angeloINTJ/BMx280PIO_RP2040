/*
 * BME280_PIO.h - BME280 sensor driver using PIO-based I2C for RP2040
 *
 * Supports temperature, humidity, and pressure readings from the
 * Bosch BME280 environmental sensor.
 *
 * Communication: I2C via PIO state machine (PIO_I2C class).
 * Compensation: Bosch BME280 datasheet formulas (Appendix A).
 *
 * Features:
 *   - Temperature, pressure, and humidity readings
 *   - Sleep, Forced, and Normal operating modes
 *   - Configurable oversampling (1x to 16x)
 *   - IIR filter configuration
 *   - Standby time configuration (Normal mode)
 *   - Register-level access for advanced use
 */

#ifndef BME280_PIO_H
#define BME280_PIO_H

#include <Arduino.h>
#include "PIO_I2C.h"

// ─── I2C Addresses ────────────────────────────────────────────────────────
#define BME280_ADDR_PRIMARY     0x76  // SDO pin connected to GND
#define BME280_ADDR_SECONDARY   0x77  // SDO pin connected to VDDIO

// ─── Register Map ─────────────────────────────────────────────────────────
#define BME280_REG_DIG_T1       0x88  // Temperature calibration (unsigned short)
#define BME280_REG_DIG_T2       0x8A  // Temperature calibration (signed short)
#define BME280_REG_DIG_T3       0x8C  // Temperature calibration (signed short)
#define BME280_REG_DIG_P1       0x8E  // Pressure calibration (unsigned short)
#define BME280_REG_DIG_P2       0x90  // Pressure calibration (signed short)
#define BME280_REG_DIG_P3       0x92  // Pressure calibration (signed short)
#define BME280_REG_DIG_P4       0x94  // Pressure calibration (signed short)
#define BME280_REG_DIG_P5       0x96  // Pressure calibration (signed short)
#define BME280_REG_DIG_P6       0x98  // Pressure calibration (signed short)
#define BME280_REG_DIG_P7       0x9A  // Pressure calibration (signed short)
#define BME280_REG_DIG_P8       0x9C  // Pressure calibration (signed short)
#define BME280_REG_DIG_P9       0x9E  // Pressure calibration (signed short)
#define BME280_REG_DIG_H1       0xA1  // Humidity calibration (unsigned char)
#define BME280_REG_DIG_H2       0xE1  // Humidity calibration (signed short)
#define BME280_REG_DIG_H3       0xE3  // Humidity calibration (unsigned char)
#define BME280_REG_DIG_H4       0xE4  // Humidity calibration (signed short, split)
#define BME280_REG_DIG_H5       0xE5  // Humidity calibration (signed short, split)
#define BME280_REG_DIG_H6       0xE7  // Humidity calibration (signed char)
#define BME280_REG_CHIP_ID      0xD0  // Chip ID register (should read 0x60)
#define BME280_REG_RESET        0xE0  // Soft reset register
#define BME280_REG_CTRL_HUM     0xF2  // Humidity oversampling control
#define BME280_REG_STATUS       0xF3  // Status register
#define BME280_REG_CTRL_MEAS    0xF4  // Measurement control register
#define BME280_REG_CONFIG       0xF5  // Configuration register
#define BME280_REG_PRESS_MSB    0xF7  // Pressure data MSB
#define BME280_REG_PRESS_LSB    0xF8  // Pressure data LSB
#define BME280_REG_PRESS_XLSB   0xF9  // Pressure data XLSB
#define BME280_REG_TEMP_MSB     0xFA  // Temperature data MSB
#define BME280_REG_TEMP_LSB     0xFB  // Temperature data LSB
#define BME280_REG_TEMP_XLSB    0xFC  // Temperature data XLSB
#define BME280_REG_HUM_MSB      0xFD  // Humidity data MSB
#define BME280_REG_HUM_LSB      0xFE  // Humidity data LSB

// ─── Chip ID ──────────────────────────────────────────────────────────────
#define BME280_CHIP_ID          0x60

// ─── Reset ────────────────────────────────────────────────────────────────
#define BME280_RESET_VALUE      0xB6  // Write to RESET register to soft-reset

// ─── Sensor Modes (CTRL_MEAS[1:0]) ────────────────────────────────────────
#define BME280_MODE_SLEEP       0x00
#define BME280_MODE_FORCED      0x01  // Also 0x02 — single measurement then sleep
#define BME280_MODE_NORMAL      0x03  // Continuous measurement

// ─── Oversampling Options ─────────────────────────────────────────────────
#define BME280_OS_SKIP          0x00  // Skip measurement
#define BME280_OS_1X            0x01
#define BME280_OS_2X            0x02
#define BME280_OS_4X            0x03
#define BME280_OS_8X            0x04
#define BME280_OS_16X           0x05

// ─── Filter Coefficients (CONFIG[4:2]) ────────────────────────────────────
#define BME280_FILTER_OFF       0x00
#define BME280_FILTER_2         0x01
#define BME280_FILTER_4         0x02
#define BME280_FILTER_8         0x03
#define BME280_FILTER_16        0x04

// ─── Standby Time (CONFIG[7:5]) - Normal mode only ────────────────────────
#define BME280_STANDBY_0_5MS    0x00
#define BME280_STANDBY_62_5MS   0x01
#define BME280_STANDBY_125MS    0x02
#define BME280_STANDBY_250MS    0x03
#define BME280_STANDBY_500MS    0x04
#define BME280_STANDBY_1000MS   0x05
#define BME280_STANDBY_10MS     0x06  // (actually 10 ms)
#define BME280_STANDBY_20MS     0x07  // (actually 20 ms)

/*
 * BME280_PIO - Main sensor driver class.
 *
 * Usage:
 *   BME280_PIO sensor(4, 5);  // SDA = GPIO4, SCL = GPIO5
 *   sensor.begin();
 *   float temp = sensor.readTemperature();
 *   float press = sensor.readPressure();
 *   float hum = sensor.readHumidity();
 */
class BME280_PIO {
public:
    /*
     * Constructor.
     *   sdaPin  - GPIO pin for I2C SDA
     *   sclPin  - GPIO pin for I2C SCL
     *   addr    - I2C address (default: 0x76, primary)
     *   freq    - I2C frequency in Hz (default: 100 kHz)
     */
    BME280_PIO(uint8_t sdaPin, uint8_t sclPin,
               uint8_t addr = BME280_ADDR_PRIMARY,
               uint32_t freq = PIO_I2C_FREQ_STANDARD);

    /*
     * Initialize the sensor.
     * - Configures PIO I2C
     * - Reads and validates chip ID
     * - Loads calibration data
     * - Applies default settings (oversampling 1x, filter off)
     * Returns true on success.
     */
    bool begin();

    // ─── Configuration ────────────────────────────────────────────────

    /*
     * Set temperature oversampling.
     * Must be called before begin() or while in SLEEP mode.
     */
    void setTemperatureOversampling(uint8_t os);

    /*
     * Set pressure oversampling.
     * Must be called before begin() or while in SLEEP mode.
     */
    void setPressureOversampling(uint8_t os);

    /*
     * Set humidity oversampling.
     * Must be called before begin() or while in SLEEP mode.
     */
    void setHumidityOversampling(uint8_t os);

    /*
     * Set IIR filter coefficient.
     * Must be called before begin() or while in SLEEP mode.
     */
    void setFilter(uint8_t filter);

    /*
     * Set standby time (only used in NORMAL mode).
     * Must be called before begin() or while in SLEEP mode.
     */
    void setStandbyTime(uint8_t standby);

    /*
     * Set sensor operating mode.
     *   BME280_MODE_SLEEP  - no measurements
     *   BME280_MODE_FORCED - single measurement then sleep
     *   BME280_MODE_NORMAL - continuous measurement
     */
    void setMode(uint8_t mode);

    /*
     * Trigger a single measurement in FORCED mode.
     * Blocks until the measurement is complete.
     * Returns true on success.
     */
    bool takeForcedMeasurement();

    // ─── Sensor Readings ──────────────────────────────────────────────

    /*
     * Read compensated temperature.
     * Returns temperature in degrees Celsius.
     */
    float readTemperature();

    /*
     * Read compensated pressure.
     * Returns pressure in hectopascals (hPa).
     */
    float readPressure();

    /*
     * Read compensated humidity.
     * Returns relative humidity in percent (%).
     */
    float readHumidity();

    /*
     * Read all three values in one I2C burst (more efficient).
     * The values are written to the provided pointers.
     * Pass nullptr for any value you don't need.
     */
    void readAll(float *temperature, float *pressure, float *humidity);

    // ─── Raw Register Access ──────────────────────────────────────────

    /*
     * Read a single register value.
     */
    uint8_t readRegister(uint8_t reg);

    /*
     * Write a single register value.
     */
    void writeRegister(uint8_t reg, uint8_t value);

    /*
     * Read multiple consecutive registers.
     */
    void readRegisters(uint8_t reg, uint8_t *data, size_t len);

    /*
     * Get the chip ID (should be 0x60 for BME280).
     */
    uint8_t getChipID();

    /*
     * Perform a soft reset of the sensor.
     */
    void reset();

    /*
     * Check if the sensor is initialized and ready.
     */
    bool isInitialized() const { return _initialized; }

private:
    PIO_I2C  _i2c;
    uint8_t  _addr;
    bool     _initialized;

    // Configuration settings (applied at begin)
    uint8_t  _osrs_t;   // Temperature oversampling
    uint8_t  _osrs_p;   // Pressure oversampling
    uint8_t  _osrs_h;   // Humidity oversampling
    uint8_t  _filter;   // IIR filter
    uint8_t  _standby;  // Standby time (normal mode)
    uint8_t  _mode;     // Operating mode

    // Calibration data (loaded from sensor at begin)
    // Temperature
    uint16_t _dig_T1;
    int16_t  _dig_T2;
    int16_t  _dig_T3;

    // Pressure
    uint16_t _dig_P1;
    int16_t  _dig_P2;
    int16_t  _dig_P3;
    int16_t  _dig_P4;
    int16_t  _dig_P5;
    int16_t  _dig_P6;
    int16_t  _dig_P7;
    int16_t  _dig_P8;
    int16_t  _dig_P9;

    // Humidity
    uint8_t  _dig_H1;
    int16_t  _dig_H2;
    uint8_t  _dig_H3;
    int16_t  _dig_H4;
    int16_t  _dig_H5;
    int8_t   _dig_H6;

    // Compensation variable (t_fine) — shared between temperature,
    // pressure, and humidity compensation.
    int32_t  _t_fine;

    // Load calibration data from sensor registers
    bool _loadCalibration();

    // Apply configuration registers
    void _applyConfig();

    // Read raw (uncompensated) sensor data
    void _readRaw(int32_t *temp_raw, int32_t *press_raw, int32_t *hum_raw);

    // Bosch compensation formulas
    float _compensateTemperature(int32_t adc_T);
    float _compensatePressure(int32_t adc_P);
    float _compensateHumidity(int32_t adc_H);

    // Helper: sign-extend a 16-bit value from two 8-bit register reads
    static int16_t _concatBytes(uint8_t msb, uint8_t lsb) {
        return (int16_t)((uint16_t)msb << 8 | (uint16_t)lsb);
    }

    // Helper: get measurement time in ms based on oversampling settings
    uint8_t _getMaxMeasurementTime();
};

#endif // BME280_PIO_H

/**
 * @file BMx280PIO_RP2040.h
 * @brief BMP280/BME280 temperature, pressure, and humidity sensor driver for RP2040.
 *
 * Supports GPIO bit-bang I2C on any pin pair, hardware I2C (Wire), and
 * PIO+DMA burst reads. Auto-detects BMP280 vs BME280 by chip ID.
 *
 * @author angeloINTJ
 * @license MIT
 */

#ifndef BMX280PIO_RP2040_H
#define BMX280PIO_RP2040_H

#include <Arduino.h>
#include <Wire.h>
#include "PIO_I2C.h"

/// @name BME280 Register Addresses
/// @{
#define BME280_ADDR_PRIMARY     0x76
#define BME280_ADDR_SECONDARY   0x77
#define BME280_REG_PRESS_MSB    0xF7
#define BME280_REG_CHIP_ID      0xD0
#define BME280_REG_RESET        0xE0
#define BME280_REG_CTRL_HUM     0xF2
#define BME280_REG_STATUS       0xF3
#define BME280_REG_CTRL_MEAS    0xF4
#define BME280_REG_CONFIG       0xF5
/// @}

/// @name Chip ID Values
/// @{
#define BMP280_CHIP_ID          0x58
#define BME280_CHIP_ID_VALUE    0x60
#define BME280_RESET_VALUE      0xB6
/// @}

/// @name Operating Modes
/// @{
#define BME280_MODE_SLEEP       0x00
#define BME280_MODE_FORCED      0x01
#define BME280_MODE_NORMAL      0x03
/// @}

/// @name Oversampling Settings
/// @{
#define BME280_OS_SKIP          0x00
#define BME280_OS_1X            0x01
#define BME280_OS_2X            0x02
#define BME280_OS_4X            0x03
#define BME280_OS_8X            0x04
#define BME280_OS_16X           0x05
/// @}

/// @name IIR Filter Settings
/// @{
#define BME280_FILTER_OFF       0x00
#define BME280_FILTER_4         0x02
/// @}

/// @name Standby Time Settings (Normal Mode)
/// @{
#define BME280_STANDBY_250MS    0x03
#define BME280_STANDBY_500MS    0x04
#define BME280_STANDBY_1000MS   0x05
/// @}

/**
 * @brief BMP280/BME280 sensor driver class.
 *
 * Provides temperature, pressure, and humidity (BME280 only) readings
 * using Bosch compensation formulas. Supports three transport modes:
 * - GPIO bit-bang I2C on any pin pair
 * - Hardware I2C (Wire)
 * - PIO+DMA burst reads (zero-CPU-overhead I2C)
 */
class BMx280PIO_RP2040 {
public:
    /**
     * @brief Construct using hardware I2C (Wire).
     * @param wire TwoWire instance (e.g., Wire, Wire1).
     * @param addr I2C address (0x76 or 0x77).
     */
    BMx280PIO_RP2040(TwoWire &wire, uint8_t addr = BME280_ADDR_PRIMARY);

    /**
     * @brief Construct using GPIO bit-bang I2C on any pin pair.
     * @param sda GPIO pin for SDA.
     * @param scl GPIO pin for SCL.
     * @param addr I2C address (0x76 or 0x77).
     * @param freq I2C clock frequency in Hz (default 100 kHz).
     */
    BMx280PIO_RP2040(uint8_t sda, uint8_t scl, uint8_t addr = BME280_ADDR_PRIMARY, uint32_t freq = 100000);
    ~BMx280PIO_RP2040();

    /// @name Initialization
    /// @{

    /**
     * @brief Initialize the sensor and load calibration data.
     * @return true if sensor detected and calibration loaded.
     */
    bool begin();

    /**
     * @brief Load the PIO program onto the specified PIO block.
     *
     * After calling this, read operations use PIO+DMA burst transfers
     * instead of GPIO bit-bang. Must be called after begin().
     *
     * @param pio PIO instance (pio0 or pio1).
     * @return true if PIO program loaded successfully.
     */
    bool beginPIO(PIO pio = pio0);
    /// @}

    /// @name Sensor Configuration
    /// @{

    /// @brief Set temperature oversampling (BME280_OS_1X to _16X).
    void setTemperatureOversampling(uint8_t os) { _osrs_t = os & 0x07; if (_init) _applyConfig(); }
    /// @brief Set pressure oversampling.
    void setPressureOversampling(uint8_t os)    { _osrs_p = os & 0x07; if (_init) _applyConfig(); }
    /// @brief Set humidity oversampling (BME280 only).
    void setHumidityOversampling(uint8_t os)    { _osrs_h = os & 0x07; if (_init) _applyConfig(); }
    /// @brief Set IIR filter coefficient.
    void setFilter(uint8_t filter)              { _filter = filter & 0x07; if (_init) _applyConfig(); }
    /// @brief Set standby time for Normal mode.
    void setStandbyTime(uint8_t standby)        { _standby = standby & 0x07; if (_init) _applyConfig(); }

    /**
     * @brief Set the sensor operating mode.
     * @param mode BME280_MODE_SLEEP, _FORCED, or _NORMAL.
     */
    void setMode(uint8_t mode);

    /**
     * @brief Trigger a single measurement in Forced mode and wait for completion.
     * @return true if measurement completed successfully.
     */
    bool takeForcedMeasurement();
    /// @}

    /// @name Direct Register Access
    /// @{
    uint8_t readRegister(uint8_t reg);
    void    writeRegister(uint8_t reg, uint8_t value);
    void    readRegisters(uint8_t reg, uint8_t *data, size_t len);
    /// @}

    /// @name Sensor Readings
    /// @{
    float readTemperature();                    ///< Temperature in °C.
    float readPressure();                       ///< Pressure in hPa.
    float readHumidity();                       ///< Humidity in %RH (0 if BMP280).
    void  readAll(float *t, float *p, float *h);///< Single burst: read temperature, pressure, humidity.
    /// @}

    /// @name PIO+DMA Burst Read
    /// @{

    /**
     * @brief Start continuous DMA-driven auto-scan (experimental).
     *
     * The PIO+DMA engine reads the sensor periodically with zero CPU
     * involvement. Call readAllAsync() to get the latest data.
     *
     * @param period_ms Sampling period in milliseconds.
     * @return true if auto-scan started successfully.
     */
    bool beginAutoScan(uint32_t period_ms = 1000);
    void stopAutoScan();                        ///< Stop DMA auto-scan and restore GPIO.

    /**
     * @brief Read latest data from the DMA ring buffer.
     *
     * Extracts the most recent sensor data from the background DMA buffer
     * and runs Bosch compensation. No I2C communication occurs here.
     */
    void readAllAsync(float *t, float *p, float *h);
    /// @}

    /// @name Identification
    /// @{
    uint8_t getChipID();                        ///< Returns 0x58 (BMP280) or 0x60 (BME280).
    bool isBME280() const { return _is_bme; }   ///< True if humidity sensor is present.
    bool isInitialized() const { return _init; }///< True if sensor is ready.
    /// @}

private:
    enum Transport { TRANSPORT_WIRE, TRANSPORT_PIO };
    Transport _transport;
    TwoWire  *_wire;
    PIO_I2C  *_pio_i2c;
    uint8_t   _addr;
    bool      _init, _is_bme;
    uint8_t   _osrs_t, _osrs_p, _osrs_h, _filter, _standby, _mode;

    /// @name Calibration Coefficients
    /// @{
    uint16_t _T1; int16_t _T2, _T3;
    uint16_t _P1; int16_t _P2, _P3, _P4, _P5, _P6, _P7, _P8, _P9;
    uint8_t  _H1; int16_t _H2; uint8_t _H3; int16_t _H4, _H5; int8_t _H6;
    /// @}

    int32_t  _t_fine;               ///< Intermediate temperature value for compensation.
    uint32_t _raw_async[11];        ///< DMA ring buffer (3 ACKs + 8 data bytes).
    uint8_t  _raw_bytes[8];         ///< Extracted data bytes from ring buffer.

    bool _i2c_write(uint8_t reg, const uint8_t *data, size_t len);
    bool _i2c_read(uint8_t reg, uint8_t *data, size_t len);
    bool _loadCalibration();
    void _applyConfig();
    void _readRaw(int32_t *t, int32_t *p, int32_t *h);
    uint8_t _measTime();
};

#endif

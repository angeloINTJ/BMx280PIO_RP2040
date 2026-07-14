/**
 * @file BMx280PIO_RP2040.h
 * @brief BMP280/BME280 sensor driver for RP2040 — I2C via WirePIO or hardware Wire.
 *
 * Auto-detects BMP280 vs BME280 by chip ID. Uses WirePIO for PIO+DMA
 * I2C on any GPIO pin pair, or hardware I2C via TwoWire&.
 *
 * @note GPIO1/GPIO2 conflict with UART0 (Serial1). These pins can still be
 *       used for I2C with USB CDC Serial (Serial) active.
 *
 * @author angeloINTJ
 * @license MIT
 */
#ifndef BMX280PIO_RP2040_H
#define BMX280PIO_RP2040_H

#include <Arduino.h>
#include <Wire.h>
#include <hardware/pio.h>
#include <pico/critical_section.h>

class WirePIO;

// =============================================================================
// I2C Addresses
// =============================================================================
#define BME280_ADDR_PRIMARY     0x76
#define BME280_ADDR_SECONDARY   0x77

// =============================================================================
// Register Map
// =============================================================================
#define BME280_REG_PRESS_MSB    0xF7
#define BME280_REG_CHIP_ID      0xD0
#define BME280_REG_RESET        0xE0
#define BME280_REG_CTRL_HUM     0xF2
#define BME280_REG_STATUS       0xF3
#define BME280_REG_CTRL_MEAS    0xF4
#define BME280_REG_CONFIG       0xF5

// =============================================================================
// Chip ID Values
// =============================================================================
#define BMP280_CHIP_ID          0x58
#define BME280_CHIP_ID_VALUE    0x60
#define BME280_RESET_VALUE      0xB6

// =============================================================================
// Operating Modes
// =============================================================================
#define BME280_MODE_SLEEP       0x00
#define BME280_MODE_FORCED      0x01
#define BME280_MODE_NORMAL      0x03

// =============================================================================
// Oversampling Options
// =============================================================================
#define BME280_OS_SKIP          0x00
#define BME280_OS_1X            0x01
#define BME280_OS_2X            0x02
#define BME280_OS_4X            0x03
#define BME280_OS_8X            0x04
#define BME280_OS_16X           0x05

// =============================================================================
// IIR Filter / Standby Options
// =============================================================================
#define BME280_FILTER_OFF       0x00
#define BME280_FILTER_2         0x01
#define BME280_FILTER_4         0x02
#define BME280_FILTER_8         0x03
#define BME280_FILTER_16        0x04

#define BME280_STANDBY_250MS    0x03
#define BME280_STANDBY_500MS    0x04
#define BME280_STANDBY_1000MS   0x05

// =============================================================================
// Measurement Timing Constants (datasheet §9.2)
// =============================================================================
#define BMX280_MEAS_TIME_BASE_US      1250    ///< T_setup base time
#define BMX280_MEAS_TIME_T_COEFF_US   2300    ///< per oversampling unit for temperature
#define BMX280_MEAS_TIME_P_COEFF_US   2300    ///< per oversampling unit for pressure
#define BMX280_MEAS_TIME_H_COEFF_US   2300    ///< per oversampling unit for humidity
#define BMX280_MEAS_TIME_OVERHEAD_US  575     ///< fixed overhead per measurement

#define BMX280_CTRL_MEAS_RETRY_COUNT  3       ///< max retries for CTRL_MEAS write
#define BMX280_CTRL_MEAS_RETRY_US     500     ///< backoff between CTRL_MEAS retries

#define BMX280_PIO_PRIME_READS        5       ///< dummy reads for PIO+DMA cold-start prime
#define BMX280_FORCED_MEAS_TIMEOUT_MS 100     ///< max wait for forced measurement completion

// =============================================================================
// Error Codes
// =============================================================================
enum BMx280Error {
    BMX280_OK = 0,
    BMX280_ERR_NOT_FOUND,         ///< sensor not detected at I2C address
    BMX280_ERR_CHIP_ID,           ///< invalid or unknown chip ID
    BMX280_ERR_CAL_FAIL,          ///< calibration data read failed
    BMX280_ERR_I2C_WRITE,         ///< I2C write (NACK) failure
    BMX280_ERR_I2C_READ,          ///< I2C read failure
    BMX280_ERR_MEAS_TIMEOUT,      ///< measurement did not complete in time
    BMX280_ERR_NOT_INIT,          ///< sensor not initialized
    BMX280_ERR_CTRL_MEAS          ///< CTRL_MEAS register write failed after retries
};

// =============================================================================
// Driver Class
// =============================================================================

class BMx280PIO_RP2040 {
public:
    // --- Constructors ---

    /**
     * @brief Construct using hardware I2C (Wire).
     * @param wire   TwoWire instance (e.g. Wire, Wire1).
     * @param addr   I2C address (0x76 or 0x77).
     */
    BMx280PIO_RP2040(TwoWire &wire, uint8_t addr = BME280_ADDR_PRIMARY);

    /**
     * @brief Construct using PIO+DMA I2C on any GPIO pin pair.
     * @param sda    SDA pin number.
     * @param scl    SCL pin number.
     * @param addr   I2C address (0x76 or 0x77).
     * @param freq   I2C clock frequency in Hz (default 200 kHz).
     * @param pio    PIO block (pio0 or pio1).
     */
    BMx280PIO_RP2040(uint8_t sda, uint8_t scl, uint8_t addr = BME280_ADDR_PRIMARY,
                     uint32_t freq = 200000, PIO pio = pio0);

    ~BMx280PIO_RP2040();

    // --- Initialization ---

    /**
     * @brief Initialize the sensor, load calibration, and start normal mode.
     * @return true on success.
     */
    bool begin();

    /**
     * @brief Initialize with debug output stream.
     * @param debug  Stream for diagnostic output (e.g. &Serial), or nullptr for silent.
     * @return true on success.
     */
    bool begin(Stream *debug);

    /**
     * @brief Re-initialize PIO+DMA on a different PIO block.
     * @param pio   PIO block (pio0 or pio1).
     * @return true if PIO+DMA started successfully.
     */
    bool beginPIO(PIO pio = pio0);

    // --- Configuration ---

    void setTemperatureOversampling(uint8_t os) { _osrs_t = os & 0x07; if (_init) _applyConfig(); }
    void setPressureOversampling(uint8_t os)    { _osrs_p = os & 0x07; if (_init) _applyConfig(); }
    void setHumidityOversampling(uint8_t os)    { _osrs_h = os & 0x07; if (_init) _applyConfig(); }
    void setFilter(uint8_t filter)              { _filter = filter & 0x07; if (_init) _applyConfig(); }
    void setStandbyTime(uint8_t standby)        { _standby = standby & 0x07; if (_init) _applyConfig(); }
    void setMode(uint8_t mode);

    /**
     * @brief Trigger a single forced measurement and wait for completion.
     * @return true if measurement completed successfully.
     */
    bool takeForcedMeasurement();

    // --- Register Access ---

    uint8_t readRegister(uint8_t reg);
    void    writeRegister(uint8_t reg, uint8_t value);
    void    readRegisters(uint8_t reg, uint8_t *data, size_t len);

    // --- Sensor Readings ---

    float readTemperature();   ///< Temperature in °C (NAN if not initialized)
    float readPressure();      ///< Pressure in hPa (NAN if not initialized)
    float readHumidity();      ///< Relative humidity in % (0 if BMP280)
    void  readAll(float *t, float *p, float *h);  ///< Read all channels in one transaction

    // --- GPIO Mode Control ---

    void forceGPIO(bool f)      { _force_gpio = f; }
    bool isForcedGPIO() const   { return _force_gpio; }

    // --- Status & Identification ---

    uint8_t getChipID();
    bool isBME280() const       { return _is_bme; }
    bool isInitialized() const  { return _init; }

    /**
     * @brief Get the last error code (cleared on each begin() call).
     * @return BMx280Error code.
     */
    BMx280Error getLastError() const { return _last_error; }

private:
    // --- I2C Transport ---
    TwoWire  *_wire;
    WirePIO  *_wirepio;
    uint8_t   _addr, _sda, _scl;
    uint32_t  _freq;
    bool      _init, _is_bme, _force_gpio;

    // --- Sensor Configuration ---
    uint8_t   _osrs_t, _osrs_p, _osrs_h, _filter, _standby, _mode;

    // --- Error State ---
    BMx280Error _last_error;
    Stream     *_debug;        ///< Optional debug output stream (nullptr = silent)

    // --- Concurrency ---
    critical_section_t _cs;     ///< Guards I2C transactions for multi-core safety

    // --- Calibration Data ---
    uint16_t _T1; int16_t _T2, _T3;
    uint16_t _P1; int16_t _P2, _P3, _P4, _P5, _P6, _P7, _P8, _P9;
    uint8_t  _H1; int16_t _H2; uint8_t _H3; int16_t _H4, _H5; int8_t _H6;
    int32_t  _t_fine;

    // --- Internal Methods ---
    bool _i2c_write(uint8_t reg, const uint8_t *data, size_t len);
    bool _i2c_read(uint8_t reg, uint8_t *data, size_t len);
    bool _loadCalibration();
    void _applyConfig();
    void _readRaw(int32_t *t, int32_t *p, int32_t *h);
    uint8_t _measTime();
    float _compensateTemperature(int32_t adc_T);  ///< Bosch T compensation (updates _t_fine)
};

#endif

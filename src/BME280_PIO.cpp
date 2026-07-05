/*
 * BME280_PIO.cpp - BME280 sensor driver implementation
 *
 * Compensation formulas based on Bosch BME280 Datasheet
 * Document: BST-BME280-DS002-15, Section 4.2.3
 * "Compensation formula in 32-bit fixed point"
 *
 * All calculations use integer (fixed-point) arithmetic to avoid
 * floating-point overhead on the RP2040.
 */

#include "BME280_PIO.h"

// ─── Constructor ──────────────────────────────────────────────────────────

BME280_PIO::BME280_PIO(uint8_t sdaPin, uint8_t sclPin,
                       uint8_t addr, uint32_t freq)
    : _i2c(sdaPin, sclPin, freq)
    , _addr(addr)
    , _initialized(false)
    , _osrs_t(BME280_OS_1X)
    , _osrs_p(BME280_OS_1X)
    , _osrs_h(BME280_OS_1X)
    , _filter(BME280_FILTER_OFF)
    , _standby(BME280_STANDBY_250MS)
    , _mode(BME280_MODE_SLEEP)
    , _dig_T1(0), _dig_T2(0), _dig_T3(0)
    , _dig_P1(0), _dig_P2(0), _dig_P3(0), _dig_P4(0)
    , _dig_P5(0), _dig_P6(0), _dig_P7(0), _dig_P8(0), _dig_P9(0)
    , _dig_H1(0), _dig_H2(0), _dig_H3(0)
    , _dig_H4(0), _dig_H5(0), _dig_H6(0)
    , _t_fine(0)
{
}

// ─── Initialization ───────────────────────────────────────────────────────

bool BME280_PIO::begin()
{
    if (_initialized) return true;

    // Initialize PIO I2C
    if (!_i2c.begin()) {
        return false;
    }

    // Reset the sensor
    reset();
    delay(10);  // Wait for reset to complete (max 2ms per datasheet)

    // Verify chip ID
    uint8_t chipId = getChipID();
    if (chipId != BME280_CHIP_ID) {
        return false;
    }

    // Load calibration data
    if (!_loadCalibration()) {
        return false;
    }

    // Apply configuration
    _applyConfig();

    _initialized = true;
    return true;
}

// ─── Calibration Data Loading ─────────────────────────────────────────────

bool BME280_PIO::_loadCalibration()
{
    uint8_t calib[26];  // Registers 0x88 through 0xA1 (26 bytes)
    uint8_t calib_h[7]; // Registers 0xE1 through 0xE7 (7 bytes)

    // Read temperature + pressure calibration (0x88 - 0xA1)
    if (!_i2c.writeThenRead(_addr, (const uint8_t[]){BME280_REG_DIG_T1}, 1,
                            calib, 26)) {
        return false;
    }

    // Read humidity calibration (0xE1 - 0xE7)
    if (!_i2c.writeThenRead(_addr, (const uint8_t[]){BME280_REG_DIG_H2}, 1,
                            calib_h, 7)) {
        return false;
    }

    // Parse temperature calibration
    _dig_T1 = (uint16_t)calib[0] | ((uint16_t)calib[1] << 8);
    _dig_T2 = (int16_t)((uint16_t)calib[2] | ((uint16_t)calib[3] << 8));
    _dig_T3 = (int16_t)((uint16_t)calib[4] | ((uint16_t)calib[5] << 8));

    // Parse pressure calibration
    _dig_P1 = (uint16_t)calib[6]  | ((uint16_t)calib[7]  << 8);
    _dig_P2 = (int16_t)((uint16_t)calib[8]  | ((uint16_t)calib[9]  << 8));
    _dig_P3 = (int16_t)((uint16_t)calib[10] | ((uint16_t)calib[11] << 8));
    _dig_P4 = (int16_t)((uint16_t)calib[12] | ((uint16_t)calib[13] << 8));
    _dig_P5 = (int16_t)((uint16_t)calib[14] | ((uint16_t)calib[15] << 8));
    _dig_P6 = (int16_t)((uint16_t)calib[16] | ((uint16_t)calib[17] << 8));
    _dig_P7 = (int16_t)((uint16_t)calib[18] | ((uint16_t)calib[19] << 8));
    _dig_P8 = (int16_t)((uint16_t)calib[20] | ((uint16_t)calib[21] << 8));
    _dig_P9 = (int16_t)((uint16_t)calib[22] | ((uint16_t)calib[23] << 8));

    // Parse humidity calibration
    // H1 is at register 0xA1 (calib[25] in the 0x88-0xA1 block)
    _dig_H1 = calib[25];

    // H2 through H6 are in the 0xE1-0xE7 block
    _dig_H2 = (int16_t)((uint16_t)calib_h[0] | ((uint16_t)calib_h[1] << 8));
    _dig_H3 = calib_h[2];
    _dig_H4 = (int16_t)((uint16_t)(calib_h[3] << 4) |
                         (uint16_t)(calib_h[4] & 0x0F));
    _dig_H5 = (int16_t)((uint16_t)(calib_h[4] >> 4) |
                         (uint16_t)(calib_h[5] << 4));
    _dig_H6 = (int8_t)calib_h[6];

    // Ensure H4 and H5 sign extension (12-bit values in 16-bit int)
    if (_dig_H4 > 2047) _dig_H4 -= 4096;
    if (_dig_H5 > 2047) _dig_H5 -= 4096;

    return true;
}

// ─── Configuration ────────────────────────────────────────────────────────

void BME280_PIO::_applyConfig()
{
    // Set humidity oversampling (register 0xF2)
    // Must be written before CTRL_MEAS according to datasheet
    writeRegister(BME280_REG_CTRL_HUM, _osrs_h & 0x07);

    // Set measurement control (register 0xF4)
    // [7:5] = osrs_t, [4:2] = osrs_p, [1:0] = mode
    uint8_t ctrl_meas = ((_osrs_t & 0x07) << 5) |
                        ((_osrs_p & 0x07) << 2) |
                        (_mode & 0x03);
    writeRegister(BME280_REG_CTRL_MEAS, ctrl_meas);

    // Set configuration (register 0xF5)
    // [7:5] = standby, [4:2] = filter, [1] = 0 (spi3w_en), [0] = 0
    uint8_t config = ((_standby & 0x07) << 5) |
                     ((_filter  & 0x07) << 2);
    writeRegister(BME280_REG_CONFIG, config);
}

void BME280_PIO::setTemperatureOversampling(uint8_t os)
{
    _osrs_t = os & 0x07;
    if (_initialized) {
        // Must be in sleep mode to change settings
        uint8_t prevMode = _mode;
        setMode(BME280_MODE_SLEEP);
        _applyConfig();
        if (prevMode != BME280_MODE_SLEEP) {
            setMode(prevMode);
        }
    }
}

void BME280_PIO::setPressureOversampling(uint8_t os)
{
    _osrs_p = os & 0x07;
    if (_initialized) {
        uint8_t prevMode = _mode;
        setMode(BME280_MODE_SLEEP);
        _applyConfig();
        if (prevMode != BME280_MODE_SLEEP) {
            setMode(prevMode);
        }
    }
}

void BME280_PIO::setHumidityOversampling(uint8_t os)
{
    _osrs_h = os & 0x07;
    if (_initialized) {
        uint8_t prevMode = _mode;
        setMode(BME280_MODE_SLEEP);
        _applyConfig();
        if (prevMode != BME280_MODE_SLEEP) {
            setMode(prevMode);
        }
    }
}

void BME280_PIO::setFilter(uint8_t filter)
{
    _filter = filter & 0x07;
    if (_initialized) {
        uint8_t prevMode = _mode;
        setMode(BME280_MODE_SLEEP);
        _applyConfig();
        if (prevMode != BME280_MODE_SLEEP) {
            setMode(prevMode);
        }
    }
}

void BME280_PIO::setStandbyTime(uint8_t standby)
{
    _standby = standby & 0x07;
    if (_initialized) {
        uint8_t prevMode = _mode;
        setMode(BME280_MODE_SLEEP);
        _applyConfig();
        if (prevMode != BME280_MODE_SLEEP) {
            setMode(prevMode);
        }
    }
}

void BME280_PIO::setMode(uint8_t mode)
{
    _mode = mode & 0x03;

    if (!_initialized) return;

    uint8_t ctrl_meas = readRegister(BME280_REG_CTRL_MEAS);
    ctrl_meas = (ctrl_meas & 0xFC) | (_mode & 0x03);
    writeRegister(BME280_REG_CTRL_MEAS, ctrl_meas);
}

// ─── Forced Measurement ───────────────────────────────────────────────────

uint8_t BME280_PIO::_getMaxMeasurementTime()
{
    // Calculate maximum measurement time based on oversampling settings.
    // From datasheet Table 11 (Appendix B):
    //   t_measure (ms) = 1.25 + 2.3*osrs_t + 2.3*osrs_p* + 0.575 + 2.3*osrs_h
    //   * pressure measurement time includes +0.5ms (already in formula)
    //   Each osrs increment doubles: oversampling time = osrs_factor * base
    //   osrs=0(skip),1(1x),2(2x),3(4x),4(8x),5(16x)
    //
    // Base measurement times:
    //   Temperature: 2.3ms at 1x
    //   Pressure:    2.3ms at 1x
    //   Humidity:    2.3ms at 1x
    //   Start-up:    1.25ms
    //
    // We use worst-case (maximum oversampling) plus margin.

    uint8_t osrs_to_mult[] = {0, 1, 2, 4, 8, 16};

    float t_measure = 1.25f; // Start-up time
    t_measure += 2.3f * (float)osrs_to_mult[_osrs_t & 0x07];
    t_measure += 2.3f * (float)osrs_to_mult[_osrs_p & 0x07] + 0.575f;
    t_measure += 2.3f * (float)osrs_to_mult[_osrs_h & 0x07] + 0.575f;

    return (uint8_t)(t_measure + 2.0f); // Add margin, round up
}

bool BME280_PIO::takeForcedMeasurement()
{
    if (!_initialized) return false;

    // Set to forced mode (triggers one measurement)
    setMode(BME280_MODE_FORCED);

    // Wait for measurement to complete
    uint8_t maxWait = _getMaxMeasurementTime();
    delay(maxWait);

    // Check status register: bit 3 = measuring, bit 0 = im_update
    // Wait until measuring bit is clear
    uint32_t start = millis();
    while (readRegister(BME280_REG_STATUS) & 0x08) {
        if (millis() - start > 100) {
            return false; // Timeout
        }
        delay(1);
    }

    return true;
}

// ─── Raw Data Reading ─────────────────────────────────────────────────────

void BME280_PIO::_readRaw(int32_t *temp_raw, int32_t *press_raw, int32_t *hum_raw)
{
    // Read all 8 data registers (0xF7 through 0xFE) in one burst
    uint8_t data[8];
    readRegisters(BME280_REG_PRESS_MSB, data, 8);

    // Pressure: 20-bit value (bits 19..0)
    // data[0] = press_msb, data[1] = press_lsb, data[2] = press_xlsb
    if (press_raw) {
        *press_raw = ((int32_t)data[0] << 12) |
                     ((int32_t)data[1] << 4)  |
                     ((int32_t)data[2] >> 4);
    }

    // Temperature: 20-bit value (bits 19..0)
    // data[3] = temp_msb, data[4] = temp_lsb, data[5] = temp_xlsb
    if (temp_raw) {
        *temp_raw = ((int32_t)data[3] << 12) |
                    ((int32_t)data[4] << 4)  |
                    ((int32_t)data[5] >> 4);
    }

    // Humidity: 16-bit value
    // data[6] = hum_msb, data[7] = hum_lsb
    if (hum_raw) {
        *hum_raw = ((int32_t)data[6] << 8) | (int32_t)data[7];
    }
}

// ─── Compensation Formulas (Bosch Datasheet, Appendix A) ──────────────────

float BME280_PIO::_compensateTemperature(int32_t adc_T)
{
    // Integer fixed-point temperature compensation (32-bit)
    // Returns temperature in degrees Celsius * 100 (e.g., 2123 = 21.23°C)

    int32_t var1, var2;

    var1 = ((((adc_T >> 3) - ((int32_t)_dig_T1 << 1))) *
            ((int32_t)_dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)_dig_T1)) *
              ((adc_T >> 4) - ((int32_t)_dig_T1))) >> 12) *
            ((int32_t)_dig_T3)) >> 14;

    _t_fine = var1 + var2;

    return (float)((_t_fine * 5 + 128) >> 8) / 100.0f;
}

float BME280_PIO::_compensatePressure(int32_t adc_P)
{
    // Integer fixed-point pressure compensation (64-bit intermediate)
    // Requires _t_fine from a prior temperature reading
    // Returns pressure in Pascals / 256 (divide by 256 for Pa, /25600 for hPa)

    int64_t var1, var2, p;

    var1 = ((int64_t)_t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)_dig_P6;
    var2 = var2 + ((var1 * (int64_t)_dig_P5) << 17);
    var2 = var2 + (((int64_t)_dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)_dig_P3) >> 8) +
           ((var1 * (int64_t)_dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)_dig_P1) >> 33;

    if (var1 == 0) {
        return 0.0f; // Avoid division by zero
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)_dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)_dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)_dig_P7) << 4);

    // p is in Pa (Pascals)
    return (float)p / 100.0f; // Convert to hPa
}

float BME280_PIO::_compensateHumidity(int32_t adc_H)
{
    // Integer fixed-point humidity compensation (32-bit)
    // Requires _t_fine from a prior temperature reading
    // Returns humidity in %RH * 1024 (divide by 1024 for %)

    int32_t var1;

    var1 = (_t_fine - ((int32_t)76800));

    var1 = (((((adc_H << 14) -
               (((int32_t)_dig_H4) << 20) -
               (((int32_t)_dig_H5) * var1)) +
              ((int32_t)16384)) >> 15) *
             (((((((var1 * ((int32_t)_dig_H6)) >> 10) *
                  (((var1 * ((int32_t)_dig_H3)) >> 11) +
                   ((int32_t)32768))) >> 10) +
                ((int32_t)2097152)) *
               ((int32_t)_dig_H2) + 8192) >> 14));

    var1 = (var1 - (((((var1 >> 15) * (var1 >> 15)) >> 7) *
                     ((int32_t)_dig_H1)) >> 4));

    var1 = (var1 < 0 ? 0 : var1);
    var1 = (var1 > 419430400 ? 419430400 : var1);

    return (float)((uint32_t)(var1 >> 12)) / 1024.0f;
}

// ─── Public Read API ──────────────────────────────────────────────────────

float BME280_PIO::readTemperature()
{
    if (!_initialized) return NAN;

    int32_t adc_T;
    _readRaw(&adc_T, nullptr, nullptr);
    return _compensateTemperature(adc_T);
}

float BME280_PIO::readPressure()
{
    if (!_initialized) return NAN;

    int32_t adc_P, adc_T;
    _readRaw(&adc_T, &adc_P, nullptr);

    // Temperature MUST be read first (sets _t_fine for pressure compensation)
    _compensateTemperature(adc_T);
    return _compensatePressure(adc_P);
}

float BME280_PIO::readHumidity()
{
    if (!_initialized) return NAN;

    int32_t adc_T, adc_H;
    // Read temperature and humidity (need both)
    // We get all 8 registers in one burst for efficiency
    uint8_t data[8];
    readRegisters(BME280_REG_PRESS_MSB, data, 8);

    adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) |
            ((int32_t)data[5] >> 4);
    adc_H = ((int32_t)data[6] << 8)  | (int32_t)data[7];

    // Temperature first (sets _t_fine)
    _compensateTemperature(adc_T);
    return _compensateHumidity(adc_H);
}

void BME280_PIO::readAll(float *temperature, float *pressure, float *humidity)
{
    if (!_initialized) {
        if (temperature) *temperature = NAN;
        if (pressure)    *pressure    = NAN;
        if (humidity)    *humidity    = NAN;
        return;
    }

    int32_t adc_T, adc_P, adc_H;

    // Read all 8 data registers in one I2C burst
    uint8_t data[8];
    readRegisters(BME280_REG_PRESS_MSB, data, 8);

    adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) |
            ((int32_t)data[2] >> 4);
    adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) |
            ((int32_t)data[5] >> 4);
    adc_H = ((int32_t)data[6] << 8)  | (int32_t)data[7];

    // Compensate: temperature first (sets _t_fine), then P and H
    float temp = _compensateTemperature(adc_T);
    float press = _compensatePressure(adc_P);
    float hum   = _compensateHumidity(adc_H);

    if (temperature) *temperature = temp;
    if (pressure)    *pressure    = press;
    if (humidity)    *humidity    = hum;
}

// ─── Register Access ──────────────────────────────────────────────────────

uint8_t BME280_PIO::readRegister(uint8_t reg)
{
    uint8_t value = 0;
    _i2c.writeThenRead(_addr, &reg, 1, &value, 1);
    return value;
}

void BME280_PIO::writeRegister(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    _i2c.write(_addr, data, 2);
}

void BME280_PIO::readRegisters(uint8_t reg, uint8_t *data, size_t len)
{
    _i2c.writeThenRead(_addr, &reg, 1, data, len);
}

uint8_t BME280_PIO::getChipID()
{
    return readRegister(BME280_REG_CHIP_ID);
}

void BME280_PIO::reset()
{
    writeRegister(BME280_REG_RESET, BME280_RESET_VALUE);
}

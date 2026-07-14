/**
 * @file BMx280PIO_RP2040.cpp
 * @brief BMP280/BME280 sensor driver implementation for RP2040.
 * @author Ângelo Moisés Alves (@angeloINTJ)
 * @license MIT
 */
#include "BMx280PIO_RP2040.h"

#include <hardware/gpio.h>
#include <WirePIO.h>

// =============================================================================
// GPIO Bit-Bang I2C Helpers (anonymous namespace — file-scope only)
// =============================================================================
namespace {

/**
 * @brief Half-period delay for I2C bit-bang timing.
 * @param freq_hz Target I2C clock frequency in Hz.
 */
inline void i2c_delay(uint32_t freq_hz) {
    uint32_t half = 500000 / freq_hz;
    delayMicroseconds(half < 2 ? 2 : half);
}

inline void sda_low(uint8_t pin) {
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

inline void sda_release(uint8_t pin) {
    gpio_set_dir(pin, GPIO_IN);
}

inline bool sda_read(uint8_t pin) {
    return gpio_get(pin);
}

inline void scl_low(uint8_t pin) {
    gpio_put(pin, 0);
}

inline void scl_high(uint8_t pin) {
    gpio_put(pin, 1);
}

void i2c_start_condition(uint8_t sda, uint8_t scl, uint32_t freq) {
    sda_low(sda);
    i2c_delay(freq);
    scl_low(scl);
    i2c_delay(freq);
}

void i2c_stop_condition(uint8_t sda, uint8_t scl, uint32_t freq) {
    sda_low(sda);
    i2c_delay(freq);
    scl_high(scl);
    i2c_delay(freq);
    sda_release(sda);
    i2c_delay(freq);
}

/**
 * @brief Write a byte via GPIO bit-bang and return ACK status.
 * @return true if slave ACKed (SDA low during 9th clock).
 */
bool i2c_write_byte_gpio(uint8_t sda, uint8_t scl, uint32_t freq, uint8_t byte) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        if (byte & mask) {
            sda_release(sda);
        } else {
            sda_low(sda);
        }
        i2c_delay(freq);
        scl_high(scl);
        i2c_delay(freq);
        scl_low(scl);
    }
    sda_release(sda);
    i2c_delay(freq);
    scl_high(scl);
    i2c_delay(freq);
    bool ack = !sda_read(sda);
    scl_low(scl);
    i2c_delay(freq);
    return ack;
}

/**
 * @brief Read a byte via GPIO bit-bang.
 * @param last If true, send NACK (SDA high); if false, send ACK (SDA low).
 */
uint8_t i2c_read_byte_gpio(uint8_t sda, uint8_t scl, uint32_t freq, bool last) {
    uint8_t value = 0;
    sda_release(sda);
    for (int i = 0; i < 8; i++) {
        scl_high(scl);
        i2c_delay(freq);
        value = (value << 1) | (sda_read(sda) ? 1 : 0);
        scl_low(scl);
        i2c_delay(freq);
    }
    if (last) {
        sda_release(sda);   // NACK
    } else {
        sda_low(sda);       // ACK
    }
    i2c_delay(freq);
    scl_high(scl);
    i2c_delay(freq);
    scl_low(scl);
    i2c_delay(freq);
    sda_release(sda);
    return value;
}

}  // anonymous namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

BMx280PIO_RP2040::BMx280PIO_RP2040(TwoWire &wire, uint8_t addr)
    : _wire(&wire), _wirepio(nullptr), _addr(addr), _sda(0), _scl(0), _freq(100000),
      _init(false), _is_bme(false), _force_gpio(false),
      _osrs_t(BME280_OS_1X), _osrs_p(BME280_OS_1X), _osrs_h(BME280_OS_1X),
      _filter(BME280_FILTER_OFF), _standby(BME280_STANDBY_250MS), _mode(BME280_MODE_SLEEP),
      _last_error(BMX280_OK), _debug(nullptr) {
    critical_section_init(&_cs);
}

BMx280PIO_RP2040::BMx280PIO_RP2040(uint8_t sda, uint8_t scl, uint8_t addr, uint32_t freq, PIO pio)
    : _wire(nullptr), _wirepio(new WirePIO(sda, scl, freq, pio)),
      _addr(addr), _sda(sda), _scl(scl), _freq(freq),
      _init(false), _is_bme(false), _force_gpio(false),
      _osrs_t(BME280_OS_1X), _osrs_p(BME280_OS_1X), _osrs_h(BME280_OS_1X),
      _filter(BME280_FILTER_OFF), _standby(BME280_STANDBY_250MS), _mode(BME280_MODE_SLEEP),
      _last_error(BMX280_OK), _debug(nullptr) {
    critical_section_init(&_cs);
}

BMx280PIO_RP2040::~BMx280PIO_RP2040() {
    if (_wirepio) {
        _wirepio->end();
        delete _wirepio;
    }
    critical_section_deinit(&_cs);
}

// =============================================================================
// I2C Transport (PIO+DMA, GPIO bit-bang, Hardware Wire)
// =============================================================================

bool BMx280PIO_RP2040::_i2c_write(uint8_t reg, const uint8_t *data, size_t len) {
    critical_section_enter_blocking(&_cs);
    bool result = false;

    if (_wirepio && !_force_gpio) {
        // PIO+DMA path
        _wirepio->beginTransmission(_addr);
        _wirepio->write(reg);
        for (size_t i = 0; i < len; i++) {
            _wirepio->write(data[i]);
        }
        result = (_wirepio->endTransmission() == 0);

    } else if (_wirepio && _force_gpio) {
        // GPIO bit-bang write: START + addr+w + reg + data bytes + STOP
        uint8_t sd = _sda, sc = _scl;
        uint32_t f = 100000;

        gpio_init(sd);
        gpio_set_dir(sd, GPIO_IN);
        gpio_pull_up(sd);
        gpio_init(sc);
        gpio_set_dir(sc, GPIO_OUT);
        gpio_put(sc, 1);

        i2c_start_condition(sd, sc, f);
        if (!i2c_write_byte_gpio(sd, sc, f, (uint8_t)(_addr << 1))) {
            i2c_stop_condition(sd, sc, f);
        } else if (!i2c_write_byte_gpio(sd, sc, f, reg)) {
            i2c_stop_condition(sd, sc, f);
        } else {
            result = true;
            for (size_t i = 0; i < len; i++) {
                if (!i2c_write_byte_gpio(sd, sc, f, data[i])) {
                    i2c_stop_condition(sd, sc, f);
                    result = false;
                    break;
                }
            }
            if (result) {
                i2c_stop_condition(sd, sc, f);
            }
        }

    } else {
        // Hardware Wire path
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        for (size_t i = 0; i < len; i++) {
            _wire->write(data[i]);
        }
        result = (_wire->endTransmission() == 0);
    }

    critical_section_exit(&_cs);
    return result;
}

bool BMx280PIO_RP2040::_i2c_read(uint8_t reg, uint8_t *data, size_t len) {
    critical_section_enter_blocking(&_cs);
    bool result = false;

    if (_wirepio) {
        // Fast path: PIO+DMA burstRead (skip if forced GPIO mode)
        if (!_force_gpio && len <= 8 && _wirepio->burstRead(_addr, reg, data, len) == len) {
            result = true;
        } else {
            // GPIO fallback: always use 100 kHz for reliable timing
            uint8_t sd = _sda, sc = _scl;
            uint32_t f = 100000;

            result = true;
            for (size_t i = 0; i < len; i++) {
                gpio_init(sd);
                gpio_set_dir(sd, GPIO_IN);
                gpio_pull_up(sd);
                gpio_init(sc);
                gpio_set_dir(sc, GPIO_OUT);
                gpio_put(sc, 1);

                i2c_start_condition(sd, sc, f);
                if (!i2c_write_byte_gpio(sd, sc, f, (uint8_t)(_addr << 1))) {
                    i2c_stop_condition(sd, sc, f);
                    result = false;
                    break;
                }
                if (!i2c_write_byte_gpio(sd, sc, f, (uint8_t)(reg + i))) {
                    i2c_stop_condition(sd, sc, f);
                    result = false;
                    break;
                }
                i2c_stop_condition(sd, sc, f);

                i2c_start_condition(sd, sc, f);
                if (!i2c_write_byte_gpio(sd, sc, f, (uint8_t)((_addr << 1) | 1))) {
                    i2c_stop_condition(sd, sc, f);
                    result = false;
                    break;
                }
                data[i] = i2c_read_byte_gpio(sd, sc, f, true);
                i2c_stop_condition(sd, sc, f);
            }
        }

    } else {
        // Hardware Wire path
        _wire->beginTransmission(_addr);
        _wire->write(reg);
        if (_wire->endTransmission(false) != 0) {
            result = false;
        } else {
            size_t n = _wire->requestFrom(_addr, len);
            for (size_t i = 0; i < n && i < len; i++) {
                data[i] = _wire->read();
            }
            result = (n == len);
        }
    }

    critical_section_exit(&_cs);
    return result;
}

// =============================================================================
// Initialization
// =============================================================================

bool BMx280PIO_RP2040::begin() {
    _last_error = BMX280_OK;

    if (_init) {
        return true;
    }

    bool usePIO = (_wirepio && !_force_gpio);

    if (usePIO) {
        _wirepio->begin();
        if (!_wirepio->isRunning()) {
            _last_error = BMX280_ERR_NOT_FOUND;
            if (_debug) _debug->println("BMx280: PIO+DMA failed to start");
            return false;
        }

        // Prime the PIO SM with dummy reads for reliable burstRead cold-start
        uint8_t dummy;
        for (int w = 0; w < BMX280_PIO_PRIME_READS; w++) {
            _wirepio->burstRead(_addr, BME280_REG_CHIP_ID, &dummy, 1);
            delayMicroseconds(200);
        }

        // Use GPIO bit-bang for critical writes (reset + config).
        // PIO+DMA writes are unreliable on cold-start; GPIO is 100% reliable.
        _force_gpio = true;
    }

    // Reset sensor
    uint8_t rst = BME280_RESET_VALUE;
    _i2c_write(BME280_REG_RESET, &rst, 1);
    sleep_ms(15);

    // Read and validate chip ID
    uint8_t cid = 0;
    if (!_i2c_read(BME280_REG_CHIP_ID, &cid, 1)) {
        _last_error = BMX280_ERR_I2C_READ;
        if (_debug) _debug->println("BMx280: I2C read failed — sensor not found");
        return false;
    }
    _is_bme = (cid == BME280_CHIP_ID_VALUE);
    if (cid != BMP280_CHIP_ID && cid != BME280_CHIP_ID_VALUE) {
        _last_error = BMX280_ERR_CHIP_ID;
        if (_debug) {
            _debug->print("BMx280: bad chip ID 0x");
            _debug->println(cid, HEX);
        }
        return false;
    }

    // Load calibration data
    if (!_loadCalibration()) {
        _last_error = BMX280_ERR_CAL_FAIL;
        if (_debug) _debug->println("BMx280: calibration read failed");
        return false;
    }

    // Apply configuration via GPIO (reliable)
    _applyConfig();

    // Set NORMAL mode while GPIO is still active (PIO writes unreliable)
    _mode = BME280_MODE_NORMAL;
    _applyConfig();

    if (usePIO) {
        _force_gpio = false;  // restore PIO+DMA for reads
    }

    _init = true;
    return true;
}

bool BMx280PIO_RP2040::begin(Stream *debug) {
    _debug = debug;
    return begin();
}

bool BMx280PIO_RP2040::beginPIO(PIO pio) {
    if (!_wirepio || !_init) {
        return false;
    }
    _wirepio->end();
    _wirepio->setPIO(pio);
    _wirepio->begin();
    return _wirepio->isRunning();
}

// =============================================================================
// Calibration
// =============================================================================

bool BMx280PIO_RP2040::_loadCalibration() {
    uint8_t b[26];

    // Read in ≤8-byte chunks so burstRead can handle each chunk
    if (!_i2c_read(0x88, b, 8)) return false;       // 0x88-0x8F
    if (!_i2c_read(0x90, b + 8, 8)) return false;   // 0x90-0x97
    if (!_i2c_read(0x98, b + 16, 8)) return false;  // 0x98-0x9F
    if (!_i2c_read(0xA0, b + 24, 2)) return false;  // 0xA0-0xA1

    _T1 = b[0] | (b[1] << 8);
    _T2 = b[2] | (b[3] << 8);
    _T3 = b[4] | (b[5] << 8);
    _P1 = b[6] | (b[7] << 8);
    _P2 = b[8] | (b[9] << 8);
    _P3 = b[10] | (b[11] << 8);
    _P4 = b[12] | (b[13] << 8);
    _P5 = b[14] | (b[15] << 8);
    _P6 = b[16] | (b[17] << 8);
    _P7 = b[18] | (b[19] << 8);
    _P8 = b[20] | (b[21] << 8);
    _P9 = b[22] | (b[23] << 8);
    _H1 = b[25];

    if (_is_bme) {
        uint8_t h[8];
        if (!_i2c_read(0xE1, h, 7)) return false;

        _H2 = h[0] | (h[1] << 8);
        _H3 = h[2];
        _H4 = (h[3] << 4) | (h[4] & 0x0F);
        _H5 = (h[4] >> 4) | (h[5] << 4);
        _H6 = h[6];

        if (_H4 > 2047) _H4 -= 4096;
        if (_H5 > 2047) _H5 -= 4096;
    }
    return true;
}

// =============================================================================
// Configuration
// =============================================================================

void BMx280PIO_RP2040::_applyConfig() {
    uint8_t d;

    // Humidity oversampling (BME280 only)
    if (_is_bme) {
        d = _osrs_h & 0x07;
        _i2c_write(BME280_REG_CTRL_HUM, &d, 1);
    }

    // CTRL_MEAS: temperature oversampling | pressure oversampling | mode
    d = ((_osrs_t & 0x07) << 5) | ((_osrs_p & 0x07) << 2) | (_mode & 0x03);

    // Retry with backoff if write fails (NACK detection in PIO v2)
    bool ok = false;
    for (int retry = 0; retry < BMX280_CTRL_MEAS_RETRY_COUNT; retry++) {
        if (_i2c_write(BME280_REG_CTRL_MEAS, &d, 1)) {
            ok = true;
            break;
        }
        delayMicroseconds(BMX280_CTRL_MEAS_RETRY_US);
    }

    if (!ok) {
        _last_error = BMX280_ERR_CTRL_MEAS;
        if (_debug) {
            _debug->print("BMx280: CTRL_MEAS write failed after ");
            _debug->print(BMX280_CTRL_MEAS_RETRY_COUNT);
            _debug->print(" retries (val=0x");
            _debug->print(d, HEX);
            _debug->println(")");
        }
    }

    // CONFIG: standby time | filter coefficient
    d = ((_standby & 0x07) << 5) | ((_filter & 0x07) << 2);
    _i2c_write(BME280_REG_CONFIG, &d, 1);
}

void BMx280PIO_RP2040::setMode(uint8_t mode) {
    _mode = mode & 0x03;
    if (!_init) return;
    _applyConfig();
}

// =============================================================================
// Raw Register Access
// =============================================================================

uint8_t BMx280PIO_RP2040::readRegister(uint8_t reg) {
    uint8_t value = 0;
    _i2c_read(reg, &value, 1);
    return value;
}

void BMx280PIO_RP2040::writeRegister(uint8_t reg, uint8_t value) {
    _i2c_write(reg, &value, 1);
}

void BMx280PIO_RP2040::readRegisters(uint8_t reg, uint8_t *data, size_t len) {
    _i2c_read(reg, data, len);
}

uint8_t BMx280PIO_RP2040::getChipID() {
    uint8_t value = 0;
    _i2c_read(BME280_REG_CHIP_ID, &value, 1);
    return value;
}

// =============================================================================
// Measurement Timing
// =============================================================================

uint8_t BMx280PIO_RP2040::_measTime() {
    const uint8_t multiplier[] = {0, 1, 2, 4, 8, 16};
    uint32_t t = BMX280_MEAS_TIME_BASE_US
               + BMX280_MEAS_TIME_T_COEFF_US * multiplier[_osrs_t & 7]
               + BMX280_MEAS_TIME_P_COEFF_US * multiplier[_osrs_p & 7]
               + BMX280_MEAS_TIME_OVERHEAD_US;
    if (_is_bme) {
        t += BMX280_MEAS_TIME_H_COEFF_US * multiplier[_osrs_h & 7] + BMX280_MEAS_TIME_OVERHEAD_US;
    }
    return (uint8_t)((t + 1500) / 1000);
}

bool BMx280PIO_RP2040::takeForcedMeasurement() {
    if (!_init) {
        _last_error = BMX280_ERR_NOT_INIT;
        return false;
    }

    setMode(BME280_MODE_FORCED);
    sleep_ms(_measTime());

    uint32_t start = millis();
    uint8_t status;
    do {
        if (!_i2c_read(BME280_REG_STATUS, &status, 1)) {
            _last_error = BMX280_ERR_I2C_READ;
            return false;
        }
        if (millis() - start > BMX280_FORCED_MEAS_TIMEOUT_MS) {
            _last_error = BMX280_ERR_MEAS_TIMEOUT;
            return false;
        }
        sleep_ms(1);
    } while (status & 0x08);

    return true;
}

// =============================================================================
// Raw Data Reading
// =============================================================================

void BMx280PIO_RP2040::_readRaw(int32_t *t, int32_t *p, int32_t *h) {
    uint8_t d[8];
    _i2c_read(BME280_REG_PRESS_MSB, d, 8);

    if (p) *p = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    if (t) *t = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
    if (h && _is_bme) {
        *h = ((int32_t)d[6] << 8) | d[7];
    } else if (h) {
        *h = 0;
    }
}

// =============================================================================
// Bosch Compensation Formulas
// =============================================================================

/**
 * @brief Bosch temperature compensation (datasheet §8.1).
 *
 * Single implementation shared by readTemperature(), readPressure(),
 * readHumidity(), and readAll(). Updates _t_fine which is used by
 * pressure and humidity compensation.
 *
 * @param adc_T Raw temperature ADC value from _readRaw().
 * @return Compensated temperature in °C.
 */
float BMx280PIO_RP2040::_compensateTemperature(int32_t adc_T) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)_T1 << 1))) * _T2) >> 11;
    int32_t var2 = (((((adc_T >> 4) - _T1) * ((adc_T >> 4) - _T1)) >> 12) * _T3) >> 14;
    _t_fine = var1 + var2;
    return (float)((_t_fine * 5 + 128) >> 8) / 100.0f;
}

float BMx280PIO_RP2040::readTemperature() {
    if (!_init) return NAN;

    int32_t adc_T;
    _readRaw(&adc_T, nullptr, nullptr);
    return _compensateTemperature(adc_T);
}

float BMx280PIO_RP2040::readPressure() {
    if (!_init) return NAN;

    int32_t adc_T, adc_P;
    _readRaw(&adc_T, &adc_P, nullptr);
    _compensateTemperature(adc_T);

    double var1 = (double)_t_fine / 2.0 - 64000.0;
    double var2 = var1 * var1 * (double)_P6 / 32768.0;
    var2 += var1 * (double)_P5 * 2.0;
    var2 = var2 / 4.0 + (double)_P4 * 65536.0;
    var1 = ((double)_P3 * var1 * var1 / 524288.0 + (double)_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)_P1;

    if (var1 == 0.0) return 0.0f;

    double p = 1048576.0 - (double)adc_P;
    p = (p - var2 / 4096.0) * 6250.0 / var1;
    var1 = (double)_P9 * p * p / 2147483648.0;
    var2 = p * (double)_P8 / 32768.0;
    p += (var1 + var2 + (double)_P7) / 16.0;

    return (float)(p / 100.0);
}

float BMx280PIO_RP2040::readHumidity() {
    if (!_init || !_is_bme) return 0.0f;

    int32_t adc_T, adc_H;
    _readRaw(&adc_T, nullptr, &adc_H);
    _compensateTemperature(adc_T);

    int32_t hv = _t_fine - 76800;
    hv = (int32_t)((((((int64_t)adc_H << 14) - ((int64_t)_H4 << 20) - ((int64_t)_H5 * hv)) + 16384) >> 15)
                   * ((((((((int64_t)hv * _H6) >> 10) * ((((int64_t)hv * _H3) >> 11) + 32768)) >> 10) + 2097152)
                       * _H2 + 8192) >> 14));
    hv = hv - (((((hv >> 15) * (hv >> 15)) >> 7) * (int32_t)_H1) >> 4);

    if (hv < 0) hv = 0;
    if (hv > 419430400) hv = 419430400;

    return (float)((uint32_t)(hv >> 12)) / 1024.0f;
}

void BMx280PIO_RP2040::readAll(float *t, float *p, float *h) {
    if (!_init) {
        if (t) *t = NAN;
        if (p) *p = NAN;
        if (h) *h = NAN;
        return;
    }

    int32_t adc_T, adc_P, adc_H;
    _readRaw(&adc_T, &adc_P, &adc_H);

    // Temperature compensation (shared implementation)
    float temp = _compensateTemperature(adc_T);
    if (t) *t = temp;

    // --- Pressure compensation ---
    if (p) {
        double pvar1 = (double)_t_fine / 2.0 - 64000.0;
        double pvar2 = pvar1 * pvar1 * (double)_P6 / 32768.0;
        pvar2 += pvar1 * (double)_P5 * 2.0;
        pvar2 = pvar2 / 4.0 + (double)_P4 * 65536.0;
        pvar1 = ((double)_P3 * pvar1 * pvar1 / 524288.0 + (double)_P2 * pvar1) / 524288.0;
        pvar1 = (1.0 + pvar1 / 32768.0) * (double)_P1;

        if (pvar1 == 0.0) {
            *p = 0.0f;
        } else {
            double pp = 1048576.0 - (double)adc_P;
            pp = (pp - pvar2 / 4096.0) * 6250.0 / pvar1;
            pvar1 = (double)_P9 * pp * pp / 2147483648.0;
            pvar2 = pp * (double)_P8 / 32768.0;
            pp += (pvar1 + pvar2 + (double)_P7) / 16.0;
            *p = (float)(pp / 100.0);
        }
    }

    // --- Humidity compensation ---
    if (h) {
        if (!_is_bme) {
            *h = 0.0f;
        } else {
            int32_t hv = _t_fine - 76800;
            hv = (int32_t)((((((int64_t)adc_H << 14) - ((int64_t)_H4 << 20) - ((int64_t)_H5 * hv)) + 16384) >> 15)
                           * ((((((((int64_t)hv * _H6) >> 10) * ((((int64_t)hv * _H3) >> 11) + 32768)) >> 10)
                               + 2097152) * _H2 + 8192) >> 14));
            hv = hv - (((((hv >> 15) * (hv >> 15)) >> 7) * (int32_t)_H1) >> 4);

            if (hv < 0) hv = 0;
            if (hv > 419430400) hv = 419430400;

            *h = (float)((uint32_t)(hv >> 12)) / 1024.0f;
        }
    }
}

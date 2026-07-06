/*
 * PIO_I2C.cpp - I2C Master via PIO state machine (LEFT shift, OUT-based SDA)
 *
 * PIO program: handles START, 8 data bits, ACK, STOP autonomously.
 * CPU pushes 16-bit commands to TX FIFO, reads results from RX FIFO.
 * PIO clock = 1 MHz for deterministic timing.
 *
 * Command encoding (LEFT shift OSR, MSB first):
 *   cmd[15]    = START (0 = SDA low while SCL high)
 *   cmd[14]    = PIO mode (0=WRITE, 1=READ)
 *   cmd[13:6]  = data byte (MSB at [13], LSB at [6])
 *   cmd[12]    = SDA release for read path (consumed at 3rd out)
 *   cmd[5]     = ACK release for write path (consumed at 11th out)
 *   cmd[4]     = STOP for write path (consumed at 12th out)
 *   cmd[11]    = STOP for read path (consumed at 4th out)
 *
 * Write: cmd = (data << 6) | (1 << 5) | (stop << 4)
 * Read:  cmd = (1 << 14) | (1 << 12) | (stop << 11)
 * Stop:  cmd = (1 << 5) | (1 << 4)  (dummy write that generates STOP)
 */

#include "PIO_I2C.h"
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/gpio.h>

// PIO program (pioasm-generated, set→out fix for SDA/ACK release)
static const uint16_t i2c_master_program_instructions[] = {
    0x98a0,0x7881,0x7041,0x106c,0x7081,0xf027,0xb942,0x5801,
    0xb142,0xb142,0x1046,0x1019,0xf027,0x7181,0xb942,0xb942,
    0xb142,0xb142,0x104d,0x7181,0xb942,0xb942,0x5801,0xb142,
    0x9000,0x7041,0x107f,0xf181,0xb942,0xb942,0xf981,0xd800,
};

static const pio_program_t i2c_program = {
    .instructions = i2c_master_program_instructions,
    .length = 32, .origin = -1,
};

PIO_I2C::PIO_I2C(uint8_t sda, uint8_t scl, uint32_t freq)
    : _sda(sda), _scl(scl), _freq(freq), _pio(nullptr), _sm(0), _offset(0), _initialized(false) {}

bool PIO_I2C::begin() {
    if (_initialized) return true;
    _pio = pio0; int sm = pio_claim_unused_sm(_pio, false);
    if (sm < 0) { _pio = pio1; sm = pio_claim_unused_sm(_pio, false); if (sm < 0) return false; }
    _sm = (uint)sm;
    if (!pio_can_add_program(_pio, &i2c_program)) { pio_sm_unclaim(_pio, _sm); return false; }
    _offset = pio_add_program(_pio, &i2c_program);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, _offset, _offset + 31);
    sm_config_set_sideset(&c, 1, true, false);
    sm_config_set_out_pins(&c, _sda, 1);
    sm_config_set_sideset_pins(&c, _scl);
    sm_config_set_set_pins(&c, _sda, 1);
    sm_config_set_in_pins(&c, _sda);

    gpio_init(_sda); gpio_set_dir(_sda, GPIO_IN); gpio_pull_up(_sda);
    gpio_init(_scl); gpio_set_dir(_scl, GPIO_OUT); gpio_put(_scl, 0);

    float sys = (float)clock_get_hz(clk_sys);
    sm_config_set_clkdiv(&c, sys / 1000000.0f);
    sm_config_set_out_shift(&c, false, true, 16); // LEFT shift (MSB first!)
    sm_config_set_in_shift(&c, true, true, 8);

    pio_sm_init(_pio, _sm, _offset, &c);
    pio_sm_set_enabled(_pio, _sm, true);
    _initialized = true;
    return true;
}

void PIO_I2C::end() {
    if (!_initialized) return;
    pio_sm_set_enabled(_pio, _sm, false);
    pio_remove_program(_pio, &i2c_program, _offset);
    pio_sm_unclaim(_pio, _sm);
    gpio_set_dir(_sda, GPIO_IN); gpio_disable_pulls(_sda);
    gpio_set_dir(_scl, GPIO_IN); gpio_disable_pulls(_scl);
    _initialized = false;
}

void PIO_I2C::putCommand(uint16_t c) { pio_sm_put_blocking(_pio, _sm, (uint32_t)c); }
bool PIO_I2C::hasData() { return !pio_sm_is_rx_fifo_empty(_pio, _sm); }
uint32_t PIO_I2C::readFIFO() { return pio_sm_get(_pio, _sm); }
void PIO_I2C::clearFIFO() { while (hasData()) readFIFO(); pio_sm_clear_fifos(_pio, _sm); }

bool PIO_I2C::_waitRX(uint32_t to) {
    uint32_t t = micros(); while (!hasData()) { if (micros()-t > to) { clearFIFO(); return false; } }
    return true;
}
void PIO_I2C::_sendStop() { putCommand((1<<4)|(1<<3)); _waitRX(5000); readFIFO(); } // STOP cmd
void PIO_I2C::_waitIdle() { while (!pio_sm_is_tx_fifo_empty(_pio, _sm)); }

// Encoding helpers (LEFT shift):
// Write data: cmd = (data << 6) | (1 << 5) | (stop << 4)
// Read:       cmd = (1 << 14) | (1 << 12) | (stop << 11)
// Address:    cmd = (addr_byte << 6) | (1 << 5)  (PIO in WRITE mode to send addr)

// LEFT shift: after 2 outs, OSR[15]=c13; after 10 outs, OSR[15]=c5; after 11th out, OSR[15]=c4
// Write: data at c[13:6], ACK release at c[4], STOP at c[3]
// Read:  READ flag at c[14], SDA release at c[13], STOP at c[11]
static inline uint16_t enc_write(uint8_t data, bool stop) {
    return ((uint16_t)data << 6) | (1 << 4) | (stop ? (1 << 3) : 0);
}
static inline uint16_t enc_read(bool stop) {
    return (1 << 14) | (1 << 13) | (stop ? (1 << 11) : 0);
}
static inline uint16_t enc_addr(uint8_t addr, bool read) {
    uint8_t ab = (addr << 1) | (read ? 1 : 0);
    return ((uint16_t)ab << 6) | (1 << 4); // PIO WRITE mode, ACK release
}

bool PIO_I2C::_sendAddress(uint8_t addr, bool read) {
    putCommand(enc_addr(addr, read));
    if (!_waitRX(5000)) return false;
    return (uint8_t)readFIFO() == 0;
}

bool PIO_I2C::write(uint8_t addr, const uint8_t *data, size_t len, bool stop) {
    if (!_initialized || len == 0) return false;
    if (!_sendAddress(addr, false)) { _sendStop(); return false; }
    for (size_t i = 0; i < len; i++) {
        bool last = (i == len-1) && stop;
        putCommand(enc_write(data[i], last));
        if (!_waitRX(5000)) return false;
        if ((uint8_t)readFIFO() != 0) { if (!last) _sendStop(); return false; }
    }
    return true;
}

bool PIO_I2C::read(uint8_t addr, uint8_t *data, size_t len, bool stop) {
    if (!_initialized || len == 0) return false;
    if (!_sendAddress(addr, true)) { _sendStop(); return false; }
    for (size_t i = 0; i < len; i++) {
        putCommand(enc_read((i == len-1) && stop));
        if (!_waitRX(5000)) return false;
        data[i] = (uint8_t)readFIFO();
    }
    return true;
}

bool PIO_I2C::writeThenRead(uint8_t addr, const uint8_t *wdata, size_t wlen,
                             uint8_t *rdata, size_t rlen) {
    if (!_initialized) return false;
    if (wlen > 0) {
        if (!_sendAddress(addr, false)) { _sendStop(); return false; }
        for (size_t i = 0; i < wlen; i++) {
            putCommand(enc_write(wdata[i], false));
            if (!_waitRX(5000)) return false;
            if ((uint8_t)readFIFO() != 0) { _sendStop(); return false; }
        }
        _sendStop(); // STOP between write and read (BMP280 needs this)
    }
    if (rlen > 0) {
        if (!_sendAddress(addr, true)) { _sendStop(); return false; }
        for (size_t i = 0; i < rlen; i++) {
            putCommand(enc_read((i == rlen-1)));
            if (!_waitRX(5000)) return false;
            rdata[i] = (uint8_t)readFIFO();
        }
    }
    return true;
}

void PIO_I2C::scan() {
    if (!_initialized) return;
    Serial.println("PIO I2C Scan:");
    int found = 0;
    for (int a = 1; a < 0x78; a++) {
        putCommand(enc_addr(a, false));
        if (_waitRX(5000) && (uint8_t)readFIFO() == 0) {
            Serial.print("0x"); Serial.print(a, HEX); Serial.print(" "); found++;
        }
        putCommand(enc_write(0, true)); // STOP
    }
    Serial.print("("); Serial.print(found); Serial.println(" devices)");
}

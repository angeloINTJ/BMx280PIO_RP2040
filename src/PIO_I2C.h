/*
 * PIO_I2C.h - I2C Master via PIO for RP2040
 *
 * Implements an I2C master using one PIO state machine.
 * Based on the official Raspberry Pi pico-examples I2C PIO implementation.
 * No dependency on Wire.h or the chip's hardware I2C peripherals.
 * Works on any pair of GPIO pins.
 *
 * Features:
 *   - Standard mode (100 kHz) and Fast mode (400 kHz)
 *   - Open-drain SDA emulation via PIO pindirs control
 *   - Clock stretching not supported (master-only, standard compliant)
 *   - 7-bit addressing
 *   - Repeated START (write-then-read)
 */

#ifndef PIO_I2C_H
#define PIO_I2C_H

#include <Arduino.h>

// I2C bus frequencies
#define PIO_I2C_FREQ_STANDARD  100000
#define PIO_I2C_FREQ_FAST      400000

// Acknowledge values
#define PIO_I2C_ACK   0
#define PIO_I2C_NACK  1

class PIO_I2C {
public:
    /*
     * Constructor.
     *   sda_pin  - GPIO pin for SDA (data line)
     *   scl_pin  - GPIO pin for SCL (clock line)
     *   freq     - Bus frequency in Hz (default: 100 kHz standard mode)
     */
    PIO_I2C(uint8_t sda_pin, uint8_t scl_pin, uint32_t freq = PIO_I2C_FREQ_STANDARD);

    /*
     * Initialize the PIO state machine and configure pins.
     * Must be called before any I2C operations.
     * Returns true on success.
     */
    bool begin();

    /*
     * Write bytes to the specified I2C device.
     *   addr  - 7-bit I2C address
     *   data  - pointer to data buffer
     *   len   - number of bytes to write
     *   stop  - send STOP condition after write (default: true)
     * Returns true if device acknowledged all bytes.
     */
    bool write(uint8_t addr, const uint8_t *data, size_t len, bool stop = true);

    /*
     * Read bytes from the specified I2C device.
     *   addr  - 7-bit I2C address
     *   data  - pointer to buffer for received data
     *   len   - number of bytes to read
     *   stop  - send STOP condition after read (default: true)
     * Returns true if device acknowledged its address.
     */
    bool read(uint8_t addr, uint8_t *data, size_t len, bool stop = true);

    /*
     * Combined write-then-read transaction (uses repeated START).
     *   addr      - 7-bit I2C address
     *   writeData - pointer to data to write (e.g., register address)
     *   writeLen  - number of bytes to write
     *   readData  - pointer to buffer for received data
     *   readLen   - number of bytes to read
     * Returns true on success.
     */
    bool writeThenRead(uint8_t addr,
                       const uint8_t *writeData, size_t writeLen,
                       uint8_t *readData, size_t readLen);

    /*
     * Scan the I2C bus for devices.
     * Prints addresses of responding devices to Serial.
     */
    void scan();

    /*
     * Deinitialize the PIO state machine and free resources.
     */
    void end();

    /*
     * Check if the I2C interface is initialized.
     */
    bool isInitialized() const { return _initialized; }

private:
    uint8_t  _sda;
    uint8_t  _scl;
    uint32_t _freq;

    PIO      _pio;
    uint     _sm;
    uint     _offset;

    bool     _initialized;

    // Send START, address+R/W byte, check ACK
    bool _sendAddress(uint8_t addr, bool read);

    // Send STOP condition
    void _sendStop();

    // Wait for PIO to finish current operation
    void _waitIdle();
};

#endif // PIO_I2C_H

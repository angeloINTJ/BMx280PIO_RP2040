/**
 * @file PIO_I2C.h
 * @brief I2C Master transport layer for RP2040 — GPIO bit-bang and PIO+DMA burst.
 *
 * Provides both blocking GPIO-based I2C (any pin pair) and high-speed
 * PIO+DMA burst transfers using the i2c.pio state machine with a
 * 3-channel DMA engine (TX, RX, CTRL pacer).
 *
 * @author angeloINTJ
 * @license MIT
 */

#ifndef PIO_I2C_H
#define PIO_I2C_H

#include <Arduino.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include <hardware/pwm.h>

/// I2C bus speed presets.
#define PIO_I2C_FREQ_STANDARD  100000   ///< 100 kHz standard mode.
#define PIO_I2C_FREQ_FAST      400000   ///< 400 kHz fast mode.

/**
 * @brief I2C master transport — GPIO bit-bang and PIO+DMA burst.
 *
 * Two operating modes:
 * - **GPIO mode**: Blocking bit-bang on any pin pair, used for sensor
 *   configuration and initialization.
 * - **PIO+DMA mode**: The PIO state machine executes I2C bursts autonomously.
 *   DMA CH1 feeds commands, CH2 collects responses. Single-burst reads
 *   via burstRead(); continuous reads via beginAutoScan() + readAllAsync().
 */
class PIO_I2C {
public:
    /**
     * @brief Construct an I2C master on the given pins.
     * @param sda GPIO pin for SDA (open-drain emulated via direction toggle).
     * @param scl GPIO pin for SCL.
     * @param freq Bus frequency in Hz (default 100 kHz).
     */
    PIO_I2C(uint8_t sda, uint8_t scl, uint32_t freq = PIO_I2C_FREQ_STANDARD);
    ~PIO_I2C();

    /// @name GPIO Bit-Bang API
    /// @{

    /// @brief Initialize GPIO pins for bit-bang I2C.
    bool begin();
    /// @brief Release GPIO pins.
    void end();
    bool isInitialized() const { return _initialized; }

    /// @brief Write data to an I2C device.
    bool write(uint8_t addr, const uint8_t *data, size_t len, bool stop = true);
    /// @brief Read data from an I2C device.
    bool read(uint8_t addr, uint8_t *data, size_t len, bool stop = true);
    /// @brief Write then read in two separate I2C transactions.
    bool writeThenRead(uint8_t addr,
                       const uint8_t *writeData, size_t writeLen,
                       uint8_t *readData, size_t readLen);
    /// @brief Scan the I2C bus and print found addresses to Serial.
    void scan();
    /// @}

    /// @name PIO+DMA Setup
    /// @{

    /**
     * @brief Load the PIO program and claim DMA channels.
     *
     * After this call, burstRead() is available for single-burst reads,
     * and beginAutoScan() can start continuous DMA operation.
     *
     * @param pio PIO instance (pio0 or pio1).
     * @return true if PIO program loaded and DMA channels claimed.
     */
    bool beginPIO(PIO pio = pio0);
    bool isPIOActive() const { return _pio_active; }
    /// @}

    /// @name PIO+DMA Burst Operations
    /// @{

    /**
     * @brief Execute a single PIO+DMA burst to read registers.
     *
     * Builds I2C burst commands (write register address + repeated start
     * + read len bytes), configures DMA CH1 (TX) and CH2 (RX), runs the
     * PIO state machine, and extracts the received bytes.
     *
     * @param addr 7-bit I2C address.
     * @param reg  Register address to read from.
     * @param dst  Destination buffer (must be at least len bytes).
     * @param len  Number of bytes to read (max 8).
     * @return true if the burst completed successfully.
     */
    bool burstRead(uint8_t addr, uint8_t reg, uint8_t *dst, size_t len);

    /**
     * @brief Start continuous DMA-driven auto-scan (experimental).
     *
     * The 3-channel DMA engine reads the sensor periodically. CH1 sends
     * commands, CH2 collects data to a ring buffer, and CH3 (triggered
     * by PWM) restarts CH1 each period.
     *
     * @param addr 7-bit I2C address.
     * @param reg  Register address to read.
     * @param buf  Ring buffer for received data (uint32_t array).
     * @param len  Number of data bytes per burst.
     * @param period_ms Sampling period in milliseconds.
     * @return true if auto-scan started.
     */
    bool beginAutoScan(uint8_t addr, uint8_t reg,
                       uint32_t *buf, size_t len, uint32_t period_ms);
    /// @brief Stop DMA auto-scan and restore GPIO pins.
    void stopAutoScan();
    bool isAutoScanActive() const { return _auto_scan; }

    /// @brief Extract bytes from 32-bit DMA words.
    /// ISR stores MSB at bit 7, LSB at bit 0 — already correct byte order.
    static void extractBytes(const uint32_t *src, uint8_t *dst, size_t len);

    /// @brief Reset RX DMA write pointer to the start of the ring buffer.
    void resetRxBuffer();
    /// @}

private:
    uint8_t  _sda, _scl;                    ///< GPIO pin numbers.
    uint32_t _freq;                         ///< I2C bus frequency in Hz.
    PIO      _pio;                          ///< PIO instance (pio0 or pio1).
    int      _sm, _offset;                  ///< PIO state machine and program offset.
    bool     _initialized;                  ///< GPIO pins initialized.
    bool     _pio_active;                   ///< PIO program loaded and DMA claimed.
    bool     _auto_scan;                    ///< Continuous DMA auto-scan active.

    int      _dma_tx_chan, _dma_rx_chan, _dma_ctrl_chan; ///< DMA channel numbers.
    int      _pwm_slice;                    ///< PWM slice for pacer trigger.
    uint     _burst_len;                    ///< Number of data bytes per burst.
    uint32_t *_burst_buf;                   ///< Pointer to ring buffer.

    static const size_t MAX_CMDS = 3 + 32;  ///< Max burst commands (3 overhead + 32 data).
    uint32_t _cmd_buf[MAX_CMDS];            ///< Command buffer for TX DMA.
    size_t   _cmd_count;                    ///< Number of commands in buffer.
    uint32_t _ctrl_data[2];                 ///< Pacer control data (count, address).

    /// @name Internal Helpers
    /// @{
    void _buildBurstCommands(uint8_t addr, uint8_t reg, size_t len);
    void _setupDMA();
    void _setupPacer();
    void _setupPWM(uint32_t period_ms);
    bool _sendAddress(uint8_t addr, bool read);
    void _sendStop();
    void _waitIdle();
    /// @}
};

#endif

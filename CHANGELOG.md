[🇧🇷 Ler em Português](CHANGELOG.pt-BR.md)

# Changelog

All notable changes to the BMx280PIO_RP2040 library.

## [1.3.0] - 2026-07-14

### Added
- PIO+DMA cold-start fix with prime of 5 dummy reads before `begin()`
- GPIO bit-bang for critical writes during `begin()` (reset and config registers)
- NACK retry on CTRL_MEAS register write (3 attempts, 500µs backoff)
- Dual-core I2C logic analyzer diagnostic example
- Comprehensive hardware test suite (5 sensors, 7 tests, 850+ readings, 0 failures)

### Fixed
- `setMode()` oversampling corruption bug (removed read-modify-write pattern)
- GPIO 0/1 + USB Serial coexistence documented correctly

### Changed
- Clarified that only `Serial1` (UART0) conflicts with GPIO 0/1, not USB CDC `Serial`

## [1.2.4] - 2026-07-12

### Fixed
- `maintainer` field corrected to boolean in `library.json`
- Dependency name in README: `TwoWirePIO_RP2040` → `WirePIO`
- Stale API docs, wrong defaults, missing methods in README
- Documentation review: phantom APIs, stale references, missing constants

### Added
- `forceGPIO()` method for benchmarking GPIO vs PIO
- Calibration fix for edge-case register reads
- Benchmark example

### Changed
- Complete README rewrite
- Added `@author` tags and Author section

## [1.2.0] - 2026-07-10

### Added
- PIO+DMA burstRead for zero-CPU register reads (~553 µs/read)
- GPIO bit-bang fallback on any pin pair
- Auto-detection of BMP280 vs BME280
- Configurable oversampling, IIR filter, and standby time
- Sleep, Forced, and Normal operating modes
- Bosch datasheet compensation (double-precision)

### Changed
- Migrated from Adafruit BME280 fork to standalone RP2040 driver
- PIO I2C state machine restructured for zero SCL glitch on START=0 commands

## [1.0.0] - 2026-07-01

### Added
- Initial release
- Hardware I2C (Wire) support
- Basic temperature, pressure, and humidity readings
- PlatformIO library registration

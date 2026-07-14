[🇧🇷 Ler em Português](CONTRIBUTING.pt-BR.md)

# Contributing to BMx280PIO_RP2040

Thanks for your interest in contributing! This document outlines the process for contributing to this project.

## Development Environment

1. **Install PlatformIO** (recommended):
   ```bash
   pip install platformio
   ```

2. **Clone the repository**:
   ```bash
   git clone https://github.com/angeloINTJ/BMx280PIO_RP2040.git
   cd BMx280PIO_RP2040
   ```

3. **Install dependencies**:
   ```bash
   pio pkg install
   ```

4. **Build**:
   ```bash
   pio run
   ```

## Code Style

This project uses **clang-format** with a Google-based style (4-space indent, 120-column limit).

- Run `clang-format -i src/*.h src/*.cpp` before committing.
- A `.clang-format` file is provided in the repository root.
- CI will reject PRs that don't pass `clang-format --dry-run --Werror`.

### Naming Conventions

| Element | Style | Example |
|---------|-------|---------|
| Classes | PascalCase | `BMx280PIO_RP2040` |
| Methods | camelCase | `readTemperature()` |
| Members | `_underscore` | `_t_fine`, `_osrs_t` |
| Constants | `UPPER_SNAKE` | `BME280_REG_CHIP_ID` |
| Defines | `UPPER_SNAKE` | `BMX280_ERR_NOT_FOUND` |
| File-scope helpers | `snake_case` | `i2c_delay()`, `sda_low()` |

### Error Handling

- Use the `BMx280Error` enum for return codes (not `Serial.println`).
- Set `_last_error` in all failure paths.
- Use `_debug` stream (if provided) for diagnostic output.

## Testing

Tests are hardware-based and run on a Raspberry Pi Pico with a BMP280/BME280 connected.

1. Flash the test firmware:
   ```bash
   cd tests/comprehensive
   pio run --target upload
   ```

2. Monitor serial output:
   ```bash
   pio device monitor
   ```

All tests must pass before a PR is merged.

## Pull Request Process

1. **Fork** the repository and create a feature branch.
2. **Format** your code with `clang-format`.
3. **Test** on real hardware if your change affects I2C communication.
4. **Update** the CHANGELOG.md with your changes.
5. **Submit** a PR with a clear description of the change.

### PR Checklist

- [ ] Code formatted with `clang-format`
- [ ] No `Serial.print` hardcoded (use `_debug` stream)
- [ ] Error codes set in all failure paths
- [ ] Tested on RP2040 hardware (if applicable)
- [ ] CHANGELOG.md updated

## Reporting Bugs

Use the [Bug Report](https://github.com/angeloINTJ/BMx280PIO_RP2040/issues/new?template=bug_report.md) template. Include:

- Library version
- Hardware setup (sensor model, wiring)
- Steps to reproduce
- Serial output / error messages

## License

By contributing, you agree that your contributions will be licensed under the MIT License.

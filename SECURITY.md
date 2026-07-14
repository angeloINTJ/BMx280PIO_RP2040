[🇧🇷 Ler em Português](SECURITY.pt-BR.md)

# Security Policy

## Reporting a Vulnerability

If you discover a security vulnerability in this library, please report it
**privately** by emailing the maintainer. Do **not** open a public issue.

The maintainer will respond within 48 hours with an acknowledgement and an
estimated timeline for a fix.

## Scope

This library communicates with Bosch BME280/BMP280 sensors over I2C. Security
considerations include:

- **I2C bus access**: Physical access to the I2C bus allows reading sensor data.
  This is a hardware-level concern and not something this library can mitigate.
- **Memory safety**: The library uses fixed-size buffers and avoids dynamic
  allocation beyond construction. Stack buffers are bounded.
- **Input validation**: Chip ID and calibration data are validated before use.

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 1.3.x   | :white_check_mark: |
| < 1.3.0 | :x:                |

## Disclosure Policy

After a fix is released, the vulnerability details will be disclosed publicly
in the release notes. Credit will be given to the reporter (unless they prefer
to remain anonymous).

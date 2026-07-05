# BME280_PIO - Driver BME280 via PIO para RP2040

[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Biblioteca Arduino para controle do sensor **Bosch BME280** (temperatura, umidade, pressão) utilizando os blocos **PIO (Programmable I/O)** do microcontrolador **RP2040** (Raspberry Pi Pico).

**Diferencial:** A comunicação I²C é implementada inteiramente via PIO state machines, sem utilizar os periféricos I²C de hardware do chip. Isso permite usar **qualquer par de pinos GPIO** e libera os periféricos de hardware para outras funções.

## Funcionalidades

- ✅ Temperatura, pressão e umidade com compensação completa (fórmulas Bosch)
- ✅ Modos de operação: Sleep, Forced (leitura única) e Normal (contínuo)
- ✅ Oversampling configurável: 1×, 2×, 4×, 8×, 16× por grandeza
- ✅ Filtro IIR configurável (off, 2, 4, 8, 16)
- ✅ I²C Standard (100 kHz) e Fast (400 kHz) via PIO
- ✅ Endereços I²C 0x76 e 0x77
- ✅ Cálculo de altitude e ponto de orvalho nos exemplos
- ✅ Compatível com PlatformIO e Arduino IDE

## Instalação

### PlatformIO

Adicione ao `platformio.ini`:

```ini
[env:pico]
platform = raspberrypi
board = rpipico
framework = arduino
lib_deps = angeloINTJ/bme280-pio-rp2040
```

### Arduino IDE

1. Instale o pacote **Raspberry Pi Pico/RP2040** do Earle Philhower
2. Instale a biblioteca pelo Library Manager: **BME280_PIO**
3. Ou clone este repositório na pasta `libraries/`

### Instalação Manual

```bash
cd ~/Arduino/libraries/
git clone https://github.com/angeloINTJ/bme280-pio-rp2040.git
```

## Uso Rápido

```cpp
#include <Arduino.h>
#include "BME280_PIO.h"

// SDA = GPIO4, SCL = GPIO5 (pode ser qualquer GPIO)
BME280_PIO bme(4, 5);

void setup() {
    Serial.begin(9600);

    if (!bme.begin()) {
        Serial.println("Sensor não encontrado!");
        while (1);
    }

    bme.setMode(BME280_MODE_NORMAL); // Modo contínuo
}

void loop() {
    float temp = bme.readTemperature();
    float press = bme.readPressure();
    float hum = bme.readHumidity();

    Serial.printf("T: %.2f °C | P: %.2f hPa | U: %.2f %%\n",
                  temp, press, hum);
    delay(2000);
}
```

## Pinagem

| BME280 | Raspberry Pi Pico |
|--------|-------------------|
| VCC    | 3.3V              |
| GND    | GND               |
| SDA    | GPIO4 (ou outro)  |
| SCL    | GPIO5 (ou outro)  |

> ⚠️ O BME280 opera a **3.3V**. Não conecte diretamente a 5V.

Os pinos SDA e SCL podem ser **quaisquer pinos GPIO** disponíveis — a mágica do PIO permite isso. Basta informar os números no construtor:

```cpp
BME280_PIO bme(sda_pin, scl_pin);  // Ex: BME280_PIO bme(2, 3);
```

## API

### Construtor

```cpp
BME280_PIO(uint8_t sdaPin, uint8_t sclPin,
           uint8_t addr = 0x76,
           uint32_t freq = 100000);
```

### Inicialização

```cpp
bool begin();            // Inicializa PIO e sensor, carrega calibração
void reset();            // Soft-reset do sensor
uint8_t getChipID();     // ID do chip (deve ser 0x60)
```

### Configuração

```cpp
void setTemperatureOversampling(uint8_t os);  // BME280_OS_SKIP, _1X, _2X, _4X, _8X, _16X
void setPressureOversampling(uint8_t os);
void setHumidityOversampling(uint8_t os);
void setFilter(uint8_t filter);               // BME280_FILTER_OFF, _2, _4, _8, _16
void setStandbyTime(uint8_t standby);          // Normal mode: 0.5ms a 1000ms
void setMode(uint8_t mode);                    // SLEEP, FORCED, NORMAL
bool takeForcedMeasurement();                  // Trigger + espera (modo forçado)
```

### Leitura

```cpp
float readTemperature();                        // °C
float readPressure();                           // hPa
float readHumidity();                           // %
void readAll(float *temp, float *press, float *hum);  // Leitura eficiente (1 burst)
```

### Registradores (acesso direto)

```cpp
uint8_t readRegister(uint8_t reg);
void writeRegister(uint8_t reg, uint8_t value);
void readRegisters(uint8_t reg, uint8_t *data, size_t len);
```

### Scanner I²C

O objeto `PIO_I2C` também oferece um scanner de barramento:

```cpp
PIO_I2C i2c(4, 5);
i2c.begin();
i2c.scan();  // Lista dispositivos no barramento I²C
```

## Exemplos

| Exemplo | Descrição |
|---------|-----------|
| [basic_reading](examples/basic_reading/) | Leitura contínua com cálculo de altitude |
| [forced_mode](examples/forced_mode/) | Modo forçado para baixo consumo (bateria) |

## Arquitetura

```
┌─────────────────────────────────────────┐
│  BME280_PIO (driver do sensor)          │
│  - Compensação Bosch (fixed-point 32/64)│
│  - Configuração de registradores        │
│  - Leitura raw + fórmulas de compensação│
└──────────────┬──────────────────────────┘
               │ writeThenRead()
┌──────────────▼──────────────────────────┐
│  PIO_I2C (I²C master via PIO)           │
│  - START/STOP via estado PIO            │
│  - Envio/recebimento de bytes           │
│  - ACK/NACK automáticos                 │
│  - Open-drain emulado via pindirs       │
└──────────────┬──────────────────────────┘
               │ PIO state machine
┌──────────────▼──────────────────────────┐
│  RP2040 PIO (hardware)                  │
│  - State machine dedicada               │
│  - Clock preciso (125-133 MHz base)     │
│  - FIFO de 4 words para TX/RX           │
└─────────────────────────────────────────┘
```

## Modos de Operação

| Modo | Consumo | Uso |
|------|---------|-----|
| **Sleep** | ~0.1 µA | Sensor parado, registradores preservados |
| **Forced** | ~0.5-3 µA médio | Uma leitura, depois volta a dormir. Ideal para bateria |
| **Normal** | ~3-5 µA | Leitura contínua com intervalo configurável |

## Oversampling e Precisão

| Setting | Resolução Temp | Resolução Press | Resolução Hum | Tempo (1× todos) |
|---------|---------------|-----------------|---------------|-------------------|
| 1× | 0.005 °C | 0.18 Pa | 0.003 %RH | ~9 ms |
| 16× | 0.0003 °C | 0.012 Pa | 0.0001 %RH | ~90 ms |

> Em modo normal com oversampling máximo, o consumo médio pode chegar a ~60 µA.

## Dependências

- [arduino-pico](https://github.com/earlephilhower/arduino-pico) — Arduino core para RP2040 (Earle Philhower)
- PlatformIO: plataforma `raspberrypi` ou Arduino IDE com pacote RP2040

## Licença

MIT — veja o arquivo [LICENSE](LICENSE) para detalhes.

O programa PIO (`pio/i2c.pio`) é baseado no exemplo oficial do [pico-examples](https://github.com/raspberrypi/pico-examples) da Raspberry Pi (licença BSD-3-Clause).

## Créditos

- **Bosch Sensortec** — Datasheet e fórmulas de compensação do BME280
- **Raspberry Pi Foundation** — Exemplo PIO I²C e SDK do RP2040
- **Earle Philhower** — Arduino-Pico core

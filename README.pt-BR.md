[🇺🇸 Read in English](README.md)

# BMx280PIO_RP2040 — Driver BMP280/BME280 para RP2040

[![Build](https://github.com/angeloINTJ/BMx280PIO_RP2040/actions/workflows/build.yml/badge.svg)](https://github.com/angeloINTJ/BMx280PIO_RP2040/actions/workflows/build.yml)
[![Lint](https://github.com/angeloINTJ/BMx280PIO_RP2040/actions/workflows/lint.yml/badge.svg)](https://github.com/angeloINTJ/BMx280PIO_RP2040/actions/workflows/lint.yml)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-compatible-orange.svg)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

Biblioteca Arduino para o sensor ambiental **Bosch BMP280/BME280** no **RP2040** (Raspberry Pi Pico). Usa **PIO+DMA burstRead** (3.3× mais rápido que Adafruit) com fallback GPIO bit-bang e suporte a hardware Wire.

## Funcionalidades

- ✅ **PIO+DMA burstRead @ 200 kHz** — 553 µs/leitura, 1808 leituras/s em hardware
- ✅ **Fallback GPIO bit-bang** — qualquer par de pinos, confiável a 100 kHz (3522 µs/leitura)
- ✅ **I2C por Hardware (Wire)** — modo compatível com Adafruit
- ✅ Modo `forceGPIO()` para comparativo de desempenho GPIO vs PIO
- ✅ Auto-detecção BMP280 vs BME280 por chip ID
- ✅ Modos de operação Sleep, Forced e Normal
- ✅ Oversampling configurável: 1× a 16× por canal
- ✅ Filtro IIR e configuração de standby time
- ✅ Compensação Bosch do datasheet (precisão dupla)
- ✅ Compatível com PlatformIO e Arduino IDE

## Início Rápido

### I2C via PIO+DMA (qualquer pino, automático)

O construtor com pinos GPIO inicializa automaticamente PIO+DMA para leituras de alto desempenho.

```cpp
#include <Arduino.h>
#include "BMx280PIO_RP2040.h"

BMx280PIO_RP2040 sensor(2, 3);  // SDA=GP2, SCL=GP3, I2C @ 200 kHz

void setup() {
    Serial.begin(115200);
    sensor.begin();    // PIO+DMA burstRead inicializado automaticamente
    sensor.setMode(BME280_MODE_NORMAL);
}
void loop() {
    float t, p, h; sensor.readAll(&t, &p, &h);
    Serial.printf("T=%.2f°C P=%.2fhPa\n", t, p);
    delay(2000);
}
```

### I2C por Hardware (Wire)

```cpp
#include <Arduino.h>
#include "BMx280PIO_RP2040.h"

BMx280PIO_RP2040 sensor(Wire, 0x76);  // Usa I2C0 por hardware

void setup() {
    Wire.begin();
    Serial.begin(115200);
    sensor.begin();
}
```

### GPIO Bit-Bang (fallback forçado)

```cpp
BMx280PIO_RP2040 sensor(2, 3);
sensor.forceGPIO(true);   // Ignora PIO+DMA, usa apenas GPIO bit-bang
sensor.begin();
```

### Saída de Depuração

```cpp
BMx280PIO_RP2040 sensor(2, 3);
sensor.begin(&Serial);    // Ativa saída de diagnóstico no Serial
// ou: sensor.begin();    // Operação silenciosa (padrão)
```

### Tratamento de Erros

```cpp
BMx280PIO_RP2040 sensor(2, 3);
if (!sensor.begin(&Serial)) {
    BMx280Error err = sensor.getLastError();
    if (err == BMX280_ERR_CHIP_ID) {
        // Sensor ou endereço incorreto
    } else if (err == BMX280_ERR_NOT_FOUND) {
        // Sensor não detectado (verifique a fiação)
    }
}
```

## Ligações

| BMP280/BME280 | Raspberry Pi Pico |
|---------------|-------------------|
| VCC           | 3.3V              |
| GND           | GND               |
| SDA           | GPIO2 (ou qualquer) |
| SCL           | GPIO3 (ou qualquer) |

> ⚠️ O sensor opera a **3.3V**. Não conecte a 5V. Recomenda-se resistores pull-up externos de 10kΩ no SDA e SCL para 3.3V.

## Resultados de Testes em Hardware

Testado em BMP280 (chip ID 0x58) no endereço 0x76, GPIO2=SDA, GPIO3=SCL.

| Método | Tempo/leitura | Throughput | Notas |
|--------|---------------|------------|-------|
| **PIO+DMA (200 kHz)** | **553 µs** | **1808 l/s** | 3.3× mais rápido que Adafruit |
| Adafruit Wire (100 kHz) | 1837 µs | 544 l/s | Baseline I2C por hardware |
| GPIO bit-bang (100 kHz) | 3522 µs | 284 l/s | Fallback confiável |

Todos os exemplos (`basic_reading`, `forced_mode`, `auto_scan`, `multi_sensor`, `benchmark`) verificados em hardware.

## Arquitetura

```
┌────────────────────────────────────────────────────────────┐
│  BMx280PIO_RP2040 (driver do sensor)                       │
│  - Fórmulas de compensação Bosch                            │
│  - Auto-detecção BMP280 vs BME280                           │
└──────────────┬─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│  WirePIO (transporte I2C)                                   │
│  - PIO+DMA burstRead: 8 registradores em uma transação     │
│  - Fallback GPIO bit-bang em qualquer par de pinos         │
│  - Motor DMA de 2 canais: TX (cmd → PIO), RX (PIO → buf)  │
└──────────────┬─────────────────────────────────────────────┘
               │
┌──────────────▼─────────────────────────────────────────────┐
│  PIO State Machine (i2c.pio — 31 instruções)               │
│  - I2C master bit-bang (SCL via side-set, SDA via OUT/SET) │
│  - Palavra de comando: START + READ + 8-bit data + STOP    │
│  - Autopush dados RX para FIFO no limiar de 8 bits         │
│  - Push explícito para bits de ACK                          │
└────────────────────────────────────────────────────────────┘
```

### Leitura em Rajada PIO+DMA

O construtor com pinos GPIO carrega automaticamente o programa PIO e configura os
canais DMA. Cada chamada `readAll()` executa uma transação em rajada sem uso de CPU:

1. DMA CH1 envia palavras de comando para o TX FIFO do PIO
2. State machine PIO executa: START + write register + RESTART + read 8 bytes
3. DMA CH2 drena os bytes de dados do RX FIFO para um buffer
4. CPU extrai os bytes e executa a compensação Bosch

Detalhes importantes de implementação:
- **Leituras de calibração em blocos** — calibração de 26 bytes dividida em blocos ≤8 bytes para compatibilidade com burstRead
- **Registradores DMA manuais** — contorna bug do SDK `dma_channel_configure()` com contagem TX
- **DMA habilitado após início do PIO SM** — garante que os sinais DREQ estejam ativos
- **Prólogo do PIO reestruturado** — flags START/READ extraídos via `out y/x` antes de qualquer borda SCL, eliminando glitches

## Programa PIO — Decisões de Design

### Codificação de Comando (16-bit, shift LSB-first do OSR)

| Bit | Nome | Descrição |
|-----|------|-----------|
| 0 | START | 1 = gerar START antes deste byte |
| 1 | READ | 1 = ler do dispositivo, 0 = escrever |
| 9:2 | DATA | Byte de dados para escrita (`~rev8(data)`) |
| 10 | STOP | 1 = gerar STOP após este byte |

### Correção de Glitch SCL

**Problema**: O prólogo original usava `out pindirs, 1 side 1` para extrair o flag START. Para comandos com START=0, isso forçava SCL HIGH por 1 ciclo — uma borda de clock espúria que o BME280 interpretava como um pulso SCL extra, deslocando todos os dados de leitura em 1 bit.

**Solução**: O prólogo reestruturado extrai ambos os flags START e READ via `out y`/`out x` **antes de qualquer borda SCL**. START é gerado condicionalmente usando `set pindirs` (sem consumo do OSR), seguido de `set pindirs, 0` para liberar o SET e permitir que SDA flutue para leituras.

```
pull block          side 0    ; SCL LOW
out y, 1            side 0    ; Y = START (OSR bit 0)
out x, 1            side 0    ; X = READ  (OSR bit 1)
jmp !y, branch      side 0    ; START=0 → pula (SCL nunca vai HIGH!)
set pindirs, 1      side 1    ; SCL↑ + SDA↓ = START
set pindirs, 0      side 0 [1]; libera SET, SDA flutua novamente
branch:
    jmp !x, write_byte side 0 ; READ=0 → caminho de escrita
```

**Resultado**: Zero glitch SCL para comandos START=0. Dados de leitura correspondem exatamente à referência GPIO.

### Extração de Dados: Sem Necessidade de Reversão de Bits

O ISR do PIO com `shift_in_right=false` armazena o primeiro bit recebido (I2C MSB)
em ISR[7] e o último bit recebido (I2C LSB) em ISR[0]. O byte já está **na
ordem I2C correta** — não é necessário `rev8()`.

A extração é simplesmente: `dst[i] = rxbuf[i] & 0xFF`

### Analisador Lógico Dual-Core

Durante o desenvolvimento, uma técnica poderosa de depuração foi usada: **Core 1 como
analisador lógico**. O Core 1 amostra SDA/SCL via `sio_hw->gpio_in` a ~5 MHz em um
buffer circular de 4K enquanto o Core 0 executa a rajada PIO+DMA. Isso revelou:

- O bug de contagem de transferência TX do `dma_channel_configure()` (registrador sempre lia 0)
- O tempo de setup ACK ausente (SDA e SCL mudando no mesmo ciclo PIO)
- O problema de ordenação de habilitação DMA (DREQ não ativo antes do PIO SM iniciar)

### Recuperação SCL

O PIO envia um pulso ACK (SDA LOW durante o 9º SCL) após cada byte de leitura,
mantendo o sensor em modo streaming para leituras multi-byte. Após a conclusão
da rajada, `burstRead()` gera um pulso de recuperação SCL via GPIO para garantir
que o barramento retorne ao estado ocioso.

## Referência da API

### Construtor

```cpp
// I2C via PIO+DMA em qualquer pino GPIO
BMx280PIO_RP2040 sensor(uint8_t sda, uint8_t scl,
                  uint8_t addr = 0x76,
                  uint32_t freq = 200000, PIO pio = pio0);

// I2C por Hardware (Wire)
BMx280PIO_RP2040 sensor(TwoWire &wire, uint8_t addr = 0x76);
```

### Configuração

```cpp
bool begin();                    // Inicializa sensor e carrega calibração
bool beginPIO(PIO pio = pio0);   // Reinicia PIO+DMA em outro bloco PIO
void setMode(uint8_t mode);      // SLEEP, FORCED, NORMAL
bool takeForcedMeasurement();    // Dispara conversão única + espera
void setTemperatureOversampling(uint8_t os);  // 1× a 16×
void setPressureOversampling(uint8_t os);
void setHumidityOversampling(uint8_t os);
void setFilter(uint8_t filter);              // OFF, 2, 4, 8, 16
void setStandbyTime(uint8_t standby);        // 250ms a 1000ms
```

### Leituras

```cpp
float readTemperature();         // °C
float readPressure();            // hPa
float readHumidity();            // % (0 se BMP280)
void  readAll(float *t, float *p, float *h);
```

### Controle de Modo GPIO

```cpp
void forceGPIO(bool f);          // Força GPIO bit-bang (ignora PIO+DMA)
bool isForcedGPIO();             // Verifica se modo GPIO está ativo
```

### Utilitários

```cpp
uint8_t getChipID();             // 0x58 = BMP280, 0x60 = BME280
bool isBME280();                 // True se sensor de umidade presente
bool isInitialized();            // True se sensor pronto
BMx280Error getLastError();      // Último código de erro (limpo no begin())
uint8_t readRegister(uint8_t reg);
void    writeRegister(uint8_t reg, uint8_t value);
void    readRegisters(uint8_t reg, uint8_t *data, size_t len);
```

### Códigos de Erro

| Código | Descrição |
|--------|-----------|
| `BMX280_OK` | Sem erro |
| `BMX280_ERR_NOT_FOUND` | Sensor não detectado no endereço I2C |
| `BMX280_ERR_CHIP_ID` | Chip ID inválido ou desconhecido |
| `BMX280_ERR_CAL_FAIL` | Falha na leitura dos dados de calibração |
| `BMX280_ERR_I2C_WRITE` | Falha na escrita I2C (NACK) |
| `BMX280_ERR_I2C_READ` | Falha na leitura I2C |
| `BMX280_ERR_MEAS_TIMEOUT` | Medição não completou a tempo |
| `BMX280_ERR_NOT_INIT` | Sensor não inicializado |
| `BMX280_ERR_CTRL_MEAS` | Falha na escrita do registrador CTRL_MEAS após tentativas |

## Modos de Operação

| Modo | Corrente | Caso de Uso |
|------|----------|-------------|
| **Sleep** | 0.1 µA | Sensor ocioso, registradores preservados |
| **Forced** | 1.2 mA (pico) / ~3 µA médio | Medição única, retorno automático ao sleep |
| **Normal** | 1.2 mA (pico) / ~2.7 µA @ 1 Hz | Medição contínua, intervalo configurável |

## Solução de Problemas

### Sensor Não Detectado

- **Verifique a fiação**: Confira as conexões SDA e SCL. Certifique-se de que VCC = 3.3V (não 5V).
- **Resistores pull-up**: O BME280 requer pull-ups externos de 10kΩ em SDA e SCL para 3.3V. Os pull-ups internos do RP2040 (~50kΩ) podem não ser suficientes em velocidades mais altas.
- **Endereço errado**: Tente `0x77` em vez de `0x76` (algumas placas ligam SDO ao VCC).
- **Atraso de cold-start**: Após ligar, aguarde pelo menos 2ms antes de chamar `begin()`. O driver trata isso internamente.

### Leituras Instáveis

- **Aumente o oversampling**: `sensor.setTemperatureOversampling(BME280_OS_16X)` etc. Maior oversampling reduz ruído.
- **Ative o filtro IIR**: `sensor.setFilter(BME280_FILTER_16)` para suavização máxima.
- **Verifique a fonte de alimentação**: O BME280 consome ~1.2mA durante a medição. Uma fonte de 3.3V ruidosa ou fraca causa leituras instáveis.
- **Aguarde estabilização**: No modo normal, as primeiras 2-3 leituras após mudanças de configuração podem ser instáveis.

### PIO+DMA Não Funciona

- **Tente o fallback GPIO**: `sensor.forceGPIO(true)` para usar bit-bang em vez de PIO+DMA.
- **Verifique compatibilidade de pinos**: PIO+DMA funciona em qualquer par de pinos GPIO. Se estiver usando GPIO 0/1, certifique-se de que `Serial1` (UART0 por hardware) não está ativo.
- **Verifique a versão do WirePIO**: Requer `WirePIO >= 1.3.5`.

### begin() Retorna false

- Use a saída de depuração para identificar o ponto de falha:
  ```cpp
  sensor.begin(&Serial);  // exibe: "BMx280: bad chip ID 0x00" etc.
  ```
- Verifique `sensor.getLastError()` para o código de erro específico.

## Limitações Conhecidas

1. **STOP após leituras**: O caminho READ lê o flag STOP da posição errada do bit OSR
   (bit 3 em vez de bit 10). A recuperação GPIO em `burstRead()` trata a limpeza do barramento.
2. **Offset na primeira leitura**: O primeiro byte da primeira leitura após o modo forced
   pode ter o MSB de pressão ligeiramente deslocado. É uma característica do sensor, não um bug do driver — leituras subsequentes são estáveis.
3. **Wrap do buffer circular** no modo burst: o buffer circular DMA CH2 (32 bytes) rotaciona
   após 8 palavras. A rajada de 11 palavras (3 ACKs + 8 data bytes) rotaciona as últimas
   3 palavras para as posições 0-2. O código de extração trata isso corretamente.
4. **Exclusivo para RP2040**: Requer o periférico PIO, exclusivo do RP2040/RP2350.

## Notas Técnicas

### Temporização de Bits I2C

Com divisor de clock PIO ≈ 96.15 (125 MHz sysclk ÷ 1.3 MHz clock PIO):

| Parâmetro | Tempo | Especificação Fast Mode |
|-----------|-------|-------------------------|
| SCL high | 2.3 µs | ≥ 0.6 µs |
| SCL low | 6.9–13.1 µs | ≥ 1.3 µs |
| Frequência SCL | ~65–87 kHz | ≤ 400 kHz |

A temporização é conservadora (mais lenta que a especificação máxima) para operação robusta. O BME280 tolera o clock mais lento de forma transparente.

## Dependências

- **[WirePIO](https://github.com/angeloINTJ/TwoWirePIO_RP2040) (>=1.3.5)** — Camada de transporte I2C via PIO+DMA (instalada automaticamente pelo Library Manager)
- [arduino-pico](https://github.com/earlephilhower/arduino-pico) — Núcleo Arduino para RP2040 (Earle Philhower)

## Changelog

Veja [CHANGELOG.md](CHANGELOG.md) para o histórico completo.

### v1.3.0 (2026-07-14)

- **Correção cold-start PIO+DMA** — Prime da SM com 5 leituras dummy antes de `begin()` para operação confiável no primeiro boot
- **GPIO bit-bang para escritas críticas** — Escritas nos registradores de reset e config usam GPIO durante `begin()`, eliminando a não-confiabilidade de escritas PIO+DMA em cold-start
- **Correção de oversampling no `setMode()`** — Remove padrão read-modify-write que corrompia bits de oversampling de temperatura/pressão
- **Retry NACK no CTRL_MEAS** — Tenta até 3 vezes em caso de NACK com backoff de 500µs para transições de modo robustas
- **Analisador lógico I2C dual-core** — Exemplo de diagnóstico usando Core 1 como analisador lógico de ~5 MHz para depurar problemas de temporização PIO+DMA
- **Suíte de testes abrangente** — 5 sensores, 7 testes, 850+ leituras, 0 falhas em PIO+DMA, GPIO bit-bang, modo forced, hardware Wire, multi-sensor, benchmark e estabilidade de 30s
- **Melhorias de qualidade de código** — Helpers GPIO renomeados (`dly`→`i2c_delay`, etc.), formatação adequada, enum de códigos de erro, suporte a stream de depuração, workflows CI/CD, arquivos de governança
- **Coexistência GPIO 0/1 + USB Serial** — Documentado que GPIO 0/1 funcionam para I2C simultaneamente com USB CDC Serial (apenas `Serial1` UART0 por hardware conflita)

### v1.2.4 (2026-07-12)

- Corrige campo `maintainer` como booleano no `library.json`
- Corrige nome da dependência no README (`TwoWirePIO_RP2040` → `WirePIO`)
- Reescrita completa do README: corrige docs de API obsoletas, defaults errados, métodos ausentes

## Licença

MIT — veja [LICENSE](LICENSE) para detalhes.

## Autor

Ângelo Moisés Alves — [@angeloINTJ](https://github.com/angeloINTJ)

## Créditos

- **Bosch Sensortec** — Datasheet BME280 e fórmulas de compensação
- **Raspberry Pi Foundation** — Exemplo I2C PIO e SDK RP2040
- **Earle Philhower** — Núcleo Arduino-Pico

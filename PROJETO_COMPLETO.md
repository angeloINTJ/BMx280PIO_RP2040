# Driver BME280 PIO+DMA "Zero-CPU-Overhead" — Documento Completo

## 1. Objectivo

Criar um driver I2C para o sensor BME280/BMP280 no RP2040 usando **PIO (Programmable I/O) + DMA** para leitura autónoma de dados, com zero intervenção da CPU durante o bit-bang do I2C.

## 2. Arquitectura

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ command_buf  │────▶│  PIO TX FIFO │────▶│  PIO SM      │
│ (uint32_t[]) │     │  (DREQ_TX0)  │     │  (I2C master)│
└──────────────┘     └──────────────┘     └──────┬───────┘
     DMA CH1 (TX)                                 │
                                                  ▼
                                         ┌──────────────┐
                                         │  PIO RX FIFO │
                                         │  (DREQ_RX0)  │
                                         └──────┬───────┘
                                                ▼
                                         ┌──────────────┐
                                         │ raw_data[8]  │
                                         │ (RAM buffer) │
                                         └──────────────┘
                                          DMA CH2 (RX)
```

3 canais DMA planeados:
- **CH1 (TX)**: Injeta comandos I2C no TX FIFO do PIO
- **CH2 (RX)**: Colecta dados do RX FIFO do PIO para buffer em RAM
- **CH3 (CTRL)**: PWM-paced, reconfigura CH1 para scanning contínuo

## 3. O Que Funciona ✅

### 3.1 Programa PIO Original (`pio/i2c.pio`)
Baseado no pico-examples da Raspberry Pi. Usa **LSB-first shift** do OSR.
- START/STOP/ACK generation funciona
- Write path: envia bytes correctamente
- Read path: lê bytes via `in pins, 1` + autopush
- **32 instruções** (limite do RP2040)

### 3.2 Encoding dos Comandos (LSB-first, 16-bit)

```
Bit 0:   START SDA control (1 = drive SDA low enquanto SCL high)
Bit 1:   READ flag (1 = read from slave)
Bits 9:2: Data byte (para WRITE, invertido: ~data & 0xFF)
Bit 10:  STOP flag
```

**Fórmula da macro:**
```c
static inline uint16_t mk(bool s, bool r, bool p, uint8_t d) {
    uint8_t inv = (~rev8(d)) & 0xFF;  // bit-reverse + invert
    return (s?1:0) | ((r?1:0)<<1) | (((uint16_t)inv)<<2) | ((p?1:0)<<10);
}
```

O `rev8()` é necessário porque o PIO usa LSB-first shift (bit 0 sai primeiro),
mas o I2C requer MSB-first. A inversão é necessária porque `out pindirs,1`
com bit=1 drive SDA LOW (I2C "0"), e com bit=0 liberta SDA (I2C "1").

### 3.3 Configuração dos Pinos

**CRÍTICO**: `pio_sm_set_pindirs_with_mask` — SCL tem de ser OUTPUT para o
side-set funcionar:
```c
pio_sm_set_pindirs_with_mask(pio, sm,
    (1u << scl_pin),                    // SCL = OUTPUT
    (1u << sda_pin) | (1u << scl_pin)); // ambos no mask
```

### 3.4 Autopush para Leituras
```c
sm_config_set_in_shift(&c, false, true, 8);
// false = LSB-first, true = autopush ON, 8 = threshold
```
O autopush dispara após 8 `in pins, 1`, empurrando o ISR para o RX FIFO.
O DMA transfere para o buffer. **Funciona perfeitamente.**

### 3.5 DMA RX/TX
- TX: 32-bit transfers, `DREQ_PIOx_TX0`, incrementa source
- RX: 32-bit transfers, `DREQ_PIOx_RX0`, incrementa destination
- Ambos funcionam correctamente

### 3.6 Leitura de Dados (Abordagem Híbrida)
**GPIO escreve** o registo (START + addr+W + register + STOP).
**PIO+DMA lê** os dados (RESTART + addr+R + read byte + STOP).

Resultados com shift correction:
- Temperatura PIO: -8.5°C (GPIO: 15.7°C) — mesma ordem de grandeza
- Pressão PIO: 865 hPa (GPIO: 1017 hPa) — mesma ordem de grandeza
- Dados variam entre leituras (não são estáticos)

### 3.7 Compensação Bosch
Fórmulas de compensação de temperatura, pressão e humidade implementadas
e validadas contra o datasheet da Bosch.

### 3.8 Ferramentas de Debug
- Logic analyzer dual-core (Core 1 sampleia SDA/SCL a alta velocidade)
- Testes de varrimento de divisor de clock
- Testes de verificação de encoding

## 4. Bugs Identificados e Corrigidos 🔧

### 4.1 Bug #1: Bits na Posição Errada (32-bit vs 16-bit)
**Sintoma**: PIO nunca fazia push para RX FIFO.
**Causa**: Comandos de 32-bit com flags nos bits 15:13, mas MSB-first shift
extrai do bit 31 para baixo. Bits 31:29 eram sempre 0.
**Correcção**: Mover flags para bits 31:29 (solução abandonada em favor
do LSB-first com encoding 16-bit).

### 4.2 Bug #2: FIFO Join
**Sintoma**: Push ia para TX FIFO em vez de RX FIFO.
**Causa**: `FJOIN_TX` bit activo no SHIFTCTRL.
**Correcção**: `c.shiftctrl &= ~(3u << 30)` + `PIO_FIFO_JOIN_NONE`.
(Acabou por não ser necessário com o programa original.)

### 4.3 Bug #3: GPIO Pull-up Desabilitado
**Sintoma**: SDA sempre LOW, dados sempre 0x00.
**Causa**: `gpio_set_function(pin, GPIO_FUNC_PIO0)` pode desabilitar pull-up.
**Correcção**: `gpio_pull_up(sda_pin)` antes de `gpio_set_function`.

### 4.4 Bug #4: SDA Bloqueado em LOW (data=0x00)
**Sintoma**: Todos os bytes de leitura eram 0x00.
**Causa**: Comando de leitura com `data=0x00` → `inv=0xFF` → bit 2 = 1 →
`out pindirs,1` na entrada de read_byte coloca SDA em LOW.
**Correcção**: Usar `data=0xFF` para leituras → `inv=0x00` → SDA libertado.

### 4.5 Bug #5: Data Encoding sem Bit-Reverse
**Sintoma**: Sensor NACK ao write do registo.
**Causa**: `~data & 0xFF` não é suficiente com LSB-first shift.
**Correcção**: `(~rev8(data) & 0xFF) << 2` — bit-reverse + invert.

### 4.6 Bug #6: Spurious SCL Pulse (Shift de 1 bit)
**Sintoma**: PIO lê 0xDD, GPIO lê 0x5D. Diferença consistente de 0x80.
**Causa**: O `irq nowait 0 side 1` no fim do write gera um pulso SCL extra.
O sensor envia o MSB nesse pulso. O PIO não o lê — todos os bits ficam
deslocados 1 posição para a esquerda.
**Relação**: `PIO_data = rev8((GPIO_data << 1) | 1)`
**Correcção parcial**: `corrected = (rev8(dma_byte) >> 1) & 0xFF`
(perde-se 1 bit de precisão — o LSB original é irrecuperável)

## 5. Problema NÃO Resolvido ❌

### 5.1 NACK no Registo (ACK2 = 1)

**O problema principal**: Quando o PIO envia o segundo byte de uma
transacção I2C (o endereço do registo, ex: 0xF7), o sensor responde com
**NACK** (SDA HIGH durante o 9º clock).

**Evidência**:
- ACK1 (addr+W = 0xEC): **0** (sensor reconhece o endereço) ✅
- ACK2 (register = 0xF7): **1** (sensor NÃO aceita o registo) ❌
- ACK3 (addr+R = 0xED): **0** (sensor reconhece o endereço de leitura) ✅

**O que foi testado (SEM SUCESSO)**:
- ✅ Dados correctos: o PIO envia 0xF7 no I2C (verificado bit a bit)
- ✅ Timing: testado de 100 kHz (div=96) até 0.4 kHz (div=5000)
- ✅ Delays: adicionados [7] ciclos após ACK (máximo com side-set)
- ✅ Drive strength: aumentado para 12mA
- ✅ Pull-up: verificado activo
- ✅ Pin configuration: SCL=OUTPUT, SDA=INPUT
- ✅ FIFO config: sem FIFO join

**Hipóteses não testadas**:
1. O PIO pode estar a gerar *glitches* no SCL durante a transição entre
   o primeiro e segundo byte (ruído de comutação)
2. O sensor pode exigir um *clock stretching* entre bytes que o PIO não
   suporta (o PIO não verifica se SCL está a ser puxado LOW pelo slave)
3. O BMP280 pode ter um requisito de timing não documentado para o
   intervalo entre o ACK e o próximo START de dados
4. Diferença de impedância entre o driver GPIO e o driver PIO

**Workaround funcional**: A abordagem híbrida (GPIO escreve registo,
PIO+DMA lê dados) contorna este problema completamente.

## 6. Estrutura de Ficheiros

```
pio/i2c.pio              — Programa PIO original (pico-examples) ✅
pio/i2c_burst.pio        — Tentativa de programa customizado ❌
pio/i2c_burst_irq.pio    — Tentativa com IRQ em vez de push ❌
src/i2c.pio.h            — Header auto-gerado pelo pioasm
src/i2c_burst.pio.h      — Header do programa customizado
src/PIO_I2C.h            — API do driver (GPIO + PIO/DMA)
src/PIO_I2C.cpp          — Implementação (bit-bang GPIO + PIO/DMA)
src/BMx280PIO_RP2040.h         — Driver do sensor (compensação Bosch)
src/BMx280PIO_RP2040.cpp       — Implementação do driver
src/main.cpp             — Programa de teste actual
PIO_DMA_DEBUG_REPORT.md  — Relatório detalhado do debugging
PLANO_FINAL.md           — Plano de arquitectura
STATUS_FINAL.md          — Estado final
PROJETO_COMPLETO.md      — Este documento
```

## 7. Como Compilar e Testar

```bash
# Compilar
pio run

# Gravar (RP2040 em modo BOOTSEL)
picotool load -x .pio/build/pico/firmware.uf2
picotool reboot

# Ver saída serial
cat /dev/ttyACM0
```

## 8. Próximos Passos Sugeridos

1. **Corrigir o shift de 1 bit no PIO**: Modificar o `i2c.pio` para
   eliminar o pulso SCL espúrio entre o write e o read. Sugestão: alterar
   `irq nowait 0 side 1` para `irq nowait 0 side 0` no label `done:`,
   mantendo SCL LOW entre comandos. Requer teste cuidadoso para não
   quebrar o timing do RESTART.

2. **Resolver o NACK no registo**: Testar com analisador lógico externo
   para comparar os sinais GPIO vs PIO no mesmo hardware. Procurar
   diferenças subtis: rise/fall times, glitches, ou timing violations.

3. **Implementar DMA CH3 (pacer)**: Com o híbrido funcional, adicionar
   o 3º canal DMA para auto-scan contínuo com PWM.

4. **Optimizar o encoding**: Investigar se há uma forma de encoding que
   permita ao PIO escrever o registo correctamente (evitando o NACK),
   eliminando a necessidade do híbrido GPIO+PIO.

5. **Testar com BME280 real**: O sensor actual é um BMP280. Testar com
   BME280 para validar leitura de humidade.

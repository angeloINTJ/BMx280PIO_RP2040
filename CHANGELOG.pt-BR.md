[🇺🇸 Read in English](CHANGELOG.md)

# Changelog

Todas as alterações notáveis na biblioteca BMx280PIO_RP2040.

## [1.3.0] - 2026-07-14

### Adicionado
- Correção de cold-start PIO+DMA com prime de 5 leituras dummy antes de `begin()`
- GPIO bit-bang para escritas críticas durante `begin()` (registradores de reset e config)
- Retry NACK na escrita do registrador CTRL_MEAS (3 tentativas, 500µs de backoff)
- Exemplo de diagnóstico com analisador lógico I2C dual-core
- Suíte de testes abrangente em hardware (5 sensores, 7 testes, 850+ leituras, 0 falhas)

### Corrigido
- Bug de corrupção de oversampling no `setMode()` (removido padrão read-modify-write)
- Documentação correta da coexistência GPIO 0/1 + USB Serial

### Alterado
- Esclarecido que apenas `Serial1` (UART0) conflita com GPIO 0/1, não USB CDC `Serial`

## [1.2.4] - 2026-07-12

### Corrigido
- Campo `maintainer` corrigido para booleano no `library.json`
- Nome da dependência no README: `TwoWirePIO_RP2040` → `WirePIO`
- Docs de API obsoletas, defaults errados, métodos ausentes no README
- Revisão de documentação: APIs fantasmas, referências obsoletas, constantes ausentes

### Adicionado
- Método `forceGPIO()` para comparativo GPIO vs PIO
- Correção de calibração para leituras de registrador em casos limite
- Exemplo de benchmark

### Alterado
- Reescrita completa do README
- Adicionadas tags `@author` e seção Author

## [1.2.0] - 2026-07-10

### Adicionado
- PIO+DMA burstRead para leituras de registrador sem uso de CPU (~553 µs/leitura)
- Fallback GPIO bit-bang em qualquer par de pinos
- Auto-detecção de BMP280 vs BME280
- Oversampling, filtro IIR e standby time configuráveis
- Modos de operação Sleep, Forced e Normal
- Compensação Bosch do datasheet (precisão dupla)

### Alterado
- Migrado do fork Adafruit BME280 para driver RP2040 independente
- State machine I2C PIO reestruturada para zero glitch SCL em comandos START=0

## [1.0.0] - 2026-07-01

### Adicionado
- Lançamento inicial
- Suporte a I2C por hardware (Wire)
- Leituras básicas de temperatura, pressão e umidade
- Registro na biblioteca PlatformIO

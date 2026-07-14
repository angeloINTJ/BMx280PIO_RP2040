[🇺🇸 Read in English](CONTRIBUTING.md)

# Contribuindo com BMx280PIO_RP2040

Obrigado pelo interesse em contribuir! Este documento descreve o processo para contribuir com este projeto.

## Ambiente de Desenvolvimento

1. **Instale o PlatformIO** (recomendado):
   ```bash
   pip install platformio
   ```

2. **Clone o repositório**:
   ```bash
   git clone https://github.com/angeloINTJ/BMx280PIO_RP2040.git
   cd BMx280PIO_RP2040
   ```

3. **Instale as dependências**:
   ```bash
   pio pkg install
   ```

4. **Compile**:
   ```bash
   pio run
   ```

## Estilo de Código

Este projeto usa **clang-format** com estilo baseado no Google (indentação de 4 espaços, limite de 120 colunas).

- Execute `clang-format -i src/*.h src/*.cpp` antes de commitar.
- Um arquivo `.clang-format` é fornecido na raiz do repositório.
- O CI rejeitará PRs que não passarem em `clang-format --dry-run --Werror`.

### Convenções de Nomenclatura

| Elemento | Estilo | Exemplo |
|----------|--------|---------|
| Classes | PascalCase | `BMx280PIO_RP2040` |
| Métodos | camelCase | `readTemperature()` |
| Membros | `_underscore` | `_t_fine`, `_osrs_t` |
| Constantes | `UPPER_SNAKE` | `BME280_REG_CHIP_ID` |
| Defines | `UPPER_SNAKE` | `BMX280_ERR_NOT_FOUND` |
| Helpers de escopo local | `snake_case` | `i2c_delay()`, `sda_low()` |

### Tratamento de Erros

- Use o enum `BMx280Error` para códigos de retorno (não use `Serial.println`).
- Defina `_last_error` em todos os caminhos de falha.
- Use o stream `_debug` (se fornecido) para saída de diagnóstico.

## Testes

Os testes são baseados em hardware e executados em um Raspberry Pi Pico com BMP280/BME280 conectado.

1. Grave o firmware de teste:
   ```bash
   cd tests/comprehensive
   pio run --target upload
   ```

2. Monitore a saída serial:
   ```bash
   pio device monitor
   ```

Todos os testes devem passar antes de um PR ser mesclado.

## Processo de Pull Request

1. **Faça um fork** do repositório e crie uma branch de funcionalidade.
2. **Formate** seu código com `clang-format`.
3. **Teste** em hardware real se sua alteração afeta comunicação I2C.
4. **Atualize** o CHANGELOG.md com suas alterações.
5. **Envie** um PR com uma descrição clara da alteração.

### Checklist do PR

- [ ] Código formatado com `clang-format`
- [ ] Sem `Serial.print` hardcoded (use o stream `_debug`)
- [ ] Códigos de erro definidos em todos os caminhos de falha
- [ ] Testado em hardware RP2040 (se aplicável)
- [ ] CHANGELOG.md atualizado

## Relatando Bugs

Use o template de [Bug Report](https://github.com/angeloINTJ/BMx280PIO_RP2040/issues/new?template=bug_report.md). Inclua:

- Versão da biblioteca
- Configuração de hardware (modelo do sensor, fiação)
- Passos para reproduzir
- Saída serial / mensagens de erro

## Licença

Ao contribuir, você concorda que suas contribuições serão licenciadas sob a Licença MIT.

[🇺🇸 Read in English](SECURITY.md)

# Política de Segurança

## Relatando uma Vulnerabilidade

Se você descobrir uma vulnerabilidade de segurança nesta biblioteca, por favor
relate-a **privadamente** enviando um e-mail ao mantenedor. **Não** abra uma issue pública.

O mantenedor responderá em até 48 horas com uma confirmação e um cronograma
estimado para a correção.

## Escopo

Esta biblioteca comunica-se com sensores Bosch BME280/BMP280 via I2C. As
considerações de segurança incluem:

- **Acesso ao barramento I2C**: Acesso físico ao barramento I2C permite a leitura
  dos dados do sensor. Esta é uma preocupação a nível de hardware e não algo que
  esta biblioteca possa mitigar.
- **Segurança de memória**: A biblioteca usa buffers de tamanho fixo e evita
  alocação dinâmica além da construção. Os buffers na stack são limitados.
- **Validação de entrada**: Chip ID e dados de calibração são validados antes do uso.

## Versões Suportadas

| Versão  | Suportada          |
|---------|--------------------|
| 1.3.x   | :white_check_mark: |
| < 1.3.0 | :x:                |

## Política de Divulgação

Após o lançamento de uma correção, os detalhes da vulnerabilidade serão divulgados
publicamente nas notas de lançamento. O crédito será dado ao relator (a menos que
prefira permanecer anônimo).

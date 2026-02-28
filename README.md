# DESPI-C02 Driver para ESP-IDF

Driver em "Bare Metal" para o framework ESP-IDF desenvolvido para operar a placa de conexão **DESPI-C02**.

**📌 IMPORTANTE:** Este driver foi exaustivamente testado e funciona com perfeição (Zero Estática/Chuvisco) especificamente no display **Good Display GDEY042T81** (Tela E-Paper de 4.2", resolução 400x300, Preto/Branco) com o controlador interno **SSD1683**.
(A sequência de inicialização foi baseada na estabilidade de sinal do projeto GxEPD2 e adaptada nativamente em C para as APIs velozes do ESP-IDF).

## Funcionalidades
* **Full Refresh** (Limpar a tela para branco ou preto sem ghosting)
* **Fast Refresh** (Atualização rápida de ~1.5s sem piscar a tela toda)
* **Partial Refresh** (Atualização em janela específica de memória)
* Rotinas puras via `spi_device_polling_transmit` com micro-delays (`esp_rom_delay_us`) para estabilizar o controlador sobre a alta velocidade do ESP.
* Implementação segura e otimizada de deep sleep `epd_sleep()`.

## Como usar no seu projeto ESP-IDF
1. Inclua este diretório na sua compilação ou instale pelo Component Registry
2. Importe `#include "epd_gdey042t81.h"`
3. Chame:
```c
epd_init();           // Sobe o barramento SPI e Acorda a tela
epd_clear_white();    // Limpa a tela
epd_display(buffer);  // Envia seu framebuffer (imagem/texto)
epd_sleep();          // Coloca a tela para dormir
```

## Como alterar Pinos
Os pinos padrão estão listados em `include/epd_gdey042t81.h`. Basta alterar os defines nesse header de acordo com a sua CPU (Ex: ESP32-S3):
```c
#define EPD_PIN_MOSI    11
#define EPD_PIN_SCLK    12
#define EPD_PIN_CS      10
#define EPD_PIN_DC      47
#define EPD_PIN_RST     13
#define EPD_PIN_BUSY    21
```

## Instalação via ESP Component Registry

```bash
idf.py add-dependency "antunesls/despi_c02_driver"
```

## Exemplos

Você pode testar o driver rapidamente rodando o exemplo incluído:

```bash
cd examples/basic_usage
idf.py set-target esp32s3 # substitua pelo seu MCU
idf.py build flash monitor
```


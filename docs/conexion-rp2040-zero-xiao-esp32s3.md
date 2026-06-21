# Conexión RP2040-Zero ↔ Seeed Studio XIAO ESP32-S3

Esta guía describe el cableado UART usado por PM WiFi Lab entre un cartucho
PM2040 basado en **Waveshare RP2040-Zero** y un **Seeed Studio XIAO ESP32-S3**.

## Conexión recomendada

| RP2040-Zero | Dirección | XIAO ESP32-S3 | Función |
|---|---:|---|---|
| GPIO27 | → | D7 / GPIO44 / RX | Datos del RP2040 al ESP32 |
| GPIO28 | ← | D6 / GPIO43 / TX | Respuestas del ESP32 al RP2040 |
| GND | — | GND | Referencia eléctrica común |

```text
Waveshare RP2040-Zero                    Seeed XIAO ESP32-S3

  GPIO27  PIO-UART TX  ---------------->  D7 / GPIO44 / RX
  GPIO28  PIO-UART RX  <----------------  D6 / GPIO43 / TX
  GND                   -----------------  GND

              115200 baud · 8 data bits · sin paridad · 1 stop bit
```

TX y RX siempre se conectan cruzados: **TX → RX** y **RX ← TX**.

## Por qué se usan GPIO27 y GPIO28

El hardware PM2040 utiliza GPIO0–24 para el bus de dirección, datos y control
del cartucho Pokémon Mini, y GPIO26 está conectado a la señal IRQ. GPIO29
controla además el LED WS2812 integrado del RP2040-Zero. GPIO27 y GPIO28 quedan
fuera de esas señales, pero no forman una
pareja de UART hardware del RP2040; el puente debe implementar el UART mediante
una máquina de estados **PIO**, preferentemente en `pio1`.

No cambies estos pines por GPIO del bus del cartucho: podría impedir que la
Pokémon Mini leyera la ROM correctamente.

## Alimentación

La opción recomendada durante desarrollo es alimentar cada placa desde su
propio conector USB-C y conectar solamente las tres líneas de la tabla.

- Ambas placas usan lógica de **3,3 V**.
- La conexión **GND ↔ GND es obligatoria**.
- No conectes los pines de 3V3 entre placas si ambas reciben alimentación USB.
- No conectes una señal UART a 5 V.
- Apaga o desconecta ambas placas antes de soldar.

Si se desea una única fuente, debe diseñarse considerando consumo, regulación
y retorno de corriente. No unas directamente dos salidas reguladas de 3,3 V.

### Uso portátil recomendado

No alimentes el XIAO desde el pin 3V3 del cartucho PM2040. La Pokémon Mini ya
alimenta el RP2040 desde su única batería AAA, mientras que el XIAO consume
aproximadamente 100 mA con Wi-Fi activo y puede producir picos mayores.

Usa una batería **LiPo protegida de 3,7 V y 300–500 mAh**, capaz de entregar al
menos 500 mA, conectada exclusivamente a los pads `BAT+` y `BAT-` del XIAO. El
XIAO incorpora administración y carga de batería mediante su USB-C.

```text
LiPo protegida 3,7 V

  positivo  --- interruptor --- BAT+  XIAO ESP32-S3
  negativo  ------------------- BAT-  XIAO ESP32-S3
                                      |
RP2040-Zero GND ---------------------- GND
```

Conviene añadir resistencias en serie de **1 kΩ** en las dos líneas UART. Estas
limitan corriente accidental si una placa está encendida mientras la otra está
apagada. Apaga el XIAO con su interruptor antes de apagar o retirar el cartucho.

## Ubicación de los pines

En el RP2040-Zero, GPIO27 y GPIO28 están en los pads adicionales de la cara
inferior; normalmente requieren soldadura directa. Confirma las etiquetas con
el pinout de tu revisión antes de soldar.

En el XIAO ESP32-S3:

- `D6` está etiquetado como `TX` y corresponde a `GPIO43`.
- `D7` está etiquetado como `RX` y corresponde a `GPIO44`.

## Configuración del firmware

El XIAO ya usa esta configuración en `firmware/esp32_xiao/esp32_xiao.ino`:

```cpp
Serial1.begin(115200, SERIAL_8N1, D7, D6); // RX=D7, TX=D6
```

El firmware RP2040 debe configurar un UART PIO equivalente:

```c
#define ESP_UART_TX_PIN 27
#define ESP_UART_RX_PIN 28
#define ESP_UART_BAUD   115200
```

> **Importante:** el UF2 generado actualmente incorpora la ROM dentro del
> firmware oficial PM2040, pero el firmware oficial no implementa por sí mismo
> el mailbox ni el puente PIO-UART. Esa extensión debe incorporarse al firmware
> RP2040 antes de esperar respuestas del ESP32.

## Comprobación inicial

1. Revisa continuidad y ausencia de cortocircuitos con ambas placas apagadas.
2. Conecta GND común, GPIO27 → D7 y GPIO28 ← D6.
3. Flashea el firmware del XIAO.
  4. Flashea un UF2 del RP2040 que incluya el puente mailbox/PIO-UART.
5. Envía `PING\n` desde el RP2040.
6. Verifica que el XIAO responda `PONG\n`.
7. Solo después prueba `SCAN\n`.

## Ver los logs del XIAO

Conecta el USB-C del XIAO al computador y abre su puerto serie a **115200 baud**
con Arduino IDE, PlatformIO o cualquier monitor serial. Los logs de diagnóstico
salen por USB (`Serial`); el protocolo del RP2040 continúa en D6/D7 (`Serial1`).

Al arrancar debe aparecer:

```text
[250] [BOOT] PM WiFi Lab ESP32-S3 starting
[250] [UART] Serial1 115200 8N1 RX=D7/GPIO44 TX=D6/GPIO43
[250] [READY] Waiting for RP2040 commands
[5000] [ALIVE] uptime=5000ms rx_bytes=0 commands=0
```

Al recibir un scan correcto debe verse algo similar a:

```text
[RX BYTE] 0x53 'S'
[RX BYTE] 0x43 'C'
[RX BYTE] 0x41 'A'
[RX BYTE] 0x4E 'N'
[RX BYTE] 0x0A
[RX CMD] SCAN
[TX] OK
[SCAN] Starting WiFi scan
[SCAN] Found 4 AP(s) in 2860 ms
[TX AP] 0 SSID=MiWifi BSSID=AA:BB:CC:DD:EE:FF CH=6 RSSI=-53
[TX] SCAN_DONE 4
```

Interpretación rápida:

- Hay `ALIVE`, pero `rx_bytes=0`: el RP2040 no transmite, el puente PIO-UART no
  está implementado/cargado o TX/RX/GND están mal conectados.
- Hay `RX BYTE`, pero no `RX CMD`: falta `\n`, el baudrate no coincide o llegan
  bytes corruptos.
- Aparece `RX CMD` y `TX`, pero la ROM termina en timeout: el problema está en
  el retorno ESP32 → RP2040 o en el mailbox RP2040 → Pokémon Mini.
- No aparece `ALIVE`: revisa alimentación, puerto USB y baudrate del monitor.

## Referencias de pinout

- [Seeed Studio XIAO ESP32-S3: pinout oficial](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/)
- [Waveshare RP2040-Zero: documentación oficial](https://www.waveshare.com/wiki/RP2040-Zero)
- [Esquemático oficial del RP2040-Zero](https://files.waveshare.com/upload/4/4c/RP2040_Zero.pdf)

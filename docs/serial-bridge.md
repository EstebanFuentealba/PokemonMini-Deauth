# Contrato RP2040 ↔ ROM ↔ ESP32-S3

El ROM no controla un UART directamente. El RP2040 expone al bus del cartucho
un mailbox de 256 bytes en `0x1FFF00` y es el único dueño del UART conectado al
ESP32-S3. El firmware específico del cartucho debe implementar este contrato.

| Offset | Campo | Descripción |
|---:|---|---|
| 0..1 | magic | `PM` |
| 2 | op | `1`, texto UART |
| 3 | status | estado del handshake |
| 4 | length | bytes válidos en payload |
| 5 | sequence | aumenta con cada comando |
| 6..255 | payload | fragmento de hasta 250 bytes |

Estados: `0 IDLE`, `1 REQUEST`, `2 DATA`, `3 ACK`, `4 DONE`, `5 ERROR`.

1. El ROM escribe el comando terminado en `\n`, longitud, secuencia y `REQUEST`.
2. El RP2040 lo copia al UART y publica respuestas en fragmentos con `DATA`.
3. El ROM procesa incrementalmente el fragmento y responde `ACK`.
4. El RP2040 no reutiliza el payload hasta observar `ACK`; entonces publica el
   siguiente fragmento o `DONE`.
5. Una falla de UART o framing se publica como `ERROR`.

Cada fragmento puede cortar una línea en cualquier byte. El parser del ROM
reensambla líneas, limita cada línea a 95 caracteres y rechaza overflow. El
RP2040 debe preservar `\n`, no interpretar comandos WiFi y aplicar un timeout a
su UART. Esta separación deja futuras órdenes de laboratorio como extensiones
del protocolo textual sin introducir lógica WiFi en el cartucho.

El sketch seguro de referencia para el otro extremo está en
`firmware/esp32_xiao/esp32_xiao.ino`. Solo implementa scan y estados simulados;
no contiene transmisión de deauth, modo promiscuo ni inyección de paquetes.

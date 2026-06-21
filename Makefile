TARGET = helloworld

C_SOURCES := src/isr.c src/main.c src/print.c src/font_6x8.c \
	src/serial_protocol.c src/wifi_ap_model.c src/menu_ui.c src/app_state.c
ASM_SOURCES := src/startup.asm

# Usar -include para no fallar si pm.mk no existe localmente
# (Dentro de Docker, pm.mk existe en /opt/c88-pokemini/)
-include /opt/c88-pokemini/pm.mk

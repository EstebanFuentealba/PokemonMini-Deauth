cmake_minimum_required(VERSION 3.12)
set(CMAKE_C_STANDARD 11)
set(PICO_PLATFORM rp2040)
set(BOARD pico_sdk)
include(pico_sdk_import.cmake)
project(PM2040 C CXX ASM)
pico_sdk_init()

add_compile_options(-O3 -Wall -Wextra)
add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
add_executable(PM2040)
pico_set_linker_script(PM2040 ${CMAKE_CURRENT_SOURCE_DIR}/memmap.ld)
pico_generate_pio_header(PM2040
    ${CMAKE_CURRENT_LIST_DIR}/oe.pio
    ${CMAKE_CURRENT_LIST_DIR}/pushData.pio
    ${CMAKE_CURRENT_LIST_DIR}/hale.pio
    ${CMAKE_CURRENT_LIST_DIR}/lale_64k.pio
    ${CMAKE_CURRENT_LIST_DIR}/cart_write.pio
    ${CMAKE_CURRENT_LIST_DIR}/uart_tx.pio
    ${CMAKE_CURRENT_LIST_DIR}/uart_rx.pio)
pico_enable_stdio_usb(PM2040 0)
pico_enable_stdio_uart(PM2040 0)
pico_add_extra_outputs(PM2040)
target_sources(PM2040 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/main.c)
target_link_libraries(PM2040 pico_stdlib hardware_pio hardware_dma)

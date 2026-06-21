#include <stdint.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"

#include "rom.h"
#include "oe.pio.h"
#include "pushData.pio.h"
#include "hale.pio.h"
#include "lale_64k.pio.h"
#include "cart_write.pio.h"
#include "uart_tx.pio.h"
#include "uart_rx.pio.h"

#define ADDR_PIN 0
#define DATA_PIN 17
#define HALE_PIN 11
#define LALE_PIN 12
#define WE_PIN 13
#define OE_PIN 14

#define ESP_TX_PIN 27
#define ESP_RX_PIN 28
#define ESP_BAUD 115200

#define ROM_WINDOW_SIZE 65536u
#define MB_ROM_OFFSET 0xff00u
#define MB_BUS_LOW_ADDR 0x300u
#define MB_SIZE 256u
#define MB_MAGIC0 0
#define MB_MAGIC1 1
#define MB_OP 2
#define MB_STATUS 3
#define MB_LENGTH 4
#define MB_SEQUENCE 5
#define MB_PAYLOAD 6
#define MB_PAYLOAD_SIZE 250u

#define MB_OP_TEXT 1
#define MB_IDLE 0
#define MB_REQUEST 1
#define MB_DATA 2
#define MB_ACK 3
#define MB_DONE 4
#define MB_ERROR 5

#define RX_RING_SIZE 4096u
#define COMMAND_SIZE 96u
#define RESPONSE_LINE_SIZE 128u
#define LINK_TIMEOUT_US 30000000ull

/* 64 KiB alignment lets the PIO form a DMA pointer from upper 16 base bits
 * plus the lower 16 Pokemon Mini address bits. */
static uint8_t rom_runtime[ROM_WINDOW_SIZE] __attribute__((aligned(ROM_WINDOW_SIZE)));
static uint8_t rx_ring[RX_RING_SIZE];
static uint16_t rx_head;
static uint16_t rx_tail;
static char response_line[RESPONSE_LINE_SIZE];
static uint8_t response_line_len;
static uint8_t response_line_overflow;
static uint8_t active;
static uint8_t terminal_seen;
static uint8_t command_kind;
static uint64_t command_started_at;

enum {
    CMD_OTHER,
    CMD_SCAN,
    CMD_PING,
    CMD_SELECT,
    CMD_SIM,
    CMD_STOP
};

static inline volatile uint8_t *mailbox(void) {
    return (volatile uint8_t *)&rom_runtime[MB_ROM_OFFSET];
}

static uint16_t ring_count(void) {
    return (uint16_t)((rx_head - rx_tail) & (RX_RING_SIZE - 1u));
}

static int ring_put(uint8_t value) {
    uint16_t next = (uint16_t)((rx_head + 1u) & (RX_RING_SIZE - 1u));
    if (next == rx_tail) return 0;
    rx_ring[rx_head] = value;
    rx_head = next;
    return 1;
}

static uint8_t ring_get(void) {
    uint8_t value = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) & (RX_RING_SIZE - 1u));
    return value;
}

static int starts_with(const char *text, const char *prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static void classify_command(const char *command) {
    command_kind = CMD_OTHER;
    if (!strcmp(command, "SCAN")) command_kind = CMD_SCAN;
    else if (!strcmp(command, "PING")) command_kind = CMD_PING;
    else if (starts_with(command, "SELECT ")) command_kind = CMD_SELECT;
    else if (starts_with(command, "SIM_DEAUTH ")) command_kind = CMD_SIM;
    else if (!strcmp(command, "STOP")) command_kind = CMD_STOP;
}

static void inspect_response_line(void) {
    response_line[response_line_len] = '\0';
    if (starts_with(response_line, "ERR ")) terminal_seen = 1;
    else if (command_kind == CMD_SCAN && starts_with(response_line, "SCAN_DONE ")) terminal_seen = 1;
    else if (command_kind == CMD_PING && !strcmp(response_line, "PONG")) terminal_seen = 1;
    else if (command_kind == CMD_SELECT && !strcmp(response_line, "OK")) terminal_seen = 1;
    else if (command_kind == CMD_SIM &&
             (!strcmp(response_line, "STATUS RUNNING") ||
              !strcmp(response_line, "STATUS ERROR"))) terminal_seen = 1;
    else if (command_kind == CMD_STOP && !strcmp(response_line, "STATUS STOPPED")) terminal_seen = 1;
    else if (command_kind == CMD_OTHER) terminal_seen = 1;
    response_line_len = 0;
    response_line_overflow = 0;
}

static void consume_uart_byte(uint8_t value) {
    if (!ring_put(value)) {
        mailbox()[MB_STATUS] = MB_ERROR;
        active = 0;
        return;
    }
    if (value == '\r') return;
    if (value == '\n') {
        if (response_line_len && !response_line_overflow) inspect_response_line();
        else {
            response_line_len = 0;
            response_line_overflow = 0;
        }
    } else if (response_line_len + 1u < RESPONSE_LINE_SIZE) {
        response_line[response_line_len++] = (char)value;
    } else {
        response_line_overflow = 1;
    }
}

static void drain_uart(PIO pio, uint sm) {
    while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        /* uart_rx left-justifies the received byte. */
        uint8_t value = (uint8_t)(pio_sm_get(pio, sm) >> 24);
        if (active) consume_uart_byte(value);
    }
}

static void pio_uart_putc(PIO pio, uint sm, uint8_t value) {
    pio_sm_put_blocking(pio, sm, value);
}

static void start_request(PIO pio, uint sm) {
    volatile uint8_t *mb = mailbox();
    char command[COMMAND_SIZE];
    uint8_t length = mb[MB_LENGTH];
    uint8_t i;

    if (mb[MB_MAGIC0] != 'P' || mb[MB_MAGIC1] != 'M' ||
        mb[MB_OP] != MB_OP_TEXT || !length || length > MB_PAYLOAD_SIZE) {
        mb[MB_STATUS] = MB_ERROR;
        return;
    }
    if (length >= COMMAND_SIZE) length = COMMAND_SIZE - 1u;
    for (i = 0; i < length; ++i) {
        uint8_t c = mb[MB_PAYLOAD + i];
        command[i] = (c == '\r' || c == '\n') ? '\0' : (char)c;
        pio_uart_putc(pio, sm, c);
    }
    command[length] = '\0';
    if (length && (command[length - 1u] == '\r' || command[length - 1u] == '\n'))
        command[length - 1u] = '\0';
    if (mb[MB_PAYLOAD + length - 1u] != '\n') pio_uart_putc(pio, sm, '\n');

    classify_command(command);
    rx_head = rx_tail = 0;
    response_line_len = 0;
    response_line_overflow = 0;
    terminal_seen = 0;
    active = 1;
    command_started_at = time_us_64();
}

static void publish_next_chunk(void) {
    volatile uint8_t *mb = mailbox();
    uint16_t count = ring_count();
    uint16_t i;
    if (count > MB_PAYLOAD_SIZE) count = MB_PAYLOAD_SIZE;
    for (i = 0; i < count; ++i) mb[MB_PAYLOAD + i] = ring_get();
    mb[MB_LENGTH] = (uint8_t)count;
    __dmb();
    mb[MB_STATUS] = MB_DATA;
}

static void handle_mailbox(void) {
    volatile uint8_t *mb = mailbox();
    uint8_t status = mb[MB_STATUS];
    if (!active) {
        if (status == MB_REQUEST) start_request(pio1, 1);
        return;
    }
    if (status == MB_REQUEST || status == MB_ACK) {
        if (ring_count()) publish_next_chunk();
        else if (terminal_seen) {
            mb[MB_STATUS] = MB_DONE;
            active = 0;
        }
    }
    if (active && time_us_64() - command_started_at > LINK_TIMEOUT_US) {
        mb[MB_STATUS] = MB_ERROR;
        active = 0;
    }
}

static void apply_cart_writes(PIO pio, uint sm) {
    volatile uint8_t *mb = mailbox();
    while (!pio_sm_is_rx_fifo_empty(pio, sm)) {
        uint32_t pins = pio_sm_get(pio, sm);
        uint16_t addr = (uint16_t)(pins & 0x3ffu);
        if (addr >= MB_BUS_LOW_ADDR && addr < MB_BUS_LOW_ADDR + MB_SIZE) {
            mb[addr - MB_BUS_LOW_ADDR] = (uint8_t)((pins >> DATA_PIN) & 0xffu);
        }
    }
}

static void init_cart_bus(void) {
    PIO pio = pio0;
    uint sm_oe = pio_claim_unused_sm(pio, true);
    uint sm_data = pio_claim_unused_sm(pio, true);
    uint sm_hale = pio_claim_unused_sm(pio, true);
    uint sm_lale = pio_claim_unused_sm(pio, true);
    uint off_oe = pio_add_program(pio, &oe_toggle_program);
    uint off_data = pio_add_program(pio, &push_databits_program);
    uint off_hale = pio_add_program(pio, &hale_latch_program);
    uint off_lale = pio_add_program(pio, &lale_latch_64k_program);
    int hale_dma = dma_claim_unused_channel(true);
    int addr_dma = dma_claim_unused_channel(true);
    int data_dma = dma_claim_unused_channel(true);
    dma_channel_config c;

    c = dma_channel_get_default_config(hale_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm_hale, false));
    dma_channel_configure(hale_dma, &c, &pio->txf[sm_lale],
                          &pio->rxf[sm_hale], 1, false);

    c = dma_channel_get_default_config(addr_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm_lale, false));
    channel_config_set_chain_to(&c, hale_dma);
    dma_channel_configure(addr_dma, &c, &dma_hw->ch[data_dma].al3_read_addr_trig,
                          &pio->rxf[sm_lale], 1, false);

    c = dma_channel_get_default_config(data_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    channel_config_set_chain_to(&c, addr_dma);
    channel_config_set_high_priority(&c, true);
    dma_channel_configure(data_dma, &c, &pio->txf[sm_data], rom_runtime, 1, false);

    oe_toggle_program_init(pio, sm_oe, off_oe, DATA_PIN, OE_PIN);
    push_databits_program_init(pio, sm_data, off_data, DATA_PIN);
    hale_latch_program_init(pio, sm_hale, off_hale, ADDR_PIN, HALE_PIN);
    lale_latch_64k_program_init(pio, sm_lale, off_lale, ADDR_PIN, LALE_PIN);
    pio_sm_put(pio, sm_lale, (uint32_t)rom_runtime >> 16);
    dma_start_channel_mask((1u << hale_dma) | (1u << addr_dma));
}

static void init_bridge(void) {
    PIO pio = pio1;
    uint off_write = pio_add_program(pio, &cart_write_program);
    uint off_tx = pio_add_program(pio, &uart_tx_program);
    uint off_rx = pio_add_program(pio, &uart_rx_program);
    /* Fixed state-machine numbers are used by the small polling loop. */
    pio_sm_claim(pio, 0);
    pio_sm_claim(pio, 1);
    pio_sm_claim(pio, 2);
    cart_write_program_init(pio, 0, off_write, WE_PIN);
    uart_tx_program_init(pio, 1, off_tx, ESP_TX_PIN, ESP_BAUD);
    uart_rx_program_init(pio, 2, off_rx, ESP_RX_PIN, ESP_BAUD);
}

int main(void) {
    sleep_ms(2);
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(2);
    set_sys_clock_khz(240000, true);

    memcpy(rom_runtime, rom, ROM_WINDOW_SIZE);
    mailbox()[MB_STATUS] = MB_IDLE;
    init_cart_bus();
    init_bridge();

    while (1) {
        apply_cart_writes(pio1, 0);
        drain_uart(pio1, 2);
        handle_mailbox();
        tight_loop_contents();
    }
}

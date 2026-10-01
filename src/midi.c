#include "board.h"
#include "hardware/irq.h"
#include "midi.h"

// Running status is dropped after this much silence, in case a receiver
// was connected mid-stream
#define RUNNING_STATUS_US 1000000

// Filled by core 0, drained by the UART TX interrupt (also on core 0).
// 256 entries so the uint8_t indexes wrap by themselves.
static uint8_t ring[256];
static volatile uint8_t head, tail;

static uint8_t last_status;
static uint32_t last_send_us;

// Move queued bytes into the TX FIFO; keep the interrupt on while any remain
static void fill_fifo(void) {
    uart_hw_t *hw = uart_get_hw(MIDI_UART);
    while (tail != head && !(hw->fr & UART_UARTFR_TXFF_BITS)) {
        hw->dr = ring[tail++];
    }
    uart_set_irqs_enabled(MIDI_UART, false, tail != head);
}

static void on_uart_irq(void) {
    fill_fifo();
}

void midi_init(void) {
    uart_init(MIDI_UART, MIDI_BAUD);
    gpio_set_function(PIN_MIDI_TX, UART_FUNCSEL_NUM(MIDI_UART, PIN_MIDI_TX));
    irq_set_exclusive_handler(UART_IRQ_NUM(MIDI_UART), on_uart_irq);
    irq_set_enabled(UART_IRQ_NUM(MIDI_UART), true);
}

void midi_flush(void) {
    while (tail != head) {
        tight_loop_contents();
    }
    uart_tx_wait_blocking(MIDI_UART);
}

static void put(uint8_t byte) {
    // Only waits when over 80 ms of MIDI is already queued
    while ((uint8_t)(head + 1) == tail) {
        tight_loop_contents();
    }
    ring[head] = byte;
    // The byte must be in place before the interrupt can see it
    __compiler_memory_barrier();
    head++;
}

static void send(uint8_t status, uint8_t d1, uint8_t d2, int len) {
    uint32_t now = time_us_32();
    if (status != last_status || now - last_send_us > RUNNING_STATUS_US) {
        put(status);
        last_status = status;
    }
    last_send_us = now;
    put(d1 & 0x7F);
    if (len == 3) {
        put(d2 & 0x7F);
    }
    irq_set_enabled(UART_IRQ_NUM(MIDI_UART), false);
    fill_fifo();
    irq_set_enabled(UART_IRQ_NUM(MIDI_UART), true);
}

void midi_note_on(uint8_t ch, uint8_t note, uint8_t velocity) {
    send(0x90 | (ch & 0x0F), note, velocity, 3);
}

// Note-on with velocity 0, so note-offs share the running status
void midi_note_off(uint8_t ch, uint8_t note) {
    send(0x90 | (ch & 0x0F), note, 0, 3);
}

void midi_cc(uint8_t ch, uint8_t cc, uint8_t value) {
    send(0xB0 | (ch & 0x0F), cc, value, 3);
}

void midi_program(uint8_t ch, uint8_t program) {
    send(0xC0 | (ch & 0x0F), program, 0, 2);
}

#pragma once

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"

#include "input.h"

// MIDI out: UART0 TX at 31250 baud
#define MIDI_UART       uart0
#define MIDI_BAUD       31250
#define PIN_MIDI_TX     16

// Octave buttons, same electrical setup as the keys
#define PIN_OCT_UP      17
#define PIN_OCT_DOWN    18

// SSD1305 OLED on I2C1, 2.2k pull-ups on the board
#define OLED_I2C        i2c1
#define OLED_I2C_HZ     400000
#define OLED_ADDR       0x3C
#define PIN_OLED_SDA    26
#define PIN_OLED_SCL    27

// Expression pedal: last free GPIO, ADC2
#define PIN_EXPRESSION  28
#define ADC_EXPRESSION  2

// Settings text in the second to last flash sector, see docs/configuration.md
#define CONFIG_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - 2 * 4096)

// GPIO of each input bit (see input.h): keys from C upwards, then octave
static const uint INPUT_PINS[INPUT_COUNT] = {
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, // C .. B
    12, 13, 14, 15, 19, 20, 21, 22,                 // C' .. G'
    PIN_OCT_UP, PIN_OCT_DOWN,
};

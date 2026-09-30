#include "board.h"
#include "display.h"
#include "hardware/dma.h"

// Command prefix + data control byte + largest frame
#define PREFIX_WORDS 9
static uint16_t words[PREFIX_WORDS + 1 + GFX_MAX_WIDTH * GFX_MAX_HEIGHT / 8];

// A frame takes ~12 ms at 400 kHz; anything much longer means a stuck bus
#define DMA_TIMEOUT_MS 50

static int dma = -1;
static uint8_t col_offset;
static bool ok;

static bool wait_dma(void) {
    absolute_time_t timeout = make_timeout_time_ms(DMA_TIMEOUT_MS);
    while (dma_channel_is_busy(dma)) {
        if (time_reached(timeout)) {
            dma_channel_abort(dma);
            return false;
        }
    }
    return true;
}

// Same sequence as legacy/ssd1305.py; 0x00 marks a command stream
static bool send_init(uint8_t height) {
    const uint8_t seq[] = {
        0x00,
        0xAE,               // display off
        0xD5, 0x80,         // clock divide
        0xA1,               // segment remap
        0xA8, height - 1,   // multiplex ratio
        0xD3, 0x00,         // display offset
        0xAD, 0x8E,         // master config
        0xD8, 0x05,         // area colour, low power
        0x20, 0x00,         // horizontal addressing
        0x40,               // start line 0
        0x2E,               // scroll off
        0xC8,               // COM scan decrement
        0xDA, 0x12,         // COM pins
        0x81, 0xFF,         // contrast
        0xD9, 0xD2,         // precharge
        0xDB, 0x34,         // VCOMH
        0xA6,               // not inverted
        0xA4,               // show RAM
        0x8D, 0x14,         // charge pump
        0xAF,               // display on
    };
    return i2c_write_timeout_us(OLED_I2C, OLED_ADDR, seq, sizeof(seq), false,
                                20000) == (int)sizeof(seq);
}

bool display_init(uint8_t height, uint8_t offset) {
    if (dma < 0) {
        gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_I2C);
        gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_I2C);

        dma = dma_claim_unused_channel(true);
        dma_channel_config c = dma_channel_get_default_config(dma);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
        channel_config_set_dreq(&c, i2c_get_dreq(OLED_I2C, true));
        dma_channel_configure(dma, &c, &i2c_get_hw(OLED_I2C)->data_cmd, words,
                              0, false);
    }
    wait_dma();
    // Resets the controller too, in case a failed frame left the bus busy
    i2c_init(OLED_I2C, OLED_I2C_HZ);
    col_offset = offset;
    // The blocking write also leaves the target address set for the DMA
    ok = send_init(height);
    return ok;
}

void display_poll(void) {
    i2c_hw_t *hw = i2c_get_hw(OLED_I2C);
    if (ok && !dma_channel_is_busy(dma) &&
        (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS)) {
        (void)hw->clr_tx_abrt;
        ok = false;
    }
}

bool display_ok(void) {
    return ok;
}

void display_send(const gfx_t *g, uint8_t contrast) {
    i2c_hw_t *hw = i2c_get_hw(OLED_I2C);

    // A NACK aborts the transfer and flushes the FIFO
    if (!wait_dma() || (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_TX_ABRT_BITS)) {
        (void)hw->clr_tx_abrt;
        ok = false;
        return;
    }

    uint8_t pages = g->height / 8;
    uint16_t *w = words;
    *w++ = 0x00;                            // commands
    *w++ = 0x81; *w++ = contrast;
    *w++ = 0x21; *w++ = col_offset; *w++ = col_offset + g->width - 1;
    *w++ = 0x22; *w++ = 0; *w++ = (pages - 1) | I2C_IC_DATA_CMD_STOP_BITS;
    *w++ = 0x40;                            // data
    const uint8_t *src = g->buf;
    for (int n = g->width * pages; n; n--) {
        *w++ = *src++;
    }
    w[-1] |= I2C_IC_DATA_CMD_STOP_BITS;

    dma_channel_transfer_from_buffer_now(dma, words, w - words);
}

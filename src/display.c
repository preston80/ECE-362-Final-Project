#include "display.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

// ILI9341 Commands
#define ILI9341_SWRESET     0x01
#define ILI9341_SLPOUT      0x11
#define ILI9341_DISPON      0x29
#define ILI9341_CASET       0x2A
#define ILI9341_PASET       0x2B
#define ILI9341_RAMWR       0x2C
#define ILI9341_MADCTL      0x36
#define ILI9341_PIXFMT      0x3A

// MADCTL Bit Masks
#define MADCTL_MY           0x80 // Row Address Order
#define MADCTL_MX           0x40 // Column Address Order
#define MADCTL_MV           0x20 // Row / Column Exchange
#define MADCTL_BGR          0x08 // BGR Color Filter Order

static uint16_t current_width  = ILI9341_TFTWIDTH;
static uint16_t current_height = ILI9341_TFTHEIGHT;

static inline void cs_select(void) {
    gpio_put(DISPLAY_PIN_CS, 0);
}

static inline void cs_deselect(void) {
    gpio_put(DISPLAY_PIN_CS, 1);
}

static void write_command(uint8_t cmd) {
    gpio_put(DISPLAY_PIN_DC, 0); // Command mode
    cs_select();
    spi_write_blocking(DISPLAY_SPI_PORT, &cmd, 1);
    cs_deselect();
}

static void write_data(const uint8_t *data, size_t len) {
    gpio_put(DISPLAY_PIN_DC, 1); // Data mode
    cs_select();
    spi_write_blocking(DISPLAY_SPI_PORT, data, len);
    cs_deselect();
}

static void write_data_byte(uint8_t val) {
    write_data(&val, 1);
}

static void hardware_reset(void) {
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(10);
    gpio_put(DISPLAY_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(150);
}

void display_init(display_rotation_t rotation) {
    // Initialize SPI1 peripheral
    spi_init(DISPLAY_SPI_PORT, DISPLAY_BAUDRATE);
    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);

    // Initialize control pins (CS, DC, RST) as standard GPIO outputs
    gpio_init(DISPLAY_PIN_CS);
    gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);
    cs_deselect();

    gpio_init(DISPLAY_PIN_DC);
    gpio_set_dir(DISPLAY_PIN_DC, GPIO_OUT);

    gpio_init(DISPLAY_PIN_RST);
    gpio_set_dir(DISPLAY_PIN_RST, GPIO_OUT);

    // Reset and configure ILI9341
    hardware_reset();

    write_command(ILI9341_SWRESET);
    sleep_ms(150);

    write_command(ILI9341_SLPOUT);
    sleep_ms(150);

    // Set 16-bit/pixel (RGB565) color format
    write_command(ILI9341_PIXFMT);
    write_data_byte(0x55);

    // Set initial orientation
    display_set_rotation(rotation);

    // Turn display on
    write_command(ILI9341_DISPON);
    sleep_ms(50);
}

void display_set_rotation(display_rotation_t rotation) {
    uint8_t madctl = MADCTL_BGR;

    switch (rotation) {
        case DISPLAY_ROTATION_0:
            madctl |= MADCTL_MX;
            current_width  = ILI9341_TFTWIDTH;
            current_height = ILI9341_TFTHEIGHT;
            break;

        case DISPLAY_ROTATION_90:
            madctl |= MADCTL_MV;
            current_width  = ILI9341_TFTHEIGHT;
            current_height = ILI9341_TFTWIDTH;
            break;

        case DISPLAY_ROTATION_180:
            madctl |= MADCTL_MY;
            current_width  = ILI9341_TFTWIDTH;
            current_height = ILI9341_TFTHEIGHT;
            break;

        case DISPLAY_ROTATION_270:
            madctl |= (MADCTL_MX | MADCTL_MY | MADCTL_MV);
            current_width  = ILI9341_TFTHEIGHT;
            current_height = ILI9341_TFTWIDTH;
            break;
    }

    write_command(ILI9341_MADCTL);
    write_data_byte(madctl);
}

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];

    // Column Address Set
    write_command(ILI9341_CASET);
    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    write_data(data, 4);

    // Page (Row) Address Set
    write_command(ILI9341_PASET);
    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    write_data(data, 4);

    // Prepare to write to RAM
    write_command(ILI9341_RAMWR);
}

void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= current_width || y >= current_height || w == 0 || h == 0) {
        return;
    }
    if (x + w > current_width) {
        w = current_width - x;
    }
    if (y + h > current_height) {
        h = current_height - y;
    }

    display_set_window(x, y, x + w - 1, y + h - 1);

    // Send color in chunks (big-endian RGB565)
    uint8_t hi = (color >> 8) & 0xFF;
    uint8_t lo = color & 0xFF;

    uint8_t buf[128];
    for (size_t i = 0; i < sizeof(buf); i += 2) {
        buf[i]     = hi;
        buf[i + 1] = lo;
    }

    uint32_t total_pixels = (uint32_t)w * (uint32_t)h;
    gpio_put(DISPLAY_PIN_DC, 1);
    cs_select();

    while (total_pixels > 0) {
        uint32_t chunk_pixels = (total_pixels > 64) ? 64 : total_pixels;
        spi_write_blocking(DISPLAY_SPI_PORT, buf, chunk_pixels * 2);
        total_pixels -= chunk_pixels;
    }

    cs_deselect();
}

void display_fill_screen(uint16_t color) {
    display_fill_rect(0, 0, current_width, current_height, color);
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= current_width || y >= current_height) {
        return;
    }
    display_set_window(x, y, x, y);
    uint8_t data[2] = { (color >> 8) & 0xFF, color & 0xFF };
    write_data(data, 2);
}

uint16_t display_get_width(void) {
    return current_width;
}

uint16_t display_get_height(void) {
    return current_height;
}
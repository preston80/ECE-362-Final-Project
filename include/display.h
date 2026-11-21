#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

// SPI Peripheral & Pin Assignments (SPI1 on GP24-GP28)
#define DISPLAY_SPI_PORT    spi1
#define DISPLAY_BAUDRATE    (10 * 1000 * 1000) // Conservative ILI9341 SPI clock

#define DISPLAY_PIN_DC      24
#define DISPLAY_PIN_CS      25
#define DISPLAY_PIN_SCK     26
#define DISPLAY_PIN_MOSI    27
#define DISPLAY_PIN_RST     28

// Native Panel Dimensions (Portrait)
#define ILI9341_TFTWIDTH    240
#define ILI9341_TFTHEIGHT   320

// Screen Orientation Options
typedef enum {
    DISPLAY_ROTATION_0   = 0, // Portrait (normal)
    DISPLAY_ROTATION_90  = 1, // Landscape
    DISPLAY_ROTATION_180 = 2, // Portrait (flipped upside-down)
    DISPLAY_ROTATION_270 = 3  // Landscape (flipped upside-down)
} display_rotation_t;

// Common RGB565 Color Definitions
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F

// Public API
void display_init(display_rotation_t rotation);
void display_set_rotation(display_rotation_t rotation);
void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void display_fill_screen(uint16_t color);
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void display_show_tracking(bool person_present,
                           bool sound_detected,
                           float distance_cm,
                           float direction_deg);

uint16_t display_get_width(void);
uint16_t display_get_height(void);

#endif // DISPLAY_H
#include "display.h"
#include "config.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ILI9341_SWRESET     0x01
#define ILI9341_SLPOUT      0x11
#define ILI9341_DISPON      0x29
#define ILI9341_CASET       0x2A
#define ILI9341_PASET       0x2B
#define ILI9341_RAMWR       0x2C
#define ILI9341_MADCTL      0x36
#define ILI9341_PIXFMT      0x3A

#define MADCTL_MY           0x80
#define MADCTL_MX           0x40
#define MADCTL_MV           0x20
#define MADCTL_BGR          0x08

static uint16_t current_width = ILI9341_TFTWIDTH;
static uint16_t current_height = ILI9341_TFTHEIGHT;

static inline void cs_select(void)
{
    gpio_put(DISPLAY_PIN_CS, 0);
}

static inline void cs_deselect(void)
{
    gpio_put(DISPLAY_PIN_CS, 1);
}

static void start_command(uint8_t command)
{
    gpio_put(DISPLAY_PIN_DC, 0);
    cs_select();
    spi_write_blocking(DISPLAY_SPI_PORT, &command, 1);
}

static void write_register(uint8_t command, const uint8_t *data, size_t count)
{
    start_command(command);
    if (count > 0) {
        gpio_put(DISPLAY_PIN_DC, 1);
        spi_write_blocking(DISPLAY_SPI_PORT, data, count);
    }
    cs_deselect();
}

static void write_command(uint8_t command)
{
    write_register(command, NULL, 0);
}

static void hardware_reset(void)
{
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(10);
    gpio_put(DISPLAY_PIN_RST, 0);
    sleep_ms(20);
    gpio_put(DISPLAY_PIN_RST, 1);
    sleep_ms(150);
}

void display_init(display_rotation_t rotation)
{
    spi_init(DISPLAY_SPI_PORT, DISPLAY_BAUDRATE);
    spi_set_format(DISPLAY_SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(DISPLAY_PIN_CS);
    cs_deselect();
    gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);
    gpio_init(DISPLAY_PIN_DC);
    gpio_set_dir(DISPLAY_PIN_DC, GPIO_OUT);
    gpio_init(DISPLAY_PIN_RST);
    gpio_set_dir(DISPLAY_PIN_RST, GPIO_OUT);

    hardware_reset();
    write_command(ILI9341_SWRESET);
    sleep_ms(150);

    const uint8_t interface_control[] = {0x03, 0x80, 0x02};
    const uint8_t power_b[] = {0x00, 0xC1, 0x30};
    const uint8_t power_sequence[] = {0x64, 0x03, 0x12, 0x81};
    const uint8_t driver_timing[] = {0x85, 0x00, 0x78};
    const uint8_t power_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    const uint8_t timing_enable[] = {0x00, 0x00};
    const uint8_t vcom1[] = {0x3E, 0x28};
    const uint8_t frame_rate[] = {0x00, 0x18};
    const uint8_t display_function[] = {0x08, 0x82, 0x27};
    const uint8_t positive_gamma[] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
    };
    const uint8_t negative_gamma[] = {
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
    };

    write_register(0xEF, interface_control, sizeof(interface_control));
    write_register(0xCF, power_b, sizeof(power_b));
    write_register(0xED, power_sequence, sizeof(power_sequence));
    write_register(0xE8, driver_timing, sizeof(driver_timing));
    write_register(0xCB, power_a, sizeof(power_a));
    write_register(0xF7, (const uint8_t[]){0x20}, 1);
    write_register(0xEA, timing_enable, sizeof(timing_enable));
    write_register(0xC0, (const uint8_t[]){0x23}, 1);
    write_register(0xC1, (const uint8_t[]){0x10}, 1);
    write_register(0xC5, vcom1, sizeof(vcom1));
    write_register(0xC7, (const uint8_t[]){0x86}, 1);
    write_register(0xB1, frame_rate, sizeof(frame_rate));
    write_register(0xB6, display_function, sizeof(display_function));
    write_register(ILI9341_PIXFMT, (const uint8_t[]){0x55}, 1);
    write_register(0xF2, (const uint8_t[]){0x00}, 1);
    write_register(0x26, (const uint8_t[]){0x01}, 1);
    write_register(0xE0, positive_gamma, sizeof(positive_gamma));
    write_register(0xE1, negative_gamma, sizeof(negative_gamma));

    write_command(ILI9341_SLPOUT);
    sleep_ms(120);
    display_set_rotation(rotation);
    write_command(ILI9341_DISPON);
    sleep_ms(50);
    display_fill_screen(COLOR_BLACK);
}

void display_set_rotation(display_rotation_t rotation)
{
    uint8_t madctl = MADCTL_BGR;

    switch (rotation) {
        case DISPLAY_ROTATION_0:
            madctl |= MADCTL_MX;
            current_width = ILI9341_TFTWIDTH;
            current_height = ILI9341_TFTHEIGHT;
            break;
        case DISPLAY_ROTATION_90:
            madctl |= MADCTL_MV;
            current_width = ILI9341_TFTHEIGHT;
            current_height = ILI9341_TFTWIDTH;
            break;
        case DISPLAY_ROTATION_180:
            madctl |= MADCTL_MY;
            current_width = ILI9341_TFTWIDTH;
            current_height = ILI9341_TFTHEIGHT;
            break;
        case DISPLAY_ROTATION_270:
            madctl |= MADCTL_MX | MADCTL_MY | MADCTL_MV;
            current_width = ILI9341_TFTHEIGHT;
            current_height = ILI9341_TFTWIDTH;
            break;
        default:
            return;
    }

    write_register(ILI9341_MADCTL, (const uint8_t[]){madctl}, 1);
}

static void begin_pixel_write(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    const uint8_t columns[] = {x0 >> 8, x0, x1 >> 8, x1};
    const uint8_t rows[] = {y0 >> 8, y0, y1 >> 8, y1};
    write_register(ILI9341_CASET, columns, sizeof(columns));
    write_register(ILI9341_PASET, rows, sizeof(rows));
    start_command(ILI9341_RAMWR);
    gpio_put(DISPLAY_PIN_DC, 1);
}

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (x0 <= x1 && y0 <= y1 && x1 < current_width && y1 < current_height) {
        begin_pixel_write(x0, y0, x1, y1);
        cs_deselect();
    }
}

void display_fill_rect(uint16_t x,
                       uint16_t y,
                       uint16_t width,
                       uint16_t height,
                       uint16_t color)
{
    if (x >= current_width || y >= current_height || width == 0 || height == 0) {
        return;
    }
    if ((uint32_t)x + width > current_width) {
        width = current_width - x;
    }
    if ((uint32_t)y + height > current_height) {
        height = current_height - y;
    }

    uint8_t high_byte = (uint8_t)(color >> 8);
    uint8_t low_byte = (uint8_t)color;
    uint8_t pixels[128];
    for (size_t i = 0; i < sizeof(pixels); i += 2) {
        pixels[i] = high_byte;
        pixels[i + 1] = low_byte;
    }

    uint32_t remaining = (uint32_t)width * height;
    begin_pixel_write(x, y, x + width - 1, y + height - 1);
    while (remaining > 0) {
        uint32_t chunk_pixels = remaining > 64 ? 64 : remaining;
        spi_write_blocking(DISPLAY_SPI_PORT, pixels, chunk_pixels * 2);
        remaining -= chunk_pixels;
    }
    cs_deselect();
}

void display_fill_screen(uint16_t color)
{
    display_fill_rect(0, 0, current_width, current_height, color);
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= current_width || y >= current_height) {
        return;
    }

    uint8_t pixel[] = {(uint8_t)(color >> 8), (uint8_t)color};
    begin_pixel_write(x, y, x, y);
    spi_write_blocking(DISPLAY_SPI_PORT, pixel, sizeof(pixel));
    cs_deselect();
}

static const uint8_t *glyph_for(char character)
{
    static const struct {
        char character;
        uint8_t columns[5];
    } glyphs[] = {
        {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}}, {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
        {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}}, {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
        {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}}, {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
        {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}}, {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
        {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}}, {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
        {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}}, {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
        {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}}, {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
        {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}}, {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
        {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}}, {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
        {'S', {0x46, 0x49, 0x49, 0x49, 0x31}}, {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
        {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}}, {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
        {'W', {0x7F, 0x20, 0x18, 0x20, 0x7F}}, {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
        {'Y', {0x03, 0x04, 0x78, 0x04, 0x03}}, {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
        {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}}, {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
        {'2', {0x42, 0x61, 0x51, 0x49, 0x46}}, {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
        {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}}, {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
        {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}}, {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
        {'8', {0x36, 0x49, 0x49, 0x49, 0x36}}, {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
        {'+', {0x08, 0x08, 0x3E, 0x08, 0x08}}, {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
        {' ', {0x00, 0x00, 0x00, 0x00, 0x00}}
    };

    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
        if (glyphs[i].character == character) {
            return glyphs[i].columns;
        }
    }
    return glyphs[sizeof(glyphs) / sizeof(glyphs[0]) - 1].columns;
}

static void draw_text(int x, int y, const char *text, uint16_t color, uint8_t scale)
{
    for (size_t character = 0; text[character] != '\0'; ++character) {
        const uint8_t *columns = glyph_for(text[character]);
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if ((columns[column] & (1u << row)) != 0) {
                    display_fill_rect((uint16_t)(x + column * scale),
                                      (uint16_t)(y + row * scale),
                                      scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

static void draw_centered_text(int y,
                               const char *text,
                               uint16_t color,
                               uint8_t scale)
{
    int text_width = (int)strlen(text) * 6 * scale - scale;
    draw_text(((int)current_width - text_width) / 2, y, text, color, scale);
}

enum {
    RADAR_RADIUS = 92,
    RADAR_WIDTH = 2 * RADAR_RADIUS + 1,
    RADAR_HEIGHT = RADAR_RADIUS + 1,
    RADAR_TOP = 16,
    TRACKING_REFRESH_MS = 250,
};

#define RADAR_GRID_COLOR 0x238A
#define DEG_TO_RAD 0.01745329252f

// RGB565 in wire byte order, allowing the entire radar to use one SPI write.
static uint8_t radar_pixels[RADAR_WIDTH * RADAR_HEIGHT * 2];

static void radar_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= RADAR_WIDTH || y < 0 || y >= RADAR_HEIGHT) {
        return;
    }
    size_t offset = 2u * (y * RADAR_WIDTH + x);
    radar_pixels[offset] = (uint8_t)(color >> 8);
    radar_pixels[offset + 1] = (uint8_t)color;
}

static void radar_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
    radar_pixel(x0, y0, color);
    for (int i = 1; i <= steps; ++i) {
        radar_pixel(x0 + i * dx / steps, y0 + i * dy / steps, color);
    }
}

static void draw_radar(bool sound_detected, int angle_deg,
                       bool object_in_range, int distance_cm, bool aligned)
{
    memset(radar_pixels, 0, sizeof(radar_pixels));
    const int center = RADAR_RADIUS;
    for (int ring = 1; ring <= 3; ++ring) {
        int radius = ring * RADAR_RADIUS / 3;
        for (int angle = -90; angle <= 90; ++angle) {
            int x = center + (int)lroundf(radius * sinf(angle * DEG_TO_RAD));
            int y = (int)lroundf(radius * cosf(angle * DEG_TO_RAD));
            radar_pixel(x, y, RADAR_GRID_COLOR);
        }
    }
    radar_line(0, 0, RADAR_WIDTH - 1, 0, RADAR_GRID_COLOR);
    radar_line(center, 0, center, RADAR_RADIUS, RADAR_GRID_COLOR);

    if (sound_detected) {
        float angle = angle_deg * DEG_TO_RAD;
        float right = sinf(angle);
        float forward = cosf(angle);
        int length = RADAR_RADIUS - 5;
        int tip_x = center + (int)lroundf(length * right);
        int tip_y = (int)lroundf(length * forward);
        int back_x = tip_x - (int)lroundf(10 * right);
        int back_y = tip_y - (int)lroundf(10 * forward);
        int wing_x = (int)lroundf(5 * forward);
        int wing_y = (int)lroundf(5 * right);
        radar_line(center, 0, tip_x, tip_y, COLOR_CYAN);
        radar_line(tip_x, tip_y, back_x + wing_x, back_y - wing_y, COLOR_CYAN);
        radar_line(tip_x, tip_y, back_x - wing_x, back_y + wing_y, COLOR_CYAN);
    }

    if (object_in_range) {
        // A fixed rangefinder supplies distance along its forward beam.
        int y = (int)lroundf(distance_cm * (RADAR_RADIUS - 4) /
                             DISPLAY_OBJECT_MAX_CM);
        uint16_t color = aligned ? COLOR_GREEN : COLOR_YELLOW;
        for (int dy = -3; dy <= 3; ++dy) {
            int half_width = 3 - abs(dy);
            radar_line(center - half_width, y + dy,
                       center + half_width, y + dy, color);
        }
    }

    uint16_t left = (current_width - RADAR_WIDTH) / 2;
    begin_pixel_write(left, RADAR_TOP, left + RADAR_WIDTH - 1,
                      RADAR_TOP + RADAR_HEIGHT - 1);
    spi_write_blocking(DISPLAY_SPI_PORT, radar_pixels, sizeof(radar_pixels));
    cs_deselect();
}

void display_show_tracking(bool object_in_range,
                           bool sound_detected,
                           float distance_cm,
                           float direction_deg)
{
    static bool drawn;
    static uint64_t last_update_ms;
    static bool previous_object;
    static bool previous_sound;
    static int previous_angle;
    static int previous_distance;

    int angle = sound_detected ? (int)lroundf(direction_deg) : 0;
    int distance = distance_cm >= 0.0f ? (int)lroundf(distance_cm) : -1;
    uint64_t now_ms = to_ms_since_boot(get_absolute_time());
    if (drawn && (now_ms - last_update_ms < TRACKING_REFRESH_MS ||
                  (previous_object == object_in_range &&
                   previous_sound == sound_detected &&
                   previous_angle == angle && previous_distance == distance))) {
        return;
    }

    bool aligned = object_in_range && sound_detected &&
                   abs(angle) <= DISPLAY_ECHO_MATCH_DEG;
    if (!drawn) {
        draw_centered_text(122, "SOUND TRACKER", COLOR_CYAN, 2);
        display_fill_rect(20, 145, current_width - 40, 2, COLOR_BLUE);
    }

    if (!drawn || previous_sound != sound_detected || previous_angle != angle) {
        char angle_text[24] = "LISTENING";
        if (sound_detected) {
            snprintf(angle_text, sizeof(angle_text), "%+d DEG", angle);
        }
        display_fill_rect(0, 158, current_width, 22, COLOR_BLACK);
        draw_centered_text(158, angle_text, COLOR_CYAN, 3);
    }

    draw_radar(sound_detected, angle, object_in_range, distance, aligned);
    display_fill_rect(0, 190, current_width, 42, COLOR_BLACK);
    const char *echo_status = aligned ? "ECHO NEAR BEARING" :
                              object_in_range ? "OBJECT AHEAD" : "NO NEAR ECHO";
    draw_centered_text(191, echo_status, aligned ? COLOR_GREEN : COLOR_WHITE, 2);
    char range_text[24] = "RANGE -- CM";
    if (distance >= 0) {
        snprintf(range_text, sizeof(range_text), "RANGE %d CM", distance);
    }
    draw_centered_text(213, range_text, COLOR_WHITE, 2);

    drawn = true;
    last_update_ms = now_ms;
    previous_object = object_in_range;
    previous_sound = sound_detected;
    previous_angle = angle;
    previous_distance = distance;
}

uint16_t display_get_width(void)
{
    return current_width;
}

uint16_t display_get_height(void)
{
    return current_height;
}

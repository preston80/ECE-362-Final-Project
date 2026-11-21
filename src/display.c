#include "display.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include <stdio.h>
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

static void write_command(uint8_t command)
{
    gpio_put(DISPLAY_PIN_DC, 0);
    cs_select();
    spi_write_blocking(DISPLAY_SPI_PORT, &command, 1);
    cs_deselect();
}

static void write_data(const uint8_t *data, size_t count)
{
    gpio_put(DISPLAY_PIN_DC, 1);
    cs_select();
    spi_write_blocking(DISPLAY_SPI_PORT, data, count);
    cs_deselect();
}

static void write_data_byte(uint8_t value)
{
    write_data(&value, 1);
}

static void write_register(uint8_t command, const uint8_t *data, size_t count)
{
    write_command(command);
    if (count > 0) {
        write_data(data, count);
    }
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
    gpio_set_function(DISPLAY_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(DISPLAY_PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(DISPLAY_PIN_CS);
    gpio_set_dir(DISPLAY_PIN_CS, GPIO_OUT);
    cs_deselect();
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
    write_command(0xF7);
    write_data_byte(0x20);
    write_register(0xEA, timing_enable, sizeof(timing_enable));
    write_command(0xC0);
    write_data_byte(0x23);
    write_command(0xC1);
    write_data_byte(0x10);
    write_register(0xC5, vcom1, sizeof(vcom1));
    write_command(0xC7);
    write_data_byte(0x86);
    write_register(0xB1, frame_rate, sizeof(frame_rate));
    write_register(0xB6, display_function, sizeof(display_function));
    write_command(ILI9341_PIXFMT);
    write_data_byte(0x55);
    write_command(0xF2);
    write_data_byte(0x00);
    write_command(0x26);
    write_data_byte(0x01);
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

    write_command(ILI9341_MADCTL);
    write_data_byte(madctl);
}

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    if (x0 > x1 || y0 > y1 || x1 >= current_width || y1 >= current_height) {
        return;
    }

    uint8_t data[4];
    write_command(ILI9341_CASET);
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    write_data(data, sizeof(data));

    write_command(ILI9341_PASET);
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    write_data(data, sizeof(data));
    write_command(ILI9341_RAMWR);
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

    display_set_window(x, y, x + width - 1, y + height - 1);

    uint8_t high_byte = (uint8_t)(color >> 8);
    uint8_t low_byte = (uint8_t)color;
    uint8_t pixels[128];
    for (size_t i = 0; i < sizeof(pixels); i += 2) {
        pixels[i] = high_byte;
        pixels[i + 1] = low_byte;
    }

    uint32_t remaining = (uint32_t)width * height;
    gpio_put(DISPLAY_PIN_DC, 1);
    cs_select();
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

    display_set_window(x, y, x, y);
    uint8_t pixel[] = {(uint8_t)(color >> 8), (uint8_t)color};
    write_data(pixel, sizeof(pixel));
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
        {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'.', {0x00, 0x60, 0x60, 0x00, 0x00}},
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

static void draw_arrow(int direction, uint16_t color)
{
    const int center_y = 205;
    if (direction > 0) {
        display_fill_rect(48, center_y - 5, 132, 10, color);
        for (int i = 0; i < 40; ++i) {
            int half_height = i * 3 / 2;
            display_fill_rect((uint16_t)(180 + i),
                              (uint16_t)(center_y - half_height),
                              1, (uint16_t)(half_height * 2 + 10), color);
        }
    } else if (direction < 0) {
        display_fill_rect(60, center_y - 5, 132, 10, color);
        for (int i = 0; i < 40; ++i) {
            int half_height = i * 3 / 2;
            display_fill_rect((uint16_t)(20 + i),
                              (uint16_t)(center_y - half_height),
                              1, (uint16_t)(half_height * 2 + 10), color);
        }
    } else {
        display_fill_rect(115, 165, 10, 80, color);
        for (int i = 0; i < 40; ++i) {
            int half_width = i * 3 / 2;
            display_fill_rect((uint16_t)(120 - half_width),
                              (uint16_t)(165 + i),
                              (uint16_t)(half_width * 2 + 10), 1, color);
        }
    }
}

void display_show_tracking(bool person_present,
                           bool sound_detected,
                           float distance_cm,
                           float direction_deg)
{
    static bool first_update = true;
    static bool previous_person_present;
    static bool previous_sound_detected;
    static int previous_direction = 2;
    static int previous_distance_bucket = -2;

    int distance_display = distance_cm < 0.0f
        ? -1
        : (int)(distance_cm + 0.5f);
    int direction = !sound_detected ? 2 : (direction_deg > 5.0f ? 1 :
                                           (direction_deg < -5.0f ? -1 : 0));

    if (!first_update &&
        person_present == previous_person_present &&
        sound_detected == previous_sound_detected &&
        direction == previous_direction &&
        distance_display == previous_distance_bucket) {
        return;
    }
    first_update = false;
    previous_person_present = person_present;
    previous_sound_detected = sound_detected;
    previous_direction = direction;
    previous_distance_bucket = distance_display;

    display_fill_screen(COLOR_BLACK);
    draw_centered_text(18, "SOUND TRACKER", COLOR_CYAN, 2);
    display_fill_rect(20, 48, current_width - 40, 2, COLOR_BLUE);

    if (!person_present) {
        draw_centered_text(68, "NO PERSON", COLOR_RED, 2);
        draw_centered_text(105, "MOVE INTO", COLOR_WHITE, 2);
        draw_centered_text(128, "RANGE", COLOR_WHITE, 2);
    } else if (sound_detected) {
        draw_centered_text(68, "PERSON HERE", COLOR_GREEN, 2);
        if (direction > 0) {
            draw_centered_text(125, "RIGHT", COLOR_YELLOW, 3);
        } else if (direction < 0) {
            draw_centered_text(125, "LEFT", COLOR_YELLOW, 3);
        } else {
            draw_centered_text(125, "CENTER", COLOR_YELLOW, 3);
        }
        draw_arrow(direction, COLOR_CYAN);
    } else {
        draw_centered_text(68, "PERSON HERE", COLOR_GREEN, 2);
        draw_centered_text(125, "LISTENING", COLOR_YELLOW, 2);
    }

    char distance_text[24];
    if (distance_display >= 0) {
        snprintf(distance_text, sizeof(distance_text), "DISTANCE %d CM",
                 distance_display);
    } else {
        snprintf(distance_text, sizeof(distance_text), "DISTANCE -- CM");
    }
    draw_centered_text(280, distance_text, COLOR_WHITE, 2);
}

uint16_t display_get_width(void)
{
    return current_width;
}

uint16_t display_get_height(void)
{
    return current_height;
}

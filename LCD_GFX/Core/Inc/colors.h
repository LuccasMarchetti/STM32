#pragma once

#include <stdint.h>

// Macro para converter RGB (0-255) para RGB565
#define RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// Cores padrão RGB565
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFD20
#define COLOR_PURPLE      0x780F
#define COLOR_GRAY        0x8410
#define COLOR_DARK_GRAY   0x4208
#define COLOR_LIGHT_GRAY  0xC618
#define COLOR_NAVY        0x000F
#define COLOR_DARK_GREEN  0x03E0
#define COLOR_DARK_CYAN   0x03EF
#define COLOR_MAROON      0x7800
#define COLOR_OLIVE       0x7BE0
#define COLOR_LIME        0x07FF
#define COLOR_PINK        0xFE19
#define COLOR_BROWN       0x9A60

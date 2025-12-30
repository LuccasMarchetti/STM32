#include "lcd.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "fonts.h"
#include <cstddef>
#include <cstring>

extern SPI_HandleTypeDef hspi4;
extern TIM_HandleTypeDef htim1;

void LCD::set_cmd_mode() {
    HAL_GPIO_WritePin(wr_port_, wr_pin_, GPIO_PIN_RESET);
}

void LCD::set_data_mode() {
    HAL_GPIO_WritePin(wr_port_, wr_pin_, GPIO_PIN_SET);
}

void LCD::write_cmd(uint8_t cmd) {
    set_cmd_mode();
    HAL_SPI_Transmit(hspi_, &cmd, 1, HAL_MAX_DELAY);
}

void LCD::write_data(const uint8_t *data, uint16_t len) {
    set_data_mode();
    HAL_SPI_Transmit(hspi_, const_cast<uint8_t *>(data), len, HAL_MAX_DELAY);
}

void LCD::init(uint16_t wr_rs_pin, GPIO_TypeDef* wr_rs_port, SPI_HandleTypeDef* hspi, TIM_HandleTypeDef* htim, uint32_t channel, bool complementary_timer)
{
    wr_pin_ = wr_rs_pin;
    wr_port_ = wr_rs_port;
    hspi_ = hspi;
    htim_ = htim;
    channel_ = channel;
    complementary_timer_ = complementary_timer;

    if (complementary_timer_) {
        HAL_TIMEx_PWMN_Start(htim_, channel_);
    } else {
        HAL_TIM_PWM_Start(htim_, channel_);
    }

    set_backlight(100.0f); // Backlight at 100%

    write_cmd(0x11);

    write_cmd(0x36);
    uint8_t madctl = 0xC0;  // Remove bit BGR (0xC8 -> 0xC0)
    write_data(&madctl, 1);

    write_cmd(0x3A);
    uint8_t colmod = 0x55;  // 16-bit RGB565
    write_data(&colmod, 1);

    write_cmd(0x21);
    write_cmd(0x29);
}


void LCD::set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    x0 += X_OFFSET;
    x1 += X_OFFSET;
    y0 += Y_OFFSET;
    y1 += Y_OFFSET;
    uint8_t caset[] = {static_cast<uint8_t>(x0>>8), static_cast<uint8_t>(x0&0xFF), static_cast<uint8_t>(x1>>8), static_cast<uint8_t>(x1&0xFF)};
    uint8_t raset[] = {static_cast<uint8_t>(y0>>8), static_cast<uint8_t>(y0&0xFF), static_cast<uint8_t>(y1>>8), static_cast<uint8_t>(y1&0xFF)};
    write_cmd(0x2A); write_data(caset, sizeof(caset));
    write_cmd(0x2B); write_data(raset, sizeof(raset));
    write_cmd(0x2C); // RAMWR
}

void LCD::fill_rgb565(uint16_t color) {
    set_window(0, 0, 79, 159);
    uint8_t buf[128];
    for (size_t i = 0; i < sizeof(buf); i += 2) {
        buf[i]   = static_cast<uint8_t>(color >> 8);
        buf[i+1] = static_cast<uint8_t>(color & 0xFF);
    }

    set_data_mode();

    constexpr int PIXELS = 80 * 160;
    constexpr int PIXELS_PER_CHUNK = sizeof(buf) / 2;

    for (int i = 0; i < PIXELS; i += PIXELS_PER_CHUNK) {
        HAL_SPI_Transmit(hspi_, buf, sizeof(buf), HAL_MAX_DELAY);
    }
}

void LCD::set_backlight(float brightness) {
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 100.0f) brightness = 100.0f;
    // Backlight driver is active-low on TIM1_CH2N: more high-time means dimmer.
    if (complementary_timer_) {
        brightness = 100.0f - brightness;
    }
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim_);
    uint32_t pulse = static_cast<uint32_t>(((brightness) / 100.0f) * (arr + 1));
    if (pulse > arr) pulse = arr; // clamp
    __HAL_TIM_SET_COMPARE(htim_, channel_, pulse);
}

// ============================================================================
// Funções de desenho de primitivas
// ============================================================================

void LCD::draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= 80 || y >= 160) return;
    
    set_window(x, y, x, y);
    
    uint8_t data[2] = {
        static_cast<uint8_t>(color >> 8),
        static_cast<uint8_t>(color & 0xFF)
    };
    write_data(data, 2);
}

void LCD::draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // Desenha um retângulo vazio (apenas bordas)
    fill_rect(x, y, w, 1, color);           // Top
    fill_rect(x, y + h - 1, w, 1, color);   // Bottom
    fill_rect(x, y, 1, h, color);           // Left
    fill_rect(x + w - 1, y, 1, h, color);   // Right
}

void LCD::fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= 80 || y >= 160) return;
    if (x + w > 80) w = 80 - x;
    if (y + h > 160) h = 160 - y;
    
    set_window(x, y, x + w - 1, y + h - 1);
    
    uint8_t buf[128];
    for (size_t i = 0; i < sizeof(buf); i += 2) {
        buf[i]   = static_cast<uint8_t>(color >> 8);
        buf[i+1] = static_cast<uint8_t>(color & 0xFF);
    }
    
    set_data_mode();
    
    uint32_t total_pixels = w * h;
    uint32_t pixels_per_chunk = sizeof(buf) / 2;
    
    for (uint32_t i = 0; i < total_pixels; i += pixels_per_chunk) {
        uint32_t remaining = total_pixels - i;
        uint32_t chunk_size = (remaining < pixels_per_chunk) ? remaining * 2 : sizeof(buf);
        HAL_SPI_Transmit(hspi_, buf, chunk_size, HAL_MAX_DELAY);
    }
}

// ============================================================================
// Funções de texto
// ============================================================================

void LCD::draw_char(uint16_t x, uint16_t y, char c, const Font* font, uint16_t color, uint16_t bg_color) {
    const uint8_t* char_data = font_get_char_data(font, c);
    if (!char_data) return;
    
    uint16_t char_width = font->width;
    uint16_t char_height = font->height;
    
    // Para fontes bitmap simples (1 bit por pixel)
    for (uint16_t row = 0; row < char_height; row++) {
        for (uint16_t col = 0; col < char_width; col++) {
            // Calcula qual byte e qual bit contém este pixel
            uint16_t pixel_index = row * char_width + col;
            uint16_t byte_index = pixel_index / 8;
            uint8_t bit_index = pixel_index % 8;
            
            // Verifica se o bit está setado
            bool pixel_set = (char_data[byte_index] & (1 << (7 - bit_index))) != 0;
            
            // Desenha o pixel com a cor apropriada
            draw_pixel(x + col, y + row, pixel_set ? color : bg_color);
        }
    }
}

void LCD::draw_string(uint16_t x, uint16_t y, const char* str, const Font* font, uint16_t color, uint16_t bg_color) {
    uint16_t current_x = x;
    
    while (*str) {
        // Quebra de linha se necessário
        if (current_x + font->width > 80) {
            current_x = x;
            y += font->height + 2; // 2 pixels de espaçamento entre linhas
        }
        
        if (y + font->height > 160) break; // Sai se passar da tela
        
        draw_char(current_x, y, *str, font, color, bg_color);
        current_x += font->width + 1; // 1 pixel de espaçamento entre caracteres
        str++;
    }
}

void LCD::draw_string_aligned(uint16_t y, const char* str, const Font* font, uint16_t color, uint16_t bg_color, TextAlign align) {
    uint16_t str_width = get_string_width(str, font);
    uint16_t x;
    
    switch (align) {
        case TEXT_ALIGN_CENTER:
            x = (80 - str_width) / 2;
            break;
        case TEXT_ALIGN_RIGHT:
            x = 80 - str_width;
            break;
        case TEXT_ALIGN_LEFT:
        default:
            x = 0;
            break;
    }
    
    draw_string(x, y, str, font, color, bg_color);
}

uint16_t LCD::get_string_width(const char* str, const Font* font) {
    uint16_t width = 0;
    while (*str) {
        width += font->width + 1; // +1 para espaçamento
        str++;
    }
    if (width > 0) width--; // Remove o último espaçamento
    return width;
}

// Funções simplificadas usando configurações padrão
void LCD::print(uint16_t x, uint16_t y, const char* str) {
    draw_string(x, y, str, current_font_, text_color_, bg_color_);
}

void LCD::print_aligned(uint16_t y, const char* str, TextAlign align) {
    draw_string_aligned(y, str, current_font_, text_color_, bg_color_, align);
}

static LCD g_lcd;

extern "C" void lcd_init(void) {
    g_lcd.init(LCD_WR_RS_Pin, LCD_WR_RS_GPIO_Port, &hspi4, &htim1, TIM_CHANNEL_2);
}

extern "C" void lcd_fill_rgb565(uint16_t color) {
    g_lcd.fill_rgb565(color);
}

extern "C" void lcd_draw_string(uint16_t x, uint16_t y, const char* str, const Font* font, uint16_t color, uint16_t bg_color) {
    g_lcd.draw_string(x, y, str, font, color, bg_color);
}

extern "C" void lcd_print(uint16_t x, uint16_t y, const char* str) {
    g_lcd.print(x, y, str);
}
#pragma once

#include "main.hpp"
#include "fonts.h"
#include <stdint.h>

#define X_OFFSET 26
#define Y_OFFSET 1

// Enum para alinhamento de texto
typedef enum {
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
} TextAlign;

class LCD {
public:
    void init(uint16_t wr_rs_pin, GPIO_TypeDef* wr_rs_port, SPI_HandleTypeDef* hspi, TIM_HandleTypeDef* htim, uint32_t channel, bool complementary_timer = false);
    void fill_rgb565(uint16_t color);
    void set_backlight(float brightness);
    
    // Funções de desenho de primitivas
    void draw_pixel(uint16_t x, uint16_t y, uint16_t color);
    void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    
    // Funções de texto
    void draw_char(uint16_t x, uint16_t y, char c, const Font* font, uint16_t color, uint16_t bg_color);
    void draw_string(uint16_t x, uint16_t y, const char* str, const Font* font, uint16_t color, uint16_t bg_color);
    void draw_string_aligned(uint16_t y, const char* str, const Font* font, uint16_t color, uint16_t bg_color, TextAlign align);
    uint16_t get_string_width(const char* str, const Font* font);
    
    // Setters para cores padrão
    void set_text_color(uint16_t color) { text_color_ = color; }
    void set_bg_color(uint16_t color) { bg_color_ = color; }
    void set_font(const Font* font) { current_font_ = font; }
    
    // Getters
    uint16_t get_text_color() const { return text_color_; }
    uint16_t get_bg_color() const { return bg_color_; }
    const Font* get_font() const { return current_font_; }
    
    // Funções simplificadas usando configurações padrão
    void print(uint16_t x, uint16_t y, const char* str);
    void print_aligned(uint16_t y, const char* str, TextAlign align);

private:
    uint16_t wr_pin_ = 0;
    GPIO_TypeDef* wr_port_ = nullptr;
    SPI_HandleTypeDef* hspi_ = nullptr;
    TIM_HandleTypeDef* htim_ = nullptr;
    uint32_t channel_ = 0;
    bool complementary_timer_ = false;
    
    // Configurações padrão de texto
    uint16_t text_color_ = 0xFFFF;  // Branco
    uint16_t bg_color_ = 0x0000;    // Preto
    const Font* current_font_ = &Font_5x7;

    void write_cmd(uint8_t cmd);
    void write_data(const uint8_t *data, uint16_t len);
    void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void set_cmd_mode();
    void set_data_mode();
};

#ifdef __cplusplus
extern "C" {
#endif

void lcd_init(void);
void lcd_fill_rgb565(uint16_t color);
void lcd_draw_string(uint16_t x, uint16_t y, const char* str, const Font* font, uint16_t color, uint16_t bg_color);
void lcd_print(uint16_t x, uint16_t y, const char* str);

#ifdef __cplusplus
}
#endif






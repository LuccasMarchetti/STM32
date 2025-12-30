#pragma once

#include <stdint.h>

// Estrutura para definir uma fonte
typedef struct {
    const uint8_t width;      // Largura de cada caractere em pixels
    const uint8_t height;     // Altura de cada caractere em pixels
    const uint8_t first_char; // Primeiro caractere disponível
    const uint8_t last_char;  // Último caractere disponível
    const uint8_t* data;      // Dados bitmap da fonte
} Font;

#ifdef __cplusplus
extern "C" {
#endif

// Fontes disponíveis
extern const Font Font_5x7;
extern const Font Font_7x10;
extern const Font Font_11x18;
extern const Font Font_16x26;

// Funções auxiliares
const uint8_t* font_get_char_data(const Font* font, char c);
uint16_t font_get_char_width(const Font* font, char c);
uint16_t font_get_char_height(const Font* font);

#ifdef __cplusplus
}
#endif

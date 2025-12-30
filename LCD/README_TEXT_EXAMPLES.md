# Exemplos de Uso do Sistema de Texto OLED

## Instalação

Os arquivos já foram criados:
- `Core/Inc/fonts.h` - Definições de fontes
- `Core/Src/fonts.c` - Dados das fontes
- `Core/Inc/colors.h` - Definições de cores RGB565
- `Core/Inc/lcd.h` e `Core/Src/lcd.cpp` - Funções de desenho

## Exemplos de Uso

### 1. Exemplo Básico - Texto Simples

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

// No seu main.c ou main.cpp
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI4_Init();
    MX_TIM1_Init();
    
    // Inicializar LCD
    lcd_init();
    
    // Limpar tela com preto
    lcd_fill_rgb565(COLOR_BLACK);
    
    // Desenhar texto usando C
    lcd_draw_string(5, 10, "Hello World!", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(5, 30, "STM32 OLED", &Font_5x7, COLOR_GREEN, COLOR_BLACK);
    
    while (1) {
        // Seu código aqui
    }
}
```

### 2. Exemplo com Diferentes Fontes

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

void display_welcome_screen(void) {
    lcd_fill_rgb565(COLOR_BLACK);
    
    // Título grande
    lcd_draw_string(2, 5, "STM32", &Font_5x7, COLOR_CYAN, COLOR_BLACK);
    
    // Subtítulo médio
    lcd_draw_string(2, 20, "OLED Display", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
    
    // Informação
    lcd_draw_string(2, 40, "Ready!", &Font_5x7, COLOR_GREEN, COLOR_BLACK);
}
```

### 3. Exemplo C++ com Orientação a Objetos

```cpp
// No seu main.cpp
#include "main.hpp"
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

extern LCD g_lcd; // Declarar acesso ao objeto global

void cpp_text_example(void) {
    // Configurar fonte e cor padrão
    g_lcd.set_font(&Font_5x7);
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.set_bg_color(COLOR_BLACK);
    
    // Limpar tela
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Usar método simplificado
    g_lcd.print(5, 10, "Simple text");
    
    // Texto centralizado
    g_lcd.print_aligned(30, "Centered", TEXT_ALIGN_CENTER);
    
    // Texto à direita
    g_lcd.print_aligned(50, "Right", TEXT_ALIGN_RIGHT);
    
    // Mudar cores
    g_lcd.set_text_color(COLOR_RED);
    g_lcd.print(5, 70, "Red text");
    
    g_lcd.set_text_color(COLOR_YELLOW);
    g_lcd.print(5, 85, "Yellow text");
}
```

### 4. Exemplo com Contador/Display Numérico

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"
#include <stdio.h>

void display_counter(uint32_t count) {
    char buffer[32];
    
    // Formatar número
    snprintf(buffer, sizeof(buffer), "Count: %lu", count);
    
    // Desenhar retângulo para limpar área anterior
    extern LCD g_lcd;
    g_lcd.fill_rect(5, 50, 70, 10, COLOR_BLACK);
    
    // Desenhar novo valor
    lcd_draw_string(5, 50, buffer, &Font_5x7, COLOR_GREEN, COLOR_BLACK);
}

void main_loop_example(void) {
    uint32_t counter = 0;
    
    lcd_init();
    lcd_fill_rgb565(COLOR_BLACK);
    lcd_draw_string(5, 10, "Counter Demo", &Font_5x7, COLOR_CYAN, COLOR_BLACK);
    
    while (1) {
        display_counter(counter);
        counter++;
        HAL_Delay(100);
    }
}
```

### 5. Exemplo com Menu

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

typedef struct {
    const char* text;
    void (*callback)(void);
} MenuItem;

void menu_option_1(void) { /* Código da opção 1 */ }
void menu_option_2(void) { /* Código da opção 2 */ }
void menu_option_3(void) { /* Código da opção 3 */ }

MenuItem menu_items[] = {
    {"1. Config", menu_option_1},
    {"2. Sensors", menu_option_2},
    {"3. About", menu_option_3},
};

void draw_menu(uint8_t selected_index) {
    extern LCD g_lcd;
    
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Título do menu
    g_lcd.set_text_color(COLOR_YELLOW);
    g_lcd.set_bg_color(COLOR_BLACK);
    g_lcd.print_aligned(5, "MAIN MENU", TEXT_ALIGN_CENTER);
    
    // Desenhar linha separadora
    g_lcd.fill_rect(0, 18, 80, 1, COLOR_WHITE);
    
    // Desenhar itens
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t y = 25 + (i * 15);
        
        if (i == selected_index) {
            // Item selecionado - invertido
            g_lcd.fill_rect(2, y-1, 76, 12, COLOR_WHITE);
            g_lcd.set_text_color(COLOR_BLACK);
            g_lcd.set_bg_color(COLOR_WHITE);
        } else {
            g_lcd.set_text_color(COLOR_WHITE);
            g_lcd.set_bg_color(COLOR_BLACK);
        }
        
        g_lcd.print(5, y, menu_items[i].text);
    }
}
```

### 6. Exemplo com Dados de Sensor

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"
#include <stdio.h>

void display_sensor_data(float temperature, float humidity) {
    extern LCD g_lcd;
    char buffer[32];
    
    // Limpar tela
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Título
    g_lcd.set_text_color(COLOR_CYAN);
    g_lcd.print_aligned(5, "SENSOR DATA", TEXT_ALIGN_CENTER);
    
    // Linha separadora
    g_lcd.fill_rect(0, 18, 80, 1, COLOR_WHITE);
    
    // Temperatura
    g_lcd.set_text_color(COLOR_ORANGE);
    g_lcd.print(5, 25, "Temp:");
    
    snprintf(buffer, sizeof(buffer), "%.1f C", temperature);
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.print(5, 38, buffer);
    
    // Umidade
    g_lcd.set_text_color(COLOR_BLUE);
    g_lcd.print(5, 55, "Humidity:");
    
    snprintf(buffer, sizeof(buffer), "%.1f %%", humidity);
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.print(5, 68, buffer);
}
```

### 7. Exemplo com Gráficos e Texto

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

void draw_status_bar(const char* status, uint8_t battery_percent) {
    extern LCD g_lcd;
    char buffer[16];
    
    // Background da barra de status
    g_lcd.fill_rect(0, 0, 80, 15, COLOR_NAVY);
    
    // Status text
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.set_bg_color(COLOR_NAVY);
    g_lcd.print(2, 4, status);
    
    // Bateria
    snprintf(buffer, sizeof(buffer), "%d%%", battery_percent);
    g_lcd.print(55, 4, buffer);
    
    // Ícone de bateria (retângulo)
    g_lcd.draw_rect(70, 5, 8, 5, COLOR_WHITE);
    
    // Preencher de acordo com porcentagem
    uint8_t fill_width = (battery_percent * 6) / 100;
    if (fill_width > 0) {
        uint16_t bat_color = battery_percent > 20 ? COLOR_GREEN : COLOR_RED;
        g_lcd.fill_rect(71, 6, fill_width, 3, bat_color);
    }
}
```

### 8. Caracteres Especiais

```cpp
// Para adicionar caracteres especiais, você precisa expandir a fonte
// Por enquanto, você pode usar os caracteres ASCII padrão (32-126):

void test_special_chars(void) {
    lcd_init();
    lcd_fill_rgb565(COLOR_BLACK);
    
    // Caracteres disponíveis
    lcd_draw_string(5, 10, "!@#$%^&*()", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(5, 25, "[]{}()<>", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(5, 40, "+-*/=", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
    lcd_draw_string(5, 55, "0123456789", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
}
```

## Macros de Cores RGB565

Use as cores definidas em `colors.h`:
- `COLOR_BLACK`, `COLOR_WHITE`
- `COLOR_RED`, `COLOR_GREEN`, `COLOR_BLUE`
- `COLOR_YELLOW`, `COLOR_CYAN`, `COLOR_MAGENTA`
- E muitas outras!

Ou crie cores customizadas:
```cpp
uint16_t custom_color = RGB565(128, 64, 200); // R, G, B (0-255)
```

## Próximos Passos

1. **Adicionar mais fontes**: Expanda `fonts.c` com fontes maiores
2. **Otimização**: Use DMA para transferências SPI mais rápidas
3. **Suporte UTF-8**: Adicione caracteres acentuados (á, é, í, etc.)
4. **Fontes proporcionais**: Implemente fontes com largura variável
5. **Anti-aliasing**: Adicione suavização de bordas

## Performance

- Fonte 5x7: ~35 bytes por caractere
- String de 10 chars: ~350 bytes transferidos
- Tempo estimado: 1-5ms dependendo da configuração SPI

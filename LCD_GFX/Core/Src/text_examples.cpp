/*
 * Exemplo completo de uso do sistema de texto OLED
 * 
 * Este arquivo demonstra como usar todas as funcionalidades de texto
 * disponíveis no display OLED.
 */

#include "main.hpp"
#include "lcd.h"
#include "fonts.h"
#include "colors.h"
#include <stdio.h>

// Acesso ao objeto LCD global
extern LCD g_lcd;

// ============================================================================
// Exemplo 1: Tela de boas-vindas
// ============================================================================
void example_welcome_screen(void) {
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Título
    g_lcd.set_text_color(COLOR_CYAN);
    g_lcd.set_bg_color(COLOR_BLACK);
    g_lcd.print_aligned(10, "STM32H723", TEXT_ALIGN_CENTER);
    
    // Subtítulo
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.print_aligned(25, "OLED Display", TEXT_ALIGN_CENTER);
    
    // Versão
    g_lcd.set_text_color(COLOR_YELLOW);
    g_lcd.print_aligned(45, "v1.0", TEXT_ALIGN_CENTER);
    
    // Status
    g_lcd.set_text_color(COLOR_GREEN);
    g_lcd.print_aligned(70, "Ready!", TEXT_ALIGN_CENTER);
    
    HAL_Delay(2000);
}

// ============================================================================
// Exemplo 2: Contador animado
// ============================================================================
void example_counter(void) {
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    g_lcd.set_text_color(COLOR_MAGENTA);
    g_lcd.print_aligned(5, "COUNTER", TEXT_ALIGN_CENTER);
    
    char buffer[32];
    
    for (uint32_t i = 0; i < 100; i++) {
        // Limpar área do contador
        g_lcd.fill_rect(0, 30, 80, 20, COLOR_BLACK);
        
        // Desenhar contador
        snprintf(buffer, sizeof(buffer), "%lu", i);
        g_lcd.set_text_color(COLOR_GREEN);
        g_lcd.print_aligned(40, buffer, TEXT_ALIGN_CENTER);
        
        // Barra de progresso
        uint16_t bar_width = (i * 70) / 100;
        g_lcd.fill_rect(5, 70, bar_width, 8, COLOR_BLUE);
        g_lcd.draw_rect(5, 70, 70, 8, COLOR_WHITE);
        
        HAL_Delay(50);
    }
    
    HAL_Delay(1000);
}

// ============================================================================
// Exemplo 3: Display de dados (simulado)
// ============================================================================
void example_sensor_data(void) {
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Cabeçalho
    g_lcd.fill_rect(0, 0, 80, 15, COLOR_NAVY);
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.set_bg_color(COLOR_NAVY);
    g_lcd.print_aligned(4, "SENSORS", TEXT_ALIGN_CENTER);
    
    char buffer[32];
    
    for (int i = 0; i < 20; i++) {
        // Simular leitura de sensores
        float temp = 20.0f + (i * 0.5f);
        float hum = 45.0f + (i * 0.3f);
        
        // Área de dados (limpar)
        g_lcd.fill_rect(0, 20, 80, 140, COLOR_BLACK);
        g_lcd.set_bg_color(COLOR_BLACK);
        
        // Temperatura
        g_lcd.set_text_color(COLOR_ORANGE);
        g_lcd.print(5, 25, "Temp:");
        
        snprintf(buffer, sizeof(buffer), "%.1f C", temp);
        g_lcd.set_text_color(COLOR_WHITE);
        g_lcd.print(10, 38, buffer);
        
        // Barra temperatura
        uint16_t temp_bar = ((uint16_t)temp * 60) / 40;
        g_lcd.fill_rect(10, 50, temp_bar, 6, COLOR_RED);
        g_lcd.draw_rect(10, 50, 60, 6, COLOR_WHITE);
        
        // Umidade
        g_lcd.set_text_color(COLOR_BLUE);
        g_lcd.print(5, 65, "Humid:");
        
        snprintf(buffer, sizeof(buffer), "%.1f %%", hum);
        g_lcd.set_text_color(COLOR_WHITE);
        g_lcd.print(10, 78, buffer);
        
        // Barra umidade
        uint16_t hum_bar = ((uint16_t)hum * 60) / 100;
        g_lcd.fill_rect(10, 90, hum_bar, 6, COLOR_CYAN);
        g_lcd.draw_rect(10, 90, 60, 6, COLOR_WHITE);
        
        // Status
        g_lcd.set_text_color(COLOR_GREEN);
        g_lcd.print_aligned(110, "OK", TEXT_ALIGN_CENTER);
        
        HAL_Delay(200);
    }
    
    HAL_Delay(1000);
}

// ============================================================================
// Exemplo 4: Menu interativo (simulado)
// ============================================================================
void example_menu(void) {
    const char* menu_items[] = {
        "Config",
        "Sensors",
        "Display",
        "About"
    };
    
    for (uint8_t selected = 0; selected < 4; selected++) {
        g_lcd.fill_rgb565(COLOR_BLACK);
        
        // Cabeçalho
        g_lcd.fill_rect(0, 0, 80, 15, COLOR_PURPLE);
        g_lcd.set_text_color(COLOR_WHITE);
        g_lcd.set_bg_color(COLOR_PURPLE);
        g_lcd.print_aligned(4, "MENU", TEXT_ALIGN_CENTER);
        
        g_lcd.set_bg_color(COLOR_BLACK);
        
        // Desenhar itens
        for (uint8_t i = 0; i < 4; i++) {
            uint16_t y = 25 + (i * 18);
            
            if (i == selected) {
                // Item selecionado
                g_lcd.fill_rect(3, y-2, 74, 14, COLOR_WHITE);
                g_lcd.set_text_color(COLOR_BLACK);
                g_lcd.set_bg_color(COLOR_WHITE);
                
                // Indicador
                g_lcd.print(5, y, ">");
            } else {
                g_lcd.set_text_color(COLOR_WHITE);
                g_lcd.set_bg_color(COLOR_BLACK);
            }
            
            // Nome do item
            g_lcd.print(15, y, menu_items[i]);
        }
        
        HAL_Delay(500);
    }
    
    HAL_Delay(1000);
}

// ============================================================================
// Exemplo 5: Gráficos e texto combinados
// ============================================================================
void example_graphics_text(void) {
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Título
    g_lcd.set_text_color(COLOR_YELLOW);
    g_lcd.set_bg_color(COLOR_BLACK);
    g_lcd.print_aligned(5, "GRAPHICS", TEXT_ALIGN_CENTER);
    
    // Desenhar formas
    g_lcd.fill_rect(10, 25, 20, 20, COLOR_RED);
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.print(12, 30, "RED");
    
    g_lcd.fill_rect(35, 25, 20, 20, COLOR_GREEN);
    g_lcd.print(37, 30, "GRN");
    
    g_lcd.fill_rect(60, 25, 15, 20, COLOR_BLUE);
    g_lcd.print(62, 30, "BLU");
    
    // Desenhar retângulos vazios
    for (uint8_t i = 0; i < 5; i++) {
        uint16_t y = 55 + (i * 12);
        uint16_t color = RGB565(i * 50, 255 - i * 50, 100 + i * 30);
        g_lcd.draw_rect(10, y, 60, 10, color);
    }
    
    g_lcd.set_text_color(COLOR_CYAN);
    g_lcd.print_aligned(135, "Shapes!", TEXT_ALIGN_CENTER);
    
    HAL_Delay(3000);
}

// ============================================================================
// Exemplo 6: Teste de todas as cores
// ============================================================================
void example_color_test(void) {
    uint16_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA,
        COLOR_ORANGE, COLOR_PURPLE, COLOR_PINK
    };
    
    const char* color_names[] = {
        "RED", "GREEN", "BLUE",
        "YELLOW", "CYAN", "MAGENTA",
        "ORANGE", "PURPLE", "PINK"
    };
    
    for (uint8_t i = 0; i < 9; i++) {
        g_lcd.fill_rgb565(COLOR_BLACK);
        
        // Preencher com a cor
        g_lcd.fill_rect(10, 20, 60, 40, colors[i]);
        
        // Nome da cor
        g_lcd.set_text_color(COLOR_WHITE);
        g_lcd.set_bg_color(COLOR_BLACK);
        g_lcd.print_aligned(70, color_names[i], TEXT_ALIGN_CENTER);
        
        HAL_Delay(800);
    }
}

// ============================================================================
// Exemplo 7: Animação de texto
// ============================================================================
void example_text_animation(void) {
    const char* message = "Hello World!";
    
    // Scroll horizontal
    for (int16_t x = 80; x > -60; x -= 2) {
        g_lcd.fill_rgb565(COLOR_BLACK);
        
        g_lcd.set_text_color(COLOR_GREEN);
        g_lcd.draw_string(x, 40, message, &Font_5x7, COLOR_GREEN, COLOR_BLACK);
        
        HAL_Delay(30);
    }
    
    HAL_Delay(500);
    
    // Aparecer letra por letra
    g_lcd.fill_rgb565(COLOR_BLACK);
    char temp[2] = {0, 0};
    uint16_t x = 5;
    
    for (const char* p = message; *p; p++) {
        temp[0] = *p;
        g_lcd.set_text_color(RGB565(rand() % 256, rand() % 256, rand() % 256));
        g_lcd.draw_string(x, 50, temp, &Font_5x7, g_lcd.get_text_color(), COLOR_BLACK);
        x += 6; // largura da fonte + espaço
        HAL_Delay(100);
    }
    
    HAL_Delay(2000);
}

// ============================================================================
// Função principal de demonstração
// ============================================================================
void run_text_examples(void) {
    // Inicializar display
    lcd_init();
    
    while (1) {
        example_welcome_screen();
        example_counter();
        example_sensor_data();
        example_menu();
        example_graphics_text();
        example_color_test();
        example_text_animation();
    }
}

/*
 * Para usar no seu main.cpp, adicione:
 * 
 * #include "text_examples.hpp"
 * 
 * int main(void) {
 *     HAL_Init();
 *     SystemClock_Config();
 *     MX_GPIO_Init();
 *     MX_SPI4_Init();
 *     MX_TIM1_Init();
 *     
 *     run_text_examples();
 * }
 */

# 🎨 Sistema Modular de Texto para OLED Display

## ✅ O que foi implementado

Criei um sistema completo e modular para renderização de texto no seu display OLED STM32. Aqui está o que você tem disponível:

### 📁 Arquivos Criados

1. **`Core/Inc/fonts.h`** + **`Core/Src/fonts.c`**
   - Sistema de fontes bitmap
   - Fonte 5x7 pixels (completa com ASCII 32-126)
   - Estrutura preparada para fontes maiores

2. **`Core/Inc/colors.h`**
   - 20+ cores pré-definidas RGB565
   - Macro RGB565() para cores customizadas

3. **`Core/Inc/lcd.h`** + **`Core/Src/lcd.cpp`** (atualizado)
   - Métodos de desenho de texto
   - Funções de primitivas gráficas
   - Alinhamento de texto (esquerda, centro, direita)

4. **`Core/Inc/text_examples.hpp`** + **`Core/Src/text_examples.cpp`**
   - 7 exemplos completos de uso
   - Demonstrações práticas

5. **`README_TEXT_EXAMPLES.md`**
   - Documentação completa
   - Tutoriais e exemplos

---

## 🚀 Como Usar - Início Rápido

### Exemplo Mais Simples Possível

```cpp
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

int main(void) {
    // ... configurações do STM32 ...
    
    lcd_init();
    lcd_fill_rgb565(COLOR_BLACK);
    
    // Escrever "Hello!" na posição (10, 20)
    lcd_draw_string(10, 20, "Hello!", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
    
    while(1) { }
}
```

### Usando C++ (Orientado a Objetos)

```cpp
#include "main.hpp"
#include "lcd.h"
#include "fonts.h"
#include "colors.h"

extern LCD g_lcd; // Objeto global já criado

int main(void) {
    // ... configurações ...
    
    lcd_init();
    
    // Configurar preferências
    g_lcd.set_font(&Font_5x7);
    g_lcd.set_text_color(COLOR_GREEN);
    g_lcd.set_bg_color(COLOR_BLACK);
    
    // Limpar tela
    g_lcd.fill_rgb565(COLOR_BLACK);
    
    // Escrever texto (usa as configurações acima)
    g_lcd.print(10, 20, "STM32");
    g_lcd.print(10, 35, "OLED Display");
    
    // Texto centralizado
    g_lcd.print_aligned(60, "Ready!", TEXT_ALIGN_CENTER);
    
    while(1) { }
}
```

---

## 🎯 Funcionalidades Principais

### ✏️ Desenho de Texto

```cpp
// Método básico - controle total
g_lcd.draw_string(x, y, "Texto", &Font_5x7, COLOR_WHITE, COLOR_BLACK);

// Método simplificado - usa configurações padrão
g_lcd.print(x, y, "Texto");

// Texto alinhado
g_lcd.print_aligned(y, "Centro", TEXT_ALIGN_CENTER);
g_lcd.print_aligned(y, "Direita", TEXT_ALIGN_RIGHT);
g_lcd.print_aligned(y, "Esquerda", TEXT_ALIGN_LEFT);
```

### 🎨 Cores Disponíveis

```cpp
// Cores pré-definidas
COLOR_WHITE, COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_BLUE
COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_ORANGE
COLOR_PURPLE, COLOR_PINK, COLOR_BROWN, COLOR_GRAY
// E mais!

// Criar cor customizada
uint16_t minha_cor = RGB565(128, 200, 255); // R, G, B (0-255)
```

### 📝 Fontes

```cpp
// Atualmente disponível
Font_5x7   // 5x7 pixels - compacta, boa para muito texto

// Estrutura preparada para adicionar:
Font_7x10  // Média
Font_11x18 // Grande
Font_16x26 // Extra grande
```

### 🔲 Primitivas Gráficas

```cpp
// Desenhar pixel
g_lcd.draw_pixel(x, y, color);

// Retângulo preenchido
g_lcd.fill_rect(x, y, largura, altura, color);

// Retângulo vazio (só borda)
g_lcd.draw_rect(x, y, largura, altura, color);

// Preencher tela inteira
g_lcd.fill_rgb565(color);
```

---

## 📊 Exemplo Prático - Display de Dados

```cpp
#include <stdio.h>

void mostrar_temperatura(float temp) {
    char buffer[32];
    
    // Limpar área
    g_lcd.fill_rect(0, 30, 80, 20, COLOR_BLACK);
    
    // Título
    g_lcd.set_text_color(COLOR_CYAN);
    g_lcd.print(5, 10, "Temp:");
    
    // Valor
    snprintf(buffer, sizeof(buffer), "%.1f C", temp);
    g_lcd.set_text_color(COLOR_WHITE);
    g_lcd.print(5, 25, buffer);
    
    // Barra gráfica
    uint16_t bar_width = (uint16_t)(temp * 2); // Escala
    g_lcd.fill_rect(5, 45, bar_width, 8, COLOR_RED);
    g_lcd.draw_rect(5, 45, 70, 8, COLOR_WHITE);
}
```

---

## 🎬 Testar os Exemplos

No seu `main.cpp`, adicione:

```cpp
#include "text_examples.hpp"

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI4_Init();
    MX_TIM1_Init();
    
    // Rodar todos os exemplos em loop
    run_text_examples();
}
```

Isso mostrará:
1. ✅ Tela de boas-vindas
2. ⏱️ Contador animado
3. 🌡️ Display de sensores (simulado)
4. 📋 Menu interativo
5. 🎨 Gráficos e texto
6. 🌈 Teste de cores
7. ✨ Animação de texto

---

## 🔧 Próximos Passos (Customização)

### 1. Adicionar Fontes Maiores

Edite `Core/Src/fonts.c` e expanda os arrays `Font7x10_Data`, `Font11x18_Data`, etc.

Use ferramentas como:
- [dotmatrixtool.com](http://dotmatrixtool.com/)
- [LCD Assistant](http://en.radzio.dxp.pl/bitmap_converter/)

### 2. Adicionar Caracteres Acentuados (UTF-8)

Expanda a fonte para incluir caracteres como á, é, í, ó, ú, ç, etc.

### 3. Otimização com DMA

```cpp
// Em vez de HAL_SPI_Transmit, use:
HAL_SPI_Transmit_DMA(hspi_, buf, size);
```

### 4. Adicionar Mais Primitivas

```cpp
void draw_line(x0, y0, x1, y1, color);
void draw_circle(x, y, radius, color);
void draw_image(x, y, width, height, const uint16_t* data);
```

---

## 📐 Especificações do Display

- Resolução: **80 x 160 pixels**
- Formato de cor: **RGB565** (16 bits)
- X_OFFSET: 26 pixels
- Y_OFFSET: 1 pixel

---

## ❓ Dúvidas Comuns

**P: Como mudar o tamanho da fonte?**
```cpp
g_lcd.set_font(&Font_5x7);  // Pequena
// Adicione mais fontes em fonts.c para ter outras opções
```

**P: Como fazer texto com fundo transparente?**
```cpp
// Atualmente não suportado. Use a mesma cor de fundo da tela:
g_lcd.draw_string(x, y, "Texto", &Font_5x7, COLOR_WHITE, COLOR_BLACK);
```

**P: Como saber a largura de uma string?**
```cpp
uint16_t largura = g_lcd.get_string_width("Meu texto", &Font_5x7);
```

**P: Posso usar printf direto no display?**
```cpp
// Use snprintf para formatar e depois draw_string:
char buf[32];
snprintf(buf, sizeof(buf), "Valor: %d", valor);
g_lcd.print(x, y, buf);
```

---

## 🎉 Pronto para usar!

Você agora tem um sistema completo e modular para:
- ✅ Escrever texto em qualquer posição
- ✅ Usar diferentes cores
- ✅ Alinhar texto (esquerda, centro, direita)
- ✅ Desenhar formas (retângulos, pixels)
- ✅ Criar interfaces gráficas
- ✅ Fazer animações
- ✅ Mostrar dados de sensores
- ✅ Criar menus

**Comece testando os exemplos e depois customize para seu projeto!** 🚀

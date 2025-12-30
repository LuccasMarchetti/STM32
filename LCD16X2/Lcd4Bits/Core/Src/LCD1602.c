/*
 * LCD1602.c
 *
 */

#include <LCD1602.h>

//Realizar o include da library da família a ser utilizada.
#include "stm32l4xx_hal.h" //Família da Nucleo-L476RG.

/*********** Define the LCD PINS below ****************/

#define RS_Pin GPIO_PIN_0 //RS -> Ok...
#define RS_GPIO_Port GPIOA
#define RW_Pin GPIO_PIN_1 //RW -> Ok...
#define RW_GPIO_Port GPIOA
#define EN_Pin GPIO_PIN_4 //EN -> Ok...
#define EN_GPIO_Port GPIOA
#define D4_Pin GPIO_PIN_0 //D4 -> Ok...
#define D4_GPIO_Port GPIOB
#define D5_Pin GPIO_PIN_15 //D5 -> Ok...
#define D5_GPIO_Port GPIOA
#define D6_Pin GPIO_PIN_12 //D6 -> Ok...
#define D6_GPIO_Port GPIOC
#define D7_Pin GPIO_PIN_10 //D7 -> Ok...
#define D7_GPIO_Port GPIOC


void send_to_lcd (char data, int rs)
{
	HAL_GPIO_WritePin(RS_GPIO_Port, RS_Pin, rs);

	HAL_GPIO_WritePin(D7_GPIO_Port, D7_Pin, ((data>>3)&0x01));
	HAL_GPIO_WritePin(D6_GPIO_Port, D6_Pin, ((data>>2)&0x01));
	HAL_GPIO_WritePin(D5_GPIO_Port, D5_Pin, ((data>>1)&0x01));
	HAL_GPIO_WritePin(D4_GPIO_Port, D4_Pin, ((data>>0)&0x01));

	/* Caso utilize HCLK com frequências maiores que 16Mhz, use delays de 20us;
	 * Se o LCD continuar apresentar problemas, aumente para 50, 80 ou 100(us);
	 * Realize testes aumentando e diminuindo as frequências em questão.
	 */
	HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, 1);
    delay_us(20); //Altere aqui os tempos...
	HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, 0);
	delay_us(20); //Altere aqui os tempos...
}

void lcd_send_cmd (char cmd)
{
    char datatosend;

    datatosend = ((cmd>>4)&0x0f);
    send_to_lcd(datatosend,0);

    datatosend = ((cmd)&0x0f);
	send_to_lcd(datatosend, 0);
}

void lcd_send_data (char data)
{
	char datatosend;

	datatosend = ((data>>4)&0x0f);
	send_to_lcd(datatosend, 1);

	datatosend = ((data)&0x0f);
	send_to_lcd(datatosend, 1);
}

void lcd_clear (void)
{
	lcd_send_cmd(0x01);
	HAL_Delay(2);
}

void lcd_put_cur(int row, int col)
{
    switch (row)
    {
        case 0:
            col |= 0x80;
            break;
        case 1:
            col |= 0xC0;
            break;
    }

    lcd_send_cmd (col);
}


void lcd_init (void)
{
	HAL_Delay(50);
	lcd_send_cmd (0x30);
	HAL_Delay(5);
	lcd_send_cmd (0x30);
	HAL_Delay(1);
	lcd_send_cmd (0x30);
	HAL_Delay(10);
	lcd_send_cmd (0x20);
	HAL_Delay(10);


	lcd_send_cmd (0x28);
	HAL_Delay(1);
	lcd_send_cmd (0x08);
	HAL_Delay(1);
	lcd_send_cmd (0x01);
	HAL_Delay(1);
	HAL_Delay(1);
	lcd_send_cmd (0x06);
	HAL_Delay(1);
	lcd_send_cmd (0x0C);
}

void lcd_send_string (char *str)
{
	while (*str) lcd_send_data (*str++);
}

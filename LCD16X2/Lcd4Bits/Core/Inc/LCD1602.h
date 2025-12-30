/*
 * lcd1602.h
 */

#ifndef INC_LCD1602_H_
#define INC_LCD1602_H_

void lcd_init (void);   //Inicializa o LCD;

void lcd_send_cmd (char cmd);  //Envia Comando Para o LCD;

void lcd_send_data (char data);  //Envia Dados Para o LCD;

void lcd_send_string (char *str);  //Envia String Para o LCD;

void lcd_put_cur(int row, int col);  //Posiciona cursor para posição definida [lin (0 or 1)], [col (0-15)];

void lcd_clear (void); //Limpa o LCD;

#endif

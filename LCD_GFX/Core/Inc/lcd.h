#pragma once

#include "main.h"
#include "spi.h"
#include "tim.h"
#include <stdint.h>

#define X_OFFSET 26
#define Y_OFFSET 1

class lcdDriver {
public:
	lcdDriver(uint16_t width, uint16_t height);
	uint16_t width_, height_;
	void init(uint16_t wr_rs_pin, GPIO_TypeDef* wr_rs_port, SPI_HandleTypeDef* hspi, TIM_HandleTypeDef* backlight_tim, uint32_t channel, TIM_HandleTypeDef* htim, osSemaphoreId_t binary_semaphore, bool complementary_timer = false);
	void fill_rgb565(uint16_t color);
	void set_backlight(float brightness);
	void write_cmd(uint8_t cmd);
	void write_data(const uint8_t *data, uint16_t len);
	void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
	void set_cmd_mode();
	void set_data_mode();
	void write_pixels(const uint16_t* pixels, uint32_t count);
	static void register_instance(lcdDriver* inst);
	void OnSpiTxComplete();
	uint8_t* buf;
	uint16_t wr_pin_ = 0;
	GPIO_TypeDef* wr_port_ = nullptr;
	SPI_HandleTypeDef* hspi_ = nullptr;
	TIM_HandleTypeDef* backlight_tim_ = nullptr;
	TIM_HandleTypeDef* htim_ = nullptr;
	uint32_t channel_ = 0;
	bool complementary_timer_ = false;
	osSemaphoreId_t binary_semaphore_ = nullptr;

	// Configurações padrão de texto
	uint16_t text_color_ = 0xFFFF;  // Branco
	uint16_t bg_color_ = 0x0000;    // Preto
	static lcdDriver* instance_;
	bool isTransmitingBlock_;
};






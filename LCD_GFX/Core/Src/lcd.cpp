#include "lcd.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "app_touchgfx.h"

lcdDriver::lcdDriver(uint16_t width, uint16_t height) {
	width_ = width;
	height_ = height;
	buf = new uint8_t[width_ * height_ * 2];
}

void lcdDriver::set_cmd_mode() {
    HAL_GPIO_WritePin(wr_port_, wr_pin_, GPIO_PIN_RESET);
}

void lcdDriver::set_data_mode() {
    HAL_GPIO_WritePin(wr_port_, wr_pin_, GPIO_PIN_SET);
}

void lcdDriver::write_cmd(uint8_t cmd) {
    set_cmd_mode();
    osSemaphoreAcquire(binary_semaphore_, portMAX_DELAY);
    isTransmitingBlock_ = true;
    HAL_SPI_Transmit_DMA(hspi_, &cmd, 1);
}

void lcdDriver::write_data(const uint8_t *data, uint16_t len) {
    set_data_mode();
    osSemaphoreAcquire(binary_semaphore_, portMAX_DELAY);
    isTransmitingBlock_ = true;
    HAL_SPI_Transmit_DMA(hspi_, const_cast<uint8_t*>(data), len);
}

void lcdDriver::init(uint16_t wr_rs_pin, GPIO_TypeDef* wr_rs_port, SPI_HandleTypeDef* hspi, TIM_HandleTypeDef* backlight_tim, uint32_t channel, TIM_HandleTypeDef* htim, osSemaphoreId_t binary_semaphore, bool complementary_timer)
{
    wr_pin_ = wr_rs_pin;
    wr_port_ = wr_rs_port;
    hspi_ = hspi;
    htim_ = htim;
    backlight_tim_ = backlight_tim;
    channel_ = channel;
    binary_semaphore_ = binary_semaphore;
    complementary_timer_ = complementary_timer;

    // Setar a função weak HAL_SPI_TxCpltCallback
    register_instance(this);

    if (complementary_timer_) {
        HAL_TIMEx_PWMN_Start(backlight_tim_, channel_);
    } else {
        HAL_TIM_PWM_Start(backlight_tim_, channel_);
    }

    set_backlight(100.0f); // Backlight at 100%

    write_cmd(0x11); // Sleep out

    write_cmd(0x36); // Orientation
    uint8_t madctl = 0xC8;
    write_data(&madctl, 1);

    write_cmd(0x3A);
    uint8_t colmod = 0x55;
    write_data(&colmod, 1);

    write_cmd(0x21); // Inversion ON -> 0x20 = OFF
    write_cmd(0x29); // Display ON

    HAL_TIM_Start_IT(htim_);
}


void lcdDriver::set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
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

void lcdDriver::fill_rgb565(uint16_t color) {
    set_window(0, 0, height_-1, width_-1);

    for (size_t i = 0; i < 2*width_*height_; i += 2) {
        buf[i]   = color >> 8;
        buf[i+1] = color & 0xFF;
    }

    set_data_mode();
    write_data(buf, 2*width_*height_);
}

void lcdDriver::write_pixels(const uint16_t* pixels, uint32_t count)
{
    set_data_mode();
    osSemaphoreAcquire(binary_semaphore_, portMAX_DELAY);
    isTransmitingBlock_ = true;

    HAL_SPI_Transmit_DMA(
        hspi_,
        (uint8_t*)pixels,
        count * 2
    );
}

void lcdDriver::DrawBitmap(uint16_t w, uint16_t h, uint8_t *s)
{
	set_window(0, 0, w - 1, h - 1);
	write_pixels((uint16_t*)s, w * h);
}



void lcdDriver::set_backlight(float brightness) {
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 100.0f) brightness = 100.0f;
    // Backlight driver is active-low on TIM1_CH2N: more high-time means dimmer.
    if (complementary_timer_) {
        brightness = 100.0f - brightness;
    }
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(backlight_tim_);
    uint32_t pulse = static_cast<uint32_t>(((brightness) / 100.0f) * (arr + 1));
    if (pulse > arr) pulse = arr; // clamp
    __HAL_TIM_SET_COMPARE(backlight_tim_, channel_, pulse);
}

void lcdDriver::register_instance(lcdDriver* inst)
{
    instance_ = inst;
}

void lcdDriver::OnSpiTxComplete()
{
    osSemaphoreRelease(binary_semaphore_);
    isTransmitingBlock_ = false;
    touchgfx::HAL::getInstance()->signalDMAInterrupt();
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (lcdDriver::instance_ && hspi == lcdDriver::instance_->hspi_)
    {
        lcdDriver::instance_->OnSpiTxComplete();
    }
}

extern void touchgfxSignalVSync();

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (lcdDriver::instance_ && htim == lcdDriver::instance_->htim_)
	{
		touchgfxSignalVSync();
	}
}

lcdDriver* lcdDriver::instance_ = nullptr;

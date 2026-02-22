#include "aht20.h"

extern AHT20_HandleTypeDef aht20;

HAL_StatusTypeDef AHT20_ReleaseSemaphore() {
	// Release I2C semaphore
	return osSemaphoreRelease(aht20.i2cSemaphore);
}

HAL_StatusTypeDef AHT20_Setup(){

	if (aht20.hi2c == NULL || aht20.i2cSemaphore == NULL) {
		return HAL_ERROR;
	}

	return HAL_OK;
}

HAL_StatusTypeDef AHT20_Init() {

	uint8_t cmd[3] = {AHT20_CMD_INIT, 0x08, 0x00};

	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Transmit_DMA(aht20.hi2c, AHT20_ADDRESS, cmd, 3);

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever); // Wait for transmission to complete
	osSemaphoreRelease(aht20.i2cSemaphore); // Release semaphore

	return ret;
}

HAL_StatusTypeDef AHT20_Reset() {

	uint8_t cmd = AHT20_CMD_RESET;
	HAL_StatusTypeDef ret;

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Transmit_DMA(aht20.hi2c, AHT20_ADDRESS, &cmd, 1);

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever); // Wait for transmission to complete
	osSemaphoreRelease(aht20.i2cSemaphore); // Release semaphore

	return ret;
}

HAL_StatusTypeDef AHT20_CheckCalibration() {

	uint8_t status;
	HAL_StatusTypeDef ret;

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Receive_DMA(aht20.hi2c, AHT20_ADDRESS, &status, 1);

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(aht20.i2cSemaphore); // Release semaphore

	return ret;
}

HAL_StatusTypeDef AHT20_TriggerMeasurement() {

	uint8_t cmd[3] = {AHT20_CMD_MEASURE, 0x33, 0x00};
	HAL_StatusTypeDef ret;

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Transmit_DMA(aht20.hi2c, AHT20_ADDRESS, cmd, 3);

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever); // Wait for transmission to complete
	osSemaphoreRelease(aht20.i2cSemaphore); // Release semaphore

	return ret;
}

HAL_StatusTypeDef AHT20_ReadData() {

	uint8_t data[6];
	HAL_StatusTypeDef ret;

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Receive_DMA(aht20.hi2c, AHT20_ADDRESS, data, 6);

	osSemaphoreAcquire(aht20.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(aht20.i2cSemaphore); // Release semaphore

	if (ret != HAL_OK) {
		return ret;
	}

	uint32_t rawHumidity = ((uint32_t)(data[1]) << 12) | ((uint32_t)(data[2]) << 4) | ((data[3] >> 4) & 0x0F);
	uint32_t rawTemperature = (((uint32_t)(data[3] & 0x0F)) << 16) | ((uint32_t)(data[4]) << 8) | (uint32_t)(data[5]);

	aht20.humidity = ((float)rawHumidity * 100.0f) / 1048576.0f; // 2^20 = 1048576
	aht20.temperature = (((float)rawTemperature * 200.0f) / 1048576.0f) - 50.0f;

	return HAL_OK;
}

void AHT20_Measure() {
	AHT20_TriggerMeasurement();
	osDelay(AHT20_MEASURE_DELAY);
	AHT20_ReadData();
}

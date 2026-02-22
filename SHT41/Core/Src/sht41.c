#include <sht41.h>

extern SHT41_HandleTypeDef sht41;

HAL_StatusTypeDef SHT41_ReleaseSemaphore() {
	// Release I2C semaphore
	return osSemaphoreRelease(sht41.i2cSemaphore);
}

HAL_StatusTypeDef SHT41_Transmit(uint8_t * cmd, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Transmit_DMA(sht41.hi2c, SHT41_ADDRESS, cmd, size);

	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever); // Wait for transmission to complete
	osSemaphoreRelease(sht41.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef SHT41_Receive(uint8_t * data, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Receive_DMA(sht41.hi2c, SHT41_ADDRESS, data, size);

	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(sht41.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef SHT41_MemRead(uint8_t reg, uint8_t * data, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Mem_Read_DMA(sht41.hi2c, SHT41_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, data, size);

	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(sht41.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef SHT41_MemWrite(uint8_t reg, uint8_t * data, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Mem_Write_DMA(sht41.hi2c, SHT41_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, data, size);

	osSemaphoreAcquire(sht41.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(sht41.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef SHT41_CRC_Check(uint8_t *data, uint8_t length, uint8_t checksum) {
	uint8_t crc = 0xFF;
	uint8_t polynomial = 0x31;

	for (uint8_t i = 0; i < length; i++) {
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ polynomial;
			} else {
				crc <<= 1;
			}
		}
	}
	if (crc != checksum) {
		return HAL_ERROR;
	}
	return HAL_OK;
}

HAL_StatusTypeDef SHT41_Setup(){

	if (sht41.hi2c == NULL || sht41.i2cSemaphore == NULL) {
		return HAL_ERROR;
	}

	return HAL_OK;
}

HAL_StatusTypeDef SHT41_Reset() {

	uint8_t cmd = SHT41_CMD_SOFT_RESET;

	return SHT41_Transmit(&cmd, 1);
}

HAL_StatusTypeDef SHT41_ReadID() {
	uint8_t cmd = SHT41_CMD_READ_SERIAL_NUMBER;
	SHT41_Transmit(&cmd, 1);
	uint8_t data[6];
	SHT41_Receive(data, 6);
	uint32_t sn = (uint32_t)(data[0] << 24 | data[1] << 16 | data[3] << 8 | data[4]);
	if (SHT41_CRC_Check(&data[0], 2, data[2]) != HAL_OK)
		return HAL_ERROR;
	if (SHT41_CRC_Check(&data[3], 2, data[5]) != HAL_OK)
		return HAL_ERROR;

	sht41.serial_number = sn;
	return HAL_OK;
}

HAL_StatusTypeDef SHT41_ReadData(uint8_t precision) {

	uint8_t cmd;
	uint8_t delay;
	switch (precision) {
		case SHT41_CMD_HIGH_PRECISION_MEASURE:
			delay = 9;
			break;
		case SHT41_CMD_MEDIUM_PRECISION_MEASURE:
			delay = 5;
			break;
		case SHT41_CMD_LOW_PRECISION_MEASURE:
			delay = 9;
			break;
		default:
			return HAL_ERROR;
	}

	cmd = precision;

	SHT41_Transmit(&cmd, 1);
	osDelay(delay); // Wait for measurement to complete
	uint8_t data[6];
	SHT41_Receive(data, 6);

	sht41.rawTemperatureData = (uint32_t)(data[0] << 8 | data[1]);
	sht41.rawHumidityData = (uint32_t)(data[3] << 8 | data[4]);

	if (SHT41_CRC_Check(&data[0], 2, data[2]) != HAL_OK)
		return HAL_ERROR;
	if (SHT41_CRC_Check(&data[3], 2, data[5]) != HAL_OK)
		return HAL_ERROR;

	return HAL_OK;
}

HAL_StatusTypeDef SHT41_ActivateHeater(uint8_t cmd) {
	SHT41_Transmit(&cmd, 1);

	switch(cmd) {
		case SHT41_CMD_HEATER_200mW_1S:
		case SHT41_CMD_HEATER_110mW_1S:
		case SHT41_CMD_HEATER_20mW_1S:
			osDelay(1000); // Wait for heater to complete
			break;
		case SHT41_CMD_HEATER_200mw_100MS:
		case SHT41_CMD_HEATER_110mW_100MS:
		case SHT41_CMD_HEATER_20mW_100MS:
			osDelay(100); // Wait for heater to complete
			break;
		default:
			return HAL_ERROR;
	}
	uint8_t data[6];
	SHT41_Receive(data, 6);

	sht41.rawTemperatureData = (uint32_t)(data[0] << 8 | data[1]);
	sht41.rawHumidityData = (uint32_t)(data[3] << 8 | data[4]);

	if (SHT41_CRC_Check(&data[0], 2, data[2]) != HAL_OK)
		return HAL_ERROR;
	if (SHT41_CRC_Check(&data[3], 2, data[5]) != HAL_OK)
		return HAL_ERROR;

	return HAL_OK;
}

HAL_StatusTypeDef SHT41_Init() {
	SHT41_Reset();
	SHT41_ReadID();
	return HAL_OK;
}

HAL_StatusTypeDef SHT41_ConvertData() {
	// Conversion algorithm from SHT41 datasheet
	sht41.humidity = (-6 + 125.0f * ((float)sht41.rawHumidityData / 65535.0f));
	sht41.temperature = (-45 + 175.0f * ((float)sht41.rawTemperatureData / 65535.0f));
	return HAL_OK;
}

HAL_StatusTypeDef SHT41_ReadAndConvertData(uint8_t precision) {
	SHT41_ReadData(precision);
	SHT41_ConvertData();
	return HAL_OK;
}

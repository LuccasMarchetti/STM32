#include <bmp280.h>

extern BMP280_HandleTypeDef bmp280;

HAL_StatusTypeDef BMP280_ReleaseSemaphore() {
	// Release I2C semaphore
	return osSemaphoreRelease(bmp280.i2cSemaphore);
}

HAL_StatusTypeDef BMP280_Transmit(uint8_t * cmd, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Transmit_DMA(bmp280.hi2c, BMP280_ADDRESS, cmd, size);

	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever); // Wait for transmission to complete
	osSemaphoreRelease(bmp280.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef BMP280_Receive(uint8_t * data, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Master_Receive_DMA(bmp280.hi2c, BMP280_ADDRESS, data, size);

	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(bmp280.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef BMP280_MemRead(uint8_t reg, uint8_t * data, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Mem_Read_DMA(bmp280.hi2c, BMP280_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, data, size);

	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(bmp280.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef BMP280_MemWrite(uint8_t reg, uint8_t * data, uint16_t size) {
	HAL_StatusTypeDef ret;
	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever);

	ret = HAL_I2C_Mem_Write_DMA(bmp280.hi2c, BMP280_ADDRESS, reg, I2C_MEMADD_SIZE_8BIT, data, size);

	osSemaphoreAcquire(bmp280.i2cSemaphore, osWaitForever); // Wait for reception to complete
	osSemaphoreRelease(bmp280.i2cSemaphore); // Release semaphore
	return ret;
}

HAL_StatusTypeDef BMP280_Setup(){

	if (bmp280.hi2c == NULL || bmp280.i2cSemaphore == NULL) {
		return HAL_ERROR;
	}

	return HAL_OK;
}

HAL_StatusTypeDef BMP280_Reset() {

	uint8_t cmd[2] = {BMP280_RESET_REG, BMP280_CMD_RESET};

	return BMP280_Transmit(cmd, 2);
}

HAL_StatusTypeDef BMP280_ReadID() {
	uint8_t id;

	BMP280_MemRead(BMP280_REG_ID, &id, 1);

	bmp280.id = id;
	return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadCalibration() {

	uint8_t calibData[BMP280_dig_T_LEN + BMP280_dig_P_LEN];
	BMP280_MemRead(BMP280_dig_T_REG, calibData, sizeof(calibData));

	// Parse temperature calibration data
	bmp280.dig_T1 = (int16_t)(calibData[0] | (calibData[1] << 8));
	for (int i = 0; i < 2; i++) {
		bmp280.dig_T[i] = (uint16_t)(calibData[2 * (i + 1) ] | (calibData[1 + 2 * (i + 1)] << 8));
	}

	// Parse pressure calibration data
	bmp280.dig_P1 = (int16_t)(calibData[6] | (calibData[7] << 8));
	for (int i = 0; i < 8; i++) {
		bmp280.dig_P[i] = (uint16_t)(calibData[8 + 2 * i] | (calibData[8 + 2 * i + 1] << 8));
	}

	return HAL_OK;
}

HAL_StatusTypeDef BMP280_Configure(uint8_t osrs_t, uint8_t osrs_p, uint8_t mode, uint8_t t_sb, uint8_t filter) {

	uint8_t cmd[2];

	// CONFIG (0xF5)
	cmd[0] = BMP280_REG_CONFIG;
	cmd[1] = t_sb | filter;
	BMP280_Transmit(cmd, 2);

	// CTRL_MEAS (0xF4)
	cmd[0] = BMP280_CTRL_MEASURE;
	cmd[1] = osrs_t | osrs_p | mode;
	BMP280_Transmit(cmd, 2);


	// Store configuration
	bmp280.config.osrs_t = osrs_t;
	bmp280.config.osrs_p = osrs_p;
	bmp280.config.mode = mode;
	bmp280.config.t_sb = t_sb;
	bmp280.config.filter = filter;

	return HAL_OK;
}

HAL_StatusTypeDef BMP280_Init() {
	BMP280_Reset();
	BMP280_ReadID();
	if (bmp280.id != BMP280_ID) {
		return HAL_ERROR;
	}
	if (BMP280_ReadCalibration() != HAL_OK) {
		return HAL_ERROR;
	}
	if (BMP280_Configure(BMP280_OSRS_T_x1, BMP280_OSRS_P_x1, BMP280_MODE_NORMAL, BMP280_T_SB_125MS, BMP280_FILTER_OFF) != HAL_OK) {
		return HAL_ERROR;
	}

	return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadTemperature() {
	uint8_t data[3];
	BMP280_MemRead(BMP280_temperature_REG, data, 3);

	bmp280.rawTemperatureData = ((uint32_t)(data[0]) << 12) | ((uint32_t)(data[1]) << 4) | (data[2] >> 4);

	return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadPressure() {
	uint8_t data[3];
	BMP280_MemRead(BMP280_pressure_REG, data, 3);

	bmp280.rawPressureData = ((uint32_t)(data[0]) << 12) | ((uint32_t)(data[1]) << 4) | (data[2] >> 4);
	return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadAll() {
	uint8_t data[6];
	BMP280_MemRead(BMP280_pressure_REG, data, 6);

	bmp280.rawPressureData = ((uint32_t)(data[0]) << 12) | ((uint32_t)(data[1]) << 4) | (data[2] >> 4);
	bmp280.rawTemperatureData = ((uint32_t)(data[3]) << 12) | ((uint32_t)(data[4]) << 4) | (data[5] >> 4);

	return HAL_OK;
}

HAL_StatusTypeDef BMP280_CompensateTemperature() {
	// Compensation algorithm from BMP280 datasheet
	int32_t var1, var2;
	var1 = ((((bmp280.rawTemperatureData >> 3) - ((int32_t)bmp280.dig_T1 << 1))) * ((int32_t)bmp280.dig_T[0])) >> 11;
	var2 = (((((bmp280.rawTemperatureData >> 4) - ((int32_t)bmp280.dig_T1)) * ((bmp280.rawTemperatureData >> 4) - ((int32_t)bmp280.dig_T1))) >> 12) * ((int32_t)bmp280.dig_T[1])) >> 14;
	bmp280.t_fine = var1 + var2;
	bmp280.rawTemperature = (bmp280.t_fine * 5 + 128) >> 8;
	bmp280.temperature = bmp280.rawTemperature / 100.0f;
	return HAL_OK;
}

HAL_StatusTypeDef BMP280_CompensatePressure() {
	// Compensation algorithm from BMP280 datasheet
	int64_t var1, var2, p;
	var1 = ((int64_t)bmp280.t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)bmp280.dig_P[4];
	var2 = var2 + ((var1 * (int64_t)bmp280.dig_P[3]) << 17);
	var2 = var2 + (((int64_t)bmp280.dig_P[2]) << 35);
	var1 = ((var1 * var1 * (int64_t)bmp280.dig_P[1]) >> 8) + ((var1 * (int64_t)bmp280.dig_P[0]) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp280.dig_P1) >> 33;

	if (var1 == 0) {
		return HAL_ERROR; // avoid exception caused by division by zero
	}
	p = 1048576 - bmp280.rawPressureData;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t)bmp280.dig_P[7]) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)bmp280.dig_P[6]) * p) >> 19;

	p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280.dig_P[5]) << 4);
	bmp280.rawPressure = p;
	bmp280.pressure = (float)p / 256.0f;
	return HAL_OK;
}

HAL_StatusTypeDef BMP280_ReadData(){
	BMP280_ReadTemperature();
	BMP280_ReadPressure();
	BMP280_CompensateTemperature();
	BMP280_CompensatePressure();
	return HAL_OK;
}

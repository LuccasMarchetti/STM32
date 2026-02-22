#ifndef AHT20_H
#define AHT20_H

#include "cmsis_os.h"
#include "i2c.h"

#define AHT20_ADDRESS         0x38 << 1  // 7-bit address shifted for HAL functions
#define AHT20_CMD_INIT        0xE1
#define AHT20_CMD_MEASURE     0xAC
#define AHT20_CMD_RESET       0xBA
#define AHT20_STATUS_BUSY     0x80
#define AHT20_STATUS_CALIBRATED 0x08
#define AHT20_MEASURE_DELAY   80  // milliseconds
#define AHT20_INIT_DELAY      10  // milliseconds
#define AHT20_RESET_DELAY     20  // milliseconds

typedef struct {
	I2C_HandleTypeDef* hi2c;
	osSemaphoreId_t i2cSemaphore;
	uint32_t rawTemperature;
	uint32_t rawHumidity;
	float temperature;
	float humidity;
} AHT20_HandleTypeDef;

HAL_StatusTypeDef AHT20_Setup();
HAL_StatusTypeDef AHT20_Init();
HAL_StatusTypeDef AHT20_Reset();
HAL_StatusTypeDef AHT20_ReadData();
HAL_StatusTypeDef AHT20_CheckCalibration();
HAL_StatusTypeDef AHT20_TriggerMeasurement();
HAL_StatusTypeDef AHT20_ReleaseSemaphore();
void AHT20_Measure();

#endif /* AHT20_H */

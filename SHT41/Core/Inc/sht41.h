#ifndef SHT41_H
#define SHT41_H

#include "cmsis_os.h"
#include "i2c.h"

#define SHT41_ADDRESS         				0x44 << 1

// Serial Number
#define SHT41_CMD_READ_SERIAL_NUMBER          	0x89

// Softare reset
#define SHT41_CMD_SOFT_RESET                 	0x94

// Measurement commands
#define SHT41_CMD_HIGH_PRECISION_MEASURE 	0xFD
#define SHT41_CMD_MEDIUM_PRECISION_MEASURE 	0xF6
#define SHT41_CMD_LOW_PRECISION_MEASURE 	0xE0

#define SHT41_CMD_HEATER_200mW_1S   		0x39
#define SHT41_CMD_HEATER_200mw_100MS 		0x32
#define SHT41_CMD_HEATER_110mW_1S   		0x2F
#define SHT41_CMD_HEATER_110mW_100MS 		0x24
#define SHT41_CMD_HEATER_20mW_1S    		0x1E
#define SHT41_CMD_HEATER_20mW_100MS   		0x15

#define SHT41_Data_Length         6


typedef struct {
	I2C_HandleTypeDef* hi2c;
	osSemaphoreId_t i2cSemaphore;
	uint32_t serial_number;
	uint32_t rawTemperatureData;
	uint32_t rawHumidityData;
	float temperature;
	float humidity;
} SHT41_HandleTypeDef;

HAL_StatusTypeDef SHT41_Setup();
HAL_StatusTypeDef SHT41_Init();
HAL_StatusTypeDef SHT41_Reset();
HAL_StatusTypeDef SHT41_ReadID();
HAL_StatusTypeDef SHT41_ReadData(uint8_t precision);
HAL_StatusTypeDef SHT41_ActivateHeater(uint8_t cmd);
HAL_StatusTypeDef SHT41_ConvertData();
HAL_StatusTypeDef SHT41_ReadAndConvertData(uint8_t precision);
HAL_StatusTypeDef SHT41_Transmit(uint8_t * data, uint16_t size);
HAL_StatusTypeDef SHT41_Receive(uint8_t * data, uint16_t size);
HAL_StatusTypeDef SHT41_MemRead(uint8_t reg, uint8_t * data, uint16_t size);
HAL_StatusTypeDef SHT41_MemWrite(uint8_t reg, uint8_t * data, uint16_t size);
HAL_StatusTypeDef SHT41_ReleaseSemaphore();
void SHT41_Measure();

#endif /* SHT41_H */

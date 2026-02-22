#ifndef BMP280_H
#define BMP280_H

#include "cmsis_os.h"
#include "i2c.h"

#define BMP280_ADDRESS         	0x77 << 1

// id register
#define BMP280_REG_ID          0xD0
#define BMP280_ID              0x58

// Commands
#define BMP280_RESET_REG        0xE0
#define BMP280_CMD_RESET        0xB6

// Status register
#define BMP280_REG_STATUS       0xF3
#define BMP280_STATUS_MEASURING 0x08
#define BMP280_STATUS_IM_UPDATE 0x01

// Measurement control
#define BMP280_CTRL_MEASURE      0xF4

#define BMP280_OSRS_T_x0         (000 << 5)  // Temperature oversampling skipped
#define BMP280_OSRS_T_x1         (001 << 5)  // Temperature oversampling x1
#define BMP280_OSRS_T_x2         (010 << 5)  // Temperature oversampling x2
#define BMP280_OSRS_T_x4         (011 << 5)  // Temperature oversampling x4
#define BMP280_OSRS_T_x8         (100 << 5)  // Temperature oversampling x8
#define BMP280_OSRS_T_x16        (101 << 5)  // Temperature oversampling x16

#define BMP280_OSRS_P_x0         (000 << 2)  // Pressure oversampling skipped
#define BMP280_OSRS_P_x1         (001 << 2)  // Pressure oversampling x1
#define BMP280_OSRS_P_x2         (010 << 2)  // Pressure oversampling x2
#define BMP280_OSRS_P_x4         (011 << 2)  // Pressure oversampling x4
#define BMP280_OSRS_P_x8         (100 << 2)  // Pressure oversampling x8
#define BMP280_OSRS_P_x16        (101 << 2)  // Pressure oversampling x16

#define BMP280_MODE_SLEEP         0x00        // Sleep mode
#define BMP280_MODE_NORMAL        0x03        // Normal mode
#define BMP280_MODE_FORCED        0x01        // Forced mode

// Config register
#define BMP280_REG_CONFIG       0xF5

#define BMP280_T_SB_0_5MS       (000 << 5)  // Tstandby 0.5ms
#define BMP280_T_SB_62_5MS      (001 << 5)  // Tstandby 62.5ms
#define BMP280_T_SB_125MS       (010 << 5)  // Tstandby 125ms
#define BMP280_T_SB_250MS       (011 << 5)  // Tstandby 250ms
#define BMP280_T_SB_500MS       (100 << 5)  // Tstandby 500ms
#define BMP280_T_SB_1000MS      (101 << 5)  // Tstandby 1000ms
#define BMP280_T_SB_2000MS      (110 << 5)  // Tstandby 2000ms
#define BMP280_T_SB_4000MS      (111 << 5)  // Tstandby 4000ms

#define BMP280_FILTER_OFF        (000 << 2)  // Filter off
#define BMP280_FILTER_2         (001 << 2)  // Filter coefficient 2
#define BMP280_FILTER_4         (010 << 2)  // Filter coefficient 4
#define BMP280_FILTER_8         (011 << 2)  // Filter coefficient 8
#define BMP280_FILTER_16        (100 << 2)  // Filter coefficient 16


// Compensation parameters registers
#define BMP280_dig_T_REG        0x88
#define BMP280_dig_T_LEN         6
#define BMP280_dig_P_REG        0x8E
#define BMP280_dig_P_LEN         18

// Data registers
#define BMP280_pressure_REG    0xF7
#define BMP280_pressure_LEN     3
#define BMP280_temperature_REG 0xFA
#define BMP280_temperature_LEN   3

#define BMP280_Start_Data_Register 0xF7
#define BMP280_Data_Length         6


typedef struct {
	I2C_HandleTypeDef* hi2c;
	osSemaphoreId_t i2cSemaphore;
	uint8_t id;
	uint16_t dig_T1;
	int16_t dig_T[2];
	uint16_t dig_P1;
	int16_t dig_P[8];
	struct {
		uint8_t osrs_t;
		uint8_t osrs_p;
		uint8_t mode;
		uint8_t t_sb;
		uint8_t filter;
	} config;
	uint32_t rawTemperatureData;
	int32_t rawTemperature;
	int32_t t_fine;
	uint32_t rawPressureData;
	int64_t rawPressure;
	float temperature;
	float pressure;
} BMP280_HandleTypeDef;

HAL_StatusTypeDef BMP280_Setup();
HAL_StatusTypeDef BMP280_Init();
HAL_StatusTypeDef BMP280_Reset();
HAL_StatusTypeDef BMP280_ReadID();
HAL_StatusTypeDef BMP280_ReadCalibration();
HAL_StatusTypeDef BMP280_Configure(uint8_t osrs_t, uint8_t osrs_p, uint8_t mode, uint8_t t_sb, uint8_t filter);
HAL_StatusTypeDef BMP280_ReadStatus();
uint16_t BMP280_CalculateTimeToMeasure(uint8_t osrs_t, uint8_t osrs_p);
HAL_StatusTypeDef BMP280_ReadData();
HAL_StatusTypeDef BMP280_CheckCalibration();
HAL_StatusTypeDef BMP280_TriggerMeasurement();
HAL_StatusTypeDef BMP280_MemRead(uint8_t reg, uint8_t * data, uint16_t size);
HAL_StatusTypeDef BMP280_MemWrite(uint8_t reg, uint8_t * data, uint16_t size);
HAL_StatusTypeDef BMP280_ReleaseSemaphore();
void BMP280_Measure();

#endif /* BMP280_H */

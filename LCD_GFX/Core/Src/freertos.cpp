/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"
#include "colors.h"
#include "spi.h"
#include "tim.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
typedef StaticSemaphore_t osStaticSemaphoreDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for touchGFX */
osThreadId_t touchGFXHandle;
uint32_t touchGFXBuffer[ 2048 ];
osStaticThreadDef_t touchGFXControlBlock;
const osThreadAttr_t touchGFX_attributes = {
  .name = "touchGFX",
  .cb_mem = &touchGFXControlBlock,
  .cb_size = sizeof(touchGFXControlBlock),
  .stack_mem = &touchGFXBuffer[0],
  .stack_size = sizeof(touchGFXBuffer),
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for touchGFXBinarySem */
osSemaphoreId_t touchGFXBinarySemHandle;
osStaticSemaphoreDef_t touchGFXBinarySemControlBlock;
const osSemaphoreAttr_t touchGFXBinarySem_attributes = {
  .name = "touchGFXBinarySem",
  .cb_mem = &touchGFXBinarySemControlBlock,
  .cb_size = sizeof(touchGFXBinarySemControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartTouchGFXTask(void *argument);

extern "C" void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
extern "C" void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of touchGFXBinarySem */
  touchGFXBinarySemHandle = osSemaphoreNew(1, 1, &touchGFXBinarySem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of touchGFX */
  touchGFXHandle = osThreadNew(StartTouchGFXTask, NULL, &touchGFX_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */

  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTouchGFXTask */
/**
* @brief Function implementing the touchGFX thread.
* @param argument: Not used
* @retval None
*/
lcdDriver spilcd(160, 80);
/* USER CODE END Header_StartTouchGFXTask */
void StartTouchGFXTask(void *argument)
{
  /* USER CODE BEGIN StartTouchGFXTask */
  spilcd.init(LCD_WR_RS_Pin, LCD_WR_RS_GPIO_Port, &hspi4, &htim1, TIM_CHANNEL_2, touchGFXBinarySemHandle, true);

  spilcd.set_backlight(100.0f);
  spilcd.fill_rgb565(COLOR_BLUE); // Branco
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTouchGFXTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */


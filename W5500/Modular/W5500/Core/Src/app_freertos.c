/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include "app_network.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticSemaphore_t osStaticMutexDef_t;
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
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};
/* Definitions for spiMutex */
osMutexId_t spiMutexHandle;
osStaticMutexDef_t spiMutexControlBlock;
const osMutexAttr_t spiMutex_attributes = {
  .name = "spiMutex",
  .cb_mem = &spiMutexControlBlock,
  .cb_size = sizeof(spiMutexControlBlock),
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of spiMutex */
  spiMutexHandle = osMutexNew(&spiMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

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
  
  // ====================================================================
  // INICIALIZAR NOVA STACK DE REDE (usa app_network.c)
  // ====================================================================
  
  NetErr_t ret = Network_Init();
  
  if (ret != NET_OK) {
      // ERRO ao inicializar rede
      Error_Handler();
  }
  
  // Iniciar task de rede (já cria a thread internamente)
  Network_StartTask();
  
  // ====================================================================
  // AGUARDAR REDE CONECTAR E EXIBIR STATUS
  // ====================================================================
  
  /* Infinite loop */
  for(;;)
  {
      // Aguardar rede conectar
      if (Network_IsConnected()) {
          
          // Obter e exibir IP (opcional - você pode remover se não usar UART)
          uint8_t ip[4];
          if (Network_GetIPAddress(ip) == NET_OK) {
              // Se tiver printf/UART configurado:
              // printf("IP: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
          }
          
          // Aguardar 10 segundos antes de verificar novamente
          osDelay(10000);
          
      } else {
          // Ainda não conectado - aguardar 1 segundo
          osDelay(1000);
      }
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* 
 * NOTA: Os callbacks de SPI DMA agora estão em Core/Src/app_network.c
 * Eles foram movidos para lá junto com toda a stack de rede.
 * 
 * As seguintes funções estão definidas em app_network.c:
 * - HAL_SPI_TxRxCpltCallback()
 * - HAL_SPI_TxCpltCallback()
 * - HAL_SPI_RxCpltCallback()
 * - HAL_SPI_ErrorCallback()
 */

/* USER CODE END Application */


/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Exemplo de integração do novo código de rede
  * 
  * SUBSTITUIR O CONTEÚDO DE StartDefaultTask() em app_freertos.c
  */

// ADICIONAR NO TOPO DO ARQUIVO (seção USER CODE BEGIN Includes):
#include "app_network.h"

/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  
  // ====================================================================
  // INICIALIZAR NOVA STACK DE REDE (substitui código antigo)
  // ====================================================================
  
  NetErr_t ret = Network_Init();
  
  if (ret != NET_OK) {
      // ERRO ao inicializar rede
      // Tratar erro (ex: piscar LED, enviar debug UART, etc)
      Error_Handler();
  }
  
  // Iniciar task de rede (já cria a thread internamente)
  Network_StartTask();
  
  // ====================================================================
  // SE QUISER ADICIONAR CÓDIGO CUSTOMIZADO
  // ====================================================================
  
  /* Infinite loop */
  for(;;)
  {
      // Esta task pode fazer outras coisas ou simplesmente deletar ela
      // porque a NetworkTask já está rodando em paralelo
      
      // Exemplo: Piscar LED de status
      // HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
      
      osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

// ====================================================================
// CALLBACKS DE SPI DMA (MANTER - usados pelo novo código também)
// ====================================================================

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi1) {
        // Chama callback do app_network.c
        extern void Network_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
        Network_SPI_TxRxCpltCallback(hspi);
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi1) {
        extern void Network_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
        Network_SPI_TxCpltCallback(hspi);
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi1) {
        extern void Network_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
        Network_SPI_RxCpltCallback(hspi);
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi1) {
        extern void Network_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);
        Network_SPI_ErrorCallback(hspi);
    }
}

// ====================================================================
// GPIO EXTI (OPCIONAL - se você conectar pino INT do W5500)
// ====================================================================

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    // Se você conectar pino INT do W5500 ao STM32
    // if (GPIO_Pin == W5500_INT_Pin) {
    //     extern void Network_IRQCallback(void);
    //     Network_IRQCallback();
    // }
}

/* USER CODE END Application */

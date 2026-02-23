/*
 * w5500_ll.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Low-level SPI communication with W5500
 * Funções básicas de leitura/escrita via SPI
 */

#ifndef NETWORK_DRIVER_W5500_W5500_LL_H_
#define NETWORK_DRIVER_W5500_W5500_LL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"
#include "w5500_regs.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

/* =============================================================================
 * ESTRUTURA DE CONTEXTO SPI
 * ============================================================================= */

typedef struct {
    SPI_HandleTypeDef *hspi;
    
    // Controle de CS (Chip Select)
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    
    // Buffers SPI compartilhados
    uint8_t *tx_buf;
    uint8_t *rx_buf;
    uint16_t buf_size;
    
    // RTOS synchronization
    osSemaphoreId_t spi_done_sem;
    osMutexId_t spi_mutex;
    
} W5500_LL_t;

/* =============================================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================================= */

/**
 * @brief Inicializa contexto de baixo nível
 * @param ll Ponteiro para contexto
 * @param hspi Handle do SPI HAL
 * @param cs_port GPIO port do CS
 * @param cs_pin GPIO pin do CS
 * @param tx_buf Buffer TX compartilhado
 * @param rx_buf Buffer RX compartilhado
 * @param buf_size Tamanho dos buffers
 * @param spi_mutex Mutex para exclusão mútua do barramento SPI
 * @param spi_done_sem Semáforo para sinalização de fim de transação DMA
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_Init(
    W5500_LL_t *ll,
    SPI_HandleTypeDef *hspi,
    GPIO_TypeDef *cs_port,
    uint16_t cs_pin,
    uint8_t *tx_buf,
    uint8_t *rx_buf,
    uint16_t buf_size,
    osMutexId_t spi_mutex,
    osSemaphoreId_t spi_done_sem
);

/**
 * @brief Monta fase de controle e endereço (3 bytes)
 * @param ll Contexto
 * @param addr Endereço de 16 bits
 * @param block Block Select Bits (BSB)
 * @param rw Read/Write bit (W5500_RWB_READ ou W5500_RWB_WRITE)
 */
void W5500_LL_SetAddressPhase(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t block,
    uint8_t rw
);

/**
 * @brief Transmite dados via SPI (DMA)
 * @param ll Contexto
 * @param len Tamanho total a transmitir (incluindo 3 bytes de controle)
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_Transmit(W5500_LL_t *ll, uint16_t len);

/**
 * @brief Transmite e recebe dados via SPI (DMA)
 * @param ll Contexto
 * @param len Tamanho total (incluindo 3 bytes de controle)
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_TransmitReceive(W5500_LL_t *ll, uint16_t len);

/**
 * @brief Escreve um único byte em registrador comum
 * @param ll Contexto
 * @param addr Endereço do registrador
 * @param value Valor a escrever
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_WriteCommonReg(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t value
);

/**
 * @brief Lê um único byte de registrador comum
 * @param ll Contexto
 * @param addr Endereço do registrador
 * @param value [out] Valor lido
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_ReadCommonReg(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t *value
);

/**
 * @brief Escreve múltiplos bytes em registrador comum
 * @param ll Contexto
 * @param addr Endereço inicial
 * @param data Dados a escrever
 * @param len Quantidade de bytes
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_WriteCommonRegBurst(
    W5500_LL_t *ll,
    uint16_t addr,
    const uint8_t *data,
    uint16_t len
);

/**
 * @brief Lê múltiplos bytes de registrador comum
 * @param ll Contexto
 * @param addr Endereço inicial
 * @param data [out] Buffer para armazenar dados lidos
 * @param len Quantidade de bytes
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_ReadCommonRegBurst(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t *data,
    uint16_t len
);

/**
 * @brief Escreve registrador de socket
 * @param ll Contexto
 * @param socket_id ID do socket (0-7)
 * @param addr Endereço do registrador
 * @param value Valor a escrever
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_WriteSocketReg(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    uint8_t value
);

/**
 * @brief Lê registrador de socket
 * @param ll Contexto
 * @param socket_id ID do socket (0-7)
 * @param addr Endereço do registrador
 * @param value [out] Valor lido
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_ReadSocketReg(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    uint8_t *value
);

/**
 * @brief Escreve múltiplos bytes em registrador de socket
 */
HAL_StatusTypeDef W5500_LL_WriteSocketRegBurst(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    const uint8_t *data,
    uint16_t len
);

/**
 * @brief Lê múltiplos bytes de registrador de socket
 */
HAL_StatusTypeDef W5500_LL_ReadSocketRegBurst(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    uint8_t *data,
    uint16_t len
);

/**
 * @brief Escreve dados no buffer TX do socket
 * @param ll Contexto
 * @param socket_id ID do socket
 * @param offset Offset no buffer TX (circular)
 * @param data Dados a escrever
 * @param len Quantidade de bytes
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_WriteSocketTxBuf(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t offset,
    const uint8_t *data,
    uint16_t len
);

/**
 * @brief Lê dados do buffer RX do socket
 * @param ll Contexto
 * @param socket_id ID do socket
 * @param offset Offset no buffer RX (circular)
 * @param data [out] Buffer para armazenar dados
 * @param len Quantidade de bytes
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_LL_ReadSocketRxBuf(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t offset,
    uint8_t *data,
    uint16_t len
);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_DRIVER_W5500_W5500_LL_H_ */

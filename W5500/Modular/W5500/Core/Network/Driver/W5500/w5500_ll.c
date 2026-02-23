/*
 * w5500_ll.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação de low-level SPI com DMA, mutex e semáforos
 */

#include "w5500_ll.h"
#include <string.h>

/* =============================================================================
 * FUNÇÕES DE INICIALIZAÇÃO
 * ============================================================================= */

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
) {
    if (!ll || !hspi || !tx_buf || !rx_buf) {
        return HAL_ERROR;
    }
    
    ll->hspi = hspi;
    ll->cs_port = cs_port;
    ll->cs_pin = cs_pin;
    ll->tx_buf = tx_buf;
    ll->rx_buf = rx_buf;
    ll->buf_size = buf_size;
    ll->spi_mutex = spi_mutex;
    ll->spi_done_sem = spi_done_sem;
    
    // CS inicia em HIGH (idle)
    HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
    
    return HAL_OK;
}

/* =============================================================================
 * FUNÇÕES DE CONTROLE DE ENDEREÇAMENTO
 * ============================================================================= */

void W5500_LL_SetAddressPhase(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t block,
    uint8_t rw
) {
    ll->tx_buf[0] = (addr >> 8) & 0xFF;      // Address MSB
    ll->tx_buf[1] = addr & 0xFF;             // Address LSB
    ll->tx_buf[2] = block | rw | W5500_OPCODE_VDM;  // Control byte
}

/* =============================================================================
 * FUNÇÕES DE TRANSMISSÃO/RECEPÇÃO SPI COM DMA
 * ============================================================================= */

HAL_StatusTypeDef W5500_LL_Transmit(W5500_LL_t *ll, uint16_t len) {
    
    if (len > ll->buf_size) {
        return HAL_ERROR;
    }
    
    // Adquirir mutex do barramento SPI
    if (osMutexAcquire(ll->spi_mutex, osWaitForever) != osOK) {
        return HAL_ERROR;
    }
    
    // Assert CS (ativo baixo)
    HAL_GPIO_WritePin(ll->cs_port, ll->cs_pin, GPIO_PIN_RESET);
    
    // Transmitir via DMA
    HAL_StatusTypeDef ret = HAL_SPI_Transmit_DMA(ll->hspi, ll->tx_buf, len);
    
    if (ret == HAL_OK) {
        // Aguardar conclusão (semáforo será liberado no callback de DMA)
        if (osSemaphoreAcquire(ll->spi_done_sem, pdMS_TO_TICKS(1000)) != osOK) {
            ret = HAL_TIMEOUT;
        }
    }
    
    // Deassert CS
    HAL_GPIO_WritePin(ll->cs_port, ll->cs_pin, GPIO_PIN_SET);
    
    // Liberar mutex
    osMutexRelease(ll->spi_mutex);
    
    return ret;
}

HAL_StatusTypeDef W5500_LL_TransmitReceive(W5500_LL_t *ll, uint16_t len) {
    
    if (len > ll->buf_size) {
        return HAL_ERROR;
    }
    
    // Adquirir mutex do barramento SPI
    if (osMutexAcquire(ll->spi_mutex, osWaitForever) != osOK) {
        return HAL_ERROR;
    }
    
    // Assert CS
    HAL_GPIO_WritePin(ll->cs_port, ll->cs_pin, GPIO_PIN_RESET);
    
    // Transmitir/Receber via DMA
    HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive_DMA(
        ll->hspi,
        ll->tx_buf,
        ll->rx_buf,
        len
    );
    
    if (ret == HAL_OK) {
        // Aguardar conclusão
        if (osSemaphoreAcquire(ll->spi_done_sem, pdMS_TO_TICKS(1000)) != osOK) {
            ret = HAL_TIMEOUT;
        }
    }
    
    // Deassert CS
    HAL_GPIO_WritePin(ll->cs_port, ll->cs_pin, GPIO_PIN_SET);
    
    // Liberar mutex
    osMutexRelease(ll->spi_mutex);
    
    return ret;
}

/* =============================================================================
 * FUNÇÕES DE ACESSO A REGISTRADORES COMUNS
 * ============================================================================= */

HAL_StatusTypeDef W5500_LL_WriteCommonReg(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t value
) {
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_COMMON, W5500_RWB_WRITE);
    ll->tx_buf[3] = value;
    
    return W5500_LL_Transmit(ll, 4);
}

HAL_StatusTypeDef W5500_LL_ReadCommonReg(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t *value
) {
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_COMMON, W5500_RWB_READ);
    ll->tx_buf[3] = 0x00;  // Dummy byte
    
    HAL_StatusTypeDef ret = W5500_LL_TransmitReceive(ll, 4);
    
    if (ret == HAL_OK) {
        *value = ll->rx_buf[3];
    }
    
    return ret;
}

HAL_StatusTypeDef W5500_LL_WriteCommonRegBurst(
    W5500_LL_t *ll,
    uint16_t addr,
    const uint8_t *data,
    uint16_t len
) {
    if (len > (ll->buf_size - 3)) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_COMMON, W5500_RWB_WRITE);
    memcpy(&ll->tx_buf[3], data, len);
    
    return W5500_LL_Transmit(ll, 3 + len);
}

HAL_StatusTypeDef W5500_LL_ReadCommonRegBurst(
    W5500_LL_t *ll,
    uint16_t addr,
    uint8_t *data,
    uint16_t len
) {
    if (len > (ll->buf_size - 3)) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_COMMON, W5500_RWB_READ);
    memset(&ll->tx_buf[3], 0, len);  // Dummy bytes
    
    HAL_StatusTypeDef ret = W5500_LL_TransmitReceive(ll, 3 + len);
    
    if (ret == HAL_OK) {
        memcpy(data, &ll->rx_buf[3], len);
    }
    
    return ret;
}

/* =============================================================================
 * FUNÇÕES DE ACESSO A REGISTRADORES DE SOCKET
 * ============================================================================= */

HAL_StatusTypeDef W5500_LL_WriteSocketReg(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    uint8_t value
) {
    if (socket_id >= 8) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_SOCKET_REG(socket_id), W5500_RWB_WRITE);
    ll->tx_buf[3] = value;
    
    return W5500_LL_Transmit(ll, 4);
}

HAL_StatusTypeDef W5500_LL_ReadSocketReg(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    uint8_t *value
) {
    if (socket_id >= 8) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_SOCKET_REG(socket_id), W5500_RWB_READ);
    ll->tx_buf[3] = 0x00;
    
    HAL_StatusTypeDef ret = W5500_LL_TransmitReceive(ll, 4);
    
    if (ret == HAL_OK) {
        *value = ll->rx_buf[3];
    }
    
    return ret;
}

HAL_StatusTypeDef W5500_LL_WriteSocketRegBurst(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    const uint8_t *data,
    uint16_t len
) {
    if (socket_id >= 8 || len > (ll->buf_size - 3)) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_SOCKET_REG(socket_id), W5500_RWB_WRITE);
    memcpy(&ll->tx_buf[3], data, len);
    
    return W5500_LL_Transmit(ll, 3 + len);
}

HAL_StatusTypeDef W5500_LL_ReadSocketRegBurst(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t addr,
    uint8_t *data,
    uint16_t len
) {
    if (socket_id >= 8 || len > (ll->buf_size - 3)) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, addr, W5500_BSB_SOCKET_REG(socket_id), W5500_RWB_READ);
    memset(&ll->tx_buf[3], 0, len);
    
    HAL_StatusTypeDef ret = W5500_LL_TransmitReceive(ll, 3 + len);
    
    if (ret == HAL_OK) {
        memcpy(data, &ll->rx_buf[3], len);
    }
    
    return ret;
}

/* =============================================================================
 * FUNÇÕES DE ACESSO A BUFFERS TX/RX DOS SOCKETS
 * ============================================================================= */

HAL_StatusTypeDef W5500_LL_WriteSocketTxBuf(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t offset,
    const uint8_t *data,
    uint16_t len
) {
    if (socket_id >= 8 || len > (ll->buf_size - 3)) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, offset, W5500_BSB_SOCKET_TX(socket_id), W5500_RWB_WRITE);
    memcpy(&ll->tx_buf[3], data, len);
    
    return W5500_LL_Transmit(ll, 3 + len);
}

HAL_StatusTypeDef W5500_LL_ReadSocketRxBuf(
    W5500_LL_t *ll,
    uint8_t socket_id,
    uint16_t offset,
    uint8_t *data,
    uint16_t len
) {
    if (socket_id >= 8 || len > (ll->buf_size - 3)) {
        return HAL_ERROR;
    }
    
    W5500_LL_SetAddressPhase(ll, offset, W5500_BSB_SOCKET_RX(socket_id), W5500_RWB_READ);
    memset(&ll->tx_buf[3], 0, len);
    
    HAL_StatusTypeDef ret = W5500_LL_TransmitReceive(ll, 3 + len);
    
    if (ret == HAL_OK) {
        memcpy(data, &ll->rx_buf[3], len);
    }
    
    return ret;
}

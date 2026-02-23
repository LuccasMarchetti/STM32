/*
 * w5500_hw.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação do driver de hardware W5500
 */

#include "w5500_hw.h"
#include <string.h>
#include <stdio.h>

/* =============================================================================
 * FUNÇÕES PRIVADAS (HELPERS)
 * ============================================================================= */

/**
 * @brief Aguarda socket sair do estado CLOSED após comando OPEN
 */
static HAL_StatusTypeDef WaitSocketOpen(W5500_HW_t *hw, uint8_t socket_id, uint32_t timeout_ms) {
    uint8_t status;
    uint32_t start = osKernelGetTickCount();
    
    do {
        if (W5500_HW_SocketGetStatus(hw, socket_id, &status) != HAL_OK) {
            return HAL_ERROR;
        }
        
        if (status != W5500_Sn_SR_CLOSED) {
            return HAL_OK;  // Socket aberto
        }
        
        if ((osKernelGetTickCount() - start) > pdMS_TO_TICKS(timeout_ms)) {
            return HAL_TIMEOUT;
        }
        
        osThreadYield();
        
    } while (1);
}

/* =============================================================================
 * INICIALIZAÇÃO E RESET
 * ============================================================================= */

HAL_StatusTypeDef W5500_HW_Init(W5500_HW_t *hw, const W5500_HW_Config_t *config) {
    
    if (!hw || !config || !config->hspi) {
        return HAL_ERROR;
    }
    
    memset(hw, 0, sizeof(W5500_HW_t));
    memcpy(&hw->config, config, sizeof(W5500_HW_Config_t));
    
    // Criar semáforo para DMA
    const osSemaphoreAttr_t sem_attr = {
        .name = "W5500_SPI_Done",
        .cb_mem = &hw->spi_done_sem_cb,
        .cb_size = sizeof(hw->spi_done_sem_cb),
    };
    
    hw->spi_done_sem = osSemaphoreNew(1, 0, &sem_attr);
    if (!hw->spi_done_sem) {
        return HAL_ERROR;
    }
    
    // Inicializar camada de baixo nível  
    HAL_StatusTypeDef ret = W5500_LL_Init(
        &hw->ll,
        config->hspi,
        config->cs_port,
        config->cs_pin,
        hw->tx_buf,
        hw->rx_buf,
        sizeof(hw->tx_buf),
        config->spi_mutex,
        hw->spi_done_sem
    );
    
    if (ret != HAL_OK) {
        return ret;
    }
    
    // Reset de hardware (se pino configurado)
    if (config->rst_port != NULL) {
        W5500_HW_Reset(hw);
    } else {
        // Reset via software
        W5500_HW_SoftReset(hw);
    }
    
    osDelay(200);  // Aguardar PLL estabilizar
    
    // Verificar versão do chip
    uint8_t version;
    ret = W5500_LL_ReadCommonReg(&hw->ll, W5500_COMMON_VERSIONR, &version);
    
    if (ret != HAL_OK || version != W5500_COMMON_VERSIONR_VALUE) {
        return HAL_ERROR;
    }
    
    hw->version = version;
    
    // Gerar MAC baseado em UID do STM32
    W5500_HW_GenerateMAC(hw);
    W5500_HW_SetMAC(hw, hw->mac);
    
    // Configurar PHY (auto-negotiation)
    uint8_t phycfg = W5500_COMMON_PHYCFGR_RST 
                   | W5500_COMMON_PHYCFGR_OPMD_OPMDC
                   | W5500_COMMON_PHYCFGR_ALL_CAPABLE_AUTO_NEGOTIATION;
    
    W5500_HW_ConfigurePHY(hw, phycfg);
    
    osDelay(100);
    
    // Habilitar interrupções globais (opcional - depende se você vai usar INT pin)
    // IMR: habilita interrupções de CONFLICT, UNREACH, PPPoE, MP
    uint8_t imr = 0x00;  // Desabilitado por padrão (usar polling)
    // Se quiser habilitar: imr = W5500_COMMON_IMR_CONFLICT | W5500_COMMON_IMR_UNREACH;
    W5500_LL_WriteCommonReg(&hw->ll, W5500_COMMON_IMR, imr);
    
    // SIMR: habilita interrupções para cada socket (bit 0-7)
    uint8_t simr = 0xFF;  // Habilitar interrupções de todos os 8 sockets
    W5500_LL_WriteCommonReg(&hw->ll, W5500_COMMON_SIMR, simr);
    
    // Inicializar todos os sockets como livres
    for (int i = 0; i < W5500_MAX_SOCKETS; i++) {
        hw->sockets[i].id = i;
        hw->sockets[i].state = W5500_SOCKET_FREE;
        hw->sockets[i].tx_buf_size = 2048;
        hw->sockets[i].tx_mask = 2047;
        hw->sockets[i].rx_buf_size = 2048;
        hw->sockets[i].rx_mask = 2047;
        
        // Habilitar interrupções do socket (RECV, DISCON, CON, TIMEOUT, SEND_OK)
        uint8_t sn_imr = W5500_Sn_IMR_RECV 
                       | W5500_Sn_IMR_DISCON 
                       | W5500_Sn_IMR_CON 
                       | W5500_Sn_IMR_TIMEOUT 
                       | W5500_Sn_IMR_SENDOK;
        W5500_LL_WriteSocketReg(&hw->ll, i, W5500_Sn_IMR, sn_imr);
    }
    
    hw->initialized = true;
    
    return HAL_OK;
}

HAL_StatusTypeDef W5500_HW_Reset(W5500_HW_t *hw) {
    
    if (hw->config.rst_port == NULL) {
        return W5500_HW_SoftReset(hw);
    }
    
    // Pulso de reset (ativo baixo)
    HAL_GPIO_WritePin(hw->config.rst_port, hw->config.rst_pin, GPIO_PIN_RESET);
    osDelay(50);
    HAL_GPIO_WritePin(hw->config.rst_port, hw->config.rst_pin, GPIO_PIN_SET);
    osDelay(200);
    
    return HAL_OK;
}

HAL_StatusTypeDef W5500_HW_SoftReset(W5500_HW_t *hw) {
    return W5500_LL_WriteCommonReg(&hw->ll, W5500_COMMON_MR, W5500_COMMON_MR_RST);
}

/* =============================================================================
 * CONFIGURAÇÃO DE REDE
 * ============================================================================= */

void W5500_HW_GenerateMAC(W5500_HW_t *hw) {
    // Obter UID único do STM32
    uint32_t uid[3];
    uid[0] = HAL_GetUIDw0();
    uid[1] = HAL_GetUIDw1();
    uid[2] = HAL_GetUIDw2();
    
    // Gerar hash
    uint32_t hash = uid[0] ^ uid[1] ^ uid[2];
    
    // Montar MAC (locally administered, unicast)
    hw->mac[0] = 0x02;
    hw->mac[1] = uid[0] & 0xFF;
    hw->mac[2] = (hash >> 24) & 0xFF;
    hw->mac[3] = (hash >> 16) & 0xFF;
    hw->mac[4] = (hash >> 8) & 0xFF;
    hw->mac[5] = hash & 0xFF;
}

HAL_StatusTypeDef W5500_HW_SetMAC(W5500_HW_t *hw, const uint8_t *mac) {
    memcpy(hw->mac, mac, 6);
    return W5500_LL_WriteCommonRegBurst(&hw->ll, W5500_COMMON_SHAR, mac, 6);
}

HAL_StatusTypeDef W5500_HW_GetMAC(W5500_HW_t *hw, uint8_t *mac) {
    return W5500_LL_ReadCommonRegBurst(&hw->ll, W5500_COMMON_SHAR, mac, 6);
}

HAL_StatusTypeDef W5500_HW_SetIP(W5500_HW_t *hw, const uint8_t *ip) {
    memcpy(hw->ip, ip, 4);
    return W5500_LL_WriteCommonRegBurst(&hw->ll, W5500_COMMON_SIPR, ip, 4);
}

HAL_StatusTypeDef W5500_HW_GetIP(W5500_HW_t *hw, uint8_t *ip) {
    return W5500_LL_ReadCommonRegBurst(&hw->ll, W5500_COMMON_SIPR, ip, 4);
}

HAL_StatusTypeDef W5500_HW_SetSubnet(W5500_HW_t *hw, const uint8_t *subnet) {
    memcpy(hw->subnet, subnet, 4);
    return W5500_LL_WriteCommonRegBurst(&hw->ll, W5500_COMMON_SUBR, subnet, 4);
}

HAL_StatusTypeDef W5500_HW_GetSubnet(W5500_HW_t *hw, uint8_t *subnet) {
    return W5500_LL_ReadCommonRegBurst(&hw->ll, W5500_COMMON_SUBR, subnet, 4);
}

HAL_StatusTypeDef W5500_HW_SetGateway(W5500_HW_t *hw, const uint8_t *gateway) {
    memcpy(hw->gateway, gateway, 4);
    return W5500_LL_WriteCommonRegBurst(&hw->ll, W5500_COMMON_GAR, gateway, 4);
}

HAL_StatusTypeDef W5500_HW_GetGateway(W5500_HW_t *hw, uint8_t *gateway) {
    return W5500_LL_ReadCommonRegBurst(&hw->ll, W5500_COMMON_GAR, gateway, 4);
}

/* =============================================================================
 * CONTROLE DE PHY
 * ============================================================================= */

HAL_StatusTypeDef W5500_HW_GetLinkStatus(W5500_HW_t *hw, bool *link_up) {
    uint8_t phycfg;
    HAL_StatusTypeDef ret = W5500_LL_ReadCommonReg(&hw->ll, W5500_COMMON_PHYCFGR, &phycfg);
    
    if (ret == HAL_OK) {
        *link_up = (phycfg & W5500_COMMON_PHYCFGR_LNK_ON) != 0;
        hw->link_up = *link_up;
    }
    
    return ret;
}

HAL_StatusTypeDef W5500_HW_ConfigurePHY(W5500_HW_t *hw, uint8_t phycfg) {
    return W5500_LL_WriteCommonReg(&hw->ll, W5500_COMMON_PHYCFGR, phycfg);
}

/* =============================================================================
 * GERENCIAMENTO DE SOCKETS
 * ============================================================================= */

uint8_t W5500_HW_SocketOpen(W5500_HW_t *hw, uint8_t protocol, uint16_t port) {
    
    uint8_t socket_id = 0xFF;
    
    // MACRAW só pode usar socket 0
    if ((protocol & 0x0F) == W5500_Sn_MR_MACRAW) {
        if (hw->allocated_sockets & (1 << 0)) {
            return 0xFF;  // Socket 0 já alocado
        }
        socket_id = 0;
    } else {
        // Buscar socket livre (começar do fim)
        for (int i = W5500_MAX_SOCKETS - 1; i >= 0; i--) {
            if ((hw->allocated_sockets & (1 << i)) == 0) {
                socket_id = i;
                break;
            }
        }
    }
    
    if (socket_id == 0xFF) {
        return 0xFF;  // Nenhum socket disponível
    }
    
    // Marcar como alocado
    hw->allocated_sockets |= (1 << socket_id);
    hw->sockets[socket_id].state = W5500_SOCKET_ALLOCATED;
    hw->sockets[socket_id].protocol = protocol;
    hw->sockets[socket_id].local_port = port;
    
    // Garantir que socket está fechado
    W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_CLOSE);
    osDelay(10);
    
    // Configurar modo
    W5500_LL_WriteSocketReg(&hw->ll, socket_id, W5500_Sn_MR, protocol);
    
    printf("[W5500_HW] Socket %u aberto com protocol=0x%02x\n", socket_id, protocol);
    
    // Configurar porta
    uint8_t port_bytes[2] = { (port >> 8) & 0xFF, port & 0xFF };
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_PORT, port_bytes, 2);
    
    // Enviar comando OPEN
    W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_OPEN);
    
    // Aguardar socket abrir
    if (WaitSocketOpen(hw, socket_id, 500) != HAL_OK) {
        // Falhou
        hw->allocated_sockets &= ~(1 << socket_id);
        hw->sockets[socket_id].state = W5500_SOCKET_FREE;
        W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_CLOSE);
        return 0xFF;
    }
    
    hw->sockets[socket_id].state = W5500_SOCKET_OPEN;
    
    printf("[W5500_HW] Socket %u aberto com sucesso\n", socket_id);
    
    return socket_id;
}

HAL_StatusTypeDef W5500_HW_SocketClose(W5500_HW_t *hw, uint8_t socket_id) {
    
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    if ((hw->allocated_sockets & (1 << socket_id)) == 0) {
        return HAL_ERROR;  // Socket não está alocado
    }
    
    // Enviar comando CLOSE
    HAL_StatusTypeDef ret = W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_CLOSE);
    
    // Marcar como livre
    hw->allocated_sockets &= ~(1 << socket_id);
    hw->sockets[socket_id].state = W5500_SOCKET_FREE;
    
    return ret;
}

HAL_StatusTypeDef W5500_HW_SocketGetStatus(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *status
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    return W5500_LL_ReadSocketReg(&hw->ll, socket_id, W5500_Sn_SR, status);
}

HAL_StatusTypeDef W5500_HW_SocketCommand(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t command
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    return W5500_LL_WriteSocketReg(&hw->ll, socket_id, W5500_Sn_CR, command);
}

/* =============================================================================
 * OPERAÇÕES TCP
 * ============================================================================= */

HAL_StatusTypeDef W5500_HW_SocketListen(W5500_HW_t *hw, uint8_t socket_id) {
    return W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_LISTEN);
}

HAL_StatusTypeDef W5500_HW_SocketConnect(
    W5500_HW_t *hw,
    uint8_t socket_id,
    const uint8_t *remote_ip,
    uint16_t remote_port
) {
    // Definir IP de destino
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_DIPR, remote_ip, 4);
    
    // Definir porta de destino
    uint8_t port_bytes[2] = { (remote_port >> 8) & 0xFF, remote_port & 0xFF };
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_DPORT, port_bytes, 2);
    
    // Enviar comando CONNECT
    return W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_CONNECT);
}

HAL_StatusTypeDef W5500_HW_SocketDisconnect(W5500_HW_t *hw, uint8_t socket_id) {
    return W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_DISCON);
}

/* =============================================================================
 * TRANSMISSÃO E RECEPÇÃO
 * ============================================================================= */

HAL_StatusTypeDef W5500_HW_SocketSend(
    W5500_HW_t *hw,
    uint8_t socket_id,
    const uint8_t *data,
    uint16_t len
) {
    if (socket_id >= W5500_MAX_SOCKETS || len == 0) {
        return HAL_ERROR;
    }
    
    W5500_Socket_t *sock = &hw->sockets[socket_id];
    
    // Verificar espaço livre
    uint16_t free_size;
    if (W5500_HW_SocketGetTxFree(hw, socket_id, &free_size) != HAL_OK) {
        return HAL_ERROR;
    }
    
    if (free_size < len) {
        return HAL_ERROR;  // Buffer cheio
    }
    
    // Ler ponteiro de escrita
    uint8_t ptr_bytes[2];
    W5500_LL_ReadSocketRegBurst(&hw->ll, socket_id, W5500_Sn_TX_WR, ptr_bytes, 2);
    uint16_t tx_ptr = (ptr_bytes[0] << 8) | ptr_bytes[1];
    
    // Escrever dados no buffer TX (circular)
    uint16_t offset = tx_ptr & sock->tx_mask;
    W5500_LL_WriteSocketTxBuf(&hw->ll, socket_id, offset, data, len);
    
    // Atualizar ponteiro de escrita
    tx_ptr += len;
    ptr_bytes[0] = (tx_ptr >> 8) & 0xFF;
    ptr_bytes[1] = tx_ptr & 0xFF;
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_TX_WR, ptr_bytes, 2);
    
    // Enviar comando SEND
    printf("[W5500_HW] Socket %u enviando %u bytes (TX_PTR=0x%04x)\n", socket_id, len, tx_ptr);
    HAL_StatusTypeDef ret = W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_SEND);
    if (ret == HAL_OK) {
        printf("[W5500_HW]   SEND_OK\n");
    } else {
        printf("[W5500_HW]   SEND_FAILED\n");
    }
    return ret;
}

HAL_StatusTypeDef W5500_HW_SocketRecv(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *buffer,
    uint16_t max_len,
    uint16_t *received
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    W5500_Socket_t *sock = &hw->sockets[socket_id];
    
    // Verificar dados disponíveis
    uint16_t rx_size;
    if (W5500_HW_SocketGetRxSize(hw, socket_id, &rx_size) != HAL_OK) {
        return HAL_ERROR;
    }
    
    if (rx_size == 0) {
        *received = 0;
        return HAL_OK;
    }
    
    // Limitar ao tamanho do buffer
    uint16_t to_read = (rx_size < max_len) ? rx_size : max_len;
    
    // Ler ponteiro de leitura
    uint8_t ptr_bytes[2];
    W5500_LL_ReadSocketRegBurst(&hw->ll, socket_id, W5500_Sn_RX_RD, ptr_bytes, 2);
    uint16_t rx_ptr = (ptr_bytes[0] << 8) | ptr_bytes[1];
    
    // Ler dados do buffer RX (circular)
    uint16_t offset = rx_ptr & sock->rx_mask;
    W5500_LL_ReadSocketRxBuf(&hw->ll, socket_id, offset, buffer, to_read);
    
    // Atualizar ponteiro de leitura
    rx_ptr += to_read;
    ptr_bytes[0] = (rx_ptr >> 8) & 0xFF;
    ptr_bytes[1] = rx_ptr & 0xFF;
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_RX_RD, ptr_bytes, 2);
    
    // Enviar comando RECV
    W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_RECV);
    
    *received = to_read;
    
    return HAL_OK;
}

HAL_StatusTypeDef W5500_HW_SocketGetRxSize(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint16_t *available
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    uint8_t size_bytes[2];
    HAL_StatusTypeDef ret = W5500_LL_ReadSocketRegBurst(
        &hw->ll,
        socket_id,
        W5500_Sn_RX_RSR,
        size_bytes,
        2
    );
    
    if (ret == HAL_OK) {
        *available = (size_bytes[0] << 8) | size_bytes[1];
    }
    
    return ret;
}

HAL_StatusTypeDef W5500_HW_SocketGetTxFree(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint16_t *free_size
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    uint8_t size_bytes[2];
    HAL_StatusTypeDef ret = W5500_LL_ReadSocketRegBurst(
        &hw->ll,
        socket_id,
        W5500_Sn_TX_FSR,
        size_bytes,
        2
    );
    
    if (ret == HAL_OK) {
        *free_size = (size_bytes[0] << 8) | size_bytes[1];
    }
    
    return ret;
}

/* =============================================================================
 * OPERAÇÕES UDP
 * ============================================================================= */

HAL_StatusTypeDef W5500_HW_SocketSetDest(
    W5500_HW_t *hw,
    uint8_t socket_id,
    const uint8_t *remote_ip,
    uint16_t remote_port
) {
    // Definir IP de destino
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_DIPR, remote_ip, 4);
    
    // Definir porta de destino
    uint8_t port_bytes[2] = { (remote_port >> 8) & 0xFF, remote_port & 0xFF };
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_DPORT, port_bytes, 2);
    
    return HAL_OK;
}

HAL_StatusTypeDef W5500_HW_SocketRecvFrom(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *buffer,
    uint16_t max_len,
    uint16_t *received,
    uint8_t *remote_ip,
    uint16_t *remote_port
) {
    // Para UDP, os primeiros 8 bytes do RX buffer contêm:
    // - 4 bytes: IP de origem
    // - 2 bytes: Porta de origem
    // - 2 bytes: Tamanho dos dados
    
    uint16_t rx_size;
    if (W5500_HW_SocketGetRxSize(hw, socket_id, &rx_size) != HAL_OK) {
        return HAL_ERROR;
    }
    
    if (rx_size < 8) {
        *received = 0;
        return HAL_OK;
    }
    
    W5500_Socket_t *sock = &hw->sockets[socket_id];
    
    // Ler ponteiro de leitura
    uint8_t ptr_bytes[2];
    W5500_LL_ReadSocketRegBurst(&hw->ll, socket_id, W5500_Sn_RX_RD, ptr_bytes, 2);
    uint16_t rx_ptr = (ptr_bytes[0] << 8) | ptr_bytes[1];
    
    // Ler cabeçalho UDP (8 bytes)
    uint8_t header[8];
    uint16_t offset = rx_ptr & sock->rx_mask;
    W5500_LL_ReadSocketRxBuf(&hw->ll, socket_id, offset, header, 8);
    
    // Extrair informações
    if (remote_ip) {
        memcpy(remote_ip, &header[0], 4);
    }
    
    if (remote_port) {
        *remote_port = (header[4] << 8) | header[5];
    }
    
    uint16_t data_len = (header[6] << 8) | header[7];
    
    // Limitar ao tamanho do buffer
    uint16_t to_read = (data_len < max_len) ? data_len : max_len;
    
    // Ler dados
    offset = (rx_ptr + 8) & sock->rx_mask;
    W5500_LL_ReadSocketRxBuf(&hw->ll, socket_id, offset, buffer, to_read);
    
    // Atualizar ponteiro de leitura (8 bytes header + data_len)
    rx_ptr += (8 + data_len);
    ptr_bytes[0] = (rx_ptr >> 8) & 0xFF;
    ptr_bytes[1] = rx_ptr & 0xFF;
    W5500_LL_WriteSocketRegBurst(&hw->ll, socket_id, W5500_Sn_RX_RD, ptr_bytes, 2);
    
    // Enviar comando RECV
    W5500_HW_SocketCommand(hw, socket_id, W5500_Sn_CR_RECV);
    
    *received = to_read;
    
    return HAL_OK;
}

/* =============================================================================
 * INTERRUPÇÕES
 * ============================================================================= */

HAL_StatusTypeDef W5500_HW_SocketGetIR(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *ir
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    return W5500_LL_ReadSocketReg(&hw->ll, socket_id, W5500_Sn_IR, ir);
}

HAL_StatusTypeDef W5500_HW_SocketClearIR(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t ir
) {
    if (socket_id >= W5500_MAX_SOCKETS) {
        return HAL_ERROR;
    }
    
    return W5500_LL_WriteSocketReg(&hw->ll, socket_id, W5500_Sn_IR, ir);
}

HAL_StatusTypeDef W5500_HW_GetCommonIR(W5500_HW_t *hw, uint8_t *ir) {
    return W5500_LL_ReadCommonReg(&hw->ll, W5500_COMMON_IR, ir);
}

HAL_StatusTypeDef W5500_HW_ClearCommonIR(W5500_HW_t *hw, uint8_t ir) {
    return W5500_LL_WriteCommonReg(&hw->ll, W5500_COMMON_IR, ir);
}

HAL_StatusTypeDef W5500_HW_GetSocketIR(W5500_HW_t *hw, uint8_t *sir) {
    return W5500_LL_ReadCommonReg(&hw->ll, W5500_COMMON_SIR, sir);
}

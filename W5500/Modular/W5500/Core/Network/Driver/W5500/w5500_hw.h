/*
 * w5500_hw.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System  
 *
 * Driver de hardware do W5500 - camada intermediária
 * Usa w5500_ll para comunicação SPI
 * NÃO conhece protocolos de aplicação (DHCP, DNS, etc)
 */

#ifndef NETWORK_DRIVER_W5500_W5500_HW_H_
#define NETWORK_DRIVER_W5500_W5500_HW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "w5500_ll.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

#define W5500_MAX_SOCKETS    8

/* =============================================================================
 * ESTRUTURAS
 * ============================================================================= */

/**
 * @brief Configuração de inicialização do W5500
 */
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *rst_port;   // Reset (opcional)
    uint16_t rst_pin;
    uint8_t int_pin;          // Interrupt (opcional)
    
    osMutexId_t spi_mutex;
    
} W5500_HW_Config_t;

/**
 * @brief Estados de socket de hardware
 */
typedef enum {
    W5500_SOCKET_FREE = 0,
    W5500_SOCKET_ALLOCATED,
    W5500_SOCKET_OPEN,
    W5500_SOCKET_CONNECTED
} W5500_SocketState_t;

/**
 * @brief Informações de um socket de hardware
 */
typedef struct {
    uint8_t id;
    W5500_SocketState_t state;
    uint8_t protocol;          // W5500_Sn_MR_TCP, _UDP ou _MACRAW
    uint16_t local_port;
    
    // Buffers internos do W5500
    uint16_t tx_buf_size;      // Em bytes (padrão 2KB)
    uint16_t tx_mask;
    uint16_t rx_buf_size;
    uint16_t rx_mask;
    
} W5500_Socket_t;

/**
 * @brief Estrutura principal do driver W5500
 */
typedef struct W5500_HW {
    
    // Configuração
    W5500_HW_Config_t config;
    
    // Contexto de baixo nível
    W5500_LL_t ll;
    
    // Buffers SPI compartilhados
    uint8_t tx_buf[2048];
    uint8_t rx_buf[2048];
    
    // RTOS
    osSemaphoreId_t spi_done_sem;
    StaticSemaphore_t spi_done_sem_cb;
    
    // Configuração de rede
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
    
    // Version do chip (deve ser 0x04)
    uint8_t version;
    
    // Sockets de hardware
    W5500_Socket_t sockets[W5500_MAX_SOCKETS];
    uint8_t allocated_sockets;   // Bitmask de sockets alocados
    
    // Flags
    bool initialized;
    bool link_up;
    
} W5500_HW_t;

/* =============================================================================
 * FUNÇÕES DE INICIALIZAÇÃO E RESET
 * ============================================================================= */

/**
 * @brief Inicializa driver W5500
 * @param hw Estrutura do driver (alocada pelo usuário)
 * @param config Configuração
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_Init(W5500_HW_t *hw, const W5500_HW_Config_t *config);

/**
 * @brief Reset de hardware do W5500
 * @param hw Driver
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_Reset(W5500_HW_t *hw);

/**
 * @brief Reset via software
 * @param hw Driver
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SoftReset(W5500_HW_t *hw);

/* =============================================================================
 * CONFIGURAÇÃO DE REDE
 * ============================================================================= */

/**
 * @brief Define endereço MAC
 */
HAL_StatusTypeDef W5500_HW_SetMAC(W5500_HW_t *hw, const uint8_t *mac);

/**
 * @brief Lê endereço MAC
 */
HAL_StatusTypeDef W5500_HW_GetMAC(W5500_HW_t *hw, uint8_t *mac);

/**
 * @brief Define endereço IP
 */
HAL_StatusTypeDef W5500_HW_SetIP(W5500_HW_t *hw, const uint8_t *ip);

/**
 * @brief Lê endereço IP
 */
HAL_StatusTypeDef W5500_HW_GetIP(W5500_HW_t *hw, uint8_t *ip);

/**
 * @brief Define máscara de sub-rede
 */
HAL_StatusTypeDef W5500_HW_SetSubnet(W5500_HW_t *hw, const uint8_t *subnet);

/**
 * @brief Lê máscara de sub-rede
 */
HAL_StatusTypeDef W5500_HW_GetSubnet(W5500_HW_t *hw, uint8_t *subnet);

/**
 * @brief Define gateway
 */
HAL_StatusTypeDef W5500_HW_SetGateway(W5500_HW_t *hw, const uint8_t *gateway);

/**
 * @brief Lê gateway
 */
HAL_StatusTypeDef W5500_HW_GetGateway(W5500_HW_t *hw, uint8_t *gateway);

/**
 * @brief Gera MAC address baseado em UID do STM32
 */
void W5500_HW_GenerateMAC(W5500_HW_t *hw);

/* =============================================================================
 * CONTROLE DE PHY
 * ============================================================================= */

/**
 * @brief Verifica status do link físico
 * @param hw Driver
 * @param link_up [out] true se link conectado
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_GetLinkStatus(W5500_HW_t *hw, bool *link_up);

/**
 * @brief Configura PHY (auto-negotiation, velocidade, etc)
 * @param hw Driver
 * @param config Configuração do PHY
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_ConfigurePHY(W5500_HW_t *hw, uint8_t phycfg);

/* =============================================================================
 * GERENCIAMENTO DE SOCKETS
 * ============================================================================= */

/**
 * @brief Abre um socket de hardware
 * @param hw Driver
 * @param protocol W5500_Sn_MR_TCP, _UDP ou _MACRAW
 * @param port Porta local
 * @return ID do socket (0-7) ou 0xFF se falha
 */
uint8_t W5500_HW_SocketOpen(W5500_HW_t *hw, uint8_t protocol, uint16_t port);

/**
 * @brief Fecha socket
 * @param hw Driver
 * @param socket_id ID do socket (0-7)
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketClose(W5500_HW_t *hw, uint8_t socket_id);

/**
 * @brief Obtém estado do socket
 * @param hw Driver
 * @param socket_id ID do socket
 * @param status [out] Estado (Sn_SR)
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketGetStatus(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *status
);

/**
 * @brief Envia comando para socket (OPEN, LISTEN, CONNECT, SEND, RECV, CLOSE, etc)
 * @param hw Driver
 * @param socket_id ID do socket
 * @param command Comando (W5500_Sn_CR_xxx)
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketCommand(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t command
);

/* =============================================================================
 * OPERAÇÕES TCP
 * ============================================================================= */

/**
 * @brief Coloca socket TCP em modo LISTEN (servidor)
 * @param hw Driver
 * @param socket_id ID do socket
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketListen(W5500_HW_t *hw, uint8_t socket_id);

/**
 * @brief Conecta socket TCP a servidor remoto (cliente)
 * @param hw Driver
 * @param socket_id ID do socket
 * @param remote_ip IP do servidor
 * @param remote_port Porta do servidor
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketConnect(
    W5500_HW_t *hw,
    uint8_t socket_id,
    const uint8_t *remote_ip,
    uint16_t remote_port
);

/**
 * @brief Desconecta socket TCP
 * @param hw Driver
 * @param socket_id ID do socket
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketDisconnect(W5500_HW_t *hw, uint8_t socket_id);

/* =============================================================================
 * TRANSMISSÃO E RECEPÇÃO
 * ============================================================================= */

/**
 * @brief Envia dados por socket
 * @param hw Driver
 * @param socket_id ID do socket
 * @param data Dados a enviar
 * @param len Tamanho dos dados
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketSend(
    W5500_HW_t *hw,
    uint8_t socket_id,
    const uint8_t *data,
    uint16_t len
);

/**
 * @brief Recebe dados de socket
 * @param hw Driver
 * @param socket_id ID do socket
 * @param buffer Buffer para armazenar dados
 * @param max_len Tamanho máximo do buffer
 * @param received [out] Bytes efetivamente recebidos
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketRecv(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *buffer,
    uint16_t max_len,
    uint16_t *received
);

/**
 * @brief Verifica quantidade de dados disponíveis para leitura
 * @param hw Driver
 * @param socket_id ID do socket
 * @param available [out] Bytes disponíveis
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketGetRxSize(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint16_t *available
);

/**
 * @brief Verifica espaço livre no buffer TX
 * @param hw Driver
 * @param socket_id ID do socket
 * @param free_size [out] Bytes livres
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketGetTxFree(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint16_t *free_size
);

/* =============================================================================
 * OPERAÇÕES UDP
 * ============================================================================= */

/**
 * @brief Define destino para socket UDP
 * @param hw Driver
 * @param socket_id ID do socket
 * @param remote_ip IP de destino
 * @param remote_port Porta de destino
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketSetDest(
    W5500_HW_t *hw,
    uint8_t socket_id,
    const uint8_t *remote_ip,
    uint16_t remote_port
);

/**
 * @brief Recebe datagrama UDP com informações de origem
 * @param hw Driver
 * @param socket_id ID do socket
 * @param buffer Buffer para dados
 * @param max_len Tamanho do buffer
 * @param received [out] Bytes recebidos
 * @param remote_ip [out] IP de origem
 * @param remote_port [out] Porta de origem
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketRecvFrom(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *buffer,
    uint16_t max_len,
    uint16_t *received,
    uint8_t *remote_ip,
    uint16_t *remote_port
);

/* =============================================================================
 * INTERRUPÇÕES
 * ============================================================================= */

/**
 * @brief Lê registrador de interrupção de socket
 * @param hw Driver
 * @param socket_id ID do socket
 * @param ir [out] Flags de interrupção
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketGetIR(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t *ir
);

/**
 * @brief Limpa flags de interrupção de socket
 * @param hw Driver
 * @param socket_id ID do socket
 * @param ir Flags a limpar (0xFF = todas)
 * @return HAL_OK ou HAL_ERROR
 */
HAL_StatusTypeDef W5500_HW_SocketClearIR(
    W5500_HW_t *hw,
    uint8_t socket_id,
    uint8_t ir
);

/**
 * @brief Lê registrador de interrupção comum
 */
HAL_StatusTypeDef W5500_HW_GetCommonIR(W5500_HW_t *hw, uint8_t *ir);

/**
 * @brief Limpa interrupção comum
 */
HAL_StatusTypeDef W5500_HW_ClearCommonIR(W5500_HW_t *hw, uint8_t ir);

/**
 * @brief Lê registrador de interrupção de sockets (SIR)
 */
HAL_StatusTypeDef W5500_HW_GetSocketIR(W5500_HW_t *hw, uint8_t *sir);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_DRIVER_W5500_W5500_HW_H_ */

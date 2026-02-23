/*
 * net_socket.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação da camada de abstração de sockets
 */

#include "net_socket.h"
#include "w5500_hw.h"
#include <string.h>
#include <stdio.h>

/* Pool de sockets (8 sockets hardware do W5500) */
static NetSocket_t socket_pool[NET_MAX_SOCKETS];
static W5500_HW_t *hw_driver = NULL;
static bool initialized = false;

/* =============================================================================
 * INICIALIZAÇÃO
 * ============================================================================= */

NetErr_t NetSocket_Init(W5500_HW_t *hw) {
    
    if (!hw) {
        return NET_ERR_PARAM;
    }
    
    if (initialized) {
        return NET_ERR_STATE;
    }
    
    hw_driver = hw;
    
    // Inicializar pool
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        socket_pool[i].id = i;
        socket_pool[i].hw_socket = 0xFF;
        socket_pool[i].type = NET_SOCKET_TYPE_NONE;
        socket_pool[i].protocol = 0;
        socket_pool[i].state = NET_SOCKET_STATE_CLOSED;
        socket_pool[i].local_port = 0;
        socket_pool[i].remote_port = 0;
        memset(&socket_pool[i].local_addr, 0, sizeof(NetAddr_t));
        memset(&socket_pool[i].remote_addr, 0, sizeof(NetAddr_t));
        socket_pool[i].rx_callback = NULL;
        socket_pool[i].user_data = NULL;
    }
    
    initialized = true;
    
    return NET_OK;
}

NetErr_t NetSocket_Deinit(void) {
    
    if (!initialized) {
        return NET_ERR_STATE;
    }
    
    // Fechar todos os sockets abertos
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (socket_pool[i].state != NET_SOCKET_STATE_CLOSED) {
            NetSocket_Close(&socket_pool[i]);
        }
    }
    
    hw_driver = NULL;
    initialized = false;
    
    return NET_OK;
}

/* =============================================================================
 * CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

NetSocket_t* NetSocket_Create(NetSocketType_t type, uint8_t protocol) {
    
    if (!initialized || !hw_driver) {
        return NULL;
    }
    
    // Buscar socket livre no pool
    NetSocket_t *socket = NULL;
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (socket_pool[i].state == NET_SOCKET_STATE_CLOSED && 
            socket_pool[i].hw_socket == 0xFF) {
            socket = &socket_pool[i];
            break;
        }
    }
    
    if (!socket) {
        return NULL;  // Pool esgotado
    }
    
    socket->type = type;
    socket->protocol = protocol;
    socket->state = NET_SOCKET_STATE_CREATED;
    
    return socket;
}

NetErr_t NetSocket_Destroy(NetSocket_t *socket) {
    
    if (!socket || !initialized) {
        return NET_ERR_PARAM;
    }
    
    // Fechar se estiver aberto
    if (socket->state != NET_SOCKET_STATE_CLOSED) {
        NetSocket_Close(socket);
    }
    
    // Limpar estrutura
    socket->hw_socket = 0xFF;
    socket->type = NET_SOCKET_TYPE_NONE;
    socket->protocol = 0;
    socket->state = NET_SOCKET_STATE_CLOSED;
    socket->rx_callback = NULL;
    socket->user_data = NULL;
    
    return NET_OK;
}

/* =============================================================================
 * OPERAÇÕES BÁSICAS
 * ============================================================================= */

NetErr_t NetSocket_Bind(NetSocket_t *socket, const NetAddr_t *addr) {
    
    if (!socket || !addr || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_CREATED) {
        return NET_ERR_STATE;
    }
    
    // Mapear tipo para protocolo W5500
    uint8_t w5500_mode;
    switch (socket->type) {
        case NET_SOCKET_TYPE_TCP:
            w5500_mode = W5500_Sn_MR_TCP;
            break;
        
        case NET_SOCKET_TYPE_UDP:
            w5500_mode = W5500_Sn_MR_UDP;
            break;
        
        case NET_SOCKET_TYPE_RAW:
            w5500_mode = W5500_Sn_MR_MACRAW;
            break;
        
        default:
            return NET_ERR_PARAM;
    }
    
    // Abrir socket de hardware
    uint8_t hw_socket = W5500_HW_SocketOpen(hw_driver, w5500_mode, addr->port);
    
    if (hw_socket == 0xFF) {
        return NET_ERR_RESOURCE;
    }
    
    socket->hw_socket = hw_socket;
    socket->local_port = addr->port;
    memcpy(&socket->local_addr, addr, sizeof(NetAddr_t));
    socket->state = NET_SOCKET_STATE_BOUND;
    
    return NET_OK;
}

NetErr_t NetSocket_Listen(NetSocket_t *socket, uint8_t backlog) {
    
    (void)backlog;  // W5500 não suporta fila de conexões
    
    if (!socket || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_BOUND) {
        return NET_ERR_STATE;
    }
    
    if (socket->type != NET_SOCKET_TYPE_TCP) {
        return NET_ERR_PARAM;
    }
    
    // Enviar comando LISTEN
    if (W5500_HW_SocketListen(hw_driver, socket->hw_socket) != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    socket->state = NET_SOCKET_STATE_LISTENING;
    
    return NET_OK;
}

NetErr_t NetSocket_Connect(NetSocket_t *socket, const NetAddr_t *remote_addr) {
    
    if (!socket || !remote_addr || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_BOUND) {
        return NET_ERR_STATE;
    }
    
    if (socket->type != NET_SOCKET_TYPE_TCP) {
        return NET_ERR_PARAM;
    }
    
    // Salvar endereço remoto
    memcpy(&socket->remote_addr, remote_addr, sizeof(NetAddr_t));
    socket->remote_port = remote_addr->port;
    
    // Enviar comando CONNECT
    HAL_StatusTypeDef ret = W5500_HW_SocketConnect(
        hw_driver,
        socket->hw_socket,
        remote_addr->ip.addr,
        remote_addr->port
    );
    
    if (ret != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    socket->state = NET_SOCKET_STATE_CONNECTING;
    
    return NET_OK;
}

NetErr_t NetSocket_Close(NetSocket_t *socket) {
    
    if (!socket || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->hw_socket != 0xFF) {
        W5500_HW_SocketClose(hw_driver, socket->hw_socket);
        socket->hw_socket = 0xFF;
    }
    
    socket->state = NET_SOCKET_STATE_CLOSED;
    
    return NET_OK;
}

/* =============================================================================
 * TRANSMISSÃO E RECEPÇÃO
 * ============================================================================= */

int32_t NetSocket_Send(
    NetSocket_t *socket,
    const uint8_t *data,
    uint16_t len,
    uint32_t timeout_ms
) {
    (void)timeout_ms;  // Não implementado nesta versão
    
    if (!socket || !data || len == 0 || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_CONNECTED) {
        return NET_ERR_STATE;
    }
    
    if (W5500_HW_SocketSend(hw_driver, socket->hw_socket, data, len) != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    return len;
}

int32_t NetSocket_Recv(
    NetSocket_t *socket,
    uint8_t *buffer,
    uint16_t max_len,
    uint32_t timeout_ms
) {
    (void)timeout_ms;  // Não implementado nesta versão
    
    if (!socket || !buffer || max_len == 0 || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_CONNECTED && 
        socket->state != NET_SOCKET_STATE_BOUND) {
        return NET_ERR_STATE;
    }
    
    uint16_t received = 0;
    HAL_StatusTypeDef ret = W5500_HW_SocketRecv(
        hw_driver,
        socket->hw_socket,
        buffer,
        max_len,
        &received
    );
    
    if (ret != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    return received;
}

int32_t NetSocket_SendTo(
    NetSocket_t *socket,
    const uint8_t *data,
    uint16_t len,
    const NetAddr_t *dest_addr,
    uint32_t timeout_ms
) {
    (void)timeout_ms;
    
    if (!socket || !data || !dest_addr || len == 0 || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->type != NET_SOCKET_TYPE_UDP) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_BOUND) {
        return NET_ERR_STATE;
    }
    
    // DEBUG: Ler status do socket antes de enviar
    uint8_t sn_sr = 0;
    W5500_HW_SocketGetStatus(hw_driver, socket->hw_socket, &sn_sr);
    // Definir destino
    if (W5500_HW_SocketSetDest(hw_driver, socket->hw_socket, dest_addr->ip.addr, dest_addr->port) != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    // Enviar dados
    if (W5500_HW_SocketSend(hw_driver, socket->hw_socket, data, len) != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    return len;
}

int32_t NetSocket_RecvFrom(
    NetSocket_t *socket,
    uint8_t *buffer,
    uint16_t max_len,
    NetAddr_t *src_addr,
    uint32_t timeout_ms
) {
    (void)timeout_ms;
    
    if (!socket || !buffer || max_len == 0 || !initialized || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (socket->state != NET_SOCKET_STATE_BOUND) {
        return NET_ERR_STATE;
    }
    
    uint8_t remote_ip[4];
    uint16_t remote_port;
    uint16_t received = 0;
    
    HAL_StatusTypeDef ret = W5500_HW_SocketRecvFrom(
        hw_driver,
        socket->hw_socket,
        buffer,
        max_len,
        &received,
        remote_ip,
        &remote_port
    );
    
    if (ret != HAL_OK) {
        return NET_ERR_DRIVER;
    }
    
    // Preencher endereço de origem se fornecido
    if (src_addr && received > 0) {
        memcpy(src_addr->ip.addr, remote_ip, 4);
        src_addr->port = remote_port;
    }
    
    return received;
}

/* =============================================================================
 * CONSULTAS
 * ============================================================================= */

NetSocketState_t NetSocket_GetState(const NetSocket_t *socket) {
    if (!socket) {
        return NET_SOCKET_STATE_CLOSED;
    }
    
    return socket->state;
}

bool NetSocket_IsConnected(const NetSocket_t *socket) {
    if (!socket) {
        return false;
    }
    
    return socket->state == NET_SOCKET_STATE_CONNECTED;
}

uint16_t NetSocket_GetLocalPort(const NetSocket_t *socket) {
    if (!socket) {
        return 0;
    }
    
    return socket->local_port;
}

NetErr_t NetSocket_GetRemoteAddr(const NetSocket_t *socket, NetAddr_t *addr) {
    if (!socket || !addr) {
        return NET_ERR_PARAM;
    }
    
    memcpy(addr, &socket->remote_addr, sizeof(NetAddr_t));
    
    return NET_OK;
}

uint16_t NetSocket_GetAvailable(const NetSocket_t *socket) {
    if (!socket || !initialized || !hw_driver) {
        return 0;
    }
    
    if (socket->hw_socket == 0xFF) {
        return 0;
    }
    
    uint16_t available = 0;
    W5500_HW_SocketGetRxSize(hw_driver, socket->hw_socket, &available);
    
    return available;
}

/* =============================================================================
 * CALLBACKS
 * ============================================================================= */

NetErr_t NetSocket_SetRxCallback(
    NetSocket_t *socket,
    NetSocketRxCallback_t callback,
    void *user_data
) {
    if (!socket) {
        return NET_ERR_PARAM;
    }
    
    socket->rx_callback = callback;
    socket->user_data = user_data;
    
    return NET_OK;
}

/* =============================================================================
 * PROCESSAMENTO (chamado pelo Network Manager)
 * ============================================================================= */

void NetSocket_ProcessAll(void) {
    
    if (!initialized || !hw_driver) {
        return;
    }
    
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        NetSocket_t *sock = &socket_pool[i];
        
        if (sock->hw_socket == 0xFF) {
            continue;  // Socket não alocado
        }
        
        // Ler status do hardware
        uint8_t hw_status;
        if (W5500_HW_SocketGetStatus(hw_driver, sock->hw_socket, &hw_status) != HAL_OK) {
            continue;
        }
        
        // Atualizar estado baseado no hardware
        switch (hw_status) {
            case W5500_Sn_SR_INIT:
                if (sock->state == NET_SOCKET_STATE_BOUND) {
                    // Permanece bound
                }
                break;
            
            case W5500_Sn_SR_LISTEN:
                sock->state = NET_SOCKET_STATE_LISTENING;
                break;
            
            case W5500_Sn_SR_ESTABLISHED:
                sock->state = NET_SOCKET_STATE_CONNECTED;
                break;
            
            case W5500_Sn_SR_CLOSE_WAIT:
                // Peer fechou conexão
                sock->state = NET_SOCKET_STATE_CLOSING;
                break;
            
            case W5500_Sn_SR_CLOSED:
                if (sock->state != NET_SOCKET_STATE_CLOSED) {
                    sock->state = NET_SOCKET_STATE_CLOSED;
                    sock->hw_socket = 0xFF;
                }
                break;
            
            case W5500_Sn_SR_UDP:
                sock->state = NET_SOCKET_STATE_BOUND;
                break;
            
            default:
                break;
        }
        
        // Verificar dados recebidos
        uint16_t available = NetSocket_GetAvailable(sock);
        
        if (available > 0 && sock->rx_callback) {
            sock->rx_callback(sock, sock->user_data);
        }
    }
}

/* =============================================================================
 * UTILITÁRIOS
 * ============================================================================= */

NetSocket_t* NetSocket_GetByIndex(uint8_t index) {
    if (index >= NET_MAX_SOCKETS) {
        return NULL;
    }
    
    return &socket_pool[index];
}

uint8_t NetSocket_GetCount(void) {
    uint8_t count = 0;
    
    for (int i = 0; i < NET_MAX_SOCKETS; i++) {
        if (socket_pool[i].hw_socket != 0xFF) {
            count++;
        }
    }
    
    return count;
}

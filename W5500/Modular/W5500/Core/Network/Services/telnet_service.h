/*
 * telnet_service.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Servidor Telnet (CLI remoto via TCP)
 */

#ifndef TELNET_SERVICE_H_
#define TELNET_SERVICE_H_

#include "net_types.h"
#include "net_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DEFINIÇÕES
 * ============================================================================= */

#define TELNET_PORT             23
#define TELNET_RX_BUFFER_SIZE   256
#define TELNET_TX_BUFFER_SIZE   512
#define TELNET_MAX_CMD_LEN      64
#define TELNET_MAX_ARGS         8

/* Estados do servidor Telnet */
typedef enum {
    TELNET_STATE_IDLE = 0,
    TELNET_STATE_LISTENING,
    TELNET_STATE_CONNECTED,
    TELNET_STATE_AUTHENTICATED
} Telnet_State_t;

/* Estrutura de comando */
typedef struct {
    char cmd[TELNET_MAX_CMD_LEN];
    char *args[TELNET_MAX_ARGS];
    uint8_t arg_count;
} Telnet_Command_t;

/* Callback de comando */
typedef void (*Telnet_CommandHandler_t)(
    const Telnet_Command_t *cmd,
    char *response,
    uint16_t max_len,
    void *user_data
);

/* Servidor Telnet */
typedef struct {
    NetService_t service;               // Interface de serviço
    NetManager_t *manager;              // Gerenciador de rede
    NetSocket_t *server_socket;         // Socket servidor
    NetSocket_t *client_socket;         // Socket do cliente conectado
    
    Telnet_State_t state;               // Estado atual
    uint16_t port;                      // Porta de escuta
    
    uint8_t rx_buffer[TELNET_RX_BUFFER_SIZE];
    uint16_t rx_pos;
    
    uint8_t tx_buffer[TELNET_TX_BUFFER_SIZE];
    
    char cmd_buffer[TELNET_MAX_CMD_LEN];
    uint8_t cmd_pos;
    
    Telnet_CommandHandler_t cmd_handler;
    void *user_data;
    
    bool echo_enabled;
    bool require_auth;
    bool authenticated;
    
} Telnet_Server_t;

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

/**
 * @brief Inicializa servidor Telnet
 * @param server Ponteiro para estrutura do servidor
 * @param port Porta de escuta (0 = usar padrão 23)
 * @param cmd_handler Função de tratamento de comandos
 * @param user_data Dados do usuário
 * @return NET_OK se sucesso
 */
NetErr_t Telnet_Init(
    Telnet_Server_t *server,
    uint16_t port,
    Telnet_CommandHandler_t cmd_handler,
    void *user_data
);

/**
 * @brief Inicia servidor (coloca em modo listen)
 * @param server Ponteiro para servidor
 * @return NET_OK se iniciado
 */
NetErr_t Telnet_Start(Telnet_Server_t *server);

/**
 * @brief Para servidor
 */
NetErr_t Telnet_Stop(Telnet_Server_t *server);

/**
 * @brief Envia mensagem para cliente conectado
 * @param server Ponteiro para servidor
 * @param message Mensagem a enviar
 * @return NET_OK se enviado
 */
NetErr_t Telnet_Send(Telnet_Server_t *server, const char *message);

/**
 * @brief Envia mensagem formatada (printf-like)
 */
NetErr_t Telnet_Printf(Telnet_Server_t *server, const char *format, ...);

/**
 * @brief Verifica se há cliente conectado
 */
bool Telnet_IsConnected(const Telnet_Server_t *server);

/**
 * @brief Desconecta cliente atual
 */
NetErr_t Telnet_Disconnect(Telnet_Server_t *server);

/**
 * @brief Obtém interface de serviço
 */
NetService_t* Telnet_GetService(Telnet_Server_t *server);

/* Comandos padrão (implementados internamente) */
void Telnet_RegisterDefaultCommands(void);

#ifdef __cplusplus
}
#endif

#endif /* TELNET_SERVICE_H_ */

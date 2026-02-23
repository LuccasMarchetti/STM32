/*
 * telnet_service.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação do servidor Telnet
 */

#include "telnet_service.h"
#include "net_socket.h"
#include "net_manager.h"
#include "cmsis_os.h"
#include "stm32g0xx.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Códigos ASCII */
#define ASCII_CR        '\r'
#define ASCII_LF        '\n'
#define ASCII_BS        0x08
#define ASCII_DEL       0x7F
#define ASCII_ESC       0x1B

/* Mensagens */
static const char *WELCOME_MSG = "\r\n*** STM32 Telnet Server ***\r\n\r\n";
static const char *PROMPT = "STM32> ";
static const char *CMD_NOT_FOUND = "Command not found. Type 'help' for available commands.\r\n";

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static void ParseCommand(const char *cmd_line, Telnet_Command_t *cmd) {
    
    memset(cmd, 0, sizeof(Telnet_Command_t));
    
    char *line_copy = (char *)cmd_line;
    char *token;
    uint8_t index = 0;
    
    // Primeiro token é o comando
    token = strtok(line_copy, " \t");
    if (token) {
        strncpy(cmd->cmd, token, TELNET_MAX_CMD_LEN - 1);
    }
    
    // Demais tokens são argumentos
    while ((token = strtok(NULL, " \t")) != NULL && index < TELNET_MAX_ARGS) {
        cmd->args[index++] = token;
    }
    
    cmd->arg_count = index;
}

static void SendPrompt(Telnet_Server_t *server) {
    if (server->client_socket && NetSocket_IsConnected(server->client_socket)) {
        NetSocket_Send(server->client_socket, (const uint8_t *)PROMPT, (uint16_t)strlen(PROMPT), 0);
    }
}

/* =============================================================================
 * COMANDOS PADRÃO
 * ============================================================================= */

static void DefaultCommandHandler(
    const Telnet_Command_t *cmd,
    char *response,
    uint16_t max_len,
    void *user_data
) {
    Telnet_Server_t *server = (Telnet_Server_t *)user_data;
    
    if (strcmp(cmd->cmd, "help") == 0) {
        snprintf(response, max_len,
            "Available commands:\r\n"
            "  help     - Show this help\r\n"
            "  status   - Show network status\r\n"
            "  uptime   - Show uptime\r\n"
            "  reset    - Reset system\r\n"
            "  exit     - Disconnect\r\n"
        );
    }
    else if (strcmp(cmd->cmd, "status") == 0) {
        NetConfig_t config;
        if (NetManager_GetNetworkConfig(server->manager, &config) == NET_OK) {
            snprintf(response, max_len,
                "Network Status:\r\n"
                "  IP:      %d.%d.%d.%d\r\n"
                "  Subnet:  %d.%d.%d.%d\r\n"
                "  Gateway: %d.%d.%d.%d\r\n"
                "  MAC:     %02X:%02X:%02X:%02X:%02X:%02X\r\n"
                "  Link:    %s\r\n",
                config.ip.addr[0], config.ip.addr[1], config.ip.addr[2], config.ip.addr[3],
                config.subnet.addr[0], config.subnet.addr[1], config.subnet.addr[2], config.subnet.addr[3],
                config.gateway.addr[0], config.gateway.addr[1], config.gateway.addr[2], config.gateway.addr[3],
                config.mac.addr[0], config.mac.addr[1], config.mac.addr[2], config.mac.addr[3], config.mac.addr[4], config.mac.addr[5],
                NetManager_IsLinkUp(server->manager) ? "UP" : "DOWN"
            );
        }
    }
    else if (strcmp(cmd->cmd, "uptime") == 0) {
        uint32_t uptime_ms = osKernelGetTickCount();
        uint32_t seconds = uptime_ms / 1000;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        
        snprintf(response, max_len,
            "Uptime: %lu:%02lu:%02lu\r\n",
            hours, minutes % 60, seconds % 60
        );
    }
    else if (strcmp(cmd->cmd, "exit") == 0) {
        snprintf(response, max_len, "Goodbye!\r\n");
        Telnet_Disconnect(server);
    }
    else if (strcmp(cmd->cmd, "reset") == 0) {
        snprintf(response, max_len, "Resetting system...\r\n");
        Telnet_Send(server, response);
        osDelay(100);
        NVIC_SystemReset();
    }
    else {
        strncpy(response, CMD_NOT_FOUND, max_len);
    }
}

/* =============================================================================
 * CALLBACK DE RECEPÇÃO
 * ============================================================================= */

static void Telnet_RxCallback(NetSocket_t *socket, void *user_data) {
    Telnet_Server_t *server = (Telnet_Server_t *)user_data;
    
    if (!server || socket != server->client_socket) return;
    
    uint16_t available = NetSocket_GetAvailable(socket);
    if (available == 0) return;
    
    // Ler dados disponíveis
    int32_t len = NetSocket_Recv(socket, server->rx_buffer, TELNET_RX_BUFFER_SIZE - 1, 0);
    
    if (len <= 0) return;
    
    // Processar cada byte
    for (int32_t i = 0; i < len; i++) {
        uint8_t ch = server->rx_buffer[i];
        
        // Ignorar comandos Telnet (começam com 0xFF)
        if (ch == 0xFF) {
            // Pular próximos 2 bytes (comando telnet)
            i += 2;
            continue;
        }
        
        // Enter: processar comando
        if (ch == ASCII_CR || ch == ASCII_LF) {
            if (server->cmd_pos > 0) {
                server->cmd_buffer[server->cmd_pos] = '\0';
                
                // Echo newline
                if (server->echo_enabled) {
                    NetSocket_Send(socket, (const uint8_t *)"\r\n", 2, 0);
                }
                
                // Parse e executar comando
                Telnet_Command_t cmd;
                ParseCommand(server->cmd_buffer, &cmd);
                
                if (cmd.cmd[0] != '\0') {
                    char response[TELNET_TX_BUFFER_SIZE];
                    memset(response, 0, sizeof(response));
                    
                    if (server->cmd_handler) {
                        server->cmd_handler(&cmd, response, sizeof(response), server->user_data);
                    }

                    if (response[0] == '\0') {
                        DefaultCommandHandler(&cmd, response, sizeof(response), server);
                    }
                    
                    if (response[0] != '\0') {
                        Telnet_Send(server, response);
                    }
                }
                
                // Resetar buffer
                server->cmd_pos = 0;
                
                // Mostrar prompt
                SendPrompt(server);
            } else {
                // Linha vazia, só mostrar prompt
                if (server->echo_enabled) {
                    NetSocket_Send(socket, (const uint8_t *)"\r\n", 2, 0);
                }
                SendPrompt(server);
            }
        }
        // Backspace/Delete: remover último caractere
        else if (ch == ASCII_BS || ch == ASCII_DEL) {
            if (server->cmd_pos > 0) {
                server->cmd_pos--;
                if (server->echo_enabled) {
                    // Enviar backspace + espaço + backspace
                    NetSocket_Send(socket, (const uint8_t *)"\b \b", 3, 0);
                }
            }
        }
        // Caractere normal
        else if (ch >= 0x20 && ch < 0x7F) {
            if (server->cmd_pos < TELNET_MAX_CMD_LEN - 1) {
                server->cmd_buffer[server->cmd_pos++] = ch;
                
                // Echo
                if (server->echo_enabled) {
                    NetSocket_Send(socket, &ch, 1, 0);
                }
            }
        }
    }
}

/* =============================================================================
 * IMPLEMENTAÇÃO DO SERVIÇO
 * ============================================================================= */

static NetErr_t Telnet_ServiceInit(void *context, void *manager) {
    Telnet_Server_t *server = (Telnet_Server_t *)context;
    NetManager_t *mgr = (NetManager_t *)manager;
    
    if (!server) return NET_ERR_PARAM;
    
    server->manager = mgr;
    
    return NET_OK;
}

static void Telnet_ServiceTask(void *context) {
    Telnet_Server_t *server = (Telnet_Server_t *)context;
    
    if (!server || !server->manager) return;
    
    switch (server->state) {
        case TELNET_STATE_IDLE:
            // Aguardar link up e IP configurado
            if (NetManager_IsLinkUp(server->manager)) {
                
                // Criar socket servidor
                server->server_socket = NetManager_AllocSocket(server->manager, NET_SOCKET_TYPE_TCP);
                if (!server->server_socket) break;
                
                NetAddr_t local_addr = { .ip = { .addr = {0, 0, 0, 0} }, .port = server->port };
                if (NetSocket_Bind(server->server_socket, &local_addr) == NET_OK) {
                    if (NetSocket_Listen(server->server_socket, 1) == NET_OK) {
                        server->state = TELNET_STATE_LISTENING;
                    }
                }
            }
            break;
        
        case TELNET_STATE_LISTENING:
            if (server->server_socket && NetSocket_IsConnected(server->server_socket)) {
                server->client_socket = server->server_socket;
                server->state = TELNET_STATE_CONNECTED;
                server->cmd_pos = 0;
                
                NetSocket_SetRxCallback(server->client_socket, Telnet_RxCallback, server);
                Telnet_Send(server, WELCOME_MSG);
                SendPrompt(server);
            }
            break;
        
        case TELNET_STATE_CONNECTED: {
            // Verificar se cliente desconectou
            if (!NetSocket_IsConnected(server->client_socket)) {
                Telnet_Disconnect(server);
                server->state = TELNET_STATE_IDLE;
            }
            break;
        }
        
        default:
            break;
    }
}

static void Telnet_ServiceDeinit(void *context) {
    Telnet_Server_t *server = (Telnet_Server_t *)context;
    
    if (server) {
        Telnet_Stop(server);
    }
}

static void Telnet_ServiceEventHandler(void *context, const NetEvent_t *event) {
    Telnet_Server_t *server = (Telnet_Server_t *)context;
    
    if (!server || !event) return;
    
    switch (event->type) {
        case NET_EVENT_LINK_DOWN:
            if (server->state != TELNET_STATE_IDLE) {
                Telnet_Stop(server);
                server->state = TELNET_STATE_IDLE;
            }
            break;
        
        case NET_EVENT_DHCP_BOUND:
            if (server->state == TELNET_STATE_IDLE) {
                Telnet_Start(server);
            }
            break;
        
        default:
            break;
    }
}

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

NetErr_t Telnet_Init(
    Telnet_Server_t *server,
    uint16_t port,
    Telnet_CommandHandler_t cmd_handler,
    void *user_data
) {
    if (!server) return NET_ERR_PARAM;
    
    memset(server, 0, sizeof(Telnet_Server_t));
    
    server->port = (port > 0) ? port : TELNET_PORT;
    server->cmd_handler = cmd_handler ? cmd_handler : DefaultCommandHandler;
    server->user_data = user_data ? user_data : server;
    server->echo_enabled = false;
    server->state = TELNET_STATE_IDLE;
    
    // Configurar interface de serviço
    server->service.name = "Telnet";
    server->service.context = server;
    server->service.init = (void (*)(struct NetService *, void *))Telnet_ServiceInit;
    server->service.task = (void (*)(struct NetService *))Telnet_ServiceTask;
    server->service.deinit = (void (*)(struct NetService *))Telnet_ServiceDeinit;
    server->service.event_handler = (void (*)(struct NetService *, const NetEvent_t *))Telnet_ServiceEventHandler;
    server->service.enabled = true;
    
    return NET_OK;
}

NetErr_t Telnet_Start(Telnet_Server_t *server) {
    if (!server) return NET_ERR_PARAM;
    
    if (server->state != TELNET_STATE_IDLE) {
        return NET_ERR_STATE;
    }
    
    server->state = TELNET_STATE_IDLE;  // Task irá iniciar automaticamente
    return NET_OK;
}

NetErr_t Telnet_Stop(Telnet_Server_t *server) {
    if (!server) return NET_ERR_PARAM;
    
    Telnet_Disconnect(server);
    
    if (server->server_socket) {
        NetSocket_Close(server->server_socket);
        NetManager_FreeSocket(server->manager, server->server_socket);
        server->server_socket = NULL;
    }
    
    server->state = TELNET_STATE_IDLE;
    return NET_OK;
}

NetErr_t Telnet_Send(Telnet_Server_t *server, const char *message) {
    
    if (!server || !message) {
        return NET_ERR_PARAM;
    }
    
    if (!server->client_socket || !NetSocket_IsConnected(server->client_socket)) {
        return NET_ERR_STATE;
    }
    
    uint16_t len = strlen(message);
    int32_t sent = NetSocket_Send(server->client_socket, (const uint8_t *)message, len, 0);
    
    return (sent > 0) ? NET_OK : NET_ERR_DRIVER;
}

NetErr_t Telnet_Printf(Telnet_Server_t *server, const char *format, ...) {
    
    if (!server || !format) {
        return NET_ERR_PARAM;
    }
    
    va_list args;
    va_start(args, format);
    
    char buffer[TELNET_TX_BUFFER_SIZE];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    va_end(args);
    
    return Telnet_Send(server, buffer);
}

bool Telnet_IsConnected(const Telnet_Server_t *server) {
    return server && 
           server->client_socket && 
           (server->state == TELNET_STATE_CONNECTED || 
            server->state == TELNET_STATE_AUTHENTICATED);
}

NetErr_t Telnet_Disconnect(Telnet_Server_t *server) {
    
    if (!server) return NET_ERR_PARAM;
    
    if (server->client_socket) {
        NetSocket_Close(server->client_socket);
        
        if (server->client_socket != server->server_socket) {
            NetManager_FreeSocket(server->manager, server->client_socket);
        }
        
        server->client_socket = NULL;
    }
    
    return NET_OK;
}

NetService_t* Telnet_GetService(Telnet_Server_t *server) {
    return server ? &server->service : NULL;
}

void Telnet_RegisterDefaultCommands(void) {
    // Comandos padrão já implementados no DefaultCommandHandler
}

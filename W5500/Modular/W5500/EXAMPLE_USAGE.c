/*
 * EXEMPLO DE USO - Como integrar a nova arquitetura na aplicação
 * 
 * Este arquivo demonstra:
 * 1. Como inicializar o Network Manager
 * 2. Como registrar serviços (DHCP, Telnet, etc.)
 * 3. Como usar sockets nas aplicações
 * 4. Como reagir a eventos de rede
 */

#include "net_manager.h"
#include "net_socket.h"

// Serviços (cada um em seu próprio módulo)
#include "Services/dhcp_service.h"
// #include "Services/dns_service.h"
// #include "Services/ntp_service.h"
// #include "Services/telnet_service.h"

// Driver de hardware
#include "Driver/W5500/w5500_hw.h"

/* =============================================================================
 * EXEMPLO 1: SETUP BÁSICO DO NETWORK MANAGER
 * ============================================================================= */

// Variáveis globais (ou alocadas na task)
NetManager_t net_manager;
W5500_HW_t w5500_hw;

// Serviços
DHCP_Client_t dhcp_client;
// DNS_Client_t dns_client;
// NTP_Client_t ntp_client;
// Telnet_Server_t telnet_server;

// Services para registro
NetService_t dhcp_service;
// NetService_t dns_service;
// NetService_t ntp_service;
// NetService_t telnet_service;

void Network_Setup(void) {
    
    // 1. Inicializar hardware W5500
    W5500_HW_Config_t hw_config = {
        .hspi = &hspi1,
        .cs_port = GPIOA,
        .cs_pin = GPIO_PIN_4,
        .rst_port = GPIOA,
        .rst_pin = GPIO_PIN_3,
        .int_pin = GPIO_PIN_2
    };
    
    if (W5500_HW_Init(&w5500_hw, &hw_config) != NET_OK) {
        Error_Handler();
    }
    
    // 2. Configurar Network Manager
    NetManagerConfig_t mgr_config = {
        .network = {
            .ip = NET_IP(192, 168, 1, 100),        // IP estático (usado se DHCP falhar)
            .subnet = NET_IP(255, 255, 255, 0),
            .gateway = NET_IP(192, 168, 1, 1),
            .dns[0] = NET_IP(8, 8, 8, 8),
            .dns[1] = NET_IP(8, 8, 4, 4),
            .mac = NET_MAC(0x02, 0xDE, 0xAD, 0xBE, 0xEF, 0x01),
            .dhcp_enabled = true                    // Usar DHCP
        },
        .task_period_ms = 100,
        .use_dhcp = true,
        .enable_link_monitor = true
    };
    
    if (NetManager_Init(&net_manager, &w5500_hw, &mgr_config) != NET_OK) {
        Error_Handler();
    }
    
    // 3. Inicializar e registrar serviços
    
    // DHCP
    DHCP_Init(&dhcp_client, "STM32-W5500");
    dhcp_service = DHCP_CREATE_SERVICE(&dhcp_client);
    NetManager_RegisterService(&net_manager, &dhcp_service);
    
    // DNS (aguarda DHCP terminar)
    // DNS_Init(&dns_client);
    // dns_service = DNS_CREATE_SERVICE(&dns_client);
    // NetManager_RegisterService(&net_manager, &dns_service);
    
    // NTP (aguarda DHCP terminar)
    // NTP_Init(&ntp_client);
    // ntp_service = NTP_CREATE_SERVICE(&ntp_client);
    // NetManager_RegisterService(&net_manager, &ntp_service);
    
    // Telnet Server
    // Telnet_Init(&telnet_server, 23);
    // telnet_service = TELNET_CREATE_SERVICE(&telnet_server);
    // NetManager_RegisterService(&net_manager, &telnet_service);
    
    // 4. (Opcional) Registrar callback global de eventos
    NetManager_SetEventCallback(&net_manager, Network_EventCallback, NULL);
}

/* =============================================================================
 * EXEMPLO 2: TASK DO NETWORK MANAGER (FreeRTOS)
 * ============================================================================= */

void NetworkManagerTask(void *argument) {
    
    // Inicializar tudo
    Network_Setup();
    
    // Loop principal
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        // Chama task do manager (ele chama as tasks dos serviços)
        NetManager_Task(&net_manager);
        
        // Aguarda próximo período
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

/* =============================================================================
 * EXEMPLO 3: CALLBACK DE EVENTOS GLOBAL
 * ============================================================================= */

void Network_EventCallback(const NetEvent_t *event, void *user_ctx) {
    
    switch(event->type) {
        
        case NET_EVENT_LINK_UP:
            printf("Link físico conectado!\n");
            // LED verde ON, por exemplo
            break;
            
        case NET_EVENT_LINK_DOWN:
            printf("Link físico desconectado!\n");
            // LED vermelho ON
            break;
            
        case NET_EVENT_DHCP_BOUND: {
            NetConfig_t config;
            if (DHCP_GetConfig(&dhcp_client, &config) == NET_OK) {
                printf("DHCP OK! IP: %d.%d.%d.%d\n",
                    config.ip.addr[0],
                    config.ip.addr[1],
                    config.ip.addr[2],
                    config.ip.addr[3]
                );
                
                // Agora que temos IP, podemos iniciar outros serviços
                // NTP_Start(&ntp_client);
                // Telnet_Start(&telnet_server);
            }
            break;
        }
        
        case NET_EVENT_DHCP_TIMEOUT:
            printf("DHCP falhou, usando IP estático\n");
            // Aplicar IP estático configurado
            break;
            
        default:
            break;
    }
}

/* =============================================================================
 * EXEMPLO 4: APLICAÇÃO QUE USA SOCKET DIRETAMENTE (UDP Echo)
 * ============================================================================= */

typedef struct {
    NetSocket_t *socket;
    NetManager_t *manager;
    uint16_t port;
} UDPEcho_t;

// Callback chamado quando dados chegam
void UDPEcho_RxCallback(
    uint8_t socket_id,
    const uint8_t *data,
    uint16_t len,
    const NetAddr_t *remote_addr,
    void *user_ctx
) {
    UDPEcho_t *echo = (UDPEcho_t*)user_ctx;
    
    // Echo: enviar de volta para quem enviou
    printf("UDP Echo: recebido %d bytes de %d.%d.%d.%d:%d\n",
        len,
        remote_addr->ip.addr[0],
        remote_addr->ip.addr[1],
        remote_addr->ip.addr[2],
        remote_addr->ip.addr[3],
        remote_addr->port
    );
    
    // Enviar de volta
    NetSocket_SendTo(echo->socket, data, len, remote_addr);
}

NetErr_t UDPEcho_Start(UDPEcho_t *echo, NetManager_t *manager, uint16_t port) {
    
    echo->manager = manager;
    echo->port = port;
    
    // Alocar socket UDP
    echo->socket = NetManager_AllocSocket(manager, NET_SOCKET_TYPE_UDP);
    if (!echo->socket) {
        return NET_ERR_NO_SOCKET;
    }
    
    // Configurar callback
    NetSocket_SetRxCallback(echo->socket, UDPEcho_RxCallback, echo);
    
    // Bind na porta
    NetErr_t err = NetSocket_Bind(echo->socket, port);
    if (err != NET_OK) {
        NetManager_FreeSocket(manager, echo->socket);
        return err;
    }
    
    printf("UDP Echo iniciado na porta %d\n", port);
    return NET_OK;
}

void UDPEcho_Stop(UDPEcho_t *echo) {
    if (echo->socket) {
        NetManager_FreeSocket(echo->manager, echo->socket);
        echo->socket = NULL;
    }
}

/* =============================================================================
 * EXEMPLO 5: APLICAÇÃO TCP CLIENT (HTTP GET simples)
 * ============================================================================= */

typedef struct {
    NetSocket_t *socket;
    NetManager_t *manager;
    NetAddr_t server_addr;
    bool connected;
} HTTPClient_t;

void HTTPClient_RxCallback(
    uint8_t socket_id,
    const uint8_t *data,
    uint16_t len,
    const NetAddr_t *remote_addr,
    void *user_ctx
) {
    // Recebeu resposta HTTP
    printf("HTTP Response (%d bytes):\n%.*s\n", len, len, data);
}

NetErr_t HTTPClient_Get(
    HTTPClient_t *http,
    NetManager_t *manager,
    const char *host_ip,
    uint16_t port,
    const char *path
) {
    http->manager = manager;
    
    // Alocar socket TCP
    http->socket = NetManager_AllocSocket(manager, NET_SOCKET_TYPE_TCP);
    if (!http->socket) {
        return NET_ERR_NO_SOCKET;
    }
    
    // Configurar endereço do servidor
    // (Em um cliente real, você usaria DNS para resolver "host_ip")
    http->server_addr.ip = NET_IP(192, 168, 1, 10);  // Exemplo
    http->server_addr.port = port;
    
    // Configurar callback
    NetSocket_SetRxCallback(http->socket, HTTPClient_RxCallback, http);
    
    // Conectar ao servidor
    NetErr_t err = NetSocket_Connect(http->socket, &http->server_addr);
    if (err != NET_OK && err != NET_ERR_IN_PROGRESS) {
        NetManager_FreeSocket(manager, http->socket);
        return err;
    }
    
    // Aguardar conexão (em um código real, fazer isso em uma task/callback)
    uint32_t timeout = 5000; // 5 segundos
    uint32_t start = HAL_GetTick();
    while (!NetSocket_IsConnected(http->socket)) {
        if ((HAL_GetTick() - start) > timeout) {
            NetManager_FreeSocket(manager, http->socket);
            return NET_ERR_TIMEOUT;
        }
        osDelay(10);
    }
    
    // Montar requisição HTTP GET
    char request[256];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host_ip
    );
    
    // Enviar requisição
    err = NetSocket_Send(http->socket, (uint8_t*)request, strlen(request));
    if (err != NET_OK) {
        NetManager_FreeSocket(manager, http->socket);
        return err;
    }
    
    printf("HTTP GET enviado para %s:%d%s\n", host_ip, port, path);
    
    // Resposta virá via callback
    return NET_OK;
}

/* =============================================================================
 * EXEMPLO 6: APLICAÇÃO TCP SERVER (Echo Server simples)
 * ============================================================================= */

typedef struct {
    NetSocket_t *socket;
    NetManager_t *manager;
    uint16_t port;
} TCPEchoServer_t;

void TCPEchoServer_RxCallback(
    uint8_t socket_id,
    const uint8_t *data,
    uint16_t len,
    const NetAddr_t *remote_addr,
    void *user_ctx
) {
    TCPEchoServer_t *server = (TCPEchoServer_t*)user_ctx;
    
    printf("TCP Echo: recebido %d bytes\n", len);
    
    // Echo de volta
    NetSocket_Send(server->socket, data, len);
}

NetErr_t TCPEchoServer_Start(
    TCPEchoServer_t *server,
    NetManager_t *manager,
    uint16_t port
) {
    server->manager = manager;
    server->port = port;
    
    // Alocar socket TCP
    server->socket = NetManager_AllocSocket(manager, NET_SOCKET_TYPE_TCP);
    if (!server->socket) {
        return NET_ERR_NO_SOCKET;
    }
    
    // Configurar callback
    NetSocket_SetRxCallback(server->socket, TCPEchoServer_RxCallback, server);
    
    // Bind na porta
    NetErr_t err = NetSocket_Bind(server->socket, port);
    if (err != NET_OK) {
        NetManager_FreeSocket(manager, server->socket);
        return err;
    }
    
    // Colocar em modo listen
    err = NetSocket_Listen(server->socket, 1);
    if (err != NET_OK) {
        NetManager_FreeSocket(manager, server->socket);
        return err;
    }
    
    printf("TCP Echo Server iniciado na porta %d\n", port);
    return NET_OK;
}

/* =============================================================================
 * RESUMO: VANTAGENS DA NOVA ARQUITETURA
 * ============================================================================= */

/*
 * 1. MODULARIDADE
 *    - Cada serviço é independente
 *    - Fácil adicionar/remover serviços
 *    - Código organizado em módulos lógicos
 *
 * 2. DESACOPLAMENTO
 *    - Aplicações não conhecem W5500
 *    - Trocar hardware? Só reimplementar camadas 2-3
 *    - Serviços reutilizáveis em outros projetos
 *
 * 3. TESTABILIDADE
 *    - Cada camada pode ser testada isoladamente
 *    - Fácil criar mocks de socket/hardware
 *    - Unit tests viáveis
 *
 * 4. MANUTENIBILIDADE
 *    - Responsabilidades claras
 *    - Código mais limpo e legível
 *    - Fácil debugar problemas
 *
 * 5. ESCALABILIDADE
 *    - Pool de sockets gerenciado centralmente
 *    - Fácil adicionar priorização
 *    - Sistema de eventos extensível
 */

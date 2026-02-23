/*
 * dhcp_service.h
 *
 * Cliente DHCP refatorado para a nova arquitetura
 * 
 * Este é um EXEMPLO de como um serviço deve ser estruturado:
 * - Usa NetSocket API (não acessa W5500 diretamente)
 * - Implementa interface NetService_t
 * - Independente de hardware
 */

#ifndef NETWORK_SERVICES_DHCP_SERVICE_H_
#define NETWORK_SERVICES_DHCP_SERVICE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "net_types.h"
#include "net_socket.h"
#include "net_manager.h"

/* =============================================================================
 * CONFIGURAÇÃO
 * ============================================================================= */

#define DHCP_SERVER_PORT   67
#define DHCP_CLIENT_PORT   68
#define DHCP_TIMEOUT_MS    4000
#define DHCP_RETRY_MAX     5

/* =============================================================================
 * ESTADOS DO CLIENTE DHCP
 * ============================================================================= */

typedef enum {
    DHCP_STATE_IDLE = 0,
    DHCP_STATE_SELECTING,      // Enviou DISCOVER, aguardando OFFER
    DHCP_STATE_REQUESTING,     // Enviou REQUEST, aguardando ACK
    DHCP_STATE_BOUND,          // IP obtido com sucesso!
    DHCP_STATE_RENEWING,       // Renovando lease (T1)
    DHCP_STATE_REBINDING,      // Rebinding (T2)
    DHCP_STATE_RELEASING,      // Liberando IP
    DHCP_STATE_ERROR
} DHCP_State_t;

/* =============================================================================
 * ESTRUTURA DO CLIENTE DHCP
 * ============================================================================= */

typedef struct {
    
    // Socket alocado pelo Network Manager
    NetSocket_t *socket;
    
    // Ponteiro de volta para o manager (para postar eventos)
    NetManager_t *manager;
    
    // Estado da FSM
    DHCP_State_t state;
    
    // Timers
    uint32_t state_timer;       // Timer genérico para timeouts
    uint32_t lease_expire_time; // Quando o lease expira
    uint32_t t1_time;           // Renewal time
    uint32_t t2_time;           // Rebinding time
    
    // Parâmetros do protocolo
    uint32_t xid;               // Transaction ID
    uint32_t lease_time;        // Duração do lease (segundos)
    uint8_t retry_count;
    
    // Informações obtidas do servidor
    NetIP_t offered_ip;
    NetIP_t server_ip;
    NetIP_t subnet_mask;
    NetIP_t gateway;
    NetIP_t dns[2];
    NetIP_t ntp_server;         // Servidor NTP providenciado por DHCP (opcional)
    char domain_name[64];
    
    // Hostname do dispositivo (enviado no DISCOVER)
    char hostname[32];
    
    // MAC address (precisa para montar pacotes)
    NetMAC_t mac;
    
} DHCP_Client_t;

/* =============================================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================================= */

/**
 * @brief Inicializa cliente DHCP
 * 
 * @param dhcp Ponteiro para estrutura do cliente (alocado pelo usuário)
 * @param hostname Hostname do dispositivo (pode ser NULL)
 * @return NET_OK ou código de erro
 * 
 * @note NÃO aloca socket ainda (isso é feito em Start)
 * @note Esta função corresponde ao NetService.init()
 */
NetErr_t DHCP_Init(DHCP_Client_t *dhcp, const char *hostname);

/**
 * @brief Desinicializa cliente DHCP
 * 
 * @param dhcp Cliente
 * 
 * @note Libera socket se alocado
 * @note Corresponde ao NetService.deinit()
 */
void DHCP_Deinit(DHCP_Client_t *dhcp);

/**
 * @brief Inicia processo DHCP (envia DISCOVER)
 * 
 * @param dhcp Cliente
 * @param manager Network Manager (para alocar socket)
 * @return NET_OK ou código de erro
 * 
 * @note Aloca socket UDP e envia primeira requisição
 */
NetErr_t DHCP_Start(DHCP_Client_t *dhcp, NetManager_t *manager);

/**
 * @brief Para cliente DHCP
 * 
 * @param dhcp Cliente
 * 
 * @note Envia RELEASE se em BOUND
 * @note Libera socket
 */
void DHCP_Stop(DHCP_Client_t *dhcp);

/**
 * @brief Task do DHCP (FSM + timers)
 * 
 * @param dhcp Cliente
 * 
 * @note Chamado periodicamente pelo Network Manager
 * @note Corresponde ao NetService.task()
 */
void DHCP_Task(DHCP_Client_t *dhcp);

/**
 * @brief Handler de eventos de rede
 * 
 * @param dhcp Cliente
 * @param event Evento recebido
 * 
 * @note Corresponde ao NetService.event_handler()
 * @note Reage a LINK_DOWN, por exemplo
 */
void DHCP_EventHandler(DHCP_Client_t *dhcp, const NetEvent_t *event);

/**
 * @brief Obtém estado atual
 * 
 * @param dhcp Cliente
 * @return Estado
 */
DHCP_State_t DHCP_GetState(const DHCP_Client_t *dhcp);

/**
 * @brief Verifica se IP foi obtido
 * 
 * @param dhcp Cliente
 * @return true se em BOUND
 */
bool DHCP_IsBound(const DHCP_Client_t *dhcp);

/**
 * @brief Obtém configuração de rede obtida via DHCP
 * 
 * @param dhcp Cliente
 * @param config [out] Configuração
 * @return NET_OK ou NET_ERR_NOT_CONNECTED se não em BOUND
 */
NetErr_t DHCP_GetConfig(const DHCP_Client_t *dhcp, NetConfig_t *config);

/**
 * @brief Força renovação do lease
 * 
 * @param dhcp Cliente
 * @return NET_OK ou código de erro
 */
NetErr_t DHCP_Renew(DHCP_Client_t *dhcp);

/**
 * @brief Libera IP (envia RELEASE)
 * 
 * @param dhcp Cliente
 * @return NET_OK ou código de erro
 */
NetErr_t DHCP_Release(DHCP_Client_t *dhcp);

/* =============================================================================
 * WRAPPER PARA REGISTRO COMO SERVIÇO
 * ============================================================================= */

/**
 * @brief Wrapper de init para interface NetService
 */
static inline void DHCP_ServiceInit(NetService_t *service, void *manager) {
    DHCP_Client_t *dhcp = (DHCP_Client_t*)service->context;
    DHCP_Start(dhcp, (NetManager_t*)manager);
}

/**
 * @brief Wrapper de task para interface NetService
 */
static inline void DHCP_ServiceTask(NetService_t *service) {
    DHCP_Task((DHCP_Client_t*)service->context);
}

/**
 * @brief Wrapper de deinit para interface NetService
 */
static inline void DHCP_ServiceDeinit(NetService_t *service) {
    DHCP_Deinit((DHCP_Client_t*)service->context);
}

/**
 * @brief Wrapper de event handler para interface NetService
 */
static inline void DHCP_ServiceEventHandler(NetService_t *service, const NetEvent_t *event) {
    DHCP_EventHandler((DHCP_Client_t*)service->context, event);
}

/**
 * @brief Macro para criar NetService_t do DHCP
 * 
 * Uso:
 * DHCP_Client_t dhcp_client;
 * NetService_t dhcp_service = DHCP_CREATE_SERVICE(&dhcp_client);
 * NetManager_RegisterService(&manager, &dhcp_service);
 */
#define DHCP_CREATE_SERVICE(dhcp_ptr) \
    NET_SERVICE_DEFINE( \
        "DHCP", \
        DHCP_ServiceInit, \
        DHCP_ServiceTask, \
        DHCP_ServiceDeinit, \
        DHCP_ServiceEventHandler, \
        (dhcp_ptr) \
    )

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_SERVICES_DHCP_SERVICE_H_ */

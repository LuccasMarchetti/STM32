/*
 * net_manager.h
 *
 * Gerenciador Central de Rede
 * 
 * Responsabilidades:
 * - Inicializar e gerenciar driver de hardware (W5500)
 * - Pool de sockets (alocar/liberar)
 * - Despachar eventos para serviços registrados
 * - Coordenar serviços (ex: iniciar NTP após DHCP)
 * - Polling/IRQ handling
 */

#ifndef NETWORK_NET_MANAGER_H_
#define NETWORK_NET_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "net_types.h"
#include "net_socket.h"

/* =============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================= */

// Driver de hardware (W5500 ou outro)
typedef struct W5500_HW W5500_HW_t;

/* =============================================================================
 * CONFIGURAÇÃO
 * ============================================================================= */

/**
 * @brief Configuração do Network Manager
 */
typedef struct {
    NetConfig_t network;         // Config de rede (IP, gateway, etc)
    uint16_t task_period_ms;     // Período da task (padrão: 100ms)
    bool use_dhcp;               // Usar DHCP? (sobrescreve network.dhcp_enabled)
    bool enable_link_monitor;    // Monitorar link físico?
} NetManagerConfig_t;

/* =============================================================================
 * INTERFACE DE SERVIÇO
 * ============================================================================= */

/**
 * @brief Interface que todos os serviços de rede devem implementar
 * 
 * Exemplo: DHCP_Client_t implementa NetService_t
 * 
 * Serviço se registra com NetManager_RegisterService() e então:
 * - init() é chamado uma vez
 * - task() é chamado periodicamente pelo manager
 * - event_handler() recebe eventos relevantes (link up/down, etc)
 */
typedef struct NetService {
    const char *name;                       // Nome do serviço (para debug)
    
    void (*init)(struct NetService *service, void *manager);
    void (*task)(struct NetService *service);
    void (*deinit)(struct NetService *service);
    
    // Callback de eventos (pode ser NULL se não interessado)
    void (*event_handler)(struct NetService *service, const NetEvent_t *event);
    
    void *context;                          // Contexto privado do serviço
    bool enabled;                           // Serviço está ativo?
    
} NetService_t;

/**
 * @brief Macro helper para definir um serviço
 * 
 * Exemplo:
 * NetService_t dhcp_service = NET_SERVICE_DEFINE(
 *     "DHCP",
 *     DHCP_Init,
 *     DHCP_Task,
 *     DHCP_Deinit,
 *     DHCP_EventHandler,
 *     &dhcp_client_instance
 * );
 */
#define NET_SERVICE_DEFINE(svc_name, init_fn, task_fn, deinit_fn, event_fn, ctx) \
    { \
        .name = svc_name, \
        .init = init_fn, \
        .task = task_fn, \
        .deinit = deinit_fn, \
        .event_handler = event_fn, \
        .context = ctx, \
        .enabled = true \
    }

/* =============================================================================
 * ESTRUTURA DO MANAGER
 * ============================================================================= */

#define NET_MANAGER_MAX_SERVICES  8   // Máximo de serviços registrados

/**
 * @brief Estado do Network Manager
 */
typedef enum {
    NET_MANAGER_STATE_UNINITIALIZED = 0,
    NET_MANAGER_STATE_INITIALIZING,
    NET_MANAGER_STATE_LINK_DOWN,
    NET_MANAGER_STATE_CONFIGURING,    // Aguardando DHCP, por exemplo
    NET_MANAGER_STATE_READY,          // Rede pronta
    NET_MANAGER_STATE_ERROR
} NetManagerState_t;

/**
 * @brief Estrutura do Network Manager (opaca para usuários)
 */
typedef struct NetManager {
    
    // Hardware driver
    W5500_HW_t *hw_driver;
    
    // Configuração
    NetManagerConfig_t config;
    
    // Estado
    NetManagerState_t state;
    bool link_up;
    
    // Pool de sockets (alocados sob demanda)
    NetSocket_t *socket_pool;
    uint8_t socket_pool_size;
    
    // Serviços registrados
    NetService_t *services[NET_MANAGER_MAX_SERVICES];
    uint8_t service_count;
    
    // Eventos pendentes (fila simples)
    NetEvent_t event_queue[16];
    uint8_t event_queue_head;
    uint8_t event_queue_tail;
    
    // Callback global de eventos (opcional)
    NetEventCallback_t global_event_callback;
    void *global_event_ctx;
    
} NetManager_t;

/* =============================================================================
 * FUNÇÕES PÚBLICAS
 * ============================================================================= */

/**
 * @brief Inicializa o Network Manager
 * 
 * @param mgr Ponteiro para estrutura do manager (alocada pelo usuário)
 * @param hw_driver Ponteiro para driver de hardware (W5500, etc)
 * @param config Configuração inicial (pode ser NULL para usar padrões)
 * @return NET_OK ou código de erro
 * 
 * @note Deve ser chamado ANTES de usar qualquer outra função
 * @note Inicializa hardware e pool de sockets
 */
NetErr_t NetManager_Init(
    NetManager_t *mgr,
    W5500_HW_t *hw_driver,
    const NetManagerConfig_t *config
);

/**
 * @brief Desinicializa o manager e libera recursos
 * 
 * @param mgr Manager
 * @return NET_OK ou código de erro
 */
NetErr_t NetManager_Deinit(NetManager_t *mgr);

/**
 * @brief Task principal do manager (chamada periodicamente)
 * 
 * @param mgr Manager
 * 
 * @note Deve ser chamada em loop (RTOS task ou main loop)
 * @note Executa polling de hardware, despacha eventos, chama tasks de serviços
 * 
 * Exemplo (FreeRTOS):
 * void NetworkTask(void *arg) {
 *     NetManager_t *mgr = (NetManager_t*)arg;
 *     TickType_t last_wake = xTaskGetTickCount();
 *     
 *     while(1) {
 *         NetManager_Task(mgr);
 *         vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
 *     }
 * }
 */
void NetManager_Task(NetManager_t *mgr);

/* =============================================================================
 * GERENCIAMENTO DE SERVIÇOS
 * ============================================================================= */

/**
 * @brief Registra um serviço de rede
 * 
 * @param mgr Manager
 * @param service Ponteiro para estrutura do serviço
 * @return NET_OK ou código de erro
 * 
 * @note Serviço deve permanecer válido enquanto registrado
 * @note service->init() será chamado imediatamente
 */
NetErr_t NetManager_RegisterService(NetManager_t *mgr, NetService_t *service);

/**
 * @brief Remove serviço do manager
 * 
 * @param mgr Manager
 * @param service Serviço a remover
 * @return NET_OK ou código de erro
 * 
 * @note service->deinit() será chamado
 */
NetErr_t NetManager_UnregisterService(NetManager_t *mgr, NetService_t *service);

/**
 * @brief Ativa/desativa um serviço
 * 
 * @param service Serviço
 * @param enabled true para ativar, false para desativar
 */
void NetManager_EnableService(NetService_t *service, bool enabled);

/* =============================================================================
 * GERENCIAMENTO DE SOCKETS
 * ============================================================================= */

/**
 * @brief Aloca um socket do pool
 * 
 * @param mgr Manager
 * @param type Tipo do socket
 * @return Ponteiro para socket ou NULL se pool esgotado
 * 
 * @note Socket deve ser liberado com NetManager_FreeSocket()
 */
NetSocket_t* NetManager_AllocSocket(NetManager_t *mgr, NetSocketType_t type);

/**
 * @brief Libera socket de volta para o pool
 * 
 * @param mgr Manager
 * @param sock Socket a liberar
 * @return NET_OK ou código de erro
 * 
 * @note Socket é fechado automaticamente
 */
NetErr_t NetManager_FreeSocket(NetManager_t *mgr, NetSocket_t *sock);

/**
 * @brief Obtém número de sockets disponíveis
 * 
 * @param mgr Manager
 * @return Número de sockets livres
 */
uint8_t NetManager_GetAvailableSockets(const NetManager_t *mgr);

/* =============================================================================
 * EVENTOS
 * ============================================================================= */

/**
 * @brief Envia evento para todos os serviços registrados
 * 
 * @param mgr Manager
 * @param event Evento a despachar
 * 
 * @note Evento é colocado em fila e despachado no próximo Task()
 */
void NetManager_PostEvent(NetManager_t *mgr, const NetEvent_t *event);

/**
 * @brief Define callback global de eventos
 * 
 * @param mgr Manager
 * @param callback Função de callback
 * @param user_ctx Contexto do usuário
 * 
 * @note Chamado ALÉM dos event_handler dos serviços
 */
void NetManager_SetEventCallback(
    NetManager_t *mgr,
    NetEventCallback_t callback,
    void *user_ctx
);

/* =============================================================================
 * CONSULTAS DE ESTADO
 * ============================================================================= */

/**
 * @brief Obtém estado atual do manager
 * 
 * @param mgr Manager
 * @return Estado
 */
NetManagerState_t NetManager_GetState(const NetManager_t *mgr);

/**
 * @brief Verifica se rede está pronta para uso
 * 
 * @param mgr Manager
 * @return true se pronta (tem IP, link up, etc)
 */
bool NetManager_IsReady(const NetManager_t *mgr);

/**
 * @brief Verifica se link físico está conectado
 * 
 * @param mgr Manager
 * @return true se link up
 */
bool NetManager_IsLinkUp(const NetManager_t *mgr);

/**
 * @brief Obtém configuração de rede atual
 * 
 * @param mgr Manager
 * @param config [out] Configuração atual
 * @return NET_OK ou código de erro
 */
NetErr_t NetManager_GetNetworkConfig(const NetManager_t *mgr, NetConfig_t *config);

/* =============================================================================
 * CONTROLE
 * ============================================================================= */

/**
 * @brief Força reset do hardware de rede
 * 
 * @param mgr Manager
 * @return NET_OK ou código de erro
 */
NetErr_t NetManager_Reset(NetManager_t *mgr);

/**
 * @brief Aplica nova configuração de rede
 * 
 * @param mgr Manager
 * @param config Nova configuração
 * @return NET_OK ou código de erro
 * 
 * @note Se DHCP estiver ativo, configuração será sobrescrita
 */
NetErr_t NetManager_ApplyConfig(NetManager_t *mgr, const NetConfig_t *config);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_NET_MANAGER_H_ */

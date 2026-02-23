/*
 * net_manager.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação do gerenciador de rede centralizado
 */

#include "net_manager.h"
#include "net_socket.h"
#include "w5500_hw.h"
#include "cmsis_os.h"
#include <string.h>

/* Estado interno do manager */
typedef struct {
    NetManager_t *mgr;
    bool running;
} NetManagerRunState_t;

static NetManagerRunState_t state = {0};

/* =============================================================================
 * INICIALIZAÇÃO E DEINICIALIZAÇÃO
 * ============================================================================= */

NetErr_t NetManager_Init(
    NetManager_t *mgr,
    W5500_HW_t *hw_driver,
    const NetManagerConfig_t *config
) {
    
    if (!mgr || !hw_driver) {
        return NET_ERR_PARAM;
    }
    
    if (state.running) {
        return NET_ERR_STATE;
    }

    memset(mgr, 0, sizeof(NetManager_t));
    
    mgr->hw_driver = hw_driver;
    
    // Usar configuração padrão se não fornecida
    if (config) {
        memcpy(&mgr->config, config, sizeof(NetManagerConfig_t));
    } else {
        mgr->config.task_period_ms = 100;
        mgr->config.use_dhcp = true;
        mgr->config.enable_link_monitor = true;
    }
    
    // Inicializar camada de sockets
    NetErr_t err = NetSocket_Init(hw_driver);
    if (err != NET_OK) {
        return err;
    }
    
    // Zerar lista de serviços
    for (int i = 0; i < NET_MANAGER_MAX_SERVICES; i++) {
        mgr->services[i] = NULL;
    }
    mgr->service_count = 0;
    
    // Inicializar fila de eventos
    mgr->event_queue_head = 0;
    mgr->event_queue_tail = 0;
    
    // Estado inicial
    mgr->state = NET_MANAGER_STATE_INITIALIZING;
    mgr->link_up = false;
    
    state.mgr = mgr;
    state.running = true;
    
    mgr->state = NET_MANAGER_STATE_LINK_DOWN;
    
    return NET_OK;
}

NetErr_t NetManager_Deinit(NetManager_t *mgr) {
    
    if (!mgr || !state.running) {
        return NET_ERR_PARAM;
    }
    
    // Parar todos os serviços
    for (int i = 0; i < mgr->service_count; i++) {
        if (mgr->services[i] && mgr->services[i]->deinit) {
            mgr->services[i]->deinit(mgr->services[i]);
        }
    }
    
    // Deinicializar sockets
    NetSocket_Deinit();
    
    mgr->state = NET_MANAGER_STATE_UNINITIALIZED;
    state.mgr = NULL;
    state.running = false;
    
    return NET_OK;
}

/* =============================================================================
 * GERENCIAMENTO DE SERVIÇOS
 * ============================================================================= */

NetErr_t NetManager_RegisterService(
    NetManager_t *mgr,
    NetService_t *service
) {
    if (!mgr || !service) {
        return NET_ERR_PARAM;
    }
    
    if (mgr->service_count >= NET_MANAGER_MAX_SERVICES) {
        return NET_ERR_RESOURCE;
    }
    
    // Verificar se já está registrado
    for (int i = 0; i < mgr->service_count; i++) {
        if (mgr->services[i] == service) {
            return NET_ERR_PARAM;  // Já registrado
        }
    }
    
    // Inicializar o serviço
    if (service->init) {
        service->init(service, mgr);
    }
    
    mgr->services[mgr->service_count++] = service;
    
    return NET_OK;
}

NetErr_t NetManager_UnregisterService(
    NetManager_t *mgr,
    NetService_t *service
) {
    if (!mgr || !service) {
        return NET_ERR_PARAM;
    }
    
    // Buscar serviço
    int index = -1;
    for (int i = 0; i < mgr->service_count; i++) {
        if (mgr->services[i] == service) {
            index = i;
            break;
        }
    }
    
    if (index < 0) {
        return NET_ERR_PARAM;  // Não encontrado
    }
    
    // Deinicializar serviço
    if (service->deinit) {
        service->deinit(service);
    }
    
    // Remover da lista (deslocar elementos)
    for (int i = index; i < mgr->service_count - 1; i++) {
        mgr->services[i] = mgr->services[i + 1];
    }
    
    mgr->services[--mgr->service_count] = NULL;
    
    return NET_OK;
}

/* =============================================================================
 * GERENCIAMENTO DE SOCKETS
 * ============================================================================= */

NetSocket_t* NetManager_AllocSocket(
    NetManager_t *mgr,
    NetSocketType_t type
) {
    if (!mgr) {
        return NULL;
    }
    
    return NetSocket_Create(type, 0);
}

NetErr_t NetManager_FreeSocket(NetManager_t *mgr, NetSocket_t *socket) {
    
    if (!mgr || !socket) {
        return NET_ERR_PARAM;
    }
    
    return NetSocket_Destroy(socket);
}

/* =============================================================================
 * SISTEMA DE EVENTOS
 * ============================================================================= */

void NetManager_PostEvent(
    NetManager_t *mgr,
    const NetEvent_t *event
) {
    if (!mgr || !event) {
        return;
    }
    
    // Verificar se fila está cheia
    uint8_t next_head = (mgr->event_queue_head + 1) % 16;
    if (next_head == mgr->event_queue_tail) {
        return;  // Fila cheia, descartar evento
    }
    
    // Adicionar evento à fila
    memcpy(&mgr->event_queue[mgr->event_queue_head], event, sizeof(NetEvent_t));
    mgr->event_queue_head = next_head;
}

static void ProcessEvent(NetManager_t *mgr, const NetEvent_t *event) {
    
    // Processar internamente
    switch (event->type) {
        case NET_EVENT_LINK_UP:
            mgr->link_up = true;
            if (mgr->state == NET_MANAGER_STATE_LINK_DOWN) {
                mgr->state = NET_MANAGER_STATE_CONFIGURING;
            }
            break;
        
        case NET_EVENT_LINK_DOWN:
            mgr->link_up = false;
            mgr->state = NET_MANAGER_STATE_LINK_DOWN;
            break;
        
        case NET_EVENT_DHCP_BOUND:
            if (mgr->state == NET_MANAGER_STATE_CONFIGURING) {
                mgr->state = NET_MANAGER_STATE_READY;
            }
            break;
        
        case NET_EVENT_DHCP_LOST:
            if (mgr->state == NET_MANAGER_STATE_READY) {
                mgr->state = NET_MANAGER_STATE_CONFIGURING;
            }
            break;
        
        default:
            break;
    }
    
    // Notificar callback global
    if (mgr->global_event_callback) {
        mgr->global_event_callback(event, mgr->global_event_ctx);
    }
    
    // Distribuir para serviços
    for (int i = 0; i < mgr->service_count; i++) {
        NetService_t *svc = mgr->services[i];
        if (svc && svc->enabled && svc->event_handler) {
            svc->event_handler(svc, event);
        }
    }
}

/* =============================================================================
 * LOOP PRINCIPAL
 * ============================================================================= */

void NetManager_Task(NetManager_t *mgr) {
    
    if (!mgr || !state.running) {
        return;
    }
    
    // 1. Processar eventos pendentes
    while (mgr->event_queue_tail != mgr->event_queue_head) {
        NetEvent_t *event = &mgr->event_queue[mgr->event_queue_tail];
        ProcessEvent(mgr, event);
        mgr->event_queue_tail = (mgr->event_queue_tail + 1) % 16;
    }
    
    // 2. Verificar link
    if (mgr->config.enable_link_monitor) {
        bool link_up;
        if (W5500_HW_GetLinkStatus(mgr->hw_driver, &link_up) == HAL_OK) {
            if (link_up != mgr->link_up) {
                // Mudança de estado - postar evento
                NetEvent_t event = {
                    .type = link_up ? NET_EVENT_LINK_UP : NET_EVENT_LINK_DOWN,
                    .data = NULL,
                    .data_len = 0,
                };
                NetManager_PostEvent(mgr, &event);
            }
        }
    }
    
    // 3. Processar sockets (polling de RX)
    NetSocket_ProcessAll();
    
    // 4. Executar tarefas de todos os serviços ativos
    for (int i = 0; i < mgr->service_count; i++) {
        NetService_t *svc = mgr->services[i];
        if (svc && svc->enabled && svc->task) {
            svc->task(svc);
        }
    }
}

/* =============================================================================
 * CONSULTAS
 * ============================================================================= */

bool NetManager_IsLinkUp(const NetManager_t *mgr) {
    return mgr && mgr->link_up;
}

bool NetManager_IsDHCPBound(const NetManager_t *mgr) {
    return mgr && (mgr->state == NET_MANAGER_STATE_READY) && mgr->config.use_dhcp;
}

bool NetManager_IsReady(const NetManager_t *mgr) {
    return mgr && (mgr->state == NET_MANAGER_STATE_READY);
}

NetManagerState_t NetManager_GetState(const NetManager_t *mgr) {
    return mgr ? mgr->state : NET_MANAGER_STATE_UNINITIALIZED;
}

NetErr_t NetManager_GetNetworkConfig(
    const NetManager_t *mgr,
    NetConfig_t *config
) {
    if (!mgr || !config || !state.running) {
        return NET_ERR_PARAM;
    }
    
    W5500_HW_GetMAC(mgr->hw_driver, config->mac.addr);
    W5500_HW_GetIP(mgr->hw_driver, config->ip.addr);
    W5500_HW_GetSubnet(mgr->hw_driver, config->subnet.addr);
    W5500_HW_GetGateway(mgr->hw_driver, config->gateway.addr);
    
    // DNS não está nos registros do W5500, pegar de serviço se houver
    memset(config->dns, 0, sizeof(config->dns));
    
    return NET_OK;
}

NetErr_t NetManager_ApplyConfig(
    NetManager_t *mgr,
    const NetConfig_t *config
) {
    if (!mgr || !config || !state.running) {
        return NET_ERR_PARAM;
    }
    
    W5500_HW_SetMAC(mgr->hw_driver, config->mac.addr);
    W5500_HW_SetIP(mgr->hw_driver, config->ip.addr);
    W5500_HW_SetSubnet(mgr->hw_driver, config->subnet.addr);
    W5500_HW_SetGateway(mgr->hw_driver, config->gateway.addr);
    
    // Postar evento de configuração atualizada
    NetEvent_t event = {
        .type = NET_EVENT_CONFIG_CHANGED,
        .data = NULL,
        .data_len = 0,
    };
    NetManager_PostEvent(mgr, &event);
    
    return NET_OK;
}

void NetManager_SetEventCallback(
    NetManager_t *mgr,
    NetEventCallback_t callback,
    void *user_ctx
) {
    if (mgr) {
        mgr->global_event_callback = callback;
        mgr->global_event_ctx = user_ctx;
    }
}

uint8_t NetManager_GetAvailableSockets(const NetManager_t *mgr) {
    (void)mgr;
    return NET_MAX_SOCKETS - NetSocket_GetCount();
}

NetErr_t NetManager_Reset(NetManager_t *mgr) {
    if (!mgr || !mgr->hw_driver) {
        return NET_ERR_PARAM;
    }
    
    return W5500_HW_Reset(mgr->hw_driver);
}

void NetManager_EnableService(NetService_t *service, bool enabled) {
    if (service) {
        service->enabled = enabled;
    }
}

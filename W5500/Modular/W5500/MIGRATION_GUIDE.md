# Guia de Migração - Arquitetura de Rede

Este documento orienta a migração do código atual para a nova arquitetura em camadas.

## 📊 Situação Atual vs. Proposta

### ❌ Atual (Problemas)
```
W5500_Driver_t (w5500.h/c)
├── Comunicação SPI
├── Registradores W5500
├── Sockets de hardware
├── DHCP_Control_t dhcp      ← Acoplamento!
├── DNS_Control_t dns         ← Acoplamento!
├── NTP_Control_t ntp         ← Acoplamento!
├── TELNET_Control_t telnet   ← Acoplamento!
└── mDNS_Service_t mdns       ← Acoplamento!
```

**Problemas:**
- Driver conhece todos os serviços
- Impossível reusar código
- Difícil testar
- Dependências circulares

### ✅ Proposta (Solução)
```
Camadas independentes:
1. W5500_HW (hardware puro)
2. NetSocket (abstração)
3. NetManager (coordenação)
4. Services (DHCP, DNS, etc) - independentes
5. Application (seu código)
```

**Vantagens:**
- Cada camada tem responsabilidade única
- Desacoplamento total
- Código reutilizável
- Fácil testar

---

## 🗺️ Plano de Migração (Passo a Passo)

### **FASE 1: Preparação (sem quebrar código atual)**

#### 1.1. Criar nova estrutura de pastas
```bash
Core/Network/
├── net_types.h            ← Tipos comuns (já criado)
├── net_socket.h/c         ← API de socket (já criado header)
├── net_manager.h/c        ← Gerenciador (já criado header)
├── Driver/
│   └── W5500/
│       ├── w5500_hw.h/c       ← Driver puro (extrair de w5500.c atual)
│       ├── w5500_regs.h       ← Registradores (extrair de w5500.h atual)
│       └── w5500_ll.h/c       ← Low-level SPI
└── Services/
    ├── dhcp_service.h/c       ← DHCP refatorado
    ├── dns_service.h/c
    ├── ntp_service.h/c
    ├── mdns_service.h/c
    └── telnet_service.h/c
```

#### 1.2. Manter código atual funcionando
- **NÃO** apagar arquivos antigos ainda
- Novos arquivos coexistem com os antigos
- Migrar incrementalmente

---

### **FASE 2: Extrair Driver W5500 Puro**

#### 2.1. Criar `w5500_regs.h`
Mover TODAS as definições de registradores do `w5500.h` atual:
```c
// De w5500.h → w5500_regs.h
#define W5500_COMMON_MR      0x0000
#define W5500_COMMON_GAR     0x0001
#define W5500_Sn_MR          0x0000
// ... etc
```

#### 2.2. Criar `w5500_ll.h/c` (Low-Level)
Funções SPI básicas de `w5500.c`:
```c
// Extrair estas funções:
W5500_SetAdressControlPhase()
W5500_SetCommand()
W5500_Transmit()
W5500_Receive()
```

#### 2.3. Criar `w5500_hw.h/c` (Driver de Hardware)
Funções de controle do chip, sem conhecer serviços:
```c
typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *rst_port;
    uint16_t rst_pin;
    uint8_t int_pin;
    
    // Buffers SPI
    uint8_t tx_buf[2048];
    uint8_t rx_buf[2048];
    
    // Configuração de rede
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
    
    // Hardware sockets (8 do W5500)
    struct {
        bool in_use;
        SocketType_t type;
        uint16_t local_port;
    } hw_sockets[8];
    
    // RTOS
    osSemaphoreId_t spi_sem;
    osMutexId_t spi_mutex;
    
} W5500_HW_t;

// API pública (extrair de w5500.c atual):
HAL_StatusTypeDef W5500_HW_Init(W5500_HW_t *hw, const W5500_HW_Config_t *config);
HAL_StatusTypeDef W5500_HW_Reset(W5500_HW_t *hw);
HAL_StatusTypeDef W5500_HW_SetIP(W5500_HW_t *hw, uint8_t *ip);
HAL_StatusTypeDef W5500_HW_SetMAC(W5500_HW_t *hw, uint8_t *mac);

// Sockets de hardware
uint8_t W5500_HW_SocketOpen(W5500_HW_t *hw, SocketType_t type, uint16_t port);
HAL_StatusTypeDef W5500_HW_SocketClose(W5500_HW_t *hw, uint8_t socket_id);
HAL_StatusTypeDef W5500_HW_SocketSend(W5500_HW_t *hw, uint8_t socket_id, 
                                       const uint8_t *data, uint16_t len);
HAL_StatusTypeDef W5500_HW_SocketRecv(W5500_HW_t *hw, uint8_t socket_id, 
                                       uint8_t *buf, uint16_t *len);
```

**IMPORTANTE:** Remover tudo relacionado a DHCP/DNS/etc do driver!

---

### **FASE 3: Implementar Camada de Abstração de Socket**

#### 3.1. Implementar `net_socket.c`

```c
// net_socket.c (implementação usando W5500 como backend)

#include "net_socket.h"
#include "Driver/W5500/w5500_hw.h"

// Estrutura interna do socket (opaca para usuários)
struct NetSocket {
    uint8_t hw_socket_id;        // ID no W5500 (0-7)
    W5500_HW_t *hw_driver;       // Ponteiro para driver
    
    NetSocketType_t type;
    NetSocketState_t state;
    
    uint16_t local_port;
    NetAddr_t remote_addr;
    
    NetSocketRxCallback_t rx_callback;
    void *rx_callback_ctx;
    
    bool in_use;
};

// Pool de sockets (8 no W5500)
static NetSocket_t g_socket_pool[8];
static W5500_HW_t *g_hw_driver;

// Inicialização do pool (chamado pelo manager)
void NetSocket_InitPool(W5500_HW_t *hw) {
    g_hw_driver = hw;
    for (int i = 0; i < 8; i++) {
        g_socket_pool[i].hw_socket_id = i;
        g_socket_pool[i].hw_driver = hw;
        g_socket_pool[i].in_use = false;
        g_socket_pool[i].state = NET_SOCKET_STATE_CLOSED;
    }
}

// Implementar todas as funções declaradas em net_socket.h
NetSocket_t* NetSocket_Create(NetSocketType_t type) {
    // Procurar socket livre no pool
    for (int i = 0; i < 8; i++) {
        if (!g_socket_pool[i].in_use) {
            g_socket_pool[i].in_use = true;
            g_socket_pool[i].type = type;
            g_socket_pool[i].state = NET_SOCKET_STATE_CLOSED;
            return &g_socket_pool[i];
        }
    }
    return NULL;  // Pool esgotado
}

NetErr_t NetSocket_Bind(NetSocket_t *sock, uint16_t port) {
    if (!sock) return NET_ERR_INVALID_ARG;
    
    // Chamar função do W5500
    uint8_t hw_id = W5500_HW_SocketOpen(
        sock->hw_driver,
        sock->type,
        port
    );
    
    if (hw_id == 0xFF) {
        return NET_ERR_HW_FAILURE;
    }
    
    sock->local_port = port;
    sock->state = NET_SOCKET_STATE_IDLE;
    
    return NET_OK;
}

// ... implementar demais funções
```

---

### **FASE 4: Implementar Network Manager**

#### 4.1. Implementar `net_manager.c`

```c
// net_manager.c

#include "net_manager.h"

NetErr_t NetManager_Init(
    NetManager_t *mgr,
    W5500_HW_t *hw_driver,
    const NetManagerConfig_t *config
) {
    if (!mgr || !hw_driver) {
        return NET_ERR_INVALID_ARG;
    }
    
    memset(mgr, 0, sizeof(NetManager_t));
    
    mgr->hw_driver = hw_driver;
    
    // Copiar configuração ou usar padrões
    if (config) {
        mgr->config = *config;
    } else {
        // Defaults
        mgr->config.task_period_ms = 100;
        mgr->config.use_dhcp = true;
        mgr->config.enable_link_monitor = true;
    }
    
    // Inicializar pool de sockets
    NetSocket_InitPool(hw_driver);
    
    mgr->state = NET_MANAGER_STATE_INITIALIZING;
    
    return NET_OK;
}

void NetManager_Task(NetManager_t *mgr) {
    if (!mgr) return;
    
    // 1. Verificar link físico
    if (mgr->config.enable_link_monitor) {
        bool link_up;
        W5500_HW_ReadLinkStatus(mgr->hw_driver, &link_up);
        
        if (link_up != mgr->link_up) {
            mgr->link_up = link_up;
            
            NetEvent_t event;
            event.type = link_up ? NET_EVENT_LINK_UP : NET_EVENT_LINK_DOWN;
            NetManager_PostEvent(mgr, &event);
        }
    }
    
    // 2. Polling de sockets (RX/TX)
    NetSocket_PollAll();  // Implementar esta função
    
    // 3. Processar fila de eventos
    while (mgr->event_queue_head != mgr->event_queue_tail) {
        NetEvent_t *event = &mgr->event_queue[mgr->event_queue_tail];
        
        // Despachar para todos os serviços
        for (int i = 0; i < mgr->service_count; i++) {
            NetService_t *svc = mgr->services[i];
            if (svc->enabled && svc->event_handler) {
                svc->event_handler(svc, event);
            }
        }
        
        // Callback global
        if (mgr->global_event_callback) {
            mgr->global_event_callback(event, mgr->global_event_ctx);
        }
        
        mgr->event_queue_tail = (mgr->event_queue_tail + 1) % 16;
    }
    
    // 4. Chamar tasks de todos os serviços registrados
    for (int i = 0; i < mgr->service_count; i++) {
        NetService_t *svc = mgr->services[i];
        if (svc->enabled && svc->task) {
            svc->task(svc);
        }
    }
}

NetErr_t NetManager_RegisterService(NetManager_t *mgr, NetService_t *service) {
    if (!mgr || !service) {
        return NET_ERR_INVALID_ARG;
    }
    
    if (mgr->service_count >= NET_MANAGER_MAX_SERVICES) {
        return NET_ERR_NO_MEMORY;
    }
    
    mgr->services[mgr->service_count++] = service;
    
    // Chamar init do serviço
    if (service->init) {
        service->init(service, mgr);
    }
    
    return NET_OK;
}

// ... implementar demais funções
```

---

### **FASE 5: Refatorar Serviços**

#### 5.1. Refatorar DHCP (`dhcp_service.c`)

**Antes** (ethernet_dhcp.c):
```c
void DHCP_FSM(W5500_Driver_t *drv) {
    // Acessa drv->dhcp
    // Usa drv->sockets[drv->dhcp.socket]
    // Chama W5500_SocketSend() diretamente
}
```

**Depois** (dhcp_service.c):
```c
void DHCP_Task(DHCP_Client_t *dhcp) {
    switch (dhcp->state) {
        case DHCP_STATE_SELECTING:
            if (timeout) {
                // Enviar DISCOVER via NetSocket API
                NetSocket_SendTo(
                    dhcp->socket,
                    discover_packet,
                    len,
                    &broadcast_addr
                );
            }
            break;
            
        // ... demais estados
    }
}

// Callback de RX (chamado quando resposta DHCP chega)
void DHCP_RxCallback(
    uint8_t socket_id,
    const uint8_t *data,
    uint16_t len,
    const NetAddr_t *remote_addr,
    void *user_ctx
) {
    DHCP_Client_t *dhcp = (DHCP_Client_t*)user_ctx;
    
    // Processar pacote DHCP
    uint8_t msg_type = parse_dhcp_packet(data, len, &dhcp->offered_ip, ...);
    
    if (msg_type == DHCP_MSG_OFFER) {
        dhcp->state = DHCP_STATE_REQUESTING;
        // Enviar REQUEST
    } else if (msg_type == DHCP_MSG_ACK) {
        dhcp->state = DHCP_STATE_BOUND;
        
        // Postar evento
        NetEvent_t event = {
            .type = NET_EVENT_DHCP_BOUND,
            .data = &dhcp->offered_ip
        };
        NetManager_PostEvent(dhcp->manager, &event);
    }
}
```

#### 5.2. Repetir para DNS, NTP, Telnet, mDNS
Cada serviço:
- Remove dependências de `W5500_Driver_t`
- Usa `NetSocket_t*` obtido do manager
- Implementa callbacks de RX
- Implementa interface `NetService_t`

---

### **FASE 6: Migrar Aplicação**

#### 6.1. Modificar `app_freertos.c`

**Antes:**
```c
void NetworkTask(void *arg) {
    W5500_Driver_t *drv = (W5500_Driver_t*)arg;
    
    while(1) {
        W5500_NetworkManagerTask(drv);  // Função monolítica antiga
        osDelay(100);
    }
}
```

**Depois:**
```c
void NetworkTask(void *arg) {
    // Setup (ver EXAMPLE_USAGE.c)
    Network_Setup();
    
    while(1) {
        NetManager_Task(&net_manager);  // Nova arquitetura em camadas
        osDelay(100);
    }
}
```

#### 6.2. Refatorar CLI/Telnet
Se você tem comandos CLI no Telnet, mantê-los separados:
```
Core/Application/
└── cli_commands.c    ← Comandos de diagnóstico
```

Telnet vira apenas o "transporte":
```c
// Telnet recebe comando, repassa para CLI processor
void Telnet_ProcessCommand(char *cmd) {
    CLI_Execute(cmd, Telnet_Printf);
}
```

---

### **FASE 7: Limpeza e Remoção do Código Antigo**

Após tudo funcionando:
1. Remover `w5500.h/c` antigos
2. Remover `ethernet_dhcp.h/c` antigos
3. Remover `network_manager.h/c` antigos (se existirem)
4. Atualizar includes em todos os arquivos

---

## 🧪 Como Testar Durante a Migração

### Teste 1: Driver W5500 puro
```c
W5500_HW_t hw;
W5500_HW_Init(&hw, &config);

uint8_t version;
W5500_HW_ReadVersion(&hw, &version);
assert(version == 0x04);

printf("W5500 OK!\n");
```

### Teste 2: Socket API
```c
NetSocket_t *sock = NetSocket_Create(NET_SOCKET_TYPE_UDP);
assert(sock != NULL);

NetSocket_Bind(sock, 1234);
assert(NetSocket_GetLocalPort(sock) == 1234);

printf("Socket API OK!\n");
```

### Teste 3: DHCP isolado
```c
DHCP_Client_t dhcp;
DHCP_Init(&dhcp, "test");
DHCP_Start(&dhcp, &net_manager);

// Simular timeout e verificar se envia DISCOVER
DHCP_Task(&dhcp);
// ...

printf("DHCP OK!\n");
```

---

## ⚠️ Pontos de Atenção

### 1. Callbacks e Contexto RTOS
- Callbacks de RX podem ser chamados em interrupção (se usar IRQ do W5500)
- Usar filas RTOS se necessário processar em task

### 2. Pool de Sockets Limitado
- W5500 tem apenas 8 sockets de hardware
- Gerenciar prioridades (DHCP > Telnet, por exemplo)
- Liberar sockets quando não usados

### 3. Endianness
- Pacotes de rede são Big-Endian
- STM32 é Little-Endian
- Usar `htons()`, `htonl()`, `ntohs()`, `ntohl()`

### 4. Buffers e Fragmentação
- W5500 tem buffers internos (2KB TX/RX por socket por padrão)
- Cuidado com pacotes grandes
- Implementar reassembly se necessário

---

## 📅 Cronograma Sugerido

| Semana | Atividade |
|--------|-----------|
| 1 | FASE 1-2: Criar estrutura, extrair driver W5500 |
| 2 | FASE 3: Implementar net_socket.c |
| 3 | FASE 4: Implementar net_manager.c |
| 4 | FASE 5: Refatorar DHCP + DNS |
| 5 | FASE 5 (cont): Refatorar NTP + Telnet + mDNS |
| 6 | FASE 6: Migrar aplicação, testes integrados |
| 7 | FASE 7: Limpeza, documentação, otimizações |

---

## 🎯 Resultado Esperado

Arquitetura modular onde:
- **Driver W5500** não conhece protocolos
- **Serviços** são independentes e reutilizáveis
- **Aplicação** usa API limpa de sockets
- **Fácil** adicionar HTTP, MQTT, CoAP, etc.
- **Portável** para outros hardwares (ESP32, LAN8742, etc.)

---

## 📚 Referências

- [ARCHITECTURE.md](ARCHITECTURE.md) - Documentação detalhada
- [EXAMPLE_USAGE.c](EXAMPLE_USAGE.c) - Exemplos de uso
- Specifications: RFC 2131 (DHCP), RFC 1035 (DNS), RFC 5905 (NTP)

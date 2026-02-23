# Diagramas de Arquitetura - Stack de Rede STM32 W5500

## 1. Visão Geral das Camadas

```
┌─────────────────────────────────────────────────────────────────┐
│                    APLICAÇÃO (main.c)                           │
│  - Lógica de negócio                                            │
│  - Comandos CLI                                                  │
│  - State machines específicas                                    │
└─────────────────────────────────────────────────────────────────┘
                              ↓↑
┌─────────────────────────────────────────────────────────────────┐
│              SERVIÇOS DE REDE (Network Services)                │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │  DHCP    │ │   DNS    │ │   NTP    │ │ Telnet   │  ...      │
│  │  Client  │ │  Client  │ │  Client  │ │  Server  │           │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │
│  - Implementam protocolos específicos                            │
│  - Registrados como NetService_t                                 │
└─────────────────────────────────────────────────────────────────┘
                              ↓↑
┌─────────────────────────────────────────────────────────────────┐
│            GERENCIADOR DE REDE (Network Manager)                │
│  - Pool de sockets (alocação/liberação)                         │
│  - Dispatch de eventos                                           │
│  - Coordenação entre serviços                                    │
│  - Task principal (polling + callbacks)                          │
└─────────────────────────────────────────────────────────────────┘
                              ↓↑
┌─────────────────────────────────────────────────────────────────┐
│              ABSTRAÇÃO DE SOCKET (Socket API)                   │
│  - NetSocket_Create(), Bind(), Connect(), Send(), Recv()        │
│  - API genérica (tipo Berkeley Sockets)                          │
│  - Esconde detalhes de implementação                             │
└─────────────────────────────────────────────────────────────────┘
                              ↓↑
┌─────────────────────────────────────────────────────────────────┐
│                 DRIVER W5500 (Hardware)                         │
│  - Controle de registradores                                     │
│  - Sockets de hardware (8x)                                      │
│  - Buffers TX/RX internos                                        │
└─────────────────────────────────────────────────────────────────┘
                              ↓↑
┌─────────────────────────────────────────────────────────────────┐
│                   HAL STM32 (SPI, GPIO, DMA)                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Fluxo de RX (Recepção de Dados)

```
[Pacote chega no W5500]
        ↓
[W5500 Hardware: armazena em buffer interno]
        ↓
[INT/IRQ ou Polling detecta dados disponíveis]
        ↓
┌───────────────────────────────────────┐
│ W5500_HW_SocketRecv()                 │
│ - Lê buffer do chip via SPI           │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ NetSocket: identifica socket abstrato │
│ - socket_id → NetSocket_t*            │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ NetManager: despacha para callback    │
│ - Chama NetSocketRxCallback_t         │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ Serviço/Aplicação: processa dados     │
│ Ex: DHCP_RxCallback()                 │
│     - Parse do pacote DHCP            │
│     - Atualiza FSM                    │
│     - Posta evento se necessário      │
└───────────────────────────────────────┘
```

---

## 3. Fluxo de TX (Envio de Dados)

```
[Aplicação quer enviar dados]
        ↓
┌───────────────────────────────────────┐
│ NetSocket_Send()                      │
│ ou NetSocket_SendTo()                 │
│ - Valida parâmetros                   │
│ - Verifica estado do socket           │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ NetSocket: traduz para HW socket      │
│ - NetSocket_t* → hw_socket_id         │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ W5500_HW_SocketSend()                 │
│ - Escreve buffer do chip via SPI      │
│ - Seta comando SEND                   │
└───────────────────────────────────────┘
        ↓
┌───────────────────────────────────────┐
│ W5500 Hardware: envia pacote na rede  │
└───────────────────────────────────────┘
```

---

## 4. Inicialização e Registro de Serviços

```
main() ou app_freertos.c
        ↓
┌─────────────────────────────────────────────────┐
│ 1. Inicializar hardware W5500                   │
│    W5500_HW_Init(&hw, &config)                  │
│    - Reset do chip                              │
│    - Configurar MAC, IP (se estático)           │
│    - Inicializar sockets de hardware            │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 2. Inicializar Network Manager                  │
│    NetManager_Init(&mgr, &hw, &mgr_config)      │
│    - Criar pool de sockets                      │
│    - Configurar DHCP/estático                   │
│    - Inicializar fila de eventos                │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 3. Criar e registrar serviços                   │
│                                                  │
│    DHCP_Init(&dhcp, "hostname")                 │
│    dhcp_service = DHCP_CREATE_SERVICE(&dhcp)    │
│    NetManager_RegisterService(&mgr, &dhcp_svc)  │
│                                                  │
│    DNS_Init(&dns)                               │
│    dns_service = DNS_CREATE_SERVICE(&dns)       │
│    NetManager_RegisterService(&mgr, &dns_svc)   │
│                                                  │
│    ... (NTP, Telnet, etc)                       │
└─────────────────────────────────────────────────┘
        ↓
┌─────────────────────────────────────────────────┐
│ 4. Loop principal (RTOS Task)                   │
│                                                  │
│    while(1) {                                   │
│        NetManager_Task(&mgr);                   │
│        osDelay(100);                            │
│    }                                            │
│                                                  │
│    NetManager_Task() chama:                     │
│    - Polling de sockets                         │
│    - Despacho de eventos                        │
│    - service->task() de cada serviço            │
└─────────────────────────────────────────────────┘
```

---

## 5. Ciclo de Vida de um Socket

```
┌──────────────┐
│   CLOSED     │  Estado inicial
└──────────────┘
       ↓ NetSocket_Create()
┌──────────────┐
│   CREATED    │  Socket alocado, mas não configurado
└──────────────┘
       ↓ NetSocket_Bind()
┌──────────────┐
│     IDLE     │  Bound a uma porta local
└──────────────┘
       ↓
       ├──→ TCP Server: NetSocket_Listen()     ┌──────────────┐
       │                                        │   LISTEN     │
       │                                        └──────────────┘
       │                                               ↓ Cliente conecta
       │                                        ┌──────────────┐
       │                                        │ ESTABLISHED  │
       │                                        └──────────────┘
       │
       ├──→ TCP Client: NetSocket_Connect()    ┌──────────────┐
       │                                        │ CONNECTING   │
       │                                        └──────────────┘
       │                                               ↓ ACK recebido
       │                                        ┌──────────────┐
       │                                        │ ESTABLISHED  │
       │                                        └──────────────┘
       │
       └──→ UDP: (permanece em IDLE)           ┌──────────────┐
                 NetSocket_Send/RecvFrom()     │     IDLE     │
                                                └──────────────┘
```

---

## 6. Sistema de Eventos

```
┌────────────────────────────────────────────────────────┐
│                  EVENTO OCORRE                         │
│  Ex: Link físico conecta, DHCP obtém IP, etc.         │
└────────────────────────────────────────────────────────┘
                        ↓
┌────────────────────────────────────────────────────────┐
│  NetManager_PostEvent(&event)                          │
│  - Adiciona evento na fila circular                    │
└────────────────────────────────────────────────────────┘
                        ↓
┌────────────────────────────────────────────────────────┐
│  NetManager_Task() - Processa fila                     │
│                                                         │
│  Para cada evento na fila:                             │
│    1. Despacha para TODOS os serviços registrados      │
│       - Chama service->event_handler(event)            │
│                                                         │
│    2. Chama callback global (se configurado)           │
│       - global_event_callback(event, ctx)              │
└────────────────────────────────────────────────────────┘
                        ↓
┌────────────────────────────────────────────────────────┐
│  Serviços reagem ao evento                             │
│                                                         │
│  Ex: NET_EVENT_LINK_DOWN                               │
│      - DHCP para de renovar                            │
│      - Telnet fecha conexões                           │
│      - NTP para queries                                │
│                                                         │
│  Ex: NET_EVENT_DHCP_BOUND                              │
│      - DNS inicia queries                              │
│      - NTP sincroniza relógio                          │
│      - Telnet inicia servidor                          │
└────────────────────────────────────────────────────────┘
```

---

## 7. Exemplo Concreto: Cliente DHCP

### Estado IDLE → BOUND

```
T=0s  [DHCP_Start()]
      ├─ Aloca socket UDP
      ├─ Bind na porta 68
      ├─ Configura callback
      └─ Estado = SELECTING

T=0s  [DHCP_Task()]
      └─ Envia DHCP DISCOVER (broadcast)
         NetSocket_SendTo(socket, discover, 255.255.255.255:67)

T=1s  [Servidor DHCP responde]
      └─ Pacote chega no W5500 socket 0

T=1s  [NetManager_Task()]
      ├─ Polling detecta dados disponíveis
      ├─ W5500_HW_SocketRecv(socket 0, buffer)
      └─ Chama DHCP_RxCallback(buffer)

T=1s  [DHCP_RxCallback()]
      ├─ Parse: tipo = OFFER
      ├─ Extrai offered_ip, server_ip
      ├─ Estado = REQUESTING
      └─ (REQUEST será enviado no próximo Task())

T=1.1s [DHCP_Task()]
       └─ Envia DHCP REQUEST
          NetSocket_SendTo(socket, request, 255.255.255.255:67)

T=2s  [Servidor DHCP responde]
      └─ Pacote ACK chega

T=2s  [DHCP_RxCallback()]
      ├─ Parse: tipo = ACK
      ├─ Extrai lease_time, subnet, gateway, dns
      ├─ Estado = BOUND
      └─ Posta evento: NET_EVENT_DHCP_BOUND

T=2s  [NetManager_Task()]
      ├─ Processa evento DHCP_BOUND
      ├─ Despacha para todos os serviços
      ├─ NTP recebe evento → inicia sync
      ├─ Telnet recebe evento → inicia servidor
      └─ Callback global mostra no printf/LED
```

---

## 8. Comparação: Função Antiga vs. Nova

### ❌ ANTES (Código Monolítico)

```c
// w5500.c - TUDO misturado!
void W5500_NetworkManagerTask(W5500_Driver_t *drv) {
    // Verificar link
    // Processar DHCP
    // Processar DNS
    // Processar NTP
    // Processar Telnet
    // Processar mDNS
    // ...
    // IMPOSSÍVEL DE MANTER!
}

// Para adicionar HTTP? Modificar w5500.c!
// Para testar DHCP? Precisa do hardware completo!
```

### ✅ DEPOIS (Arquitetura Modular)

```c
// net_manager.c - Coordenação limpa
void NetManager_Task(NetManager_t *mgr) {
    NetSocket_PollAll();           // Polling de RX/TX
    ProcessEventQueue(mgr);        // Despacha eventos
    
    for (each service) {
        service->task(service);    // Cada serviço cuida de si
    }
}

// dhcp_service.c - Isolado
void DHCP_Task(DHCP_Client_t *dhcp) {
    switch (dhcp->state) {
        case DHCP_STATE_SELECTING:
            // Lógica só do DHCP
            break;
    }
}

// Para adicionar HTTP? Criar http_service.c e registrar!
// Para testar DHCP? Mock do NetSocket!
```

---

## 9. Dependências Entre Módulos

```
┌──────────────┐
│  Application │
└──────┬───────┘
       │ usa
       ↓
┌──────────────┐     registra     ┌──────────────┐
│   Services   │ ←───────────────→│    Manager   │
└──────┬───────┘                  └──────┬───────┘
       │ usa                              │ gerencia
       ↓                                  ↓
┌──────────────┐                  ┌──────────────┐
│ Socket API   │←─────────────────│ Socket Pool  │
└──────┬───────┘                  └──────────────┘
       │ chama
       ↓
┌──────────────┐
│  W5500 HW    │
└──────┬───────┘
       │ acessa
       ↓
┌──────────────┐
│   HAL SPI    │
└──────────────┘

Regras:
- Setas apontam para dependências
- Camadas inferiores NÃO conhecem superiores
- W5500 HW não conhece Services
- Services não conhecem W5500 HW diretamente
```

---

## 10. Estrutura de Arquivos (Resumo)

```
W5500/
├── ARCHITECTURE.md         ← Documentação da arquitetura
├── MIGRATION_GUIDE.md      ← Guia de migração
├── EXAMPLE_USAGE.c         ← Exemplos de código
├── DIAGRAMS.md             ← Este arquivo
│
├── Core/
│   ├── Inc/
│   │   └── main.h, spi.h, gpio.h, ...  (STM32 HAL)
│   │
│   ├── Src/
│   │   ├── main.c
│   │   ├── app_freertos.c              ← Task principal
│   │   └── spi.c, gpio.c, ...          (STM32 HAL)
│   │
│   └── Network/
│       ├── net_types.h                 ← Tipos comuns
│       ├── net_socket.h/c              ← API de socket
│       ├── net_manager.h/c             ← Gerenciador
│       │
│       ├── Driver/
│       │   └── W5500/
│       │       ├── w5500_regs.h        ← Registradores
│       │       ├── w5500_ll.h/c        ← Low-level SPI
│       │       └── w5500_hw.h/c        ← Driver de hardware
│       │
│       └── Services/
│           ├── dhcp_service.h/c
│           ├── dns_service.h/c
│           ├── ntp_service.h/c
│           ├── mdns_service.h/c
│           └── telnet_service.h/c
│
└── Drivers/                            (Fornecidos pela ST)
    ├── CMSIS/
    └── STM32G0xx_HAL_Driver/
```

---

## Resumo

Esta arquitetura em camadas oferece:

✅ **Separação clara de responsabilidades**
✅ **Código modular e reutilizável**
✅ **Fácil de testar e debugar**
✅ **Extensível para novos protocolos**
✅ **Portável entre diferentes hardwares**
✅ **Manutenível a longo prazo**

Cada camada tem sua interface bem definida e não depende de detalhes de implementação das outras camadas.

# STM32 W5500 Network Stack - Arquitetura Modular

Implementação de stack de rede em camadas para STM32 com chip Ethernet W5500.

## 🎯 Motivação

Este projeto refatora a arquitetura monolítica típica de projetos STM32 + W5500 em uma **arquitetura em camadas modular**, seguindo princípios de Clean Architecture e SOLID.

### Problemas do Código Monolítico

❌ Driver W5500 acoplado aos serviços (DHCP, DNS, NTP, Telnet)  
❌ Difícil adicionar novos protocolos sem modificar código existente  
❌ Impossível testar serviços isoladamente  
❌ Código não reutilizável em outros projetos  
❌ Dependências circulares e alta complexidade  

### Solução: Arquitetura em Camadas

✅ **Camadas independentes** com responsabilidades únicas  
✅ **Desacoplamento total** entre hardware e serviços  
✅ **API de socket abstrata** (tipo Berkeley Sockets)  
✅ **Serviços como plugins** registrados dinamicamente  
✅ **Fácil testar** com mocks  
✅ **Portável** para outros hardwares (ESP32, LAN8742, etc.)  

---

## 📚 Documentação

| Documento | Descrição |
|-----------|-----------|
| **[ARCHITECTURE.md](ARCHITECTURE.md)** | 📐 Arquitetura completa em camadas |
| **[MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)** | 🗺️ Guia passo a passo de migração |
| **[DIAGRAMS.md](DIAGRAMS.md)** | 📊 Diagramas visuais e fluxos |
| **[EXAMPLE_USAGE.c](EXAMPLE_USAGE.c)** | 💻 Exemplos práticos de código |

**↑ Comece por ARCHITECTURE.md para entender o design!**

---

## 🏗️ Arquitetura em Resumo

```
┌───────────────────────────────────────────┐
│    APPLICATION (main.c, CLI)              │  ← Sua lógica
└───────────────────────────────────────────┘
                   ↕
┌───────────────────────────────────────────┐
│ SERVICES (DHCP, DNS, NTP, Telnet, HTTP)   │  ← Protocolos
└───────────────────────────────────────────┘
                   ↕
┌───────────────────────────────────────────┐
│  NETWORK MANAGER (coordenação, eventos)   │  ← Orquestração
└───────────────────────────────────────────┘
                   ↕
┌───────────────────────────────────────────┐
│   SOCKET API (abstração genérica)         │  ← Interface limpa
└───────────────────────────────────────────┘
                   ↕
┌───────────────────────────────────────────┐
│      W5500 DRIVER (hardware only)         │  ← Baixo nível
└───────────────────────────────────────────┘
                   ↕
┌───────────────────────────────────────────┐
│         STM32 HAL (SPI, GPIO, DMA)        │  ← Periféricos
└───────────────────────────────────────────┘
```

**Regra de ouro:** Cada camada só conhece a camada imediatamente abaixo.

---

## 📁 Estrutura de Arquivos

```
Core/Network/
├── net_types.h              # Tipos comuns (NetIP_t, NetAddr_t, etc)
├── net_socket.h/c           # API abstrata de sockets
├── net_manager.h/c          # Gerenciador central
│
├── Driver/W5500/            # Camada de hardware
│   ├── w5500_regs.h         # Definições de registradores
│   ├── w5500_ll.h/c         # Low-level SPI
│   └── w5500_hw.h/c         # Driver principal
│
└── Services/                # Protocolos independentes
    ├── dhcp_service.h/c     # Cliente DHCP
    ├── dns_service.h/c      # Cliente DNS
    ├── ntp_service.h/c      # Cliente NTP
    ├── mdns_service.h/c     # Responder mDNS
    └── telnet_service.h/c   # Servidor Telnet
```

---

## 🚀 Quick Start

### 1. Inicialização Básica

```c
#include "net_manager.h"
#include "Driver/W5500/w5500_hw.h"
#include "Services/dhcp_service.h"

// Globals
NetManager_t net_manager;
W5500_HW_t w5500_hw;
DHCP_Client_t dhcp_client;
NetService_t dhcp_service;

void Network_Setup(void) {
    // 1. Inicializar hardware W5500
    W5500_HW_Config_t hw_cfg = {
        .hspi = &hspi1,
        .cs_port = GPIOA,
        .cs_pin = GPIO_PIN_4,
    };
    W5500_HW_Init(&w5500_hw, &hw_cfg);
    
    // 2. Inicializar manager
    NetManagerConfig_t mgr_cfg = {
        .network.dhcp_enabled = true,
        .task_period_ms = 100,
    };
    NetManager_Init(&net_manager, &w5500_hw, &mgr_cfg);
    
    // 3. Registrar serviços
    DHCP_Init(&dhcp_client, "STM32-Device");
    dhcp_service = DHCP_CREATE_SERVICE(&dhcp_client);
    NetManager_RegisterService(&net_manager, &dhcp_service);
}

void NetworkTask(void *arg) {
    Network_Setup();
    
    while(1) {
        NetManager_Task(&net_manager);  // Chama tudo!
        osDelay(100);
    }
}
```

### 2. Usar Socket UDP

```c
// Criar socket
NetSocket_t *sock = NetManager_AllocSocket(&net_manager, NET_SOCKET_TYPE_UDP);

// Bind na porta
NetSocket_Bind(sock, 1234);

// Configurar callback de RX
NetSocket_SetRxCallback(sock, MyRxCallback, &my_context);

// Enviar dados
NetAddr_t dest = {
    .ip = NET_IP(192, 168, 1, 100),
    .port = 5678
};
NetSocket_SendTo(sock, data, len, &dest);
```

### 3. Usar Socket TCP (Cliente)

```c
NetSocket_t *sock = NetManager_AllocSocket(&net_manager, NET_SOCKET_TYPE_TCP);

NetAddr_t server = {
    .ip = NET_IP(192, 168, 1, 50),
    .port = 80
};

NetSocket_Connect(sock, &server);

// Aguardar conexão
while (!NetSocket_IsConnected(sock)) {
    osDelay(10);
}

// Enviar HTTP request
NetSocket_Send(sock, "GET / HTTP/1.1\r\n...", ...);
```

---

## 🔑 Conceitos-Chave

### NetService_t - Interface de Serviço

Todos os serviços implementam:

```c
typedef struct NetService {
    const char *name;
    void (*init)(struct NetService *service, void *manager);
    void (*task)(struct NetService *service);
    void (*deinit)(struct NetService *service);
    void (*event_handler)(struct NetService *service, const NetEvent_t *event);
    void *context;
    bool enabled;
} NetService_t;
```

### Sistema de Eventos

```c
typedef enum {
    NET_EVENT_LINK_UP,
    NET_EVENT_LINK_DOWN,
    NET_EVENT_DHCP_BOUND,
    NET_EVENT_SOCKET_CONNECTED,
    // ...
} NetEventType_t;

// Serviços reagem a eventos
void DHCP_EventHandler(NetService_t *svc, const NetEvent_t *event) {
    if (event->type == NET_EVENT_LINK_DOWN) {
        // Parar renovação de lease
    }
}
```

### Pool de Sockets

W5500 tem 8 sockets de hardware. O Network Manager gerencia o pool:

```c
NetSocket_t* sock = NetManager_AllocSocket(&mgr, NET_SOCKET_TYPE_TCP);
// Usar socket...
NetManager_FreeSocket(&mgr, sock);  // Liberar de volta ao pool
```

---

## 🛠️ Migração do Código Antigo

Se você tem código legado (W5500_Driver_t com DHCP/DNS/etc embutidos):

1. **Leia [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md)** completo
2. **Fase 1-2:** Extrair driver W5500 puro (sem serviços)
3. **Fase 3:** Implementar `net_socket.c`
4. **Fase 4:** Implementar `net_manager.c`
5. **Fase 5:** Refatorar cada serviço (DHCP, DNS, NTP, Telnet)
6. **Fase 6:** Migrar aplicação para usar nova API
7. **Fase 7:** Remover código antigo

**Tempo estimado:** 4-7 semanas (trabalho incremental)

---

## 📖 Exemplos de Uso

Ver [EXAMPLE_USAGE.c](EXAMPLE_USAGE.c) para exemplos completos:

- ✅ Setup do Network Manager
- ✅ Registro de serviços (DHCP, DNS, NTP, Telnet)
- ✅ UDP Echo Server
- ✅ HTTP Client (TCP)
- ✅ TCP Echo Server
- ✅ Callbacks de eventos

---

## 🧪 Testes

A arquitetura em camadas facilita testes unitários:

```c
// Mock de socket para testar DHCP
NetSocket_t mock_socket;
mock_socket.send_fn = Mock_Send;  // Captura pacotes enviados
mock_socket.recv_fn = Mock_Recv;  // Simula respostas

// Testar DHCP isoladamente
DHCP_Client_t dhcp;
DHCP_Init(&dhcp, "test");
dhcp.socket = &mock_socket;

DHCP_Task(&dhcp);  // Deve enviar DISCOVER
assert(mock_socket.sent_packet_type == DHCP_MSG_DISCOVER);

// Simular OFFER
Mock_InjectPacket(&mock_socket, dhcp_offer_packet);
DHCP_Task(&dhcp);

assert(dhcp.state == DHCP_STATE_REQUESTING);
```

---

## 🎓 Princípios de Design

| Princípio | Como aplicado |
|-----------|---------------|
| **Single Responsibility** | Cada camada/módulo tem uma função |
| **Open/Closed** | Fácil estender (novo serviço) sem modificar core |
| **Dependency Inversion** | Camadas superiores não dependem de hardware |
| **Interface Segregation** | APIs pequenas e focadas (NetSocket, NetService) |
| **DRY** | Código compartilhado em net_types.h |

---

## 🔧 Tecnologias

- **MCU:** STM32G0B1 (qualquer STM32 serve)
- **Ethernet:** W5500 (SPI)
- **RTOS:** FreeRTOS
- **HAL:** STM32 HAL
- **Stack:** Próprio (não usa lwIP)

---

## 📊 Comparação com Alternativas

| Aspecto | Este projeto | lwIP | Código monolítico |
|---------|--------------|------|-------------------|
| Modularidade | ★★★★★ | ★★★☆☆ | ★☆☆☆☆ |
| Simplicidade | ★★★★☆ | ★★☆☆☆ | ★★★★☆ |
| Tamanho de código | Médio | Grande | Pequeno |
| Testabilidade | ★★★★★ | ★★★☆☆ | ★☆☆☆☆ |
| Portabilidade | ★★★★★ | ★★★★☆ | ★☆☆☆☆ |
| Curva de aprendizado | Moderada | Alta | Baixa |

---

## 🤝 Contribuindo

Este é um projeto de arquitetura de referência. Sinta-se livre para:

- Adicionar novos serviços (HTTP, MQTT, CoAP, etc.)
- Portar para outros hardwares (ESP32-C3, LAN8742, etc.)
- Melhorar performance
- Adicionar testes unitários
- Melhorar documentação

---

## 📝 Licença

Este código é fornecido como exemplo educacional. Use como base para seus projetos.

---

## 🙏 Créditos

Baseado em:
- **W5500 Datasheet** (WIZnet)
- **RFC 2131** (DHCP)
- **RFC 1035** (DNS)
- **RFC 5905** (NTPv4)
- **Berkeley Sockets API**
- **Clean Architecture** (Robert C. Martin)

---

## 📬 Contato

Para dúvidas sobre a arquitetura, consulte os documentos:
1. [ARCHITECTURE.md](ARCHITECTURE.md) - Design detalhado
2. [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Passo a passo
3. [DIAGRAMS.md](DIAGRAMS.md) - Diagramas visuais
4. [EXAMPLE_USAGE.c](EXAMPLE_USAGE.c) - Código de exemplo

---

**Boa sorte com seu projeto de rede STM32! 🚀**

# Arquitetura de Rede - STM32 W5500

## 📋 Visão Geral

Este documento descreve a arquitetura em camadas para o stack de rede do projeto STM32 com W5500.

## 🎯 Objetivos da Arquitetura

1. **Modularidade**: Cada camada tem responsabilidade única
2. **Desacoplamento**: Serviços não conhecem detalhes de hardware
3. **Testabilidade**: Camadas podem ser testadas isoladamente
4. **Extensibilidade**: Fácil adicionar novos serviços ou trocar hardware
5. **Reusabilidade**: Código pode ser portado para outros projetos

---

## 🏗️ Camadas da Arquitetura

### **CAMADA 1: HAL STM32** (Fornecida pela ST)
```
Core/Inc/spi.h, gpio.h, dma.h
Core/Src/spi.c, gpio.c, dma.c
```
- **Responsabilidade**: Interface com periféricos STM32
- **Não modificar**: Código gerado pelo CubeMX

---

### **CAMADA 2: Driver W5500 (Hardware)**
```
Core/Network/Driver/W5500/
├── w5500_ll.h/c          → Low-level: SPI, registradores
├── w5500_hw_socket.h/c   → Sockets em hardware W5500
└── w5500_regs.h          → Definições de registradores
```

#### Responsabilidades:
- Comunicação SPI com W5500
- Leitura/escrita de registradores
- Gerenciamento dos 8 sockets de hardware
- IRQ handling do chip

#### **NÃO** deve conhecer:
- Protocolos de aplicação (DHCP, DNS, etc.)
- Lógica de negócio
- Gerenciador de rede

#### API Típica:
```c
HAL_StatusTypeDef W5500_Init(W5500_HW_t *hw);
HAL_StatusTypeDef W5500_Reset(W5500_HW_t *hw);
uint8_t W5500_HW_SocketOpen(W5500_HW_t *hw, SocketType_t type, uint16_t port);
HAL_StatusTypeDef W5500_HW_SocketClose(W5500_HW_t *hw, uint8_t socket_id);
HAL_StatusTypeDef W5500_HW_SocketSend(W5500_HW_t *hw, uint8_t socket_id, 
                                       const uint8_t *data, uint16_t len);
HAL_StatusTypeDef W5500_HW_SocketRecv(W5500_HW_t *hw, uint8_t socket_id, 
                                       uint8_t *buffer, uint16_t *len);
```

---

### **CAMADA 3: Abstração de Socket (Socket API)**
```
Core/Network/Socket/
├── net_socket.h/c    → API abstrata de socket
└── net_types.h       → Tipos comuns (IP, porta, etc.)
```

#### Responsabilidades:
- API genérica de sockets (independente de W5500/lwIP/etc)
- Conversão entre API abstrata e hardware específico
- Permite trocar backend (W5500 → ESP32, por exemplo)

#### API Abstrata:
```c
NetSocket_t* NetSocket_Create(SocketType_t type);
NetErr_t NetSocket_Bind(NetSocket_t *sock, uint16_t port);
NetErr_t NetSocket_Listen(NetSocket_t *sock, uint8_t backlog);
NetErr_t NetSocket_Connect(NetSocket_t *sock, const NetAddr_t *addr);
NetErr_t NetSocket_Send(NetSocket_t *sock, const uint8_t *data, uint16_t len);
NetErr_t NetSocket_Recv(NetSocket_t *sock, uint8_t *buffer, uint16_t *len);
NetErr_t NetSocket_Close(NetSocket_t *sock);
void NetSocket_SetCallback(NetSocket_t *sock, NetSocketCallback_t cb, void *ctx);
```

---

### **CAMADA 4: Gerenciador de Rede (Network Manager)**
```
Core/Network/Manager/
├── net_manager.h/c       → Core do gerenciador
├── net_config.h          → Configurações (DHCP on/off, etc.)
└── net_event.h/c         → Sistema de eventos
```

#### Responsabilidades:
- **Inicialização e configuração da rede**
- **Multiplexação de sockets** entre serviços
- **Dispatch de eventos** (link up/down, DHCP complete, etc.)
- **Coordenação de serviços** (quando DHCP terminar, inicia NTP)
- **Pool de sockets** (gerencia os 8 sockets do W5500)

#### NÃO implementa protocolos!

#### API:
```c
void NetManager_Init(NetManager_t *mgr, W5500_HW_t *hw);
void NetManager_Task(NetManager_t *mgr);  // Chamada no loop RTOS
NetSocket_t* NetManager_AllocSocket(NetManager_t *mgr, SocketType_t type);
void NetManager_FreeSocket(NetManager_t *mgr, NetSocket_t *sock);
void NetManager_RegisterService(NetManager_t *mgr, NetService_t *service);
```

---

### **CAMADA 5: Serviços de Rede (Network Services)**
```
Core/Network/Services/
├── dhcp/
│   ├── dhcp_client.h/c
│   └── dhcp_protocol.h
├── dns/
│   ├── dns_client.h/c
│   └── dns_protocol.h
├── ntp/
│   ├── ntp_client.h/c
│   └── ntp_protocol.h
├── mdns/
│   ├── mdns_responder.h/c
│   └── mdns_protocol.h
└── telnet/
    ├── telnet_server.h/c
    └── telnet_protocol.h
```

#### Responsabilidades:
- Implementar protocolos específicos
- Gerenciar estado do protocolo
- Requisitar socket do Network Manager
- Processar dados recebidos via callback

#### Cada serviço é **independente**!

#### Estrutura Padrão de um Serviço:
```c
// dhcp_client.h
typedef struct {
    NetSocket_t *socket;        // Socket alocado pelo manager
    DHCP_State_t state;         // Estado da FSM do DHCP
    uint32_t xid;               // Transaction ID
    uint32_t lease_time;
    uint32_t renew_timer;
    // ... campos específicos do protocolo
} DHCP_Client_t;

void DHCP_Init(DHCP_Client_t *dhcp, NetManager_t *mgr);
void DHCP_Start(DHCP_Client_t *dhcp);
void DHCP_Stop(DHCP_Client_t *dhcp);
void DHCP_Task(DHCP_Client_t *dhcp);  // FSM
```

---

### **CAMADA 6: Aplicação**
```
Core/Src/
├── app_freertos.c     → Main task
├── cli_telnet.c       → CLI commands
└── main.c
```

#### Responsabilidades:
- Inicializar o Network Manager
- Configurar e registrar serviços
- Implementar lógica da aplicação

---

## 🔄 Fluxo de Dados

### RX Path (Recebimento):
```
[W5500 IRQ] 
    ↓
[W5500 Driver: lê socket HW]
    ↓
[Socket API: identifica socket abstrato]
    ↓
[Network Manager: dispatch callback]
    ↓
[Serviço/Aplicação: processa dados]
```

### TX Path (Envio):
```
[Aplicação: chama NetSocket_Send()]
    ↓
[Socket API: valida e encaminha]
    ↓
[W5500 Driver: escreve no socket HW]
    ↓
[W5500 envia pacote]
```

---

## 🧩 Vantagens da Arquitetura

### ✅ Desacoplamento
- Telnet não conhece W5500
- DHCP apenas usa NetSocket API
- Trocar W5500 por ESP32-C3? Só mudar camadas 2 e 3!

### ✅ Testabilidade
- Testar DHCP com socket mockado
- Simular respostas sem hardware

### ✅ Modularidade
- Adicionar HTTP? Criar em Services/ sem tocar no resto
- Remover mDNS? Só não registrar o serviço

### ✅ Escalabilidade
- Pool de sockets gerenciado centralmente
- Fácil priorizar serviços críticos

---

## 📊 Comparação: Antes vs Depois

### ❌ ANTES (Arquitetura Monolítica)
```
W5500_Driver_t {
    SPI, GPIO, timers...
    DHCP_Control_t dhcp;
    DNS_Control_t dns;
    NTP_Control_t ntp;
    TELNET_Control_t telnet;
    mDNS_Service_t mdns;
}
```
**Problemas:**
- Driver conhece todos os serviços
- Difícil testar
- Impossível reusar código
- Acoplamento forte

### ✅ DEPOIS (Arquitetura em Camadas)
```
NetManager_t {
    W5500_HW_t *hw_driver;
    NetSocket_t sockets[8];
    NetService_t *services[];
}

// Serviços separados
DHCP_Client_t dhcp;
DNS_Client_t dns;
NTP_Client_t ntp;
Telnet_Server_t telnet;
```
**Vantagens:**
- Cada módulo tem uma responsabilidade
- Fácil testar isoladamente
- Código reutilizável
- Desacoplamento total

---

## 🚀 Como Migrar?

### Passo 1: Extrair Driver W5500 Puro
- Mover funções SPI/registradores para `w5500_ll.c`
- Remover referências a DHCP/DNS/etc do driver

### Passo 2: Criar Socket API
- Implementar `net_socket.c` usando W5500 como backend
- API genérica que esconde detalhes do W5500

### Passo 3: Criar Network Manager
- Gerenciar pool de sockets
- Sistema de callbacks e eventos

### Passo 4: Refatorar Serviços
- DHCP, DNS, etc. usam NetSocket API
- Não acessam W5500 diretamente

### Passo 5: Integração
- Main inicializa manager e registra serviços
- Task periódica chama `NetManager_Task()`

---

## 📝 Convenções de Nomenclatura

| Camada | Prefixo | Exemplo |
|--------|---------|---------|
| Driver W5500 | `W5500_` | `W5500_HW_SocketOpen()` |
| Socket API | `NetSocket_` | `NetSocket_Create()` |
| Network Manager | `NetManager_` | `NetManager_Init()` |
| Serviços | `<Service>_` | `DHCP_Start()`, `DNS_Query()` |
| Tipos | `_t` | `NetSocket_t`, `DHCP_Client_t` |
| Enums | `_e` ou `_t` | `NetErr_t`, `SocketType_t` |

---

## 🎓 Princípios de Design

1. **Single Responsibility**: Cada módulo faz uma coisa
2. **Dependency Inversion**: Camadas superiores não conhecem detalhes de hardware
3. **Open/Closed**: Fácil estender (adicionar serviço) sem modificar core
4. **Interface Segregation**: APIs pequenas e focadas
5. **Don't Repeat Yourself**: Reusar código entre serviços

---

## 📚 Referências

- [Berkeley Sockets API](https://en.wikipedia.org/wiki/Berkeley_sockets)
- [lwIP Architecture](https://www.nongnu.org/lwip/)
- [Clean Architecture - Robert C. Martin](https://blog.cleancoder.com/uncle-bob/2012/08/13/the-clean-architecture.html)

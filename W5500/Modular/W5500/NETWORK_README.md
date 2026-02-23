# Stack de Rede Modular STM32 + W5500

Implementação completa de uma stack de rede modular para STM32 com chip Ethernet W5500.

---

## 📋 Visão Geral

Esta stack implementa uma arquitetura em camadas para facilitar o desenvolvimento de aplicações de rede em sistemas embarcados STM32. Todos os componentes são modulares e desacoplados.

### Características

- ✅ **Arquitetura em 5 camadas** (HAL → Driver HW → Socket → Manager → Services)
- ✅ **Comunicação SPI com DMA** para máxima performance
- ✅ **Sincronização via RTOS** (mutex, semaphores, event flags)
- ✅ **Pool de 8 sockets** (limite do W5500)
- ✅ **Protocolos implementados**: DHCP, DNS, NTP, Telnet
- ✅ **Sistema de eventos** para comunicação entre serviços
- ✅ **API unificada** estilo Berkeley Sockets

---

## 📁 Estrutura de Arquivos

```
Core/
├── Inc/
│   ├── app_network.h              # API principal da aplicação
│   └── ...
├── Network/
│   ├── net_types.h                # Tipos comuns (IP, MAC, NetErr_t, etc)
│   ├── net_socket.h/.c            # API de sockets (Create, Bind, Send, Recv)
│   ├── net_manager.h/.c           # Gerenciador central de rede
│   ├── Driver/
│   │   └── W5500/
│   │       ├── w5500_regs.h       # Definições de registradores
│   │       ├── w5500_ll.h/.c      # Low-Level SPI com DMA
│   │       ├── w5500_hw.h/.c      # Driver de hardware
│   │       ├── w5500_socket.h/.c  # (LEGADO - não usar)
│   │       └── w5500_hal.h/.c     # (LEGADO - não usar)
│   └── Services/
│       ├── dhcp_service.h/.c      # Cliente DHCP
│       ├── dns_service.h/.c       # Cliente DNS
│       ├── ntp_service.h/.c       # Cliente NTP
│       └── telnet_service.h/.c    # Servidor Telnet
├── Src/
│   ├── app_network.c              # Inicialização e integração
│   ├── app_network_integration_example.c  # Exemplos de uso
│   └── app_freertos.c             # Task principal do RTOS
└── ...
```

---

## 🏗️ Arquitetura

### Camada 1: HAL (STM32 HAL)
- Provida pela ST
- SPI, GPIO, DMA

### Camada 2: Driver de Hardware W5500
- **w5500_ll.c**: Comunicação SPI de baixo nível (DMA)
- **w5500_hw.c**: Controle do chip (sockets, rede, PHY)
- Funções: `W5500_HW_Init()`, `W5500_HW_SocketOpen()`, `W5500_HW_SocketSend()`, etc.

### Camada 3: API de Sockets
- **net_socket.c**: Abstração de sockets
- Independente do hardware (pode trocar W5500 por outro chip)
- Funções: `NetSocket_Create()`, `NetSocket_Bind()`, `NetSocket_Connect()`, `NetSocket_Send()`, etc.

### Camada 4: Network Manager
- **net_manager.c**: Coordenador central
- Gerencia pool de 8 sockets
- Sistema de eventos
- Registro de serviços
- Loop principal: `NetManager_Task()`

### Camada 5: Serviços
- **DHCP**: Obtenção automática de IP
- **DNS**: Resolução de nomes
- **NTP**: Sincronização de tempo
- **Telnet**: CLI remoto via TCP

---

## 🚀 Início Rápido

### 1. Configurar Hardware no CubeMX

#### SPI
- Habilitar SPI1 (ou SPI desejado)
- Mode: Full-Duplex Master
- Data Size: 8 Bits
- Clock: Máximo suportado pelo W5500 (até 80 MHz)

#### DMA
- Adicionar DMA Request para SPI1_TX
- Adicionar DMA Request para SPI1_RX
- Priority: Medium ou High
- Mode: Normal

#### GPIO
- CS (Chip Select): GPIO Output, Initial Level High
- RST (Reset, opcional): GPIO Output, Initial Level High

### 2. Adicionar Arquivos ao Projeto

Copiar todos os arquivos da pasta `Core/Network/` para o projeto.

### 3. Modificar app_freertos.c

```c
#include "app_network.h"

void MX_FREERTOS_Init(void) {
    
    // Inicializar rede
    if (Network_Init() != NET_OK) {
        Error_Handler();
    }
    
    // Criar task de rede
    if (Network_StartTask() != NET_OK) {
        Error_Handler();
    }
    
    // Suas outras tasks aqui...
}
```

### 4. Ajustar Configuração em app_network.c

```c
// Na função Network_Init():

W5500_HW_Config_t hw_config = {
    .hspi = &hspi1,              // Seu SPI
    .cs_port = GPIOA,            // Porta do CS
    .cs_pin = GPIO_PIN_4,        // Pino do CS
    .rst_port = NULL,            // NULL = soft reset
    .rst_pin = 0,
    .spi_mutex = spi_mutex,
};

NetManagerConfig_t mgr_config = {
    .hw_driver = &w5500_hw,
    .enable_dhcp = true,         // true = DHCP, false = IP estático
    
    // Se IP estático:
    .static_config = {
        .ip = {192, 168, 1, 100},
        .subnet = {255, 255, 255, 0},
        .gateway = {192, 168, 1, 1},
        .dns = {8, 8, 8, 8},
    },
};
```

### 5. Compilar e Executar

1. Build do projeto
2. Flash no STM32
3. Conectar cabo Ethernet
4. Aguardar link UP e DHCP bound

### 6. Testar via Telnet

```bash
telnet <IP_DO_STM32>
```

Comandos disponíveis:
- `help` - Lista de comandos
- `status` - Status da rede
- `uptime` - Tempo de execução
- `reset` - Reinicia o sistema
- `exit` - Desconecta

---

## 💡 Exemplos de Uso

### Obter IP Atual

```c
uint8_t ip[4];
if (Network_GetIPAddress(ip) == NET_OK) {
    printf("IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
}
```

### Resolver DNS

```c
void MyDNSCallback(const char *hostname, const uint8_t *ip, void *user_data) {
    if (ip) {
        printf("%s -> %d.%d.%d.%d\n", hostname, ip[0], ip[1], ip[2], ip[3]);
    }
}

DNS_Client_t *dns = Network_GetDNSClient();
DNS_Query(dns, "www.google.com", MyDNSCallback, NULL);
```

### Cliente TCP

```c
NetManager_t *mgr = Network_GetManager();
NetSocket_t *sock = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_TCP, 0);

NetAddr_t local = {.port = 0};  // Porta dinâmica
NetSocket_Bind(sock, &local);

NetAddr_t remote = {
    .ip = {192, 168, 1, 10},
    .port = 80
};

if (NetSocket_Connect(sock, &remote) == NET_OK) {
    
    // Aguardar conexão
    while (!NetSocket_IsConnected(sock)) {
        osDelay(100);
    }
    
    // Enviar HTTP GET
    const char *request = "GET / HTTP/1.1\r\nHost: 192.168.1.10\r\n\r\n";
    NetSocket_Send(sock, (uint8_t *)request, strlen(request), 0);
    
    // Receber resposta
    uint8_t buffer[512];
    int32_t len = NetSocket_Recv(sock, buffer, sizeof(buffer), 5000);
    
    if (len > 0) {
        buffer[len] = '\0';
        printf("Response: %s\n", buffer);
    }
    
    NetSocket_Close(sock);
}

NetManager_FreeSocket(mgr, sock);
```

### Servidor UDP

```c
NetSocket_t *sock = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_UDP, 0);

NetAddr_t local = {.port = 5000};
NetSocket_Bind(sock, &local);

while (1) {
    uint8_t buffer[256];
    NetAddr_t src;
    
    int32_t len = NetSocket_RecvFrom(sock, buffer, sizeof(buffer), &src, 1000);
    
    if (len > 0) {
        // Processar mensagem
        printf("Received from %d.%d.%d.%d:%d\n",
            src.ip[0], src.ip[1], src.ip[2], src.ip[3], src.port);
        
        // Echo de volta
        NetSocket_SendTo(sock, buffer, len, &src, 0);
    }
}
```

### Sincronização NTP

```c
NTP_Client_t *ntp = Network_GetNTPClient();

// Forçar sync (já acontece automaticamente a cada 1 min)
NTP_Sync(ntp, NULL, NULL);

// Aguardar sincronização
while (!NTP_IsSynchronized(ntp)) {
    osDelay(1000);
}

// Obter tempo atual
NTP_Timestamp_t time;
if (NTP_GetTime(ntp, &time) == NET_OK) {
    printf("Unix time: %lu\n", time.seconds);
}
```

---

## 🔧 Troubleshooting

### Link não sobe

- Verificar cabo Ethernet conectado
- Verificar LED de link no W5500
- Verificar alimentação 3.3V
- Verificar conexão SPI (MOSI, MISO, SCK, CS)

### DHCP não obtém IP

- Verificar servidor DHCP na rede
- Testar com IP estático primeiro
- Verificar timeout do DHCP (padrão: 5s)
- Usar Wireshark para ver pacotes DHCP

### SPI não funciona

- Verificar DMA configurado no CubeMX
- Verificar callbacks do SPI implementados
- Verificar pino CS correto
- Verificar clock do SPI (máx 80 MHz)

### Stack overflow

- Aumentar stack size da NetworkTask (recomendado: 2048 bytes)
- Aumentar heap do FreeRTOS
- Verificar recursão em callbacks

### Timeout em operações

- Aumentar delays em app_network.c
- Verificar prioridade da NetworkTask
- Verificar se `NetManager_Task()` está sendo chamada

---

## 📊 Uso de Memória

### RAM (~6-8 KB)

- W5500_HW_t: ~300 bytes
- NetManager_t: ~1.5 KB
- NetSocket_t[8]: ~512 bytes
- DHCP_Client_t: ~600 bytes
- DNS_Client_t: ~800 bytes
- NTP_Client_t: ~200 bytes
- Telnet_Server_t: ~1 KB
- Buffers SPI: 4 KB (2K TX + 2K RX)

### Flash (~20-30 KB)

- Drivers: ~8 KB
- Serviços: ~12 KB
- Protocolos: ~5 KB

(Valores aproximados, dependem de otimizações do compilador)

---

## 🔒 Thread Safety

- ✅ Todas as funções são **thread-safe**
- ✅ SPI protegido por **mutex**
- ✅ Operações DMA sincronizadas via **semaphore**
- ✅ Eventos entregues via **message queue**
- ⚠️ **NÃO** chamar funções HAL diretamente em ISRs

---

## 📚 Documentação Adicional

- [ARCHITECTURE.md](ARCHITECTURE.md) - Detalhes da arquitetura
- [MIGRATION_GUIDE.md](MIGRATION_GUIDE.md) - Como migrar código legado
- [DIAGRAMS.md](DIAGRAMS.md) - Diagramas de fluxo
- [EXAMPLE_USAGE.c](EXAMPLE_USAGE.c) - Mais exemplos
- [IMPLEMENTATION_TIPS.md](IMPLEMENTATION_TIPS.md) - Dicas de implementação

---

## 🤝 Contribuindo

Para adicionar novos serviços:

1. Criar `myservice.h/.c` em `Core/Network/Services/`
2. Implementar interface `NetService_t`:
   - `init()`
   - `task()`
   - `deinit()`
   - `event_handler()`
3. Registrar no `NetManager` em `app_network.c`

Exemplo em [dhcp_service.c](Core/Network/Services/dhcp_service.c).

---

## 📄 Licença

Este código é fornecido como exemplo educacional.
Ajuste conforme necessário para seu projeto.

---

## ✨ Créditos

Desenvolvido para projeto STM32 + W5500 Ethernet  
Baseado em arquitetura modular em camadas  
FreeRTOS + CMSIS-OS v2

---

**Happy Coding! 🚀**

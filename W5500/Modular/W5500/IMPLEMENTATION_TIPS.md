# Dicas Práticas de Implementação

Conselhos práticos baseados em problemas comuns ao trabalhar com STM32 + W5500.

## 🎯 Começando pelo Básico

### 1. Primeiro: Validar Hardware

**ANTES** de implementar qualquer camada, confirme que o hardware funciona:

```c
// Teste básico de comunicação SPI
void TestW5500Communication(void) {
    uint8_t version;
    
    // Ler registrador VERSIONR (deve retornar 0x04)
    W5500_ReadRegister(W5500_COMMON_VERSIONR, &version);
    
    if (version == 0x04) {
        printf("W5500 OK!\n");
    } else {
        printf("ERRO: W5500 não responde! (lido: 0x%02X)\n", version);
        // Verificar:
        // - Conexões SPI (MOSI, MISO, SCK, CS)
        // - Alimentação 3.3V
        // - Cristal/clock do chip
    }
}
```

### 2. Validar Link Físico

Antes de tentar DHCP, confirme que o link está UP:

```c
void CheckPhysicalLink(void) {
    bool link_up;
    W5500_ReadLinkStatus(&hw, &link_up);
    
    if (link_up) {
        printf("Link UP - cabo conectado\n");
    } else {
        printf("Link DOWN - verificar cabo/switch\n");
    }
}
```

### 3. Testar com IP Estático Primeiro

DHCP adiciona complexidade. Teste conectividade básica com IP estático:

```c
void TestStaticIP(void) {
    // Configurar IP estático
    uint8_t ip[4] = {192, 168, 1, 100};
    uint8_t subnet[4] = {255, 255, 255, 0};
    uint8_t gateway[4] = {192, 168, 1, 1};
    
    W5500_SetIP(&hw, ip);
    W5500_SetSubnet(&hw, subnet);
    W5500_SetGateway(&hw, gateway);
    
    // Tentar ping do PC: ping 192.168.1.100
}
```

---

## 🔧 Problemas Comuns e Soluções

### Problema 1: W5500 não responde (SPI retorna 0x00 ou 0xFF)

**Causas comuns:**
- CS (Chip Select) invertido
- MOSI e MISO trocados
- Clock SPI muito rápido (tentar 1-2 MHz primeiro)
- Reset não aplicado corretamente

**Solução:**
```c
// Reset sequence correto
HAL_GPIO_WritePin(W5500_RST_PORT, W5500_RST_PIN, GPIO_PIN_RESET);
HAL_Delay(50);  // Mínimo 10ms
HAL_GPIO_WritePin(W5500_RST_PORT, W5500_RST_PIN, GPIO_PIN_SET);
HAL_Delay(200); // Aguardar PLL estabilizar

// CS ativo baixo
HAL_GPIO_WritePin(W5500_CS_PORT, W5500_CS_PIN, GPIO_PIN_SET);  // Idle HIGH
```

### Problema 2: Link não sobe

**Causas:**
- Cabo não conectado ou defeituoso
- PHY não configurado
- Auto-negociação falhando

**Solução:**
```c
// Forçar 100Mbps Full-Duplex se auto-neg falhar
uint8_t phycfgr = W5500_COMMON_PHYCFGR_RST 
                | W5500_COMMON_PHYCFGR_OPMD_OPMDC
                | W5500_COMMON_PHYCFGR_OPMDC_100BT_F;

W5500_WriteRegister(W5500_COMMON_PHYCFGR, phycfgr);
HAL_Delay(100);

// Verificar
W5500_ReadRegister(W5500_COMMON_PHYCFGR, &phycfgr);
if (phycfgr & W5500_COMMON_PHYCFGR_LNK_ON) {
    printf("Link UP\n");
}
```

### Problema 3: DHCP não responde

**Checklist:**
1. Link físico está UP? ✓
2. Socket UDP aberto na porta 68? ✓
3. Broadcast flag setado (0x8000)? ✓
4. MAC address válido? ✓
5. Transaction ID (xid) consistente? ✓

**Debug:**
```c
// Capturar pacote DISCOVER com Wireshark
// Verificar:
// - Destination: 255.255.255.255:67 ✓
// - Source: 0.0.0.0:68 ✓
// - BOOTP flags: 0x8000 (broadcast) ✓
// - chaddr: seu MAC ✓
// - Magic cookie: 0x63825363 ✓
// - Option 53: DHCP Discover ✓

void PrintDHCPPacket(uint8_t *pkt, uint16_t len) {
    dhcp_packet_t *dhcp = (dhcp_packet_t*)pkt;
    
    printf("DHCP Packet:\n");
    printf("  XID: 0x%08lX\n", ntohl(dhcp->xid));
    printf("  Flags: 0x%04X\n", ntohs(dhcp->flags));
    printf("  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
        dhcp->chaddr[0], dhcp->chaddr[1], dhcp->chaddr[2],
        dhcp->chaddr[3], dhcp->chaddr[4], dhcp->chaddr[5]);
}
```

### Problema 4: Socket não envia/recebe dados

**Verificar estado do socket:**
```c
void DebugSocket(uint8_t socket_id) {
    uint8_t sr, ir;
    uint16_t tx_fsr, rx_rsr;
    
    W5500_ReadSocketRegister(socket_id, W5500_Sn_SR, &sr);
    W5500_ReadSocketRegister(socket_id, W5500_Sn_IR, &ir);
    W5500_ReadSocketRegister16(socket_id, W5500_Sn_TX_FSR, &tx_fsr);
    W5500_ReadSocketRegister16(socket_id, W5500_Sn_RX_RSR, &rx_rsr);
    
    printf("Socket %d:\n", socket_id);
    printf("  SR (State): 0x%02X\n", sr);
    printf("  IR (Interrupt): 0x%02X\n", ir);
    printf("  TX Free: %d bytes\n", tx_fsr);
    printf("  RX Available: %d bytes\n", rx_rsr);
}
```

### Problema 5: Mutex/Semaphore deadlock em FreeRTOS

**Cuidado com ordem de locks:**
```c
// ❌ ERRADO - pode dar deadlock
void Task1(void) {
    osMutexAcquire(mutex_A, osWaitForever);
    osMutexAcquire(mutex_B, osWaitForever);
    // ...
}

void Task2(void) {
    osMutexAcquire(mutex_B, osWaitForever);  // Ordem invertida!
    osMutexAcquire(mutex_A, osWaitForever);
    // ...
}

// ✅ CORRETO - sempre mesma ordem
void Task1(void) {
    osMutexAcquire(mutex_A, osWaitForever);
    osMutexAcquire(mutex_B, osWaitForever);
}

void Task2(void) {
    osMutexAcquire(mutex_A, osWaitForever);  // Mesma ordem
    osMutexAcquire(mutex_B, osWaitForever);
}
```

**Timeout em vez de espera infinita:**
```c
if (osMutexAcquire(spi_mutex, pdMS_TO_TICKS(1000)) != osOK) {
    printf("TIMEOUT aguardando SPI mutex!\n");
    return NET_ERR_TIMEOUT;
}
```

---

## 🚀 Otimizações de Performance

### 1. Usar DMA para SPI

```c
// Não-bloqueante com DMA
HAL_SPI_TransmitReceive_DMA(hspi, tx_buf, rx_buf, len);
osSemaphoreAcquire(spi_done_sem, osWaitForever);  // Aguarda DMA
```

### 2. Aumentar Tamanho dos Buffers de Socket

W5500 permite configurar buffers por socket (padrão: 2KB cada):

```c
// Alocar mais buffer para Telnet (socket 0)
// TX: 8KB, RX: 8KB
W5500_WriteSocketRegister(0, W5500_Sn_TXBUF_SIZE, 8);  // 8KB
W5500_WriteSocketRegister(0, W5500_Sn_RXBUF_SIZE, 8);  // 8KB

// Outros sockets ficam com 2KB (total = 16KB)
for (int i = 1; i < 4; i++) {
    W5500_WriteSocketRegister(i, W5500_Sn_TXBUF_SIZE, 2);
    W5500_WriteSocketRegister(i, W5500_Sn_RXBUF_SIZE, 2);
}
```

### 3. Usar Interrupção em vez de Polling

```c
// Configurar pino INT do W55500
void W5500_ConfigureIRQ(void) {
    // Habilitar interrupções de socket
    uint8_t simr = 0xFF;  // Todos os sockets
    W5500_WriteRegister(W5500_COMMON_SIMR, simr);
    
    // Configurar GPIO EXTI
    // INT do W5500 → PA2 do STM32
}

// Callback da IRQ
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == W5500_INT_PIN) {
        osEventFlagsSet(net_events, EVT_W5500_IRQ);
    }
}

// Task processa IRQ
void NetManager_Task(NetManager_t *mgr) {
    uint32_t flags = osEventFlagsWait(net_events, EVT_W5500_IRQ, 
                                       osFlagsWaitAny, pdMS_TO_TICKS(100));
    
    if (flags & EVT_W5500_IRQ) {
        // Processar sockets com dados
        W5500_ProcessIRQ(&mgr->hw_driver);
    }
}
```

### 4. Minimizar Acessos SPI

```c
// ❌ Ineficiente - múltiplas transações SPI
for (int i = 0; i < 4; i++) {
    W5500_ReadRegister(W5500_COMMON_SIPR + i, &ip[i]);
}

// ✅ Eficiente - uma única transação
W5500_ReadRegisterBurst(W5500_COMMON_SIPR, ip, 4);
```

---

## 🐛 Debug e Logging

### 1. Macros de Debug Condicionais

```c
// net_debug.h
#ifdef NET_DEBUG
    #define NET_LOG(fmt, ...) printf("[NET] " fmt "\n", ##__VA_ARGS__)
    #define NET_DBG(fmt, ...) printf("[DBG] " fmt "\n", ##__VA_ARGS__)
    #define NET_ERR(fmt, ...) printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#else
    #define NET_LOG(...)
    #define NET_DBG(...)
    #define NET_ERR(fmt, ...) printf("[ERR] " fmt "\n", ##__VA_ARGS__)
#endif

// Uso:
NET_LOG("DHCP: obtido IP %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
NET_DBG("Socket %d state: 0x%02X", sock_id, state);
NET_ERR("Falha ao abrir socket!");
```

### 2. Dump de Pacotes

```c
void HexDump(const uint8_t *data, uint16_t len) {
    printf("Packet dump (%d bytes):\n", len);
    for (int i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

// Uso no callback
void DHCP_RxCallback(...) {
    #ifdef NET_DEBUG
    HexDump(data, len);
    #endif
    
    // Processar pacote
}
```

### 3. Estatísticas

```c
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t socket_errors;
    uint32_t dhcp_discoveries;
    uint32_t dhcp_binds;
} NetStats_t;

NetStats_t g_net_stats = {0};

// Incrementar ao enviar
void NetSocket_Send(...) {
    // ...
    g_net_stats.packets_sent++;
    g_net_stats.bytes_sent += len;
}

// Comando CLI para mostrar
void CLI_NetStats(void) {
    printf("Network Statistics:\n");
    printf("  TX: %lu packets (%lu bytes)\n", 
           g_net_stats.packets_sent, g_net_stats.bytes_sent);
    printf("  RX: %lu packets (%lu bytes)\n",
           g_net_stats.packets_received, g_net_stats.bytes_received);
    printf("  DHCP binds: %lu\n", g_net_stats.dhcp_binds);
}
```

---

## 💡 Boas Práticas

### 1. Sempre Validar Retornos

```c
// ❌ Perigoso
NetSocket_Send(sock, data, len);

// ✅ Seguro
NetErr_t err = NetSocket_Send(sock, data, len);
if (err != NET_OK) {
    NET_ERR("Falha ao enviar: %s", NetSocket_ErrorToString(err));
    // Tomar ação corretiva
}
```

### 2. Usar Enums em vez de Magic Numbers

```c
// ❌ Difícil de entender
if (state == 0x17) {
    // ...
}

// ✅ Auto-documentado
if (state == W5500_SOCK_ESTABLISHED) {
    // ...
}
```

### 3. Timeouts em Operações de Rede

```c
typedef struct {
    uint32_t start_time;
    uint32_t timeout_ms;
} Timeout_t;

static inline void Timeout_Start(Timeout_t *t, uint32_t ms) {
    t->start_time = HAL_GetTick();
    t->timeout_ms = ms;
}

static inline bool Timeout_Expired(const Timeout_t *t) {
    return (HAL_GetTick() - t->start_time) >= t->timeout_ms;
}

// Uso:
Timeout_t timeout;
Timeout_Start(&timeout, 5000);  // 5 segundos

while (!NetSocket_IsConnected(sock)) {
    if (Timeout_Expired(&timeout)) {
        NET_ERR("Timeout conectando!");
        return NET_ERR_TIMEOUT;
    }
    osDelay(10);
}
```

### 4. Comentários Úteis

```c
// ❌ Comentário inútil
i++;  // Incrementa i

// ✅ Comentário valioso
// Aguardar ACK do servidor antes de enviar próximo chunk
// W5500 tem apenas 2KB de buffer TX, fragmentar se necessário
if (total_len > 2048) {
    // Enviar em pedaços...
}
```

### 5. Asserts em Debug

```c
#ifdef DEBUG
    #define NET_ASSERT(cond) do { \
        if (!(cond)) { \
            printf("ASSERT FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            while(1); \
        } \
    } while(0)
#else
    #define NET_ASSERT(cond)
#endif

// Uso:
void NetSocket_Send(NetSocket_t *sock, ...) {
    NET_ASSERT(sock != NULL);
    NET_ASSERT(sock->state == NET_SOCKET_STATE_ESTABLISHED);
    // ...
}
```

---

## 📊 Ferramentas Úteis

### Wireshark
```bash
# Filtros úteis para debug

# Ver apenas DHCP
bootp

# Ver tráfego de/para seu STM32 (por MAC)
eth.addr == 02:de:ad:be:ef:01

# UDP na porta específica
udp.port == 1234

# TCP handshake
tcp.flags.syn == 1

# Seguir stream TCP
tcp.stream eq 0
```

### CubeMX
- Configurar SPI: modo Master, 8-bit, CPOL=Low, CPHA=1Edge
- Ativar DMA para SPI TX/RX
- Configurar GPIO CS como Output Push-Pull
- Configurar GPIO INT como EXTI (Falling edge)

### CubeMonitor
- Monitorar variáveis em tempo real
- `net_manager.state`
- `dhcp_client.state`
- `socket[0].state`

---

## ⚡ Checklist de Implementação

Antes de considerar uma camada "pronta":

### Driver W5500
- [ ] Reset funciona
- [ ] Lê VERSIONR (0x04)
- [ ] Link UP detectado
- [ ] Escreve/lê MAC, IP, Subnet, Gateway
- [ ] Abre/fecha sockets
- [ ] Envia/recebe dados em socket UDP
- [ ] Envia/recebe dados em socket TCP

### Socket API
- [ ] Create/Destroy funciona
- [ ] Bind aloca socket de hardware
- [ ] Send/Recv não vaza memória
- [ ] Callbacks são chamados
- [ ] Pool não estoura (máx 8 sockets)

### Network Manager
- [ ] Init não falha
- [ ] Task roda sem travar
- [ ] Eventos são despachados
- [ ] Serviços recebem eventos
- [ ] Sockets são alocados/liberados corretamente

### Cada Serviço
- [ ] Implementa interface NetService_t
- [ ] Aloca/libera socket via manager
- [ ] FSM está correta
- [ ] Timeouts funcionam
- [ ] Posta eventos quando apropriado
- [ ] Libera recursos em Deinit()

---

## 🎓 Recursos de Aprendizado

### Datasheets
- [W5500 Datasheet](https://www.wiznet.io/product-item/w5500/)
- STM32G0 Reference Manual

### RFCs
- RFC 2131 - DHCP
- RFC 1035 - DNS
- RFC 5905 - NTPv4
- RFC 793 - TCP
- RFC 768 - UDP

### Exemplos
- [WIZnet GitHub](https://github.com/Wiznet)
- [STM32 Examples](https://github.com/STMicroelectronics)

---

**Boa implementação! 🚀**

/*
 * NTP_DNS_IMPROVEMENTS.md
 *
 * Resumo das melhorias implementadas para resolver os problemas do NTP
 */

# Melhorias Implementadas

## 1. SUPORTE A DNS PARA NTP ✅

### Antes
- NTP usava IP hardcoded: 193.182.111.12 (pool.ntp.org)
- Não era possível usar hostname

### Depois
- Adicionar no `ntp_service.h`:
  - Campo `server_hostname[64]` na struct `NTP_Client_t`
  - Campo `use_dns` para indicar se usar DNS

- Nova função em `ntp_service.h`:
  ```c
  NetErr_t NTP_SetServerByHostname(NTP_Client_t *client, const char *hostname);
  ```

- Uso:
  ```c
  NTP_SetServerByHostname(&ntp_client, "pool.ntp.org");
  // ou
  NTP_SetServerByHostname(&ntp_client, "time.google.com");
  ```

- A resolução de DNS é feita pela `NetworkTask` antes de sincronizar via NTP

---

## 2. SUPORTE A NTP VIA DHCP ✅

### Antes
- Servidor NTP era fixo ou configurado manualmente
- DHCP não extraía opção 42 (NTP_SERVER)

### Depois
- Adicionado em `dhcp_service.c`:
  - `#define DHCP_OPT_NTP_SERVER 42` (RFC 2132)
  - Parsing de servidor NTP nas opções DHCP
  - Armazenar em `dhcp_client.ntp_server`

- Adicionado em `dhcp_service.h`:
  - Campo `ntp_server` na struct `DHCP_Client_t`

- Adicionado em `app_network_config.c`:
  ```c
  void AppNetwork_ApplyDHCPNTPServer(void);
  ```
  Chamada automaticamente quando DHCP_BOUND via callback

- Comportamento:
  1. App inicia com NTP padrão (pool.ntp.org)
  2. DHCP obtém IP e opcionalmente servidor NTP (opção 42)
  3. Se servidor NTP veio do DHCP, NTP muda para ele automaticamente
  4. Se DHCP não forneceu NTP, continua com padrão

---

## 3. CONFIGURAÇÃO DINÂMICA DA APLICAÇÃO ✅

### Novo arquivo: `app_network_config.h/c`

Define estrutura centralizadora:
```c
typedef struct {
    // NTP
    bool ntp_enabled;
    bool ntp_use_dns;
    const char *ntp_hostname;
    uint8_t ntp_server_ip[4];
    uint32_t ntp_sync_interval_ms;
    bool ntp_accept_from_dhcp;  // Usar NTP do DHCP?
    
    // mDNS
    bool mdns_enabled;
    const char *mdns_hostname;
    
    // Task de rede
    uint32_t network_task_delay_ms;
    uint32_t network_task_stack_bytes;
    
    bool use_dhcp;
} AppNetworkConfig_t;
```

### Funções públicas:

```c
// Obter configuração padrão
AppNetworkConfig_t config = AppNetwork_GetDefaultConfig();

// Modificar e aplicar
config.ntp_hostname = "time.google.com";
config.ntp_use_dns = true;
config.network_task_delay_ms = 5;  // 5ms = 200Hz

// Configurações de NTP
AppNetwork_SetNTPServerByHostname("time.nist.gov");
AppNetwork_SetNTPServerByIP((uint8_t[]){45, 33, 32, 211});

// mDNS (placeholder para futuro)
AppNetwork_SetmDNSHostname("stm32-w5500.local");
```

---

## 4. DELAY CONFIGURÁVEL DA NETWORK TASK ✅

### Antes
```c
static const osThreadAttr_t network_task_attr = {
    .name = "NetworkTask",
    .stack_size = 4*2048,  // Fixo em 8KB
    .priority = osPriorityNormal,
};
// osDelay(10);  // Fixo em 10ms
```

### Depois
```c
// Variáveis configuráveis
static uint32_t network_task_delay_ms = 10;       // Padrão 10ms
static uint32_t network_task_stack_size = 4*2048; // Padrão 8KB

// Funções para configurar (ANTES de Network_StartTask)
Network_SetTaskDelay(5);       // 5ms = 200Hz (mais responsivo)
Network_SetTaskStackSize(6144); // 6KB (economiza RAM)
```

### Recomendações:
| Delay | Frequência | Uso | CPU |
|-------|-----------|-----|-----|
| 5ms   | 200Hz     | Telnet responsivo | Alto |
| 10ms  | 100Hz     | Padrão, bom balanço | Médio |
| 20ms  | 50Hz      | Economia CPU | Baixo |
| 100ms | 10Hz      | Ultra low-power | Muito baixo |

---

## 5. EXEMPLO COMPLETO DE TCP/UDP ✅

### Novo arquivo: `app_tcp_udp_example.c`

Contém 5 exemplos práticos:
1. **Cliente TCP**: Conectar e enviar/receber dados
2. **Servidor TCP**: Aguardar conexões (1 cliente)
3. **Cliente UDP**: Enviar/receber datagrams
4. **Servidor UDP**: Echo server
5. **Com Callbacks**: Processamento não-bloqueante

### Exemplo rápido - Cliente TCP:
```c
NetManager_t *mgr = Network_GetManager();
NetSocket_t *sock = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_TCP);

NetAddr_t addr = { .ip = { .addr = {8,8,8,8} }, .port = 2323 };
NetSocket_Connect(sock, &addr);

while (!NetSocket_IsConnected(sock)) osDelay(100);

NetSocket_Send(sock, (uint8_t*)"Hello!", 6, 0);

uint8_t rx[256];
int len = NetSocket_Recv(sock, rx, sizeof(rx), 0);

NetSocket_Close(sock);
NetManager_FreeSocket(mgr, sock);
```

---

## 6. CHECKLIST DE USO

Para usar as novas funcionalidades:

- [ ] `#include "app_network_config.h"`
- [ ] Chamar `Network_SetTaskDelay()` antes de `Network_StartTask()` (opcional)
- [ ] Chamar `AppNetwork_SetNTPServerByHostname()` se usar DNS (opcional)
- [ ] DHCP automaticamente aplica servidor NTP se disponível
- [ ] Ver `app_tcp_udp_example.c` para fazer aplicações TCP/UDP
- [ ] NTP agora tenta resolver via DNS se `use_dns = true`

---

## 7. MUDANÇAS NOS ARQUIVOS

### Modificados:
- `Core/Inc/app_network.h` - Adicionadas funções de config
- `Core/Src/app_network.c` - Implementadas funções, delay configurável, callback DHCP
- `Core/Network/Services/ntp_service.h` - Adicionados campos DNS, nova função
- `Core/Network/Services/ntp_service.c` - Implementado suporte a DNS
- `Core/Network/Services/dhcp_service.h` - Adicionado campo ntp_server
- `Core/Network/Services/dhcp_service.c` - Parsing de NTP_SERVER (opção 42)

### Criados:
- `Core/Inc/app_network_config.h` - Header de configuração
- `Core/Src/app_network_config.c` - Implementação de configuração
- `Core/Src/app_tcp_udp_example.c` - Exemplos de TCP/UDP

---

## 8. TESTES RECOMENDADOS

```bash
# 1. Telnet e conferir se NTP sincroniza
telnet <IP_STM32>
> status   # Verificar se IP/MAC/DNS estão preenchidos
> uptime   # Verificar se NTP pegou hora (senão, avança devagar)

# 2. Se usar DHCP com servidor NTP:
# Configurar seu DHCP server para fornecer opção 42 (NTP_SERVER)
# STM32 automaticamente usará

# 3. Se usar DNS para NTP:
config.ntp_hostname = "time.google.com";
config.ntp_use_dns = true;
// STM32 resolverá via DNS antes de sincronizar

# 4. Experimente diferentes delays:
Network_SetTaskDelay(5);   // 200Hz - Telnet mais responsivo
Network_SetTaskDelay(10);  // 100Hz - Padrão
Network_SetTaskDelay(20);  // 50Hz - Menos CPU
```

---

## 9. TROUBLESHOOTING

**Problema**: NTP não sincroniza
- [ ] Verificar se Link Ethernet está UP (`status` no telnet)
- [ ] Conferir se IP foi obtido via DHCP
- [ ] Se usar DNS, verificar se DNS server está correto
- [ ] Aumentar `ntp_sync_interval_ms` (aguardar mais antes de retry)
- [ ] Ver se servidor NTP está acessível (pode estar bloqueado)

**Problema**: Telnet travando
- [ ] Aumentar `network_task_delay_ms` (deixar com 10ms ou 20ms)
- [ ] Aumentar stack da NetworkTask em `Network_SetTaskStackSize()`
- [ ] Ver se há muitos sockets abertos (Max 8)

**Problema**: Stack overflow na NetworkTask
- [ ] Aumentar com `Network_SetTaskStackSize(8192)` ou mais
- [ ] Reduzir número de sockets simultâneos
- [ ] Reduzir RX buffer sizes

---

## 10. NOTAS FINAIS

- **NTP via DNS**: Resolve hostname na primeira sincronização, depois usa IP
- **NTP via DHCP**: Aplica automaticamente quando DHCP_BOUND
-  **Task Delay**: Menor = mais responsivo mas mais CPU. 10ms é bom padrão
- **TCP/UDP**: Ver exemplos em `app_tcp_udp_example.c`
- **mDNS**: Placeholder no config, implementar se necessário

---

Todas as mudanças são **backward-compatible**. Código existente continua funcionando sem modificações.

# ✅ Checklist de Integração - Stack de Rede W5500

Use este checklist para garantir que todos os passos foram seguidos corretamente.

---

## 📋 Pré-requisitos

- [ ] Projeto STM32 criado no STM32CubeMX ou STM32CubeIDE
- [ ] FreeRTOS habilitado (CMSIS-OS v2)
- [ ] W5500 conectado via SPI

---

## 🔧 Configuração do Hardware (CubeMX)

### SPI
- [ ] SPI habilitado (SPI1, SPI2, etc.)
- [ ] Mode: **Full-Duplex Master**
- [ ] Data Size: **8 Bits**
- [ ] First Bit: **MSB First**
- [ ] Clock Polarity: **Low** (CPOL = 0)
- [ ] Clock Phase: **1 Edge** (CPHA = 0)
- [ ] Baud Rate: Máximo possível (até 80 MHz)
- [ ] NSS: **Disabled** (usaremos GPIO manual para CS)

### DMA
- [ ] SPI_TX Stream configurado
  - [ ] Direction: **Memory To Peripheral**
  - [ ] Priority: **Medium** ou **High**
  - [ ] Mode: **Normal**
  - [ ] Data Width: **Byte**
  
- [ ] SPI_RX Stream configurado
  - [ ] Direction: **Peripheral To Memory**
  - [ ] Priority: **Medium** ou **High**
  - [ ] Mode: **Normal**
  - [ ] Data Width: **Byte**

### GPIO
- [ ] Pino CS configurado como **GPIO_Output**
  - [ ] Output Level: **High** (inicialmente desativado)
  - [ ] Mode: **Output Push Pull**
  - [ ] Pull: **No Pull**
  - [ ] Speed: **Very High**
  
- [ ] Pino RST configurado (OPCIONAL)
  - [ ] Se usar: **GPIO_Output**, Initial Level **High**
  - [ ] Se não usar: deixar NULL no código

### FreeRTOS
- [ ] Interface: **CMSIS_V2**
- [ ] Heap Size: Mínimo **16384 bytes** (16 KB)
- [ ] Total Heap Size suficiente para todas as tasks
- [ ] USE_NEWLIB_REENTRANT: **Disabled** (se não usar printf em múltiplas tasks)

---

## 📁 Integração dos Arquivos

### Copiar Headers
- [ ] `Core/Network/net_types.h`
- [ ] `Core/Network/net_socket.h`
- [ ] `Core/Network/net_manager.h`
- [ ] `Core/Network/Driver/W5500/w5500_regs.h`
- [ ] `Core/Network/Driver/W5500/w5500_ll.h`
- [ ] `Core/Network/Driver/W5500/w5500_hw.h`
- [ ] `Core/Network/Services/dhcp_service.h`
- [ ] `Core/Network/Services/dns_service.h`
- [ ] `Core/Network/Services/ntp_service.h`
- [ ] `Core/Network/Services/telnet_service.h`
- [ ] `Core/Inc/app_network.h`

### Copiar Sources
- [ ] `Core/Network/net_socket.c`
- [ ] `Core/Network/net_manager.c`
- [ ] `Core/Network/Driver/W5500/w5500_ll.c`
- [ ] `Core/Network/Driver/W5500/w5500_hw.c`
- [ ] `Core/Network/Services/dhcp_service.c`
- [ ] `Core/Network/Services/dns_service.c`
- [ ] `Core/Network/Services/ntp_service.c`
- [ ] `Core/Network/Services/telnet_service.c`
- [ ] `Core/Src/app_network.c`

### Adicionar ao Projeto
Se usando **STM32CubeIDE**:
- [ ] Botão direito no projeto → **Refresh** (F5)
- [ ] Verificar que todos os `.c` aparecem na pasta `Core/Src` e subpastas

Se usando **Makefile**:
- [ ] Adicionar todos os `.c` na variável `C_SOURCES`
- [ ] Adicionar paths dos headers em `C_INCLUDES`

Se usando **Keil MDK**:
- [ ] Adicionar todos os `.c` nos grupos do projeto
- [ ] Adicionar include paths em Project Options → C/C++ → Include Paths

---

## ✏️ Modificações no Código

### app_freertos.c

- [ ] Incluir header: `#include "app_network.h"`

- [ ] Em `MX_FREERTOS_Init()`, adicionar:
```c
/* USER CODE BEGIN RTOS_THREADS */

// Inicializar rede
if (Network_Init() != NET_OK) {
    Error_Handler();
}

// Criar task de rede
if (Network_StartTask() != NET_OK) {
    Error_Handler();
}

/* USER CODE END RTOS_THREADS */
```

### app_network.c

- [ ] Ajustar handle do SPI:
```c
.hspi = &hspi1,  // Trocar hspi1 pelo seu SPI
```

- [ ] Ajustar GPIO do CS:
```c
.cs_port = GPIOA,        // Trocar pela porta correta
.cs_pin = GPIO_PIN_4,    // Trocar pelo pino correto
```

- [ ] Ajustar GPIO do RST (se usar):
```c
.rst_port = GPIOB,       // Porta do RST
.rst_pin = GPIO_PIN_0,   // Pino do RST
```

Ou deixar NULL para soft reset:
```c
.rst_port = NULL,
.rst_pin = 0,
```

- [ ] Configurar DHCP ou IP estático:
```c
.enable_dhcp = true,  // true = DHCP, false = IP estático
```

Se IP estático, configurar:
```c
.static_config = {
    .ip = {192, 168, 1, 100},        // Seu IP
    .subnet = {255, 255, 255, 0},    // Máscara
    .gateway = {192, 168, 1, 1},     // Gateway
    .dns = {8, 8, 8, 8},             // DNS
},
```

---

## 🔨 Compilação

- [ ] Build do projeto sem erros
- [ ] Verificar warnings (resolver se possível)
- [ ] Verificar tamanho do binário (.bin ou .hex < tamanho da Flash)

### Errors Comuns

**"undefined reference to `osKernelGetTickCount`"**
- [ ] Verificar que FreeRTOS está habilitado
- [ ] Verificar que está usando CMSIS-OS v2

**"undefined reference to `HAL_SPI_Transmit_DMA`"**
- [ ] Verificar que DMA está configurado no CubeMX
- [ ] Regenerar código do CubeMX

**"multiple definition of `HAL_SPI_TxRxCpltCallback`"**
- [ ] Remover callbacks SPI duplicados
- [ ] Manter apenas os de app_network.c ou modificá-los

---

## 🚀 Execução e Teste

### Primeira Execução

- [ ] Flash do firmware no STM32
- [ ] Conectar cabo Ethernet
- [ ] Conectar serial debug (opcional, para logs)
- [ ] Reset/Power cycle do STM32

### Verificações

- [ ] LED de link do W5500 acende (se houver)
- [ ] LED de atividade do W5500 pisca durante operação
- [ ] Aguardar ~5 segundos para DHCP

### Descobrir o IP

**Opção 1: Router**
- [ ] Acessar interface do router/modem
- [ ] Procurar por "DHCP Clients" ou "Connected Devices"
- [ ] Procurar por dispositivo com nome "STM32-W5500" ou MAC 02:xx:xx:xx:xx:xx

**Opção 2: Scan de rede**
```bash
# Linux/Mac
nmap -sn 192.168.1.0/24

# Windows
arp -a
```

**Opção 3: Debug serial**
- [ ] Adicionar printf do IP em `NetworkEventCallback()` quando receber `NET_EVENT_DHCP_BOUND`

### Testar Telnet

```bash
telnet <IP_DO_STM32>
```

- [ ] Mensagem de boas-vindas aparece
- [ ] Prompt "STM32> " aparece
- [ ] Comando `help` funciona
- [ ] Comando `status` mostra IP correto
- [ ] Comando `uptime` mostra tempo de execução
- [ ] Comando `exit` desconecta

### Testar DNS (se habilitado)

```c
DNS_Client_t *dns = Network_GetDNSClient();
uint8_t ip[4];

if (DNS_LookupCache(dns, "www.google.com", ip) == NET_OK) {
    // OK
} else {
    DNS_Query(dns, "www.google.com", MyCallback, NULL);
}
```

- [ ] DNS resolve corretamente
- [ ] Cache funciona em consultas repetidas

### Testar NTP (se habilitado)

```c
NTP_Client_t *ntp = Network_GetNTPClient();
NTP_Timestamp_t time;

if (NTP_GetTime(ntp, &time) == NET_OK) {
    printf("Unix time: %lu\n", time.seconds);
}
```

- [ ] NTP sincroniza (pode demorar até 1 min na primeira vez)
- [ ] Timestamp Unix está correto (comparar com https://www.unixtimestamp.com/)

---

## 🐛 Debugging

### Se DHCP não funcionar

**Teste 1: IP Estático**
- [ ] Desabilitar DHCP em app_network.c: `enable_dhcp = false`
- [ ] Configurar IP estático compatível com sua rede
- [ ] Recompilar e testar
- [ ] Se funcionar → problema no servidor DHCP ou implementação DHCP
- [ ] Se não funcionar → problema no hardware ou driver SPI

**Teste 2: Wireshark**
- [ ] Instalar Wireshark no PC
- [ ] Conectar PC e STM32 no mesmo switch
- [ ] Filtrar por `bootp` (protocolo DHCP)
- [ ] Verificar se pacotes DHCP Discover são enviados
- [ ] Verificar se DHCP Offer é recebido

### Se SPI não funcionar

**Verificar Conexões**
```
W5500        STM32
-----        -----
MOSI    <--  MOSI
MISO    -->  MISO
SCK     <--  SCK
CS      <--  GPIO (configurado como CS)
GND     <--  GND
3V3     <--  3V3
```

**Teste com Osciloscópio/Logic Analyzer**
- [ ] CS vai para LOW durante comunicação
- [ ] SCK pulsa durante comunicação
- [ ] MOSI transmite dados
- [ ] MISO recebe dados

**Teste de Registro**
Adicionar em `Network_Init()` após `W5500_HW_Init()`:
```c
uint8_t version;
W5500_LL_ReadCommonReg(&w5500_hw.ll, W5500_COMMON_VERSIONR, &version);
if (version != 0x04) {
    // SPI não está funcionando
    Error_Handler();
}
```

### Stack Overflow

Se o sistema travar ou resetar aleatoriamente:

- [ ] Aumentar stack da NetworkTask em app_network.c:
```c
.stack_size = 4096,  // Dobrar para 4KB
```

- [ ] Aumentar heap do FreeRTOS no CubeMX ou FreeRTOSConfig.h:
```c
#define configTOTAL_HEAP_SIZE    (20480)  // 20 KB
```

- [ ] Habilitar stack overflow detection:
```c
#define configCHECK_FOR_STACK_OVERFLOW   2
```

---

## 📊 Verificação Final

### Funcionalidades Básicas
- [ ] Link Ethernet UP
- [ ] IP obtido via DHCP (ou configurado estaticamente)
- [ ] Ping responde (opcional, requer implementação ICMP)
- [ ] Telnet conecta e responde comandos

### Serviços
- [ ] DHCP: IP renovado automaticamente
- [ ] DNS: Resolve hostnames
- [ ] NTP: Tempo sincronizado
- [ ] Telnet: Comandos funcionam

### Performance
- [ ] Latência < 50ms em operações de rede local
- [ ] Throughput adequado para aplicação
- [ ] CPU usage < 30% em idle
- [ ] Sem memory leaks (monitorar heap disponível)

---

## 🎯 Próximos Passos

Agora que a stack está funcionando:

1. **Adicionar sua lógica de aplicação**
   - Criar tasks customizadas
   - Usar sockets para comunicação
   - Integrar com periféricos

2. **Implementar protocolos adicionais**
   - HTTP client/server
   - MQTT
   - Modbus TCP
   - SNMP

3. **Otimizações**
   - Ajustar timeouts
   - Tuning de buffers
   - Reduzir CPU usage

4. **Testes de estresse**
   - Conexões simultâneas
   - Transferências longas
   - Recovery de erros

---

✅ **Parabéns! Sua stack de rede está pronta para uso!** 🎉

Para dúvidas, consulte:
- [NETWORK_README.md](NETWORK_README.md) - Documentação completa
- [ARCHITECTURE.md](ARCHITECTURE.md) - Detalhes da arquitetura
- [EXAMPLE_USAGE.c](EXAMPLE_USAGE.c) - Mais exemplos de código

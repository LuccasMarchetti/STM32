/*
 * app_network_integration_example.c
 *
 * EXEMPLO DE COMO INTEGRAR A STACK DE REDE NO app_freertos.c
 * 
 * Este arquivo mostra como modificar o app_freertos.c existente para
 * usar a nova arquitetura de rede modular.
 */

/* =============================================================================
 * PASSO 1: INCLUIR HEADERS
 * ============================================================================= */

#if defined(APP_NETWORK_INTEGRATION_EXAMPLE)

// No topo do seu app_freertos.c, adicionar:
#include "app_network.h"
#include "cmsis_os.h"
#include <string.h>

/* =============================================================================
 * PASSO 2: MODIFICAR MX_FREERTOS_Init()
 * ============================================================================= */

void MX_FREERTOS_Init(void) {
  
  /* USER CODE BEGIN Init */
  
  // Inicializar a stack de rede
  if (Network_Init() != NET_OK) {
      // Erro ao inicializar rede
      Error_Handler();
  }
  
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* USER CODE BEGIN RTOS_THREADS */
  
  // Criar task de rede
  if (Network_StartTask() != NET_OK) {
      // Erro ao criar task
      Error_Handler();
  }
  
  // Criar suas outras tasks customizadas aqui (se houver)
  
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */
}

/* =============================================================================
 * PASSO 3: CRIAR TASKS CUSTOMIZADAS (OPCIONAL)
 * ============================================================================= */

/* Attributes para sua task */
const osThreadAttr_t MyTask_attributes = {
  .name = "MyTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static void MyDNSCallback(const char *hostname, const uint8_t *ip, void *user_data);

/* Função da task */
void MyTask(void *argument)
{
  /* USER CODE BEGIN MyTask */
  
  // Aguardar rede conectar
  while (!Network_IsConnected()) {
      osDelay(1000);
  }
  
  // Obter IP obtido via DHCP
  uint8_t my_ip[4];
  if (Network_GetIPAddress(my_ip) == NET_OK) {
      // IP: my_ip[0].my_ip[1].my_ip[2].my_ip[3]
  }
  
  // Exemplo: Resolver DNS
  DNS_Client_t *dns = Network_GetDNSClient();
  uint8_t resolved_ip[4];
  
  // Verificar cache primeiro
  if (DNS_LookupCache(dns, "www.google.com", resolved_ip) == NET_OK) {
      // IP encontrado em cache
  } else {
      // Iniciar query assíncrona (resultado vem via callback)
      DNS_Query(dns, "www.google.com", MyDNSCallback, NULL);
  }
  
  // Exemplo: Aguardar sincronização NTP
  NTP_Timestamp_t current_time;
  while (Network_GetCurrentTime(&current_time) != NET_OK) {
      osDelay(5000);  // Aguardar 5s
  }
  
  // Unix timestamp disponível: current_time.seconds
  
  // Exemplo: Usar socket TCP customizado
  NetManager_t *mgr = Network_GetManager();
  NetSocket_t *my_socket = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_TCP);
  
  if (my_socket) {
      // Bind em porta local
      NetAddr_t local_addr = { .ip = NET_IP(0, 0, 0, 0), .port = 8080 };
      if (NetSocket_Bind(my_socket, &local_addr) == NET_OK) {
          
          // Conectar a servidor remoto
          NetAddr_t remote_addr = {
              .ip = NET_IP(192, 168, 1, 10),
              .port = 80
          };
          
          if (NetSocket_Connect(my_socket, &remote_addr) == NET_OK) {
              
              // Aguardar conexão estabelecer
              while (!NetSocket_IsConnected(my_socket)) {
                  osDelay(100);
              }
              
              // Enviar dados
              const char *http_request = "GET / HTTP/1.1\r\nHost: 192.168.1.10\r\n\r\n";
              NetSocket_Send(my_socket, (const uint8_t *)http_request, (uint16_t)strlen(http_request), 0);
              
              // Receber resposta
              uint8_t buffer[512];
              int32_t received = NetSocket_Recv(
                  my_socket,
                  buffer,
                  (uint16_t)(sizeof(buffer) - 1),
                  0
              );
              
              if (received > 0) {
                  buffer[received] = '\0';
                  // Processar resposta
              }
              
              // Fechar socket
              NetSocket_Close(my_socket);
          }
      }
      
      // Liberar socket
      NetManager_FreeSocket(mgr, my_socket);
  }
  
  /* Loop infinito */
  while (1)
  {
      osDelay(1000);
  }
  /* USER CODE END MyTask */
}

/* Callback DNS */
static void MyDNSCallback(const char *hostname, const uint8_t *ip, void *user_data)
{
    if (ip) {
        // DNS resolvido com sucesso
        // hostname foi resolvido para ip[0].ip[1].ip[2].ip[3]
    } else {
        // Falha ao resolver
    }
}

/* =============================================================================
 * PASSO 4: CALLBACK DO SPI DMA (IMPORTANTE!)
 * ============================================================================= */

/*
 * Os callbacks do SPI DMA já estão implementados em app_network.c
 * 
 * Se você tem outros callbacks SPI no seu código, mantenha-os e apenas
 * certifique-se de que não entram em conflito.
 * 
 * Os callbacks em app_network.c são:
 * - HAL_SPI_TxRxCpltCallback()
 * - HAL_SPI_TxCpltCallback()
 * - HAL_SPI_RxCpltCallback()
 * - HAL_SPI_ErrorCallback()
 * 
 * Se você precisa de callbacks customizados, modifique app_network.c
 * para chamar suas funções também.
 */

/* =============================================================================
 * PASSO 5: CONECTAR VIA TELNET
 * ============================================================================= */

/*
 * Após o sistema iniciar e obter IP via DHCP:
 * 
 * 1. Descubra o IP do STM32:
 *    - Verifique no router DHCP
 *    - Ou use comando "status" via serial se tiver debug
 * 
 * 2. Conecte via telnet do PC:
 *    telnet <IP_DO_STM32>
 *    
 *    Ou no Windows PowerShell:
 *    Test-NetConnection <IP_DO_STM32> -Port 23
 * 
 * 3. Comandos disponíveis:
 *    - help     : Lista comandos
 *    - status   : Mostra status da rede
 *    - uptime   : Mostra tempo de execução
 *    - reset    : Reinicia o sistema
 *    - exit     : Desconecta
 *    - ping     : Comando customizado de exemplo
 *    - led on/off : Comando customizado de exemplo
 * 
 * Você pode adicionar mais comandos modificando TelnetCommandHandler()
 * em app_network.c
 */

/* =============================================================================
 * PASSO 6: CONFIGURAÇÕES CUSTOMIZADAS
 * ============================================================================= */

/*
 * Para usar IP ESTÁTICO em vez de DHCP:
 * 
 * Em app_network.c, na função Network_Init(), modificar:
 * 
 *     NetManagerConfig_t mgr_config = {
 *         .hw_driver = &w5500_hw,
 *         .enable_dhcp = false,  // <-- MUDAR PARA false
 *         .event_callback = NetworkEventCallback,
 *         .user_data = NULL,
 *         
 *         .static_config = {
 *             .ip = {192, 168, 1, 100},        // <-- SEU IP
 *             .subnet = {255, 255, 255, 0},    // <-- SUA MÁSCARA
 *             .gateway = {192, 168, 1, 1},     // <-- SEU GATEWAY
 *             .dns = {8, 8, 8, 8},             // <-- SEU DNS
 *         },
 *     };
 * 
 * E comentar/remover o registro do serviço DHCP:
 * 
 *     // Não inicializar DHCP se usando IP estático
 *     // DHCP_Init(&dhcp_client, "STM32-W5500");
 *     // NetManager_RegisterService(&net_manager, DHCP_GetService(&dhcp_client));
 */

/* =============================================================================
 * PASSO 7: AJUSTAR PINOS GPIO DO W5500
 * ============================================================================= */

/*
 * Em app_network.c, função Network_Init(), ajustar os pinos:
 * 
 *     W5500_HW_Config_t hw_config = {
 *         .hspi = &hspi1,              // <-- Modificar se usar outro SPI
 *         .cs_port = GPIOA,            // <-- Porta do CS
 *         .cs_pin = GPIO_PIN_4,        // <-- Pino do CS
 *         .rst_port = NULL,            // <-- Porta do RST (NULL = soft reset)
 *         .rst_pin = 0,                // <-- Pino do RST (0 se NULL)
 *         .spi_mutex = spi_mutex,
 *     };
 * 
 * Se quiser usar reset por hardware:
 *         .rst_port = GPIOB,
 *         .rst_pin = GPIO_PIN_0,
 * 
 * E configurar o pino no CubeMX como GPIO Output.
 */

/* =============================================================================
 * PASSO 8: TROUBLESHOOTING
 * ============================================================================= */

/*
 * Se não funcionar:
 * 
 * 1. Verificar conexões SPI:
 *    - MOSI, MISO, SCK, CS conectados corretamente
 *    - W5500 alimentado (3.3V)
 *    - Cabo Ethernet conectado e link LED piscando
 * 
 * 2. Verificar DMA do SPI:
 *    - No CubeMX, SPI1 deve ter DMA configurado para TX e RX
 *    - DMA Requests: SPI1_TX e SPI1_RX
 *    - Priority: Medium ou High
 * 
 * 3. Verificar tamanho das stacks:
 *    - NetworkTask precisa de pelo menos 2048 bytes
 *    - Tasks que usam sockets precisam de 1024+ bytes
 * 
 * 4. Verificar prioridades:
 *    - NetworkTask deve ter prioridade Normal ou acima
 *    - Não colocar prioridade muito alta (pode travar outras tasks)
 * 
 * 5. Debug via Telnet:
 *    - Se DHCP funcionar, você conseguirá conectar via telnet
 *    - Use comando "status" para ver configuração de rede
 * 
 * 6. Timeouts:
 *    - Se DHCP demorar muito, pode ser problema no servidor DHCP
 *    - Tente IP estático primeiro para validar hardware
 */

#endif /* APP_NETWORK_INTEGRATION_EXAMPLE */

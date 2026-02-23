/*
 * app_network.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Header da aplicação de rede
 */

#ifndef APP_NETWORK_H_
#define APP_NETWORK_H_

#include "net_types.h"
#include "net_manager.h"
#include "w5500_hw.h"
#include "dhcp_service.h"
#include "dns_service.h"
#include "ntp_service.h"
#include "telnet_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * INICIALIZAÇÃO
 * ============================================================================= */

/**
 * @brief Inicializa toda a stack de rede
 * 
 * Esta função deve ser chamada UMA VEZ durante a inicialização do sistema,
 * ANTES de iniciar o scheduler do RTOS.
 * 
 * Inicializa:
 * - Driver de hardware W5500
 * - Gerenciador de rede
 * - Todos os serviços (DHCP, DNS, NTP, Telnet)
 * 
 * @return NET_OK se sucesso
 */
NetErr_t Network_Init(void);

/**
 * @brief Inicia a task de rede
 * 
 * Cria e inicia a task FreeRTOS que executa o loop principal da rede.
 * Deve ser chamada APÓS osKernelInitialize() mas ANTES de osKernelStart().
 * 
 * @return NET_OK se task criada com sucesso
 */
NetErr_t Network_StartTask(void);

/* =============================================================================
 * ACESSO AOS COMPONENTES
 * ============================================================================= */

/**
 * @brief Obtém ponteiro para o gerenciador de rede
 */
NetManager_t* Network_GetManager(void);

/**
 * @brief Obtém ponteiro para o driver de hardware W5500
 */
W5500_HW_t* Network_GetHardwareDriver(void);

/**
 * @brief Obtém ponteiro para o cliente DHCP
 */
DHCP_Client_t* Network_GetDHCPClient(void);

/**
 * @brief Obtém ponteiro para o cliente DNS
 */
DNS_Client_t* Network_GetDNSClient(void);

/**
 * @brief Obtém ponteiro para o cliente NTP
 */
NTP_Client_t* Network_GetNTPClient(void);

/**
 * @brief Obtém ponteiro para o servidor Telnet
 */
Telnet_Server_t* Network_GetTelnetServer(void);

/* =============================================================================
 * FUNÇÕES DE CONVENIÊNCIA
 * ============================================================================= */

/**
 * @brief Obtém endereço IP atual
 * @param ip Buffer para receber IP [4 bytes]
 * @return NET_OK se IP obtido
 */
NetErr_t Network_GetIPAddress(uint8_t ip[4]);

/**
 * @brief Verifica se a rede está conectada e configurada
 * @return true se link UP e IP configurado (DHCP bound ou estático)
 */
bool Network_IsConnected(void);

/**
 * @brief Resolve hostname para IP (assíncrono via DNS)
 * @param hostname Nome a resolver (ex: "www.google.com")
 * @param ip Buffer para receber IP [4 bytes] (apenas se já estiver em cache)
 * @return NET_OK se encontrado em cache, caso contrário query assíncrona iniciada
 */
NetErr_t Network_ResolveHostname(const char *hostname, uint8_t ip[4]);

/**
 * @brief Obtém tempo atual via NTP
 * @param timestamp Buffer para receber timestamp Unix
 * @return NET_OK se NTP sincronizado
 */
NetErr_t Network_GetCurrentTime(NTP_Timestamp_t *timestamp);

/**
 * @brief Define delay da NetworkTask (em millisegundos)
 * 
 * Quanto menor o delay, maior a responsividade mas maior o consumo de CPU.
 * Valores recomendados: 5ms (200Hz), 10ms (100Hz), 20ms (50Hz)
 * 
 * @note DEVE ser chamado ANTES de Network_StartTask()
 */
NetErr_t Network_SetTaskDelay(uint32_t delay_ms);

/**
 * @brief Define stack size da NetworkTask (em bytes)
 * 
 * Aumentar se receber stack overflow, reduzir para economizar memória.
 * Padrão: 8192 bytes (8KB)
 * 
 * @note DEVE ser chamado ANTES de Network_StartTask()
 */
NetErr_t Network_SetTaskStackSize(uint32_t stack_bytes);

/* =============================================================================
 * EXEMPLO DE USO
 * ============================================================================= */

/*
 * No main.c ou app_freertos.c:
 * 
 * int main(void) {
 *     HAL_Init();
 *     SystemClock_Config();
 *     MX_GPIO_Init();
 *     MX_DMA_Init();
 *     MX_SPI1_Init();
 *     
 *     // Inicializar kernel RTOS
 *     osKernelInitialize();
 *     
 *     // Inicializar rede
 *     if (Network_Init() != NET_OK) {
 *         Error_Handler();
 *     }
 *     
 *     // Criar task de rede
 *     if (Network_StartTask() != NET_OK) {
 *         Error_Handler();
 *     }
 *     
 *     // Criar outras tasks...
 *     
 *     // Iniciar scheduler
 *     osKernelStart();
 *     
 *     while (1);
 * }
 * 
 * 
 * Usar os serviços em outra task:
 * 
 * void MyTask(void *argument) {
 *     
 *     // Aguardar rede conectar
 *     while (!Network_IsConnected()) {
 *         osDelay(1000);
 *     }
 *     
 *     // Obter IP
 *     uint8_t ip[4];
 *     Network_GetIPAddress(ip);
 *     printf("IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
 *     
 *     // Resolver DNS
 *     uint8_t google_ip[4];
 *     Network_ResolveHostname("www.google.com", google_ip);
 *     
 *     // Aguardar sincronização NTP
 *     NTP_Timestamp_t time;
 *     while (Network_GetCurrentTime(&time) != NET_OK) {
 *         osDelay(1000);
 *     }
 *     printf("Unix time: %lu\n", time.seconds);
 *     
 *     // Conectar ao telnet: telnet <IP_DO_STM32>
 *     // Comandos disponíveis: help, status, uptime, reset, exit
 *     
 *     while (1) {
 *         osDelay(1000);
 *     }
 * }
 */

#ifdef __cplusplus
}
#endif

#endif /* APP_NETWORK_H_ */

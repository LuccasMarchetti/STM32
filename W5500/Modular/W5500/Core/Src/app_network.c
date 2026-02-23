/*
 * app_network.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Aplicação de rede - Inicialização e integração de todos os componentes
 */

#include "app_network.h"
#include "app_network_config.h"
#include "w5500_hw.h"
#include "net_manager.h"
#include "dhcp_service.h"
#include "dns_service.h"
#include "ntp_service.h"
#include "telnet_service.h"

#include "spi.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * VARIÁVEIS GLOBAIS
 * ============================================================================= */

/* Driver de hardware W5500 */
static W5500_HW_t w5500_hw;

/* Gerenciador de rede */
static NetManager_t net_manager;

/* Serviços */
static DHCP_Client_t dhcp_client;
static DNS_Client_t dns_client;
static NTP_Client_t ntp_client;
static Telnet_Server_t telnet_server;
static NetService_t dhcp_service = DHCP_CREATE_SERVICE(&dhcp_client);

/* Mutex SPI (compartilhado entre todas as operações W5500) */
static osMutexId_t spi_mutex;
static StaticSemaphore_t spi_mutex_cb;

/* Task handle */
static osThreadId_t network_task_handle;

/* Configuração de task (podem ser alteradas antes de Network_StartTask) */
static uint32_t network_task_delay_ms = 10;       // 10ms padrão = 100Hz
static uint32_t network_task_stack_size = 4*2048; // 8KB padrão

static const osThreadAttr_t network_task_attr_base = {
    .name = "NetworkTask",
    .priority = (osPriority_t) osPriorityNormal,
};

/* =============================================================================
 * CALLBACKS DE EVENTOS
 * ============================================================================= */

static void NetworkEventCallback(const NetEvent_t *event, void *user_data) {
    (void)user_data;
    
    switch (event->type) {
        case NET_EVENT_LINK_UP:
            // Link Ethernet conectado
            break;
        
        case NET_EVENT_LINK_DOWN:
            // Link Ethernet desconectado
            break;
        
        case NET_EVENT_DHCP_BOUND:
            // DHCP obteve IP - aplicar servidor NTP se disponível
            AppNetwork_ApplyDHCPNTPServer();
            break;
        
        case NET_EVENT_DHCP_LOST:
            // DHCP perdeu IP
            break;
        
        default:
            break;
    }
}

/* =============================================================================
 * CALLBACKS DE SERVIÇOS
 * ============================================================================= */

static void DHCPCallback_OnBound(void) {
    // DHCP bound - IP obtido
}

static void DNSCallback_OnResolved(const char *hostname, const uint8_t *ip, void *user_data) {
    (void)user_data;
    
    if (ip) {
        // DNS resolvido com sucesso: hostname -> ip[0].ip[1].ip[2].ip[3]
    } else {
        // Falha ao resolver hostname
    }
}

static void NTPCallback_OnSync(const NTP_Timestamp_t *timestamp, void *user_data) {
    (void)user_data;
    
    if (timestamp) {
        // NTP sincronizado: timestamp->seconds (Unix time)
    } else {
        // Falha na sincronização
    }
}

static void TelnetCommandHandler(
    const Telnet_Command_t *cmd,
    char *response,
    uint16_t max_len,
    void *user_data
) {
    (void)user_data;
    
    // Tratar comandos customizados aqui
    // Comandos padrão (help, status, uptime, reset, exit) já são tratados internamente
    
    // Interceptar "help" para adicionar comandos customizados à lista
    if (strcmp(cmd->cmd, "help") == 0) {
        snprintf(response, max_len,
            "Available commands:\r\n"
            "  help     - Show this help\r\n"
            "  status   - Show network status\r\n"
            "  uptime   - Show uptime\r\n"
            "  reset    - Reset system\r\n"
            "  exit     - Disconnect\r\n"
            "  ping     - Test connection (custom)\r\n"
            "  led      - LED control (custom)\r\n"
            "  info     - Show system info\r\n"
        );
    }
    else if (strcmp(cmd->cmd, "info") == 0) {
        snprintf(response, max_len,
            "System Info:\r\n"
            "  MCU: STM32G0B1\r\n"
            "  Network: W5500 Ethernet\r\n"
            "  DHCP: %s\r\n"
            "  State: %d\r\n",
            Network_IsConnected() ? "Enabled" : "Disabled",
            (int)NetManager_GetState(&net_manager)
        );
    }
    // Exemplo de comando customizado:
    else if (strcmp(cmd->cmd, "ping") == 0) {
        snprintf(response, max_len, "Pong!\r\n");
    }
    else if (strcmp(cmd->cmd, "led") == 0) {
        if (cmd->arg_count > 0) {
            if (strcmp(cmd->args[0], "on") == 0) {
                // Acender LED
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
                snprintf(response, max_len, "LED ON\r\n");
            } else if (strcmp(cmd->args[0], "off") == 0) {
                // Apagar LED
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
                snprintf(response, max_len, "LED OFF\r\n");
            } else if (strcmp(cmd->args[0], "toggle") == 0) {
                // Alternar LED
                GPIO_PinState state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_6);
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
                snprintf(response, max_len, "LED %s\r\n", (state == GPIO_PIN_SET) ? "OFF" : "ON");
            }
        } else {
            snprintf(response, max_len, "Usage: led [on|off|toggle]\r\n");
        }
    }
    // Se não for comando conhecido, será tratado pelo handler padrão
}

/* =============================================================================
 * CONFIGURAÇÃO GPIO W5500
 * ============================================================================= */

static void W5500_GPIO_Init(void) {
    // Configurar pino RST (se usado)
    // Já deve estar configurado pelo MX_GPIO_Init() do CubeMX
    
    // Garantir que W5500 está em reset
    // HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    // osDelay(50);
    // HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    // osDelay(200);
}

/* =============================================================================
 * INICIALIZAÇÃO DA REDE
 * ============================================================================= */

NetErr_t Network_Init(void) {
    
    HAL_StatusTypeDef hal_ret;
    NetErr_t net_ret;
    
    // 1. Inicializar GPIO do W5500
    W5500_GPIO_Init();
    
    // 2. Criar mutex SPI
    const osMutexAttr_t mutex_attr = {
        .name = "SPI_Mutex",
        .cb_mem = &spi_mutex_cb,
        .cb_size = sizeof(spi_mutex_cb),
    };
    
    spi_mutex = osMutexNew(&mutex_attr);
    if (!spi_mutex) {
        return NET_ERR_NO_MEMORY;
    }
    
    // 3. Configurar driver de hardware W5500
    W5500_HW_Config_t hw_config = {
        .hspi = &hspi1,              // SPI handle (ajustar conforme projeto)
        .cs_port = GPIOA,            // GPIO do CS (ajustar conforme projeto)
        .cs_pin = GPIO_PIN_4,        // Pino do CS (ajustar conforme projeto)
        .rst_port = NULL,            // Pino RST (NULL = usar soft reset)
        .rst_pin = 0,
        .spi_mutex = spi_mutex,
    };
    
    hal_ret = W5500_HW_Init(&w5500_hw, &hw_config);
    if (hal_ret != HAL_OK) {
        return NET_ERR_HW_FAILURE;
    }
    
    // 4. Configurar gerenciador de rede
    NetManagerConfig_t mgr_config = {
        .task_period_ms = 100,
        .use_dhcp = true,         // Usar DHCP (mudar para false para IP estático)
        .enable_link_monitor = true,
    };
    
    // Se IP estático, configurar manualmente após init
    bool use_static_ip = false;
    NetConfig_t static_config = {
        .ip = NET_IP(192, 168, 1, 100),
        .subnet = NET_IP(255, 255, 255, 0),
        .gateway = NET_IP(192, 168, 1, 1),
        .dns = { NET_IP(8, 8, 8, 8), NET_IP(8, 8, 4, 4) },
    };
    
    net_ret = NetManager_Init(&net_manager, &w5500_hw, &mgr_config);
    if (net_ret != NET_OK) {
        return net_ret;
    }
    
    // Configurar callback de eventos
    NetManager_SetEventCallback(&net_manager, NetworkEventCallback, NULL);
    
    // Se IP estático, aplicar agora
    if (use_static_ip) {
        W5500_HW_GetMAC(&w5500_hw, static_config.mac.addr);
        NetManager_ApplyConfig(&net_manager, &static_config);
    }
    
    // 5. Inicializar e registrar serviços
    
    // DHCP
    if (mgr_config.use_dhcp) {
        DHCP_Init(&dhcp_client, "STM32-W5500");
        NetManager_RegisterService(&net_manager, &dhcp_service);
    }
    
    // DNS
    uint8_t dns_server[] = {8, 8, 8, 8};  // Google DNS
    DNS_Init(&dns_client, dns_server);
    NetManager_RegisterService(&net_manager, DNS_GetService(&dns_client));
    
    // NTP
    uint8_t ntp_server[] = {193, 182, 111, 12};  // pool.ntp.org
    NTP_Init(&ntp_client, ntp_server, 0);  // 0 = usar intervalo padrão
    NetManager_RegisterService(&net_manager, NTP_GetService(&ntp_client));
    
    // Telnet
    Telnet_Init(&telnet_server, TELNET_PORT, TelnetCommandHandler, NULL);
    NetManager_RegisterService(&net_manager, Telnet_GetService(&telnet_server));
    Telnet_Start(&telnet_server);
    
    return NET_OK;
}

/* =============================================================================
 * TASK PRINCIPAL DA REDE
 * ============================================================================= */

static void NetworkTask(void *argument) {
    (void)argument;
    
    // Loop principal
    while (1) {
        // Executar task do gerenciador (processa sockets, eventos e serviços)
        NetManager_Task(&net_manager);
        
        // Delay configurável (padrão 10ms)
        osDelay(network_task_delay_ms);
    }
}

/* =============================================================================
 * INICIALIZAÇÃO DA TASK
 * ============================================================================= */

NetErr_t Network_StartTask(void) {
    
    // Preparar atributos com valores configuráveis
    osThreadAttr_t task_attr = network_task_attr_base;
    task_attr.stack_size = network_task_stack_size;
    
    network_task_handle = osThreadNew(NetworkTask, NULL, &task_attr);
    
    if (!network_task_handle) {
        return NET_ERR_NO_MEMORY;
    }
    
    return NET_OK;
}

/**
 * @brief Define delay da NetworkTask (DEVE ser chamado ANTES de Network_StartTask)
 */
NetErr_t Network_SetTaskDelay(uint32_t delay_ms) {
    if (delay_ms == 0) {
        return NET_ERR_PARAM;
    }
    
    if (network_task_handle != NULL) {
        // Já iniciado
        return NET_ERR_STATE;
    }
    
    network_task_delay_ms = delay_ms;
    return NET_OK;
}

/**
 * @brief Define stack size da NetworkTask (DEVE ser chamado ANTES de Network_StartTask)
 */
NetErr_t Network_SetTaskStackSize(uint32_t stack_bytes) {
    if (stack_bytes == 0 || stack_bytes < 2048) {
        return NET_ERR_PARAM;  // Mínimo 2KB
    }
    
    if (network_task_handle != NULL) {
        // Já iniciado
        return NET_ERR_STATE;
    }
    
    network_task_stack_size = stack_bytes;
    return NET_OK;
}

/* =============================================================================
 * API PÚBLICA PARA APLICAÇÃO
 * ============================================================================= */

NetManager_t* Network_GetManager(void) {
    return &net_manager;
}

W5500_HW_t* Network_GetHardwareDriver(void) {
    return &w5500_hw;
}

DHCP_Client_t* Network_GetDHCPClient(void) {
    return &dhcp_client;
}

DNS_Client_t* Network_GetDNSClient(void) {
    return &dns_client;
}

NTP_Client_t* Network_GetNTPClient(void) {
    return &ntp_client;
}

Telnet_Server_t* Network_GetTelnetServer(void) {
    return &telnet_server;
}

/* =============================================================================
 * FUNÇÕES DE USO COMUM
 * ============================================================================= */

NetErr_t Network_GetIPAddress(uint8_t ip[4]) {
    NetConfig_t config;
    NetErr_t ret = NetManager_GetNetworkConfig(&net_manager, &config);
    if (ret == NET_OK) {
        memcpy(ip, config.ip.addr, 4);
    }
    return ret;
}

bool Network_IsConnected(void) {
    return NetManager_IsReady(&net_manager);
}

NetErr_t Network_ResolveHostname(const char *hostname, uint8_t ip[4]) {
    
    // Primeiro verificar cache
    if (DNS_LookupCache(&dns_client, hostname, ip) == NET_OK) {
        return NET_OK;
    }
    
    // Iniciar query assíncrona (resultado virá via callback)
    return DNS_Query(&dns_client, hostname, DNSCallback_OnResolved, NULL);
}

NetErr_t Network_GetCurrentTime(NTP_Timestamp_t *timestamp) {
    return NTP_GetTime(&ntp_client, timestamp);
}

/* =============================================================================
 * CALLBACK DO SPI (chamado pela HAL após DMA completar)
 * ============================================================================= */

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    
    if (hspi == w5500_hw.ll.hspi) {
        // Sinalizar semáforo de conclusão do DMA
        if (w5500_hw.ll.spi_done_sem) {
            osSemaphoreRelease(w5500_hw.ll.spi_done_sem);
        }
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    
    if (hspi == w5500_hw.ll.hspi) {
        // Sinalizar semáforo de conclusão do DMA
        if (w5500_hw.ll.spi_done_sem) {
            osSemaphoreRelease(w5500_hw.ll.spi_done_sem);
        }
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    
    if (hspi == w5500_hw.ll.hspi) {
        // Sinalizar semáforo de conclusão do DMA
        if (w5500_hw.ll.spi_done_sem) {
            osSemaphoreRelease(w5500_hw.ll.spi_done_sem);
        }
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
    
    if (hspi == w5500_hw.ll.hspi) {
        // Erro no SPI - sinalizar semáforo para desbloquear
        if (w5500_hw.ll.spi_done_sem) {
            osSemaphoreRelease(w5500_hw.ll.spi_done_sem);
        }
    }
}

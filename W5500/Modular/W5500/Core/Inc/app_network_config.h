/*
 * app_network_config.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Configuração da aplicação de rede (NTP, mDNS, task delay)
 */

#ifndef APP_NETWORK_CONFIG_H_
#define APP_NETWORK_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "net_types.h"

/* =============================================================================
 * CONFIGURAÇÃO DE INICIALIZAÇÃO
 * ============================================================================= */

typedef struct {
    /*--- NTP ---*/
    bool     ntp_enabled;           // Habilitar serviço NTP
    bool     ntp_use_dns;           // Usar DNS para resolver servidor NTP
    const char *ntp_hostname;       // Hostname (ex: "pool.ntp.org") se ntp_use_dns=true
    uint8_t  ntp_server_ip[4];      // IP fixo se ntp_use_dns=false
    uint32_t ntp_sync_interval_ms;  // Intervalo entre sincronizações (0=padrão 60s)
    
    /*--- NTP via DHCP ---*/
    bool     ntp_accept_from_dhcp;  // Aceitar servidor NTP fornecido por DHCP
    
    /*--- mDNS ---*/
    bool     mdns_enabled;          // Habilitar responder mDNS
    const char *mdns_hostname;      // Nome mDNS (ex: "stm32-w5500.local")
    
    /*--- Task de rede ---*/
    uint32_t network_task_delay_ms; // Delay da NetworkTask (0=padrão 10ms)
    uint32_t network_task_stack_bytes; // Stack size (0=padrão 8KB)
    
    /*--- Gerais ---*/
    bool     use_dhcp;              // Usar DHCP vs IP estático
    
} AppNetworkConfig_t;

/* =============================================================================
 * FUNÇÕES DE CONFIGURAÇÃO
 * ============================================================================= */

/**
 * @brief Obtém configuração padrão
 */
AppNetworkConfig_t AppNetwork_GetDefaultConfig(void);

/**
 * @brief Inicializa rede com configuração customizada
 */
int AppNetwork_InitEx(const AppNetworkConfig_t *config);

/**
 * @brief Inicia task de rede com delay customizado
 */
int AppNetwork_StartTaskEx(uint32_t delay_ms);

/**
 * @brief Define servidor NTP por hostname (usa DNS para resolver)
 */
int AppNetwork_SetNTPServerByHostname(const char *hostname);

/**
 * @brief Define servidor NTP por IP
 */
int AppNetwork_SetNTPServerByIP(const uint8_t ip[4]);

/**
 * @brief Define nome mDNS
 */
int AppNetwork_SetmDNSHostname(const char *hostname);

#ifdef __cplusplus
}
#endif

#endif /* APP_NETWORK_CONFIG_H_ */

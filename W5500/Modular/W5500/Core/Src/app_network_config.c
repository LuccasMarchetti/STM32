/*
 * app_network_config.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação de configuração para app_network
 */

#include "app_network_config.h"
#include "app_network.h"
#include "ntp_service.h"
#include "dns_service.h"
#include <string.h>

/* Configuração padrão global */
static AppNetworkConfig_t g_net_config = {
    .ntp_enabled = true,
    .ntp_use_dns = false,
    .ntp_hostname = NULL,
    .ntp_server_ip = {193, 182, 111, 12},  // pool.ntp.org
    .ntp_sync_interval_ms = 60000,  // 1 minuto
    .ntp_accept_from_dhcp = true,
    
    .mdns_enabled = false,
    .mdns_hostname = NULL,
    
    .network_task_delay_ms = 10,
    .network_task_stack_bytes = 0,  // 0 = usar padrão (8KB)
    
    .use_dhcp = true,
};

/**
 * @brief Obtém configuração padrão
 */
AppNetworkConfig_t AppNetwork_GetDefaultConfig(void) {
    return g_net_config;
}

/**
 * @brief Define servidor NTP por hostname (usa DNS para resolver)
 */
int AppNetwork_SetNTPServerByHostname(const char *hostname) {
    if (!hostname) return -1;
    
    NTP_Client_t *ntp = Network_GetNTPClient();
    if (!ntp) return -1;
    
    return (NTP_SetServerByHostname(ntp, hostname) == NET_OK) ? 0 : -1;
}

/**
 * @brief Define servidor NTP por IP
 */
int AppNetwork_SetNTPServerByIP(const uint8_t ip[4]) {
    if (!ip) return -1;
    
    NTP_Client_t *ntp = Network_GetNTPClient();
    if (!ntp) return -1;
    
    return (NTP_SetServer(ntp, ip) == NET_OK) ? 0 : -1;
}

/**
 * @brief Define nome mDNS
 */
int AppNetwork_SetmDNSHostname(const char *hostname) {
    // Será implementado quando mDNS for adicionado
    (void)hostname;
    return -1;
}

/**
 * @brief Aplica servidor NTP do DHCP se disponível
 */
void AppNetwork_ApplyDHCPNTPServer(void) {
    if (!g_net_config.ntp_accept_from_dhcp) return;
    
    DHCP_Client_t *dhcp = Network_GetDHCPClient();
    NTP_Client_t *ntp = Network_GetNTPClient();
    
    if (!dhcp || !ntp) return;
    
    // Verificar se DHCP obteve servidor NTP
    uint8_t ntp_ip[4];
    memcpy(ntp_ip, dhcp->ntp_server.addr, 4);
    
    // Se IP válido (não tudo zeros), usar
    if ((ntp_ip[0] | ntp_ip[1] | ntp_ip[2] | ntp_ip[3]) != 0) {
        NTP_SetServer(ntp, ntp_ip);
    }
}

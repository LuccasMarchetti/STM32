/*
 * dns_service.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Cliente DNS (Domain Name System)
 */

#ifndef DNS_SERVICE_H_
#define DNS_SERVICE_H_

#include "net_types.h"
#include "net_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DEFINIÇÕES
 * ============================================================================= */

#define DNS_MAX_HOSTNAME    64
#define DNS_MAX_CACHE       4
#define DNS_TIMEOUT_MS      5000
#define DNS_RETRIES         3

/* Estados do cliente DNS */
typedef enum {
    DNS_STATE_IDLE = 0,
    DNS_STATE_QUERYING,
    DNS_STATE_RESOLVED,
    DNS_STATE_ERROR
} DNS_State_t;

/* Entrada de cache DNS */
typedef struct {
    char hostname[DNS_MAX_HOSTNAME];
    uint8_t ip[4];
    uint32_t timestamp;
    uint32_t ttl;
    bool valid;
} DNS_CacheEntry_t;

/* Query callback */
typedef void (*DNS_QueryCallback_t)(const char *hostname, const uint8_t *ip, void *user_data);

/* Cliente DNS */
typedef struct {
    NetService_t service;           // Interface de serviço
    NetManager_t *manager;          // Gerenciador de rede
    NetSocket_t *socket;            // Socket UDP
    
    uint8_t dns_server[4];          // Servidor DNS
    DNS_State_t state;              // Estado atual
    
    char query_hostname[DNS_MAX_HOSTNAME];
    uint16_t query_id;
    uint32_t last_send;
    uint8_t retry_count;
    
    DNS_QueryCallback_t callback;
    void *user_data;
    
    DNS_CacheEntry_t cache[DNS_MAX_CACHE];
    
} DNS_Client_t;

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

/**
 * @brief Inicializa cliente DNS
 * @param client Ponteiro para estrutura do cliente
 * @param dns_server IP do servidor DNS [4 bytes]
 * @return NET_OK se sucesso
 */
NetErr_t DNS_Init(DNS_Client_t *client, const uint8_t *dns_server);

/**
 * @brief Resolve hostname para IP (assíncrono)
 * @param client Ponteiro para cliente
 * @param hostname Nome a resolver
 * @param callback Função chamada quando resolver
 * @param user_data Dados do usuário
 * @return NET_OK se query foi iniciada
 */
NetErr_t DNS_Query(
    DNS_Client_t *client,
    const char *hostname,
    DNS_QueryCallback_t callback,
    void *user_data
);

/**
 * @brief Busca IP no cache
 * @param client Ponteiro para cliente
 * @param hostname Nome a buscar
 * @param ip Buffer para IP [4 bytes]
 * @return NET_OK se encontrado no cache
 */
NetErr_t DNS_LookupCache(const DNS_Client_t *client, const char *hostname, uint8_t *ip);

/**
 * @brief Define servidor DNS
 * @param client Ponteiro para cliente
 * @param dns_server IP do servidor [4 bytes]
 * @return NET_OK se sucesso
 */
NetErr_t DNS_SetServer(DNS_Client_t *client, const uint8_t *dns_server);

/**
 * @brief Limpa cache DNS
 */
void DNS_ClearCache(DNS_Client_t *client);

/**
 * @brief Obtém interface de serviço
 */
NetService_t* DNS_GetService(DNS_Client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* DNS_SERVICE_H_ */

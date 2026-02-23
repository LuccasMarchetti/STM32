/*
 * dns_service.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação do cliente DNS
 */

#include "dns_service.h"
#include "net_socket.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include <string.h>
#include <ctype.h>

/* DNS Header */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} DNS_Header_t;

#define DNS_PORT            53
#define DNS_FLAG_QUERY      0x0000
#define DNS_FLAG_RESPONSE   0x8000
#define DNS_CLASS_IN        1
#define DNS_TYPE_A          1

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static uint16_t EncodeDomainName(const char *hostname, uint8_t *buffer) {
    uint16_t offset = 0;
    const char *start = hostname;
    const char *ptr = hostname;
    
    while (*ptr) {
        if (*ptr == '.' || *(ptr + 1) == '\0') {
            uint8_t len = ptr - start;
            if (*(ptr + 1) == '\0' && *ptr != '.') len++;
            
            buffer[offset++] = len;
            memcpy(&buffer[offset], start, len);
            offset += len;
            
            if (*ptr == '.') {
                ptr++;
                start = ptr;
            } else {
                ptr++;
            }
        } else {
            ptr++;
        }
    }
    
    buffer[offset++] = 0;  // Terminator
    return offset;
}

static uint16_t DecodeDomainName(const uint8_t *buffer, uint16_t offset, char *hostname, uint16_t max_len) {
    uint16_t pos = offset;
    uint16_t hostname_pos = 0;
    bool jumped = false;
    uint16_t jumps = 0;
    
    while (buffer[pos] != 0 && jumps < 10) {
        if ((buffer[pos] & 0xC0) == 0xC0) {
            // Pointer
            if (!jumped) offset = pos + 2;
            pos = ((buffer[pos] & 0x3F) << 8) | buffer[pos + 1];
            jumped = true;
            jumps++;
        } else {
            // Label
            uint8_t len = buffer[pos++];
            if (hostname_pos + len + 1 < max_len) {
                if (hostname_pos > 0) hostname[hostname_pos++] = '.';
                memcpy(&hostname[hostname_pos], &buffer[pos], len);
                hostname_pos += len;
            }
            pos += len;
        }
    }
    
    hostname[hostname_pos] = '\0';
    
    if (!jumped) offset = pos + 1;
    
    return offset;
}

static void AddToCache(DNS_Client_t *client, const char *hostname, const uint8_t *ip, uint32_t ttl) {
    
    // Buscar entrada existente ou mais antiga
    DNS_CacheEntry_t *entry = NULL;
    uint32_t oldest_time = 0xFFFFFFFF;
    
    for (int i = 0; i < DNS_MAX_CACHE; i++) {
        if (!client->cache[i].valid) {
            entry = &client->cache[i];
            break;
        }
        if (strcmp(client->cache[i].hostname, hostname) == 0) {
            entry = &client->cache[i];
            break;
        }
        if (client->cache[i].timestamp < oldest_time) {
            oldest_time = client->cache[i].timestamp;
            entry = &client->cache[i];
        }
    }
    
    if (entry) {
        strncpy(entry->hostname, hostname, DNS_MAX_HOSTNAME - 1);
        memcpy(entry->ip, ip, 4);
        entry->timestamp = osKernelGetTickCount();
        entry->ttl = ttl;
        entry->valid = true;
    }
}

/* =============================================================================
 * CALLBACK DE RECEPÇÃO
 * ============================================================================= */

static void DNS_RxCallback(NetSocket_t *socket, void *user_data) {
    DNS_Client_t *client = (DNS_Client_t *)user_data;
    
    if (!client || client->state != DNS_STATE_QUERYING) return;
    
    uint8_t buffer[512];
    NetAddr_t src_addr;
    
    int32_t len = NetSocket_RecvFrom(socket, buffer, sizeof(buffer), &src_addr, 0);
    
    if (len < (int32_t)sizeof(DNS_Header_t)) return;
    
    DNS_Header_t *hdr = (DNS_Header_t *)buffer;
    
    // Verificar ID
    if (__builtin_bswap16(hdr->id) != client->query_id) return;
    
    // Verificar se é resposta
    if ((__builtin_bswap16(hdr->flags) & DNS_FLAG_RESPONSE) == 0) return;
    
    // Pular header e question section
    uint16_t offset = sizeof(DNS_Header_t);
    
    // Pular question (contém o nome que enviamos)
    char temp_hostname[DNS_MAX_HOSTNAME];
    offset = DecodeDomainName(buffer, offset, temp_hostname, sizeof(temp_hostname));
    offset += 4;  // Pular QTYPE e QCLASS
    
    // Ler answers
    uint16_t answer_count = __builtin_bswap16(hdr->ancount);
    
    for (uint16_t i = 0; i < answer_count && offset < len; i++) {
        // Decodificar nome
        offset = DecodeDomainName(buffer, offset, temp_hostname, sizeof(temp_hostname));
        
        // Ler TYPE, CLASS, TTL, RDLENGTH
        uint16_t type = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
        offset += 2;  // CLASS
        uint32_t ttl = ((uint32_t)buffer[offset] << 24) | 
                       ((uint32_t)buffer[offset + 1] << 16) |
                       ((uint32_t)buffer[offset + 2] << 8) |
                       buffer[offset + 3];
        offset += 4;
        uint16_t rdlength = (buffer[offset] << 8) | buffer[offset + 1];
        offset += 2;
        
        // Se for tipo A (IPv4)
        if (type == DNS_TYPE_A && rdlength == 4) {
            uint8_t ip[4];
            memcpy(ip, &buffer[offset], 4);
            
            // Adicionar ao cache
            AddToCache(client, client->query_hostname, ip, ttl);
            
            // Chamar callback
            if (client->callback) {
                client->callback(client->query_hostname, ip, client->user_data);
            }
            
            client->state = DNS_STATE_RESOLVED;
            return;
        }
        
        offset += rdlength;
    }
}

/* =============================================================================
 * IMPLEMENTAÇÃO DO SERVIÇO
 * ============================================================================= */

static NetErr_t DNS_ServiceInit(void *context, void *manager) {
    DNS_Client_t *client = (DNS_Client_t *)context;
    NetManager_t *mgr = (NetManager_t *)manager;
    
    if (!client) return NET_ERR_PARAM;
    
    client->manager = mgr;
    
    // Criar socket UDP
    client->socket = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_UDP);
    if (!client->socket) return NET_ERR_RESOURCE;
    
    NetAddr_t local_addr = { .ip = { .addr = {0, 0, 0, 0} }, .port = 0 };
    NetSocket_Bind(client->socket, &local_addr);
    NetSocket_SetRxCallback(client->socket, DNS_RxCallback, client);
    
    return NET_OK;
}

static void DNS_ServiceTask(void *context) {
    DNS_Client_t *client = (DNS_Client_t *)context;
    
    if (!client || !client->manager) return;
    
    if (client->state == DNS_STATE_QUERYING) {
        uint32_t now = osKernelGetTickCount();
        
        if ((now - client->last_send) > pdMS_TO_TICKS(DNS_TIMEOUT_MS)) {
            if (client->retry_count < DNS_RETRIES) {
                // Reenviar query
                uint8_t buffer[256];
                DNS_Header_t *hdr = (DNS_Header_t *)buffer;
                
                hdr->id = __builtin_bswap16(client->query_id);
                hdr->flags = __builtin_bswap16(DNS_FLAG_QUERY | 0x0100);  // Recursion desired
                hdr->qdcount = __builtin_bswap16(1);
                hdr->ancount = 0;
                hdr->nscount = 0;
                hdr->arcount = 0;
                
                uint16_t offset = sizeof(DNS_Header_t);
                offset += EncodeDomainName(client->query_hostname, &buffer[offset]);
                
                // QTYPE
                buffer[offset++] = (DNS_TYPE_A >> 8) & 0xFF;
                buffer[offset++] = DNS_TYPE_A & 0xFF;
                
                // QCLASS
                buffer[offset++] = (DNS_CLASS_IN >> 8) & 0xFF;
                buffer[offset++] = DNS_CLASS_IN & 0xFF;
                
                NetAddr_t dest = {
                    .ip = { .addr = { client->dns_server[0], client->dns_server[1], client->dns_server[2], client->dns_server[3] } },
                    .port = DNS_PORT
                };
                
                NetSocket_SendTo(client->socket, buffer, offset, &dest, 0);
                client->last_send = now;
                client->retry_count++;
            } else {
                // Timeout
                client->state = DNS_STATE_ERROR;
                if (client->callback) {
                    client->callback(client->query_hostname, NULL, client->user_data);
                }
            }
        }
    }
}

static void DNS_ServiceDeinit(void *context) {
    DNS_Client_t *client = (DNS_Client_t *)context;
    
    if (client && client->socket) {
        NetSocket_Close(client->socket);
        NetManager_FreeSocket(client->manager, client->socket);
        client->socket = NULL;
    }
}

static void DNS_ServiceEventHandler(void *context, const NetEvent_t *event) {
    DNS_Client_t *client = (DNS_Client_t *)context;
    
    if (!client || !event) return;
    
    switch (event->type) {
        case NET_EVENT_DHCP_BOUND:
            // Atualizar servidor DNS do DHCP (se disponível)
            break;
        
        default:
            break;
    }
}

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

NetErr_t DNS_Init(DNS_Client_t *client, const uint8_t *dns_server) {
    
    if (!client) return NET_ERR_PARAM;
    
    memset(client, 0, sizeof(DNS_Client_t));
    
    if (dns_server) {
        memcpy(client->dns_server, dns_server, 4);
    }
    
    client->state = DNS_STATE_IDLE;
    
    // Configurar interface de serviço
    client->service.name = "DNS";
    client->service.context = client;
    client->service.init = (void (*)(struct NetService *, void *))DNS_ServiceInit;
    client->service.task = (void (*)(struct NetService *))DNS_ServiceTask;
    client->service.deinit = (void (*)(struct NetService *))DNS_ServiceDeinit;
    client->service.event_handler = (void (*)(struct NetService *, const NetEvent_t *))DNS_ServiceEventHandler;
    client->service.enabled = true;
    
    return NET_OK;
}

NetErr_t DNS_Query(
    DNS_Client_t *client,
    const char *hostname,
    DNS_QueryCallback_t callback,
    void *user_data
) {
    if (!client || !hostname || !callback) {
        return NET_ERR_PARAM;
    }
    
    if (client->state == DNS_STATE_QUERYING) {
        return NET_ERR_STATE;  // Query em andamento
    }
    
    // Verificar cache primeiro
    uint8_t cached_ip[4];
    if (DNS_LookupCache(client, hostname, cached_ip) == NET_OK) {
        callback(hostname, cached_ip, user_data);
        return NET_OK;
    }
    
    // Iniciar query
    strncpy(client->query_hostname, hostname, DNS_MAX_HOSTNAME - 1);
    client->query_id = (uint16_t)(osKernelGetTickCount() & 0xFFFF);
    client->callback = callback;
    client->user_data = user_data;
    client->retry_count = 0;
    client->state = DNS_STATE_QUERYING;
    client->last_send = 0;  // Força envio imediato
    
    return NET_OK;
}

NetErr_t DNS_LookupCache(const DNS_Client_t *client, const char *hostname, uint8_t *ip) {
    
    if (!client || !hostname || !ip) {
        return NET_ERR_PARAM;
    }
    
    uint32_t now = osKernelGetTickCount();
    
    for (int i = 0; i < DNS_MAX_CACHE; i++) {
        if (client->cache[i].valid && 
            strcmp(client->cache[i].hostname, hostname) == 0) {
            
            // Verificar se expirou
            uint32_t age = (now - client->cache[i].timestamp) / 1000;
            if (age < client->cache[i].ttl) {
                memcpy(ip, client->cache[i].ip, 4);
                return NET_OK;
            }
        }
    }
    
    return NET_ERR_NOT_FOUND;
}

NetErr_t DNS_SetServer(DNS_Client_t *client, const uint8_t *dns_server) {
    
    if (!client || !dns_server) {
        return NET_ERR_PARAM;
    }
    
    memcpy(client->dns_server, dns_server, 4);
    return NET_OK;
}

void DNS_ClearCache(DNS_Client_t *client) {
    if (!client) return;
    
    for (int i = 0; i < DNS_MAX_CACHE; i++) {
        client->cache[i].valid = false;
    }
}

NetService_t* DNS_GetService(DNS_Client_t *client) {
    return client ? &client->service : NULL;
}

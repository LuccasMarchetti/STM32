/*
 * dhcp_service.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação do cliente DHCP
 */

#include "dhcp_service.h"
#include "net_socket.h"
#include "net_manager.h"
#include "w5500_hw.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include <string.h>
#include <stdlib.h>

/* Constantes DHCP */
#define DHCP_SERVER_PORT    67
#define DHCP_CLIENT_PORT    68

#define DHCP_MAGIC_COOKIE   0x63825363

#define DHCP_OP_BOOTREQUEST 1
#define DHCP_OP_BOOTREPLY   2

#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET  6

/* Códigos de opções DHCP */
#define DHCP_OPT_PAD            0
#define DHCP_OPT_SUBNET_MASK    1
#define DHCP_OPT_ROUTER         3
#define DHCP_OPT_DNS_SERVER     6
#define DHCP_OPT_HOSTNAME       12
#define DHCP_OPT_NTP_SERVER     42    // Servidor NTP (RFC 2132)
#define DHCP_OPT_REQUESTED_IP   50
#define DHCP_OPT_LEASE_TIME     51
#define DHCP_OPT_MSG_TYPE       53
#define DHCP_OPT_SERVER_ID      54
#define DHCP_OPT_PARAM_REQUEST  55
#define DHCP_OPT_RENEWAL_TIME   58
#define DHCP_OPT_REBIND_TIME    59
#define DHCP_OPT_CLIENT_ID      61
#define DHCP_OPT_END            255

/* Tipos de mensagens DHCP */
#define DHCP_MSG_DISCOVER   1
#define DHCP_MSG_OFFER      2
#define DHCP_MSG_REQUEST    3
#define DHCP_MSG_DECLINE    4
#define DHCP_MSG_ACK        5
#define DHCP_MSG_NAK        6
#define DHCP_MSG_RELEASE    7

/* Timeouts */
#define DHCP_DISCOVER_TIMEOUT_MS    5000
#define DHCP_REQUEST_TIMEOUT_MS     5000
#define DHCP_RENEW_RETRIES          3

/* Estrutura de pacote DHCP */
typedef struct __attribute__((packed)) {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint8_t  ciaddr[4];
    uint8_t  yiaddr[4];
    uint8_t  siaddr[4];
    uint8_t  giaddr[4];
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[312];
} DHCP_Packet_t;

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

static uint32_t GenerateXID(const uint8_t *mac) {
    uint32_t xid = osKernelGetTickCount();
    for (int i = 0; i < 6; i++) {
        xid ^= (mac[i] << (i * 4));
    }
    return xid;
}

static uint16_t AddOption(uint8_t *options, uint16_t offset, uint8_t code, const void *data, uint8_t len) {
    options[offset++] = code;
    if (code != DHCP_OPT_PAD && code != DHCP_OPT_END) {
        options[offset++] = len;
        if (data && len > 0) {
            memcpy(&options[offset], data, len);
            offset += len;
        }
    }
    return offset;
}

static bool ParseOptions(
    const uint8_t *options,
    uint16_t options_len,
    uint8_t *msg_type,
    uint8_t offered_ip[4],
    uint8_t server_ip[4],
    uint8_t subnet[4],
    uint8_t gateway[4],
    uint8_t dns[4],
    uint8_t ntp_server[4],
    uint32_t *lease_time
) {
    uint16_t i = 0;
    bool found_msg_type = false;
    
    // Inicializar ntp_server
    memset(ntp_server, 0, 4);
    
    while (i < options_len) {
        uint8_t opt = options[i++];
        
        if (opt == DHCP_OPT_END) break;
        if (opt == DHCP_OPT_PAD) continue;
        
        uint8_t len = options[i++];
        
        switch (opt) {
            case DHCP_OPT_MSG_TYPE:
                if (len == 1) {
                    *msg_type = options[i];
                    found_msg_type = true;
                }
                break;
            
            case DHCP_OPT_SUBNET_MASK:
                if (len == 4) memcpy(subnet, &options[i], 4);
                break;
            
            case DHCP_OPT_ROUTER:
                if (len >= 4) memcpy(gateway, &options[i], 4);
                break;
            
            case DHCP_OPT_DNS_SERVER:
                if (len >= 4) memcpy(dns, &options[i], 4);
                break;
            
            case DHCP_OPT_NTP_SERVER:
                if (len >= 4) memcpy(ntp_server, &options[i], 4);
                break;
            
            case DHCP_OPT_SERVER_ID:
                if (len == 4) memcpy(server_ip, &options[i], 4);
                break;
            
            case DHCP_OPT_LEASE_TIME:
                if (len == 4) {
                    *lease_time = ((uint32_t)options[i] << 24) |
                                  ((uint32_t)options[i+1] << 16) |
                                  ((uint32_t)options[i+2] << 8) |
                                  ((uint32_t)options[i+3]);
                }
                break;
        }
        
        i += len;
    }
    
    return found_msg_type;
}

/* =============================================================================
 * CONSTRUÇÃO DE PACOTES
 * ============================================================================= */

static uint16_t BuildDHCPDiscover(
    DHCP_Packet_t *pkt,
    const uint8_t *mac,
    uint32_t xid,
    const char *hostname
) {
    memset(pkt, 0, sizeof(DHCP_Packet_t));
    
    pkt->op = DHCP_OP_BOOTREQUEST;
    pkt->htype = DHCP_HTYPE_ETHERNET;
    pkt->hlen = DHCP_HLEN_ETHERNET;
    pkt->hops = 0;
    pkt->xid = __builtin_bswap32(xid);
    pkt->secs = 0;
    pkt->flags = __builtin_bswap16(0x8000);  // Broadcast flag
    memcpy(pkt->chaddr, mac, 6);
    pkt->magic = __builtin_bswap32(DHCP_MAGIC_COOKIE);
    
    uint16_t opt_offset = 0;
    
    // Message type
    uint8_t msg_type = DHCP_MSG_DISCOVER;
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_MSG_TYPE, &msg_type, 1);
    
    // Client identifier
    uint8_t client_id[7];
    client_id[0] = DHCP_HTYPE_ETHERNET;
    memcpy(&client_id[1], mac, 6);
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_CLIENT_ID, client_id, 7);
    
    // Hostname
    if (hostname) {
        uint8_t hostname_len = strlen(hostname);
        opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_HOSTNAME, hostname, hostname_len);
    }
    
    // Parameter request list (incluindo NTP_SERVER - opção 42)
    uint8_t params[] = {DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, DHCP_OPT_DNS_SERVER, DHCP_OPT_NTP_SERVER};
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_PARAM_REQUEST, params, sizeof(params));
    
    // End
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_END, NULL, 0);
    
    return sizeof(DHCP_Packet_t) - sizeof(pkt->options) + opt_offset;
}

static uint16_t BuildDHCPRequest(
    DHCP_Packet_t *pkt,
    const uint8_t *mac,
    uint32_t xid,
    const uint8_t *requested_ip,
    const uint8_t *server_ip,
    const char *hostname
) {
    memset(pkt, 0, sizeof(DHCP_Packet_t));
    
    pkt->op = DHCP_OP_BOOTREQUEST;
    pkt->htype = DHCP_HTYPE_ETHERNET;
    pkt->hlen = DHCP_HLEN_ETHERNET;
    pkt->hops = 0;
    pkt->xid = __builtin_bswap32(xid);
    pkt->secs = 0;
    pkt->flags = __builtin_bswap16(0x8000);  // Broadcast flag
    memcpy(pkt->chaddr, mac, 6);
    pkt->magic = __builtin_bswap32(DHCP_MAGIC_COOKIE);
    
    uint16_t opt_offset = 0;
    
    // Message type
    uint8_t msg_type = DHCP_MSG_REQUEST;
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_MSG_TYPE, &msg_type, 1);
    
    // Client identifier
    uint8_t client_id[7];
    client_id[0] = DHCP_HTYPE_ETHERNET;
    memcpy(&client_id[1], mac, 6);
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_CLIENT_ID, client_id, 7);
    
    // Requested IP
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_REQUESTED_IP, requested_ip, 4);
    
    // Server identifier
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_SERVER_ID, server_ip, 4);
    
    // Hostname
    if (hostname) {
        uint8_t hostname_len = strlen(hostname);
        opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_HOSTNAME, hostname, hostname_len);
    }
    
    // Parameter request list (incluindo NTP_SERVER - opção 42)
    uint8_t params[] = {DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, DHCP_OPT_DNS_SERVER, DHCP_OPT_NTP_SERVER};
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_PARAM_REQUEST, params, sizeof(params));
    
    // End
    opt_offset = AddOption(pkt->options, opt_offset, DHCP_OPT_END, NULL, 0);
    
    return sizeof(DHCP_Packet_t) - sizeof(pkt->options) + opt_offset;
}

/* =============================================================================
 * CALLBACK DE RECEPÇÃO
 * ============================================================================= */

static void DHCP_RxCallback(NetSocket_t *socket, void *user_data) {
    DHCP_Client_t *client = (DHCP_Client_t *)user_data;
    
    if (!client || !socket) return;
    
    uint16_t available = NetSocket_GetAvailable(socket);
    
    DHCP_Packet_t pkt;
    NetAddr_t src_addr;
    
    int32_t len = NetSocket_RecvFrom(socket, (uint8_t *)&pkt, sizeof(pkt), &src_addr, 0);
    
    if (len < (int32_t)(sizeof(DHCP_Packet_t) - sizeof(pkt.options))) {
        return;  // Pacote muito pequeno
    }
    
    // Verificar XID
    if (__builtin_bswap32(pkt.xid) != client->xid) {
        return;  // Não é resposta para nossa requisição
    }
    
    // Verificar magic cookie
    if (__builtin_bswap32(pkt.magic) != DHCP_MAGIC_COOKIE) {
        return;
    }
    
    // Parse options
    uint8_t msg_type = 0;
    uint8_t server_ip[4] = {0};
    uint8_t subnet[4] = {0};
    uint8_t gateway[4] = {0};
    uint8_t dns[4] = {0};
    uint8_t ntp_server[4] = {0};
    uint32_t lease_time = 0;
    
    uint16_t options_len = len - (sizeof(DHCP_Packet_t) - sizeof(pkt.options));
    
    if (!ParseOptions(pkt.options, options_len, &msg_type, pkt.yiaddr, server_ip, subnet, gateway, dns, ntp_server, &lease_time)) {
        return;
    }
    
    // Processar baseado no estado
    switch (client->state) {
        case DHCP_STATE_SELECTING:
            if (msg_type == DHCP_MSG_OFFER) {
                // Salvar configuração oferecida
                memcpy(client->offered_ip.addr, pkt.yiaddr, 4);
                memcpy(client->subnet_mask.addr, subnet, 4);
                memcpy(client->gateway.addr, gateway, 4);
                memcpy(client->dns[0].addr, dns, 4);
                memcpy(client->ntp_server.addr, ntp_server, 4);
                memcpy(client->server_ip.addr, server_ip, 4);
                client->lease_time = lease_time;
                
                client->state = DHCP_STATE_REQUESTING;
            }
            break;
        
        case DHCP_STATE_REQUESTING:
        case DHCP_STATE_RENEWING:
            if (msg_type == DHCP_MSG_ACK) {
                // Atualizar configuração
                uint32_t now = osKernelGetTickCount();
                memcpy(client->offered_ip.addr, pkt.yiaddr, 4);
                memcpy(client->subnet_mask.addr, subnet, 4);
                memcpy(client->gateway.addr, gateway, 4);
                memcpy(client->dns[0].addr, dns, 4);
                memcpy(client->ntp_server.addr, ntp_server, 4);
                client->lease_time = (lease_time == 0) ? 3600 : lease_time;
                client->lease_expire_time = now + (client->lease_time * 1000);
                client->t1_time = now + (client->lease_time * 500);
                client->t2_time = now + (client->lease_time * 875);
                
                client->state = DHCP_STATE_BOUND;
                
                if (client->manager) {
                    // Usar DNS padrão se não recebi do servidor
                    NetIP_t dns1 = client->dns[0];
                    NetIP_t dns2 = client->dns[1];
                    
                    if ((dns1.addr[0] | dns1.addr[1] | dns1.addr[2] | dns1.addr[3]) == 0) {
                        dns1 = NET_IP(8, 8, 8, 8);  // Google DNS
                    }
                    if ((dns2.addr[0] | dns2.addr[1] | dns2.addr[2] | dns2.addr[3]) == 0) {
                        dns2 = NET_IP(8, 8, 4, 4);  // Google DNS secundário
                    }
                    
                    NetConfig_t config = {
                        .ip = client->offered_ip,
                        .subnet = client->subnet_mask,
                        .gateway = client->gateway,
                        .dns = { dns1, dns2 },
                        .mac = client->mac,
                        .dhcp_enabled = true,
                    };
                    NetManager_ApplyConfig(client->manager, &config);
                }

                // Postar evento
                if (client->manager) {
                    NetEvent_t event = {
                        .type = NET_EVENT_DHCP_BOUND,
                        .data = NULL,
                        .data_len = 0,
                    };
                    NetManager_PostEvent(client->manager, &event);
                }
            } else if (msg_type == DHCP_MSG_NAK) {
                // Recomeçar
                client->state = DHCP_STATE_IDLE;
            }
            break;
        
        default:
            break;
    }
}

/* =============================================================================
 * API PUBLICA
 * ============================================================================= */

NetErr_t DHCP_Init(DHCP_Client_t *client, const char *hostname) {
    if (!client) {
        return NET_ERR_PARAM;
    }

    memset(client, 0, sizeof(DHCP_Client_t));
    client->state = DHCP_STATE_IDLE;

    if (hostname) {
        strncpy(client->hostname, hostname, sizeof(client->hostname) - 1);
    }

    return NET_OK;
}

void DHCP_Deinit(DHCP_Client_t *client) {
    if (!client) {
        return;
    }
    DHCP_Stop(client);
}

NetErr_t DHCP_Start(DHCP_Client_t *client, NetManager_t *manager) {
    if (!client || !manager) {
        return NET_ERR_PARAM;
    }

    client->manager = manager;
    client->state = DHCP_STATE_SELECTING;
    client->retry_count = 0;
    client->state_timer = 0;
    
    // Ler MAC do hardware W5500
    W5500_HW_GetMAC(manager->hw_driver, client->mac.addr);
    
    // Se não conseguiu MAC do hardware, usar fallback
    if (client->mac.addr[0] == 0 && client->mac.addr[1] == 0) {
        client->mac.addr[0] = 0x02;
        client->mac.addr[1] = 0xAA;
        client->mac.addr[2] = 0xBB;
        client->mac.addr[3] = 0xCC;
        client->mac.addr[4] = 0xDD;
        client->mac.addr[5] = 0xEE;
    }

    return NET_OK;
}

void DHCP_Stop(DHCP_Client_t *client) {
    if (!client) {
        return;
    }

    if (client->socket) {
        NetSocket_Close(client->socket);
        NetManager_FreeSocket(client->manager, client->socket);
        client->socket = NULL;
    }

    client->state = DHCP_STATE_IDLE;
}

void DHCP_Task(DHCP_Client_t *client) {
    if (!client || !client->manager) {
        return;
    }

    uint32_t now = osKernelGetTickCount();

    switch (client->state) {
        case DHCP_STATE_IDLE:
            if (NetManager_IsLinkUp(client->manager)) {
                client->state = DHCP_STATE_SELECTING;
                client->retry_count = 0;
                client->state_timer = 0;
            }
            break;

        case DHCP_STATE_SELECTING: {
            if (!client->socket) {
                NetAddr_t local_addr = { .ip = NET_IP(0, 0, 0, 0), .port = DHCP_CLIENT_PORT };
                client->socket = NetManager_AllocSocket(client->manager, NET_SOCKET_TYPE_UDP);
                if (!client->socket) {
                    return;
                }
                
                NetSocket_Bind(client->socket, &local_addr);
                NetSocket_SetRxCallback(client->socket, DHCP_RxCallback, client);
            }

            if ((now - client->state_timer) > pdMS_TO_TICKS(DHCP_DISCOVER_TIMEOUT_MS)) {
                client->xid = GenerateXID(client->mac.addr);

                DHCP_Packet_t pkt;
                uint16_t len = BuildDHCPDiscover(&pkt, client->mac.addr, client->xid, client->hostname);

                NetAddr_t dest = { .ip = NET_IP(255, 255, 255, 255), .port = DHCP_SERVER_PORT };
                int32_t sent = NetSocket_SendTo(client->socket, (uint8_t *)&pkt, len, &dest, 0);

                client->state_timer = now;
                client->retry_count++;

                if (client->retry_count > DHCP_RETRY_MAX) {
                    client->state = DHCP_STATE_ERROR;
                }
            }
            break;
        }

        case DHCP_STATE_REQUESTING: {
            if ((now - client->state_timer) > pdMS_TO_TICKS(DHCP_REQUEST_TIMEOUT_MS)) {
                DHCP_Packet_t pkt;
                uint16_t len = BuildDHCPRequest(
                    &pkt,
                    client->mac.addr,
                    client->xid,
                    client->offered_ip.addr,
                    client->server_ip.addr,
                    client->hostname
                );

                NetAddr_t dest = { .ip = NET_IP(255, 255, 255, 255), .port = DHCP_SERVER_PORT };
                NetSocket_SendTo(client->socket, (uint8_t *)&pkt, len, &dest, 0);

                client->state_timer = now;
                client->retry_count++;

                if (client->retry_count > DHCP_RETRY_MAX) {
                    client->state = DHCP_STATE_IDLE;
                }
            }
            break;
        }

        case DHCP_STATE_BOUND:
            if (now >= client->t1_time) {
                client->state = DHCP_STATE_RENEWING;
                client->retry_count = 0;
                client->state_timer = 0;
            }
            if (now >= client->lease_expire_time) {
                client->state = DHCP_STATE_IDLE;
                if (client->manager) {
                    NetEvent_t event = { .type = NET_EVENT_DHCP_LOST, .data = NULL, .data_len = 0 };
                    NetManager_PostEvent(client->manager, &event);
                }
            }
            break;

        case DHCP_STATE_RENEWING: {
            if ((now - client->state_timer) > pdMS_TO_TICKS(DHCP_REQUEST_TIMEOUT_MS)) {
                DHCP_Packet_t pkt;
                uint16_t len = BuildDHCPRequest(
                    &pkt,
                    client->mac.addr,
                    client->xid,
                    client->offered_ip.addr,
                    client->server_ip.addr,
                    client->hostname
                );

                NetAddr_t dest = { .ip = client->server_ip, .port = DHCP_SERVER_PORT };
                NetSocket_SendTo(client->socket, (uint8_t *)&pkt, len, &dest, 0);

                client->state_timer = now;
                client->retry_count++;

                if (client->retry_count > DHCP_RENEW_RETRIES) {
                    client->state = DHCP_STATE_IDLE;
                    if (client->manager) {
                        NetEvent_t event = { .type = NET_EVENT_DHCP_LOST, .data = NULL, .data_len = 0 };
                        NetManager_PostEvent(client->manager, &event);
                    }
                }
            }
            break;
        }

        default:
            break;
    }
}

void DHCP_EventHandler(DHCP_Client_t *client, const NetEvent_t *event) {
    if (!client || !event) {
        return;
    }

    switch (event->type) {
        case NET_EVENT_LINK_DOWN:
            DHCP_Stop(client);
            break;

        case NET_EVENT_LINK_UP:
            if (client->state == DHCP_STATE_IDLE) {
                client->state = DHCP_STATE_SELECTING;
                client->retry_count = 0;
                client->state_timer = 0;
            }
            break;

        default:
            break;
    }
}

DHCP_State_t DHCP_GetState(const DHCP_Client_t *client) {
    return client ? client->state : DHCP_STATE_ERROR;
}

bool DHCP_IsBound(const DHCP_Client_t *client) {
    return client && (client->state == DHCP_STATE_BOUND);
}

NetErr_t DHCP_GetConfig(const DHCP_Client_t *client, NetConfig_t *config) {
    if (!client || !config) {
        return NET_ERR_PARAM;
    }

    if (client->state != DHCP_STATE_BOUND) {
        return NET_ERR_NOT_CONNECTED;
    }

    config->ip = client->offered_ip;
    config->subnet = client->subnet_mask;
    config->gateway = client->gateway;
    config->dns[0] = client->dns[0];
    config->dns[1] = client->dns[1];
    config->mac = client->mac;
    config->dhcp_enabled = true;

    return NET_OK;
}

NetErr_t DHCP_Renew(DHCP_Client_t *client) {
    if (!client) {
        return NET_ERR_PARAM;
    }

    if (client->state != DHCP_STATE_BOUND) {
        return NET_ERR_NOT_CONNECTED;
    }

    client->state = DHCP_STATE_RENEWING;
    client->retry_count = 0;
    client->state_timer = 0;

    return NET_OK;
}

NetErr_t DHCP_Release(DHCP_Client_t *client) {
    if (!client) {
        return NET_ERR_PARAM;
    }

    if (client->state != DHCP_STATE_BOUND) {
        return NET_ERR_NOT_CONNECTED;
    }

    DHCP_Stop(client);
    return NET_OK;
}

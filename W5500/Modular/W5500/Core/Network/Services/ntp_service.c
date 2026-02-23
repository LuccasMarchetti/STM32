/*
 * ntp_service.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Implementação do cliente NTP
 */

#include "ntp_service.h"
#include "net_socket.h"
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* Estrutura do pacote NTP */
typedef struct __attribute__((packed)) {
    uint8_t  li_vn_mode;        // Leap indicator, Version, Mode
    uint8_t  stratum;           // Stratum level
    uint8_t  poll;              // Maximum poll interval
    uint8_t  precision;         // Precision
    uint32_t root_delay;        // Root delay
    uint32_t root_dispersion;   // Root dispersion
    uint32_t reference_id;      // Reference identifier
    uint32_t ref_timestamp_sec; // Reference timestamp (seconds)
    uint32_t ref_timestamp_frac;// Reference timestamp (fraction)
    uint32_t orig_timestamp_sec;// Origin timestamp (seconds)
    uint32_t orig_timestamp_frac;//Origin timestamp (fraction)
    uint32_t recv_timestamp_sec;// Receive timestamp (seconds)
    uint32_t recv_timestamp_frac;//Receive timestamp (fraction)
    uint32_t trans_timestamp_sec;//Transmit timestamp (seconds)
    uint32_t trans_timestamp_frac;//Transmit timestamp (fraction)
} NTP_Packet_t;

/* Constantes NTP */
#define NTP_LI_VN_MODE          0x1B    // LI=0, VN=3, Mode=3 (client)
#define NTP_UNIX_OFFSET         2208988800UL  // Segundos entre 1900 e 1970

/* =============================================================================
 * FUNÇÕES AUXILIARES
 * ============================================================================= */

void NTP_ConvertToUnix(uint32_t ntp_seconds, NTP_Timestamp_t *unix_time) {
    if (ntp_seconds >= NTP_UNIX_OFFSET) {
        unix_time->seconds = ntp_seconds - NTP_UNIX_OFFSET;
    } else {
        unix_time->seconds = 0;
    }
}

/**
 * @brief Calcula número de dias desde 1970-01-01
 */
static uint32_t UnixDaysSince1970(uint32_t unix_seconds) {
    return unix_seconds / 86400;  // 86400 = segundos por dia
}

/**
 * @brief Verifica se um ano é bissexto
 */
static bool IsLeapYear(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

/**
 * @brief Retorna número de dias em um mês
 */
static uint8_t DaysInMonth(uint8_t month, uint16_t year) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && IsLeapYear(year)) return 29;
    return days[month - 1];
}

/**
 * @brief Converte Unix timestamp para data/hora estruturada
 */
void NTP_UnixToDateTime(uint32_t seconds, NTP_DateTime_t *datetime, int32_t timezone_offset_sec) {
    if (!datetime) return;
    
    // Aplicar timezone offset
    int32_t adjusted_seconds = (int32_t)seconds + timezone_offset_sec;
    if (adjusted_seconds < 0) adjusted_seconds = 0;
    
    uint32_t unix_secs = (uint32_t)adjusted_seconds;
    
    // Extrair hora, minuto, segundo do dia
    uint32_t secs_today = unix_secs % 86400;
    datetime->second = secs_today % 60;
    datetime->minute = (secs_today / 60) % 60;
    datetime->hour = secs_today / 3600;
    
    // Calcular dia da semana (0=domingo)
    uint32_t days_since_1970 = unix_secs / 86400;
    datetime->dow = (days_since_1970 + 4) % 7;  // 1970-01-01 era quinta (4)
    
    // Calcular ano, mês, dia
    uint16_t year = 1970;
    uint32_t days_remaining = days_since_1970;
    
    // Avançar anos
    while (1) {
        uint32_t days_in_year = IsLeapYear(year) ? 366 : 365;
        if (days_remaining < days_in_year) break;
        days_remaining -= days_in_year;
        year++;
    }
    
    // Avançar meses
    uint8_t month = 1;
    while (1) {
        uint8_t days_in_current_month = DaysInMonth(month, year);
        if (days_remaining < days_in_current_month) break;
        days_remaining -= days_in_current_month;
        month++;
    }
    
    datetime->year = year;
    datetime->month = month;
    datetime->day = days_remaining + 1;  // +1 porque dias são 1-31, não 0-30
}

/**
 * @brief Obter data/hora atual
 */
NetErr_t NTP_GetDateTime(const NTP_Client_t *client, NTP_DateTime_t *datetime, int32_t timezone_offset_sec) {
    if (!client || !datetime) {
        return NET_ERR_PARAM;
    }
    
    if (client->state != NTP_STATE_SYNCHRONIZED) {
        return NET_ERR_STATE;
    }
    
    NTP_UnixToDateTime(client->current_time.seconds, datetime, timezone_offset_sec);
    return NET_OK;
}

/**
 * @brief Formata data/hora para string legível
 */
char* NTP_DateTimeToString(const NTP_DateTime_t *datetime, char *buffer, size_t size) {
    if (!datetime || !buffer || size < 20) {
        if (buffer && size > 0) buffer[0] = '\0';
        return buffer;
    }
    
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d",
             datetime->year, datetime->month, datetime->day,
             datetime->hour, datetime->minute, datetime->second);
    
    return buffer;
}

static void BuildNTPRequest(NTP_Packet_t *pkt) {
    memset(pkt, 0, sizeof(NTP_Packet_t));
    pkt->li_vn_mode = NTP_LI_VN_MODE;
}

/* =============================================================================
 * CALLBACK DE RECEPÇÃO
 * ============================================================================= */

static void NTP_RxCallback(NetSocket_t *socket, void *user_data) {
    NTP_Client_t *client = (NTP_Client_t *)user_data;
    
    if (!client || client->state != NTP_STATE_SYNCING) return;
    
    NTP_Packet_t pkt;
    NetAddr_t src_addr;
    
    int32_t len = NetSocket_RecvFrom(socket, (uint8_t *)&pkt, sizeof(pkt), &src_addr, 0);
    
    if (len < (int32_t)sizeof(NTP_Packet_t)) return;
    
    // Extrair timestamp de transmissão do servidor
    uint32_t ntp_seconds = __builtin_bswap32(pkt.trans_timestamp_sec);
    
    if (ntp_seconds == 0) {
        client->state = NTP_STATE_ERROR;
        return;
    }
    
    // Converter para Unix timestamp
    NTP_ConvertToUnix(ntp_seconds, &client->current_time);
    
    client->last_sync = osKernelGetTickCount();
    client->state = NTP_STATE_SYNCHRONIZED;
    
    // Chamar callback
    if (client->callback) {
        client->callback(&client->current_time, client->user_data);
    }
}

/* =============================================================================
 * IMPLEMENTAÇÃO DO SERVIÇO
 * ============================================================================= */

static NetErr_t NTP_ServiceInit(void *context, void *manager) {
    NTP_Client_t *client = (NTP_Client_t *)context;
    NetManager_t *mgr = (NetManager_t *)manager;
    
    if (!client) return NET_ERR_PARAM;
    
    client->manager = mgr;
    
    // Criar socket UDP
    client->socket = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_UDP);
    if (!client->socket) return NET_ERR_RESOURCE;
    
    NetAddr_t local_addr = { .ip = { .addr = {0, 0, 0, 0} }, .port = 0 };
    NetSocket_Bind(client->socket, &local_addr);
    NetSocket_SetRxCallback(client->socket, NTP_RxCallback, client);
    
    return NET_OK;
}

static void NTP_ServiceTask(void *context) {
    NTP_Client_t *client = (NTP_Client_t *)context;
    
    if (!client || !client->manager) return;
    
    uint32_t now = osKernelGetTickCount();
    
    // Atualizar tempo se sincronizado
    // Nota: incrementamos apenas 1 segundo por vez para evitar erros de acúmulo
    if (client->state == NTP_STATE_SYNCHRONIZED) {
        uint32_t elapsed_ms = now - client->last_sync;
        uint32_t elapsed_seconds = elapsed_ms / 1000;  // Segundos desde sync
        
        // Atualizar apenas se passou novo segundo
        if (elapsed_seconds > client->last_second_update) {
            client->current_time.seconds += 1;  // Incrementar apenas 1 segundo por vez
            client->last_second_update = elapsed_seconds;
        }
        
        // Verificar se é hora de ressincronizar
        if (elapsed_ms > client->sync_interval_ms) {
            client->state = NTP_STATE_IDLE;
            client->last_second_update = 0;
        }
    } else {
        client->last_second_update = 0;  // Reset quando sair de SYNCHRONIZED
    }
    
    // Iniciar sincronização automática se necessário
    if (client->state == NTP_STATE_IDLE && NetManager_IsLinkUp(client->manager)) {
        if ((now - client->last_sync) > client->sync_interval_ms) {
            // Se usar DNS, verificar se hostname foi resolvido
            if (client->use_dns && client->server_hostname[0] != '\0') {
                // Verificar se IP ainda é zero (não resolvido)
                if (client->server_ip[0] == 0 && client->server_ip[1] == 0 &&
                    client->server_ip[2] == 0 && client->server_ip[3] == 0) {
                    // Tentar usar DNS para resolver (será implementado em app_network.c)
                    // Por enquanto, só sincronizar se IP for válido
                    return;
                }
            }
            
            client->state = NTP_STATE_SYNCING;
            client->retry_count = 0;
            client->last_send = 0;
        }
    }
    
    // Processar sincronização
    if (client->state == NTP_STATE_SYNCING) {
        if ((now - client->last_send) > pdMS_TO_TICKS(NTP_TIMEOUT_MS)) {
            if (client->retry_count < NTP_RETRIES) {
                // Construir e enviar request
                NTP_Packet_t pkt;
                BuildNTPRequest(&pkt);
                
                NetAddr_t dest = {
                    .ip = { .addr = { client->server_ip[0], client->server_ip[1], client->server_ip[2], client->server_ip[3] } },
                    .port = NTP_SERVER_PORT
                };
                
                NetSocket_SendTo(client->socket, (uint8_t *)&pkt, sizeof(pkt), &dest, 0);
                client->last_send = now;
                client->retry_count++;
            } else {
                // Timeout
                client->state = NTP_STATE_ERROR;
                if (client->callback) {
                    client->callback(NULL, client->user_data);
                }
            }
        }
    }
}

static void NTP_ServiceDeinit(void *context) {
    NTP_Client_t *client = (NTP_Client_t *)context;
    
    if (client && client->socket) {
        NetSocket_Close(client->socket);
        NetManager_FreeSocket(client->manager, client->socket);
        client->socket = NULL;
    }
}

static void NTP_ServiceEventHandler(void *context, const NetEvent_t *event) {
    NTP_Client_t *client = (NTP_Client_t *)context;
    
    if (!client || !event) return;
    
    switch (event->type) {
        case NET_EVENT_DHCP_BOUND:
            // Sincronizar após obter IP
            if (client->state == NTP_STATE_IDLE) {
                client->state = NTP_STATE_SYNCING;
                client->retry_count = 0;
                client->last_send = 0;
            }
            break;
        
        case NET_EVENT_LINK_DOWN:
            client->state = NTP_STATE_IDLE;
            break;
        
        default:
            break;
    }
}

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

NetErr_t NTP_Init(
    NTP_Client_t *client,
    const uint8_t *server_ip,
    uint32_t sync_interval_ms
) {
    if (!client) return NET_ERR_PARAM;
    
    memset(client, 0, sizeof(NTP_Client_t));
    
    client->use_dns = false;
    memset(client->server_hostname, 0, sizeof(client->server_hostname));
    
    if (server_ip) {
        memcpy(client->server_ip, server_ip, 4);
    } else {
        // Servidor NTP padrão (pool.ntp.org: 193.182.111.12)
        client->server_ip[0] = 193;
        client->server_ip[1] = 182;
        client->server_ip[2] = 111;
        client->server_ip[3] = 12;
    }
    
    client->sync_interval_ms = (sync_interval_ms > 0) ? sync_interval_ms : NTP_SYNC_INTERVAL_MS;
    client->state = NTP_STATE_IDLE;
    
    // Configurar interface de serviço
    client->service.name = "NTP";
    client->service.context = client;
    client->service.init = (void (*)(struct NetService *, void *))NTP_ServiceInit;
    client->service.task = (void (*)(struct NetService *))NTP_ServiceTask;
    client->service.deinit = (void (*)(struct NetService *))NTP_ServiceDeinit;
    client->service.event_handler = (void (*)(struct NetService *, const NetEvent_t *))NTP_ServiceEventHandler;
    client->service.enabled = true;
    
    return NET_OK;
}

NetErr_t NTP_Sync(
    NTP_Client_t *client,
    NTP_SyncCallback_t callback,
    void *user_data
) {
    if (!client) return NET_ERR_PARAM;
    
    if (client->state == NTP_STATE_SYNCING) {
        return NET_ERR_STATE;  // Sync em andamento
    }
    
    client->callback = callback;
    client->user_data = user_data;
    client->state = NTP_STATE_SYNCING;
    client->retry_count = 0;
    client->last_send = 0;  // Força envio imediato
    
    return NET_OK;
}

NetErr_t NTP_GetTime(const NTP_Client_t *client, NTP_Timestamp_t *timestamp) {
    
    if (!client || !timestamp) {
        return NET_ERR_PARAM;
    }
    
    if (client->state != NTP_STATE_SYNCHRONIZED) {
        return NET_ERR_STATE;
    }
    
    memcpy(timestamp, &client->current_time, sizeof(NTP_Timestamp_t));
    return NET_OK;
}

bool NTP_IsSynchronized(const NTP_Client_t *client) {
    return client && (client->state == NTP_STATE_SYNCHRONIZED);
}

NetErr_t NTP_SetServer(NTP_Client_t *client, const uint8_t *server_ip) {
    
    if (!client || !server_ip) {
        return NET_ERR_PARAM;
    }
    
    memcpy(client->server_ip, server_ip, 4);
    client->use_dns = false;
    memset(client->server_hostname, 0, sizeof(client->server_hostname));
    
    return NET_OK;
}

/**
 * @brief Define servidor NTP por hostname (usa DNS para resolver)
 */
NetErr_t NTP_SetServerByHostname(NTP_Client_t *client, const char *hostname) {
    
    if (!client || !hostname) {
        return NET_ERR_PARAM;
    }
    
    // Copiar hostname
    strncpy(client->server_hostname, hostname, sizeof(client->server_hostname) - 1);
    client->server_hostname[sizeof(client->server_hostname) - 1] = '\0';
    
    client->use_dns = true;
    memset(client->server_ip, 0, 4);  // Zerar IP até que DNS resolva
    
    // Forçar sincronização na próxima oportunidade
    client->state = NTP_STATE_IDLE;
    client->last_sync = 0;
    
    return NET_OK;
}

NetService_t* NTP_GetService(NTP_Client_t *client) {
    return client ? &client->service : NULL;
}

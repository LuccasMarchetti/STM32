/*
 * ntp_service.h
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Cliente NTP (Network Time Protocol)
 */

#ifndef NTP_SERVICE_H_
#define NTP_SERVICE_H_

#include "net_types.h"
#include "net_manager.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * DEFINIÇÕES
 * ============================================================================= */

#define NTP_SERVER_PORT         123
#define NTP_SYNC_INTERVAL_MS    60000   // 1 minuto
#define NTP_TIMEOUT_MS          5000
#define NTP_RETRIES             3

/* Estados do cliente NTP */
typedef enum {
    NTP_STATE_IDLE = 0,
    NTP_STATE_SYNCING,
    NTP_STATE_SYNCHRONIZED,
    NTP_STATE_ERROR
} NTP_State_t;

/* Timestamp Unix (segundos desde 1970) */
typedef struct {
    uint32_t seconds;
} NTP_Timestamp_t;

/* Data e Hora Estruturada */
typedef struct {
    uint16_t year;      // 2000-2099
    uint8_t month;      // 1-12
    uint8_t day;        // 1-31
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t second;     // 0-59
    uint8_t dow;        // Dia da semana (0=domingo, 6=sábado)
} NTP_DateTime_t;

/* Callback de sincronização */
typedef void (*NTP_SyncCallback_t)(const NTP_Timestamp_t *timestamp, void *user_data);

/* Cliente NTP */
typedef struct {
    NetService_t service;           // Interface de serviço
    NetManager_t *manager;          // Gerenciador de rede
    NetSocket_t *socket;            // Socket UDP
    
    uint8_t server_ip[4];           // IP do servidor NTP
    NTP_State_t state;              // Estado atual
    
    #define NTP_HOSTNAME_MAX 64
    char server_hostname[NTP_HOSTNAME_MAX];  // Hostname do servidor (se usar DNS)
    bool use_dns;                   // Se true, usa DNS para resolver hostname
    
    uint32_t last_sync;             // Timestamp da última sincronização (osKernelGetTickCount)
    uint32_t last_send;             // Timestamp do último envio de requisição
    uint32_t last_second_update;    // Rastreia último incremento de segundo (para evitar duplicação)
    uint8_t retry_count;
    
    NTP_Timestamp_t current_time;   // Tempo atual (Unix timestamp em segundos)
    uint32_t sync_interval_ms;      // Intervalo entre sincronizações
    
    NTP_SyncCallback_t callback;
    void *user_data;
    
} NTP_Client_t;

/* =============================================================================
 * API PÚBLICA
 * ============================================================================= */

/**
 * @brief Inicializa cliente NTP
 * @param client Ponteiro para estrutura do cliente
 * @param server_ip IP do servidor NTP [4 bytes]
 * @param sync_interval_ms Intervalo entre sincronizações (ms), 0 = usar padrão
 * @return NET_OK se sucesso
 */
NetErr_t NTP_Init(
    NTP_Client_t *client,
    const uint8_t *server_ip,
    uint32_t sync_interval_ms
);

/**
 * @brief Força sincronização imediata
 * @param client Ponteiro para cliente
 * @param callback Função chamada após sincronização
 * @param user_data Dados do usuário
 * @return NET_OK se sync iniciada
 */
NetErr_t NTP_Sync(
    NTP_Client_t *client,
    NTP_SyncCallback_t callback,
    void *user_data
);

/**
 * @brief Obtém tempo atual
 * @param client Ponteiro para cliente
 * @param timestamp Buffer para receber timestamp
 * @return NET_OK se sincronizado
 */
NetErr_t NTP_GetTime(const NTP_Client_t *client, NTP_Timestamp_t *timestamp);

/**
 * @brief Verifica se está sincronizado
 */
bool NTP_IsSynchronized(const NTP_Client_t *client);

/**
 * @brief Define servidor NTP por IP
 */
NetErr_t NTP_SetServer(NTP_Client_t *client, const uint8_t *server_ip);

/**
 * @brief Define servidor NTP por hostname (usa DNS para resolver)
 * @param client Ponteiro para cliente
 * @param hostname Hostname do servidor (ex: "pool.ntp.org")
 * @return NET_OK se sucesso
 */
NetErr_t NTP_SetServerByHostname(NTP_Client_t *client, const char *hostname);

/**
 * @brief Obtém interface de serviço
 */
NetService_t* NTP_GetService(NTP_Client_t *client);

/* Utilitário: converte timestamp NTP para Unix */
void NTP_ConvertToUnix(uint32_t ntp_seconds, NTP_Timestamp_t *unix_time);

/**
 * @brief Converte timestamp Unix para formato data/hora estruturado
 * @param seconds Segundos desde 1970 (Unix timestamp)
 * @param datetime Estrutura para receber data/hora
 * @param timezone_offset_sec Offset de timezone em segundos (ex: -3*3600 para GMT-3)
 */
void NTP_UnixToDateTime(uint32_t seconds, NTP_DateTime_t *datetime, int32_t timezone_offset_sec);

/**
 * @brief Obter data/hora atual sincronizada
 * @param client Ponteiro para cliente NTP
 * @param datetime Estrutura para receber data/hora
 * @param timezone_offset_sec Offset de timezone em segundos
 * @return NET_OK se sincronizado
 */
NetErr_t NTP_GetDateTime(const NTP_Client_t *client, NTP_DateTime_t *datetime, int32_t timezone_offset_sec);

/**
 * @brief Formata data/hora para string legível (DD/MM/YYYY HH:MM:SS)
 * @param datetime Estrutura com data/hora
 * @param buffer Buffer para string (mínimo 20 bytes)
 * @param size Tamanho do buffer
 * @return Ponteiro para buffer
 */
char* NTP_DateTimeToString(const NTP_DateTime_t *datetime, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* NTP_SERVICE_H_ */

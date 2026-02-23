/*
 * net_types.h
 *
 * Tipos comuns da stack de rede - independentes de implementação
 * Usados em todas as camadas acima do driver de hardware
 */

#ifndef NETWORK_NET_TYPES_H_
#define NETWORK_NET_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct NetSocket NetSocket_t;

/* =============================================================================
 * TIPOS BÁSICOS DE REDE
 * ============================================================================= */

/**
 * @brief Endereço IP (IPv4)
 */
typedef struct {
    uint8_t addr[4];
} NetIP_t;

/**
 * @brief Endereço MAC
 */
typedef struct {
    uint8_t addr[6];
} NetMAC_t;

/**
 * @brief Endereço completo (IP + porta)
 */
typedef struct {
    NetIP_t ip;
    uint16_t port;
} NetAddr_t;

/**
 * @brief Configuração de rede
 */
typedef struct {
    NetIP_t ip;
    NetIP_t subnet;
    NetIP_t gateway;
    NetIP_t dns[2];          // DNS primário e secundário
    NetMAC_t mac;
    bool dhcp_enabled;       // Se true, ignora IPs e usa DHCP
} NetConfig_t;

/* =============================================================================
 * ENUMS DE SOCKET
 * ============================================================================= */

/**
 * @brief Tipos de socket suportados
 */
typedef enum {
    NET_SOCKET_TYPE_NONE = 0,
    NET_SOCKET_TYPE_TCP = 1,
    NET_SOCKET_TYPE_UDP = 2,
    NET_SOCKET_TYPE_RAW = 3  // Para ICMP, etc (se suportado)
} NetSocketType_t;

/**
 * @brief Estados de um socket abstrato
 */
typedef enum {
    NET_SOCKET_STATE_CLOSED = 0,
    NET_SOCKET_STATE_OPENING,
    NET_SOCKET_STATE_IDLE,         // Aberto mas inativo
    NET_SOCKET_STATE_LISTEN,       // TCP server aguardando conexão
    NET_SOCKET_STATE_CONNECTING,   // TCP client conectando
    NET_SOCKET_STATE_ESTABLISHED,  // TCP conectado
    NET_SOCKET_STATE_CLOSING,
    NET_SOCKET_STATE_ERROR
} NetSocketState_t;

#define NET_SOCKET_STATE_CREATED   NET_SOCKET_STATE_OPENING
#define NET_SOCKET_STATE_BOUND     NET_SOCKET_STATE_IDLE
#define NET_SOCKET_STATE_LISTENING NET_SOCKET_STATE_LISTEN
#define NET_SOCKET_STATE_CONNECTED NET_SOCKET_STATE_ESTABLISHED

/**
 * @brief Papel do socket TCP (cliente ou servidor)
 */
typedef enum {
    NET_TCP_ROLE_NONE = 0,
    NET_TCP_ROLE_CLIENT,
    NET_TCP_ROLE_SERVER
} NetTcpRole_t;

/* =============================================================================
 * CÓDIGOS DE ERRO
 * ============================================================================= */

/**
 * @brief Códigos de erro da stack de rede
 */
typedef enum {
    NET_OK = 0,
    NET_ERR_INVALID_ARG,
    NET_ERR_NO_MEMORY,
    NET_ERR_NO_SOCKET,         // Pool de sockets esgotado
    NET_ERR_TIMEOUT,
    NET_ERR_NOT_CONNECTED,
    NET_ERR_ALREADY_CONNECTED,
    NET_ERR_IN_PROGRESS,
    NET_ERR_WOULD_BLOCK,
    NET_ERR_BUFFER_FULL,
    NET_ERR_HW_FAILURE,        // Falha de hardware
    NET_ERR_UNKNOWN
} NetErr_t;

#define NET_ERR_PARAM     NET_ERR_INVALID_ARG
#define NET_ERR_RESOURCE  NET_ERR_NO_MEMORY
#define NET_ERR_STATE     NET_ERR_IN_PROGRESS
#define NET_ERR_DRIVER    NET_ERR_HW_FAILURE
#define NET_ERR_NOT_FOUND NET_ERR_UNKNOWN

/* =============================================================================
 * EVENTOS DE REDE
 * ============================================================================= */

/**
 * @brief Tipos de eventos de rede (para callbacks)
 */
typedef enum {
    NET_EVENT_NONE = 0,
    NET_EVENT_LINK_UP,
    NET_EVENT_LINK_DOWN,
    NET_EVENT_DHCP_BOUND,       // DHCP obteve IP
    NET_EVENT_DHCP_RENEW,
    NET_EVENT_DHCP_TIMEOUT,
    NET_EVENT_DHCP_LOST,
    NET_EVENT_CONFIG_CHANGED,
    NET_EVENT_DNS_RESOLVED,
    NET_EVENT_SOCKET_CONNECTED,
    NET_EVENT_SOCKET_DISCONNECTED,
    NET_EVENT_SOCKET_DATA_AVAILABLE,
    NET_EVENT_SOCKET_SENT,
    NET_EVENT_SOCKET_ERROR
} NetEventType_t;

/**
 * @brief Estrutura de evento de rede
 */
typedef struct {
    NetEventType_t type;
    void *data;              // Ponteiro para dados específicos do evento
    uint16_t data_len;
} NetEvent_t;

/* =============================================================================
 * CALLBACKS
 * ============================================================================= */

/**
 * @brief Callback genérico de evento de rede
 * @param event Ponteiro para estrutura de evento
 * @param user_ctx Contexto do usuário (passado no registro do callback)
 */
typedef void (*NetEventCallback_t)(const NetEvent_t *event, void *user_ctx);

/**
 * @brief Callback de recepção de dados de socket
 * @param socket_id ID do socket que recebeu dados
 * @param data Ponteiro para os dados recebidos
 * @param len Tamanho dos dados
 * @param remote_addr Endereço de origem (pode ser NULL para TCP já conectado)
 * @param user_ctx Contexto do usuário
 */
typedef void (*NetSocketRxCallback_t)(
    NetSocket_t *socket,
    void *user_ctx
);

/* =============================================================================
 * MACROS AUXILIARES
 * ============================================================================= */

/**
 * @brief Define um IP literal
 */
#define NET_IP(a, b, c, d) ((NetIP_t){{(a), (b), (c), (d)}})

/**
 * @brief Define um MAC literal
 */
#define NET_MAC(a, b, c, d, e, f) ((NetMAC_t){{(a), (b), (c), (d), (e), (f)}})

/**
 * @brief Copia IP
 */
#define NET_IP_COPY(dst, src) do { \
    (dst)->addr[0] = (src)->addr[0]; \
    (dst)->addr[1] = (src)->addr[1]; \
    (dst)->addr[2] = (src)->addr[2]; \
    (dst)->addr[3] = (src)->addr[3]; \
} while(0)

/**
 * @brief Compara IPs
 */
#define NET_IP_EQUAL(ip1, ip2) ( \
    (ip1)->addr[0] == (ip2)->addr[0] && \
    (ip1)->addr[1] == (ip2)->addr[1] && \
    (ip1)->addr[2] == (ip2)->addr[2] && \
    (ip1)->addr[3] == (ip2)->addr[3] \
)

/**
 * @brief Verifica se IP é zero
 */
#define NET_IP_IS_ZERO(ip) ( \
    (ip)->addr[0] == 0 && \
    (ip)->addr[1] == 0 && \
    (ip)->addr[2] == 0 && \
    (ip)->addr[3] == 0 \
)

/**
 * @brief Converte IP para uint32_t (network byte order)
 */
static inline uint32_t NetIP_ToU32(const NetIP_t *ip) {
    return ((uint32_t)ip->addr[0] << 24) |
           ((uint32_t)ip->addr[1] << 16) |
           ((uint32_t)ip->addr[2] << 8)  |
           ((uint32_t)ip->addr[3]);
}

/**
 * @brief Converte uint32_t para IP (network byte order)
 */
static inline NetIP_t NetIP_FromU32(uint32_t addr) {
    NetIP_t ip;
    ip.addr[0] = (addr >> 24) & 0xFF;
    ip.addr[1] = (addr >> 16) & 0xFF;
    ip.addr[2] = (addr >> 8) & 0xFF;
    ip.addr[3] = addr & 0xFF;
    return ip;
}

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_NET_TYPES_H_ */

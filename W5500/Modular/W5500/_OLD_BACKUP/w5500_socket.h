/*
 * w5500_socket.h
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */

#ifndef NETWORK_DRIVER_W5500_SOCKET_H_
#define NETWORK_DRIVER_W5500_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOCKET_TCP = 1,
    SOCKET_UDP = 2,
    SOCKET_MACRAW = 4
} SocketType_t;

typedef enum {
    TCP_ROLE_NONE,
    TCP_ROLE_CLIENT,
    TCP_ROLE_SERVER
} TcpRole_t;

// Assinatura padrão para qualquer serviço que queira receber dados
// O 'context' é o pulo do gato: permite que a função saiba de quem é o dado (ex: ponteiro pra struct do Telnet)
typedef void (*Socket_RxCallback_t)(void *context, uint8_t *buffer, uint16_t len);

typedef struct {
    uint8_t id;
    SocketType_t type;
    SocketState_t state;

    void *app_context;            // Ponteiro para a struct da aplicação (ex: &drv->telnet)
    Socket_RxCallback_t rx_cb;    // A função que deve ser chamada quando chegar RX

    TcpRole_t tcp_role;

    bool rx_pending;
    bool tx_pending;

    uint8_t rx_buf[1024];
    uint16_t rx_len;

    uint8_t tx_buf[1024];
    uint16_t tx_len;

    uint16_t source_port;

    uint8_t remote_ip[4];
    uint16_t remote_port;

    uint8_t recv_ip[4];
    uint16_t recv_port;

} NetSocket_t;

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_DRIVER_W5500_SOCKET_H_ */

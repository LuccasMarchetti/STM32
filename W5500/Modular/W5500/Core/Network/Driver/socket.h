/*
 * socket.h
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */

#ifndef NETWORK_DRIVER_SOCKET_H_
#define NETWORK_DRIVER_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SOCKET_UNUSED,        // Nunca aberto ou já liberado
    SOCKET_OPENING,       // Criando / configurando
    SOCKET_READY,         // Pronto para TX/RX
    SOCKET_CLOSING,       // Fechamento em andamento
    SOCKET_ERROR
} SocketState_t;

#ifdef __cplusplus
}
#endif
#endif /* NETWORK_DRIVER_SOCKET_H_ */

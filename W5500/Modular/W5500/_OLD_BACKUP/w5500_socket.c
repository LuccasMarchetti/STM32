/*
 * w5500_socket.c
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */
#include "w5500_hal.h"
#include "w5500_socket.h"

NetSocket_t* Net_SocketBind(
		W5500_Driver_t *drv,
		NetSocket_t * sockets,
		SocketType_t type,
		uint16_t port,
		void *context,
		Socket_RxCallback_t rx_cb
	) {
	uint8_t socket = W5500_OpenSocket(drv, type, port);
	if (socket != 0xFF) {
		sockets[socket].app_context = context;
		sockets[socket].rx_cb = rx_cb;
		return &sockets[socket];
	} else {
		return NULL; // Nenhum socket disponível
	}
}

void Net_SocketUnbind(
		W5500_Driver_t *drv,
		NetSocket_t *sockets,
		uint8_t socket_id
	) {
	W5500_CloseSocket(drv, socket_id);
	sockets[socket_id].app_context = NULL;
	sockets[socket_id].rx_cb = NULL;
}

HAL_StatusTypeDef Net_SocketSend(
		W5500_Driver_t *drv,
		uint8_t socket_id,
		uint8_t *data,
		uint16_t len
	) {
	return W5500_SocketSend(drv, socket_id, data, len);
}



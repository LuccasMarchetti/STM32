/*
 * network_manager.h
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */

#ifndef NETWORK_CORE_NETWORK_MANAGER_H_
#define NETWORK_CORE_NETWORK_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "w5500_hal.h"
#include "w5500_socket.h"
#include "ethernet_dhcp.h"
#include "ethernet_dns.h"

typedef struct {
    // 1. O Hardware (O Operário)
	W5500_Driver_t drv;

    // 2. O Middleware (O Gerente e seus 8 sockets lógicos)
    NetSocket_t sockets[8];

    // 3. As Aplicações
    DHCP_Control_t dhcp;
    TELNET_Control_t telnet;

    // 4. O Estado do Chefe
    System_Net_State_t net_state;
} NetworkManager_t;

#ifdef __cplusplus
}
#endif


#endif /* NETWORK_CORE_NETWORK_MANAGER_H_ */

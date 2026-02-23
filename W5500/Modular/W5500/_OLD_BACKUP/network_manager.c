/*
 * network_manager.c
 *
 *  Created on: Feb 21, 2026
 *      Author: Luccas
 */

#include "network_manager.h"
#include "w5500.h"
#include "ethernet_dhcp.h"
#include "ethernet_dns.h"
#include "ethernet_mdns.h"
#include "ethernet_ntp.h"
#include "ethernet_telnet.h"
#include <FreeRTOS.h>
#include "cmsis_os.h"

#ifndef osStaticEventGroupDef_t
typedef StaticEventGroup_t osStaticEventGroupDef_t;
#endif

void W5500_NetworkManagerTask(W5500_Driver_t *drv)
{
    for(;;)
    {
        uint32_t flags = osEventFlagsWait(
            drv->EventHandle,
            0b111111111,
            osFlagsWaitAny,
            osWaitForever
        );

        if (flags & EVT_W5500_IRQ)
        {
            W5500_HandleIRQ(drv);
        }

        if (flags & EVT_SOCKET_TX)
		{
			W5500_HandleTx(drv);
		}

        if (flags & EVT_SOCKET_TIMEOUT)
		{
			//W5500_HandleTx(drv);
		}

        if (flags & EVT_SOCKET_RX)
        {
            W5500_HandleRx(drv);
        }

        if (flags & EVT_SOCKET_DISCON)
		{
			//W5500_HandleRx(drv);
		}

        if (flags & EVT_SOCKET_CON)
		{
        	uint8_t sn_sr = 0;
			//W5500_HandleRx(drv);
		}

        if (flags & (EVT_DHCP_EVENT | EVT_NET_TICK))
        {
            DHCP_FSM(drv);

			DNS_FSM(drv);

			NTP_FSM(drv);

			TELNET_FSM(drv);

        }
    }
}

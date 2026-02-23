/*
 * w5500_hal.c
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */


#include <w5500_hal.h>
#include "cmsis_os.h"
#include <stdbool.h>

HAL_StatusTypeDef W5500_SetAdressControlPhase(W5500_Driver_t * drv, uint16_t addr, uint8_t block, uint8_t rw) {

	drv->tx[0] = (addr >> 8) & 0xFF;
	drv->tx[1] = addr & 0xFF;
	drv->tx[2] = block | rw | W5500_OPCODE_VDM;

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetCommand(W5500_Driver_t * drv, uint8_t * cmd, uint16_t size) {
	memcpy(drv->tx + 3, cmd, size);
	return HAL_OK;
}

HAL_StatusTypeDef W5500_Transmit(W5500_Driver_t * drv, uint8_t * cmd, uint16_t size) {
	osMutexAcquire(drv->spiBusMutex, osWaitForever);
	HAL_GPIO_WritePin(drv->cs_port, drv->cs_pin, GPIO_PIN_RESET); // Assert CS
	HAL_StatusTypeDef ret = HAL_SPI_Transmit_DMA(drv->hspi, cmd, size);

	osSemaphoreAcquire(drv->spiDoneSem, osWaitForever); // Wait for reception to complete
	HAL_GPIO_WritePin(drv->cs_port, drv->cs_pin, GPIO_PIN_SET); // Deassert CS
	osMutexRelease(drv->spiBusMutex);

	return ret;
}

HAL_StatusTypeDef W5500_Receive(W5500_Driver_t * drv, uint8_t * tx, uint8_t * rx, uint16_t size) {

	osMutexAcquire(drv->spiBusMutex, osWaitForever);
	HAL_GPIO_WritePin(drv->cs_port, drv->cs_pin, GPIO_PIN_RESET); // Assert CS
	HAL_StatusTypeDef ret = HAL_SPI_TransmitReceive_DMA(drv->hspi, tx, rx, size);

	osSemaphoreAcquire(drv->spiDoneSem, osWaitForever); // Wait for reception to complete
	HAL_GPIO_WritePin(drv->cs_port, drv->cs_pin, GPIO_PIN_SET); // Deassert CS
	osMutexRelease(drv->spiBusMutex);

	return ret;
}

void NetTimerCallback(void *arg)
{
	W5500_Driver_t * drv = (W5500_Driver_t *)arg;
    osEventFlagsSet(drv->EventHandle, EVT_NET_TICK);
}


HAL_StatusTypeDef W5500_Setup(W5500_Driver_t * drv){

	if (drv->hspi == NULL || drv->spiBusMutex == NULL) {
		return HAL_ERROR;
	}

	const osSemaphoreAttr_t Sem_attributes = {
	  .name = "w5500SpiDoneSem",
	  .cb_mem = &drv->spiDoneSemControlBlock,
	  .cb_size = sizeof(drv->spiDoneSemControlBlock),
	};

	drv->spiDoneSem = osSemaphoreNew(1, 0, &Sem_attributes);

	if (drv->spiDoneSem == NULL) {
		return HAL_ERROR;
	}

	const osEventFlagsAttr_t Event_attributes = {
	  .name = "w5500Event",
	  .cb_mem = &drv->EventControlBlock,
	  .cb_size = sizeof(drv->EventControlBlock),
	};
	drv->EventHandle = osEventFlagsNew(&Event_attributes);

	if (drv->EventHandle == NULL) {
		return HAL_ERROR;
	}

	const osTimerAttr_t Timer_attributes = {
	  .name = "netTimerw5500",
	  .cb_mem = &drv->netTimerControlBlock,
	  .cb_size = sizeof(drv->netTimerControlBlock),
	};
	drv->netTimerHandle = osTimerNew(NetTimerCallback, osTimerPeriodic, drv, &Timer_attributes);

	if (drv->netTimerHandle == NULL) {
		return HAL_ERROR;
	}

	osStatus_t status = osTimerStart(drv->netTimerHandle, pdMS_TO_TICKS(200));
	if (status != osOK) {
	    return HAL_ERROR;
	}

	drv->net_state = SYS_NET_INIT;

	return HAL_OK;
}

HAL_StatusTypeDef W5500_Reset(W5500_Driver_t * drv) {
	W5500_SetAdressControlPhase(drv, W5500_COMMON_MR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	uint8_t command = W5500_COMMON_MR_RST;
	W5500_SetCommand(drv, &command, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_PHYReset(W5500_Driver_t * drv) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_PHYCFGR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	uint8_t command = W5500_COMMON_PHYCFGR_RST | W5500_COMMON_PHYCFGR_OPMD_OPMDC | W5500_COMMON_PHYCFGR_ALL_CAPABLE_AUTO_NEGOTIATION_ENABLED;
	W5500_SetCommand(drv, &command, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadLinkStatus(W5500_Driver_t * drv, bool * link_up) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_PHYCFGR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	*link_up = (drv->rx[3] & W5500_COMMON_PHYCFGR_LNK_ON) != 0;

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadVERSIONR(W5500_Driver_t * drv) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_VERSIONR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	if (drv->rx[3] != W5500_COMMON_VERSIONR_VALUE) {
		return HAL_ERROR;
	}
	drv->versionr = drv->rx[3];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_GenerateMAC(W5500_Driver_t * drv){
	// Get Unique ID for generate MAC Address
	uint32_t uid[3];
	uid[0] = HAL_GetUIDw0();
	uid[1] = HAL_GetUIDw1();
	uid[2] = HAL_GetUIDw2();

	// Generate MAC Hash from UID
	uint32_t mac_hash = uid[0] ^ uid[1] ^ uid[2];

	drv->MAC[0] = 0x02; // 0x02 = Locally Administered & Unicast
	drv->MAC[1] = uid[0]  & 0xFF;
	drv->MAC[2] = (mac_hash >> 24) & 0xFF;
	drv->MAC[3] = (mac_hash >> 16) & 0xFF;
	drv->MAC[4] = (mac_hash >> 8)  & 0xFF;
	drv->MAC[5] = (mac_hash >> 0)  & 0xFF;

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetMAC(W5500_Driver_t * drv) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SHAR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	W5500_SetCommand(drv, drv->MAC, W5500_COMMON_SHAR_LEN);
	W5500_Transmit(drv, drv->tx, 3 + W5500_COMMON_SHAR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadMAC(W5500_Driver_t * drv, uint8_t * mac) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SHAR, W5500_BSB_COMMON, W5500_RWB_READ);
	memset(&drv->tx[3], 0, W5500_COMMON_SHAR_LEN);
	if(W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_COMMON_SHAR_LEN) != HAL_OK) {
		return HAL_ERROR;
	}

	memcpy(mac, &drv->rx[3], W5500_COMMON_SHAR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetIP(W5500_Driver_t * drv){

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SIPR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	W5500_SetCommand(drv, drv->IP, W5500_COMMON_SIPR_LEN);
	W5500_Transmit(drv, drv->tx, 3 + W5500_COMMON_SIPR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadIP(W5500_Driver_t * drv, uint8_t * ip) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SIPR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_COMMON_SIPR_LEN) != HAL_OK) {
		return HAL_ERROR;
	}

	memcpy(ip, &drv->rx[3], W5500_COMMON_SIPR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetGATEWAY(W5500_Driver_t * drv){

	W5500_SetAdressControlPhase(drv, W5500_COMMON_GAR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	W5500_SetCommand(drv, drv->GATEWAY, W5500_COMMON_GAR_LEN);
	W5500_Transmit(drv, drv->tx, 3 + W5500_COMMON_GAR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadGATEWAY(W5500_Driver_t * drv, uint8_t * gateway) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_GAR, W5500_BSB_COMMON, W5500_RWB_READ);
	W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_COMMON_GAR_LEN);

	memcpy(gateway, &drv->rx[3], W5500_COMMON_GAR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetSUBNET(W5500_Driver_t * drv){

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SUBR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	W5500_SetCommand(drv, drv->SUBNET, W5500_COMMON_SUBR_LEN);
	W5500_Transmit(drv, drv->tx, 3 + W5500_COMMON_SUBR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSUBNET(W5500_Driver_t * drv, uint8_t * subnet) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SUBR, W5500_BSB_COMMON, W5500_RWB_READ);
	if(W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_COMMON_SUBR_LEN) != HAL_OK) {
		return HAL_ERROR;
	}

	memcpy(subnet, &drv->rx[3], W5500_COMMON_SUBR_LEN);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadCommonIMR(W5500_Driver_t * drv, uint8_t * imr) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_IMR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	*imr = drv->rx[3];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ConfigureCommonIMR(W5500_Driver_t * drv, uint8_t imr, bool clear) {

	if (!clear) {
		uint8_t current_imr;
		if (W5500_ReadCommonIMR(drv, &current_imr) != HAL_OK) {
			return HAL_ERROR;
		}
		imr = current_imr | imr;
	}

	W5500_SetAdressControlPhase(drv, W5500_COMMON_IMR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	W5500_SetCommand(drv, &imr, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadCommonIR(W5500_Driver_t * drv, uint8_t * ir) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_IR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	*ir = drv->rx[3];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ClearCommonIR(W5500_Driver_t * drv, uint8_t * ir) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_IR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	uint8_t val = (ir == NULL) ? 0xFF : *ir;
	W5500_SetCommand(drv, &val, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadCommonSocketIMR(W5500_Driver_t * drv, uint8_t * imr) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SIMR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	*imr = drv->rx[3];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ConfigureCommonSocketIMR(W5500_Driver_t * drv, uint8_t imr, bool clear) {
	if (!clear) {
		uint8_t current_imr;
		if (W5500_ReadCommonSocketIMR(drv, &current_imr) != HAL_OK) {
			return HAL_ERROR;
		}
		imr = current_imr | imr;
	}

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SIMR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	W5500_SetCommand(drv, &imr, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadCommonSocketIR(W5500_Driver_t * drv, uint8_t * ir) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SIR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	*ir = drv->rx[3];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ClearCommonSocketIR(W5500_Driver_t * drv, uint8_t * ir) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_SIR, W5500_BSB_COMMON, W5500_RWB_WRITE);
	uint8_t val = (ir == NULL) ? 0xFF : *ir;
	W5500_SetCommand(drv, &val, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSocketIR(W5500_Driver_t * drv, uint8_t socket, uint8_t * ir) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_IR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK) {
		return HAL_ERROR;
	}

	*ir = drv->rx[3];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ClearSocketIR(W5500_Driver_t * drv, uint8_t socket, uint8_t * ir) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_IR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	uint8_t val = (ir == NULL) ? 0xFF : *ir;
	W5500_SetCommand(drv, &val, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadUnreachIPPort(W5500_Driver_t * drv, uint8_t * ip, uint8_t * port) {

	W5500_SetAdressControlPhase(drv, W5500_COMMON_UIPR, W5500_BSB_COMMON, W5500_RWB_READ);
	if (W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_COMMON_UIPR_LEN + W5500_COMMON_UPORTR_LEN) != HAL_OK) {
		return HAL_ERROR;
	}
	memcpy(ip, &drv->rx[3], W5500_COMMON_UIPR_LEN);
	memcpy(port, &drv->rx[3 + W5500_COMMON_UIPR_LEN], W5500_COMMON_UPORTR_LEN);

	return HAL_OK;
}

uint8_t W5500_OpenSocket(W5500_Driver_t * drv, uint8_t protocol, uint16_t port) {
    uint8_t socket = 0xFF;

    // Busca socket livre
    if ((protocol & 0b111) == W5500_Sn_MR_MACRAW) {
        if (drv->used_sockets & (1 << 0)) return 0xFF;
        socket = 0;
    } else {
        for (int8_t i = W5500_MAX_SOCKETS-1; i >= 0 ; i--) {
            if ((drv->used_sockets & (1 << i)) == 0) {
                socket = i;
                break;
            }
        }
    }

    if (socket == 0xFF) return 0xFF;

    // Marca como usado logicamente
    drv->used_sockets |= (1 << socket);
    drv->sockets[socket].id = socket;
    drv->sockets[socket].type = protocol;
    drv->sockets[socket].source_port = port;

    // 1. Garante que o socket está FECHADO no hardware antes de configurar
    W5500_WriteSnCR(drv, socket, W5500_Sn_CR_CLOSE);

    // 2. Configura Modo (MR)
    W5500_SetAdressControlPhase(drv, W5500_Sn_MR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
    W5500_SetCommand(drv, &protocol, 1);
    W5500_Transmit(drv, drv->tx, 4);

    // 3. Configura Porta (PORT)
    W5500_SetAdressControlPhase(drv, W5500_Sn_PORT, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
    uint8_t p[2] = { (port >> 8) & 0xFF, port & 0xFF };
    W5500_SetCommand(drv, p, 2);
    W5500_Transmit(drv, drv->tx, 5);

    // 4. Envia comando OPEN (CR)
    uint8_t command = W5500_Sn_CR_OPEN;
    W5500_SetAdressControlPhase(drv, W5500_Sn_CR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
    W5500_SetCommand(drv, &command, 1);
    W5500_Transmit(drv, drv->tx, 4);

    // 5. Aguarda transição de estado
    uint8_t status = 0;
    uint32_t startTick = osKernelGetTickCount();
    // Aumentei o timeout para 500ms por segurança
    uint32_t timeoutTicks = pdMS_TO_TICKS(500);

    do {
        W5500_ReadSnSR(drv, socket, &status);

        if ((osKernelGetTickCount() - startTick) > timeoutTicks) {
            // Falhou (Timeout)
            drv->used_sockets &= ~(1 << socket);
            W5500_WriteSnCR(drv, socket, W5500_Sn_CR_CLOSE); // Tenta fechar
            return 0xFF;
        }
        osThreadYield();
    } while (status == 0x00); // 0x00 = SOCK_CLOSED

    // Sucesso - configura tamanhos de buffer (padrão 2KB)
    drv->hw_sockets[socket].id = socket;
    drv->hw_sockets[socket].tx_buf_size = 2048;
    drv->hw_sockets[socket].tx_mask = drv->hw_sockets[socket].tx_buf_size - 1;
    drv->hw_sockets[socket].rx_buf_size = 2048;
    drv->hw_sockets[socket].rx_mask = drv->hw_sockets[socket].rx_buf_size - 1;

    return socket;
}

HAL_StatusTypeDef W5500_CloseSocket(W5500_Driver_t * drv, uint8_t socket) {

	if (!(drv->used_sockets & (1 << socket))) {
		return HAL_ERROR; // Socket not in use
	}
	uint8_t command = W5500_Sn_CR_CLOSE;
	W5500_SetAdressControlPhase(drv, W5500_Sn_CR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	W5500_SetCommand(drv, &command, 1);
	if (W5500_Transmit(drv, drv->tx, 4) != HAL_OK) {
		return HAL_ERROR;
	}
	drv->used_sockets &= ~(1 << socket);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetSocketIMR(W5500_Driver_t * drv, uint8_t socket, uint8_t imr) {
	W5500_SetAdressControlPhase(drv, W5500_Sn_IMR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	W5500_SetCommand(drv, &imr, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;

}

HAL_StatusTypeDef W5500_SetSocketDestIP(W5500_Driver_t * drv, uint8_t socket, uint8_t * ip) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_DIPR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	W5500_SetCommand(drv, ip, 4);

	memcpy(&drv->sockets[socket].remote_ip, ip, 4);

	W5500_Transmit(drv, drv->tx, 7);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetSocketDestPort(W5500_Driver_t * drv, uint8_t socket, uint16_t port) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_DPORT, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	drv->sockets[socket].remote_port = port;
	uint8_t p[2] = { (port >> 8) & 0xFF, port & 0xFF };
	W5500_SetCommand(drv, p, 2);
	W5500_Transmit(drv, drv->tx, 5);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SetSocketDestMAC(W5500_Driver_t * drv, uint8_t socket, uint8_t * mac) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_DHAR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	W5500_SetCommand(drv, mac, 6);
	W5500_Transmit(drv, drv->tx, 9);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_WriteSnCR(W5500_Driver_t * drv, uint8_t socket, uint8_t command) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_CR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	W5500_SetCommand(drv, &command, 1);
	W5500_Transmit(drv, drv->tx, 4);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSnTXWR(W5500_Driver_t * drv, uint8_t socket, uint16_t * tx_ptr) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_TX_WR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
	W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_Sn_TX_WR_LEN);
	*tx_ptr = (drv->rx[3] << 8) | drv->rx[4];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_WriteSnTXWR(W5500_Driver_t * drv, uint8_t socket, uint16_t tx_ptr) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_TX_WR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_WRITE);
	uint8_t p[2];
	p[0] = (tx_ptr >> 8) & 0xFF;   // MSB first
	p[1] = tx_ptr & 0xFF;
	W5500_SetCommand(drv, p, 2);
	W5500_Transmit(drv, drv->tx, 5);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSnTXRD(W5500_Driver_t * drv, uint8_t socket, uint16_t * tx_rd_ptr) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_TX_RD, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
	W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_Sn_TX_RD_LEN);

	*tx_rd_ptr = (drv->rx[3] << 8) | drv->rx[4];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_WriteSnRXRD(
    W5500_Driver_t * drv,
    uint8_t socket,
    uint16_t rx_rd_ptr
) {
    uint8_t p[2];
    p[0] = (rx_rd_ptr >> 8) & 0xFF;
    p[1] = rx_rd_ptr & 0xFF;

    W5500_SetAdressControlPhase(drv, W5500_Sn_RX_RD,
                                W5500_BSB_SOCKET_REG(socket),
                                W5500_RWB_WRITE);

    W5500_SetCommand(drv, p, 2);
    W5500_Transmit(drv, drv->tx, 5);

    return HAL_OK;
}


HAL_StatusTypeDef W5500_ReadSnRXRSR(W5500_Driver_t * drv, uint8_t socket, uint16_t * rx_size) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_RX_RSR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
	W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_Sn_RX_RSR_LEN);

	*rx_size = (drv->rx[3] << 8) | drv->rx[4];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSnRXWR(W5500_Driver_t * drv, uint8_t socket, uint16_t * rx_ptr) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_RX_WR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
	W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_Sn_RX_WR_LEN);
	*rx_ptr = (drv->rx[3] << 8) | drv->rx[4];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSnRXRD(W5500_Driver_t * drv, uint8_t socket, uint16_t * rx_rd_ptr) {

	W5500_SetAdressControlPhase(drv, W5500_Sn_RX_RD, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
	W5500_Receive(drv, drv->tx, drv->rx, 3 + W5500_Sn_RX_RD_LEN);

	*rx_rd_ptr = (drv->rx[3] << 8) | drv->rx[4];

	return HAL_OK;
}

HAL_StatusTypeDef W5500_ReadSnSR(W5500_Driver_t * drv, uint8_t socket, uint8_t * status)
{
	W5500_SetAdressControlPhase(drv, W5500_Sn_SR, W5500_BSB_SOCKET_REG(socket), W5500_RWB_READ);
    if(W5500_Receive(drv, drv->tx, drv->rx, 4) != HAL_OK)
        return HAL_ERROR;

    *status = drv->rx[3];

    return HAL_OK;
}


HAL_StatusTypeDef W5500_SocketSend(
	W5500_Driver_t *drv,
	uint8_t socket,
	uint8_t *data,
	uint16_t size
) {
	uint16_t tx_wr;
	uint16_t offset;

	// Read TX Write Pointer
	W5500_ReadSnTXWR(drv, socket, &tx_wr);

	// Calculate offset address
	offset = tx_wr & drv->hw_sockets[socket].tx_mask;

	W5500_SetAdressControlPhase(drv, offset, W5500_BSB_SOCKET_TX(socket), W5500_RWB_WRITE);
	W5500_SetCommand(drv, data, size);
	W5500_Transmit(drv, drv->tx, 3 + size);

	// Update TX Write Pointer
	tx_wr += size;
	W5500_WriteSnTXWR(drv, socket, tx_wr);

	W5500_WriteSnCR(drv, socket, W5500_Sn_CR_SEND);

	return HAL_OK;
}

HAL_StatusTypeDef W5500_SocketRecv(
    W5500_Driver_t *drv,
    uint8_t socket,
    uint8_t *out,
    uint16_t max_len,
    uint16_t *out_len
) {
    uint16_t rx_size;
    uint16_t rx_rd;
    uint16_t offset;

    W5500_ReadSnRXRSR(drv, socket, &rx_size);

    if (rx_size == 0) {
        *out_len = 0;
        return HAL_OK;
    }

    if (rx_size > max_len)
        rx_size = max_len;

    W5500_ReadSnRXRD(drv, socket, &rx_rd);

    offset = rx_rd & drv->hw_sockets[socket].rx_mask;

    W5500_SetAdressControlPhase(drv, offset, W5500_BSB_SOCKET_RX(socket), W5500_RWB_READ);
    W5500_Receive(drv, drv->tx, drv->rx, 3 + rx_size);

    switch (drv->sockets[socket].type) {
		case SOCKET_TCP:
			memcpy(out, &drv->rx[3], rx_size);
			*out_len = rx_size;
			break;

		case SOCKET_UDP:
			memcpy(drv->sockets[socket].recv_ip, &drv->rx[3], 6);
			memcpy(&drv->sockets[socket].recv_port, &drv->rx[9], 2);
			memcpy(out, &drv->rx[11], rx_size - 8);
			*out_len = rx_size - 8;
			break;

		case SOCKET_MACRAW:
			break;
    }

    rx_rd += rx_size;

    W5500_WriteSnRXRD(drv, socket, rx_rd);
    W5500_WriteSnCR(drv, socket, W5500_Sn_CR_RECV);

    return HAL_OK;
}

static void W5500_SocketFSM_Opening(W5500_Driver_t *drv, NetSocket_t *s)
{
    switch (s->hw_state) {

    case W5500_SOCK_CLOSED:
        // Criar socket
        s->id = W5500_OpenSocket(drv, s->type, s->source_port);
        break;

    case W5500_SOCK_INIT:
        if (s->type == SOCKET_TCP) {
        	if (s->tcp_role == TCP_ROLE_SERVER) {
        		W5500_WriteSnCR(drv, s->id, W5500_Sn_CR_LISTEN);
			}
			else if (s->tcp_role == TCP_ROLE_CLIENT) {
				W5500_WriteSnCR(drv, s->id, W5500_Sn_CR_CONNECT);
			}
        } else {
            // UDP e MACRAW já estão prontos
            s->state = SOCKET_READY;
        }
        break;

    case W5500_SOCK_LISTEN:
        // Aguardando conexão
        break;

    case W5500_SOCK_ESTABLISHED:
    case W5500_SOCK_UDP:
    case W5500_SOCK_MACRAW:
        s->state = SOCKET_READY;
        break;

    default:
        break;
    }
}

static void W5500_SocketFSM_Ready(W5500_Driver_t *drv, NetSocket_t *s)
{
    switch (s->hw_state) {

    case W5500_SOCK_ESTABLISHED:
    case W5500_SOCK_UDP:
    case W5500_SOCK_MACRAW:

        if (s->rx_pending) {
        	W5500_SocketRecv(drv, s->id, s->rx_buf, 1024, &s->rx_len);
            s->rx_pending = false;
        }

        if (s->tx_pending) {
            W5500_SocketSend(drv, s->id, s->tx_buf, s->tx_len);
			s->tx_pending = false;
			s->tx_len = 0;
        }
        break;

    case W5500_SOCK_CLOSEWAIT:
        s->state = SOCKET_CLOSING;
        break;

    case W5500_SOCK_CLOSED:
        s->state = SOCKET_UNUSED;
        break;

    default:
        break;
    }
}

static void W5500_SocketFSM_Closing(W5500_Driver_t *drv, NetSocket_t *s)
{
    switch (s->hw_state) {

    case W5500_SOCK_CLOSEWAIT:
    	W5500_WriteSnCR(drv, s->id, W5500_Sn_CR_CLOSE);
        break;

    case W5500_SOCK_FINWAIT:
    case W5500_SOCK_LASTACK:
    case W5500_SOCK_TIMEWAIT:
        // Aguardando fechamento completo
        break;

    case W5500_SOCK_CLOSED:
        s->state = SOCKET_UNUSED;
        break;

    default:
        break;
    }
}

static void W5500_SocketFSM_Error(W5500_Driver_t *drv, NetSocket_t *s)
{
	W5500_WriteSnCR(drv, s->id, W5500_Sn_CR_CLOSE);
    s->state = SOCKET_UNUSED;
}

void W5500_SocketFSM(W5500_Driver_t *drv, NetSocket_t *s)
{

    if(W5500_ReadSnSR(drv, s->id, &s->hw_state) != HAL_OK) {
    	s->state = SOCKET_ERROR;
		return;
	}

    switch (s->hw_state)
    {
    case W5500_SOCK_ESTABLISHED:
    case W5500_SOCK_UDP:
    case W5500_SOCK_MACRAW:
        s->state = SOCKET_READY;
        break;

    case W5500_SOCK_CLOSEWAIT:
        s->state = SOCKET_CLOSING;
        break;

    case W5500_SOCK_CLOSED:
        s->state = SOCKET_UNUSED;
        break;

    default:
        break;
    }



    switch (s->state) {

    case SOCKET_UNUSED:
        // Socket não alocado
        break;

    case SOCKET_OPENING:
        W5500_SocketFSM_Opening(drv, s);
        break;

    case SOCKET_READY:
    	W5500_SocketFSM_Ready(drv, s);
        break;

    case SOCKET_CLOSING:
    	W5500_SocketFSM_Closing(drv, s);
        break;

    case SOCKET_ERROR:
    	W5500_SocketFSM_Error(drv, s);
        break;
    }
}

void W5500_HandleTx(W5500_Driver_t *drv)
{
	for (int i = 0; i < W5500_MAX_SOCKETS; i++) {
		if (drv->sockets[i].tx_pending) {
			W5500_SocketFSM(drv, &drv->sockets[i]);
		}
	}
}

void W5500_HandleRx(W5500_Driver_t *drv)
{
    for (int i = 0; i < W5500_MAX_SOCKETS; i++)
    {
        NetSocket_t *s = &drv->sockets[i];

        if (!s->rx_pending)
            continue;

        s->rx_pending = false;

        uint16_t len;
        W5500_SocketRecv(drv, s->id, s->rx_buf, sizeof(s->rx_buf), &len);

        if (len == 0)
            continue;
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

    }
}

void W5500_IRQCallback(W5500_Driver_t * drv) {
	osEventFlagsSet(drv->EventHandle, EVT_W5500_IRQ);
}

void W5500_HandleIRQ(W5500_Driver_t * drv) {
    uint8_t ir = 0;
    W5500_ReadCommonIR(drv, &ir);
    W5500_ClearCommonIR(drv, &ir); // Limpa as flags globais imediatamente (Write 1 to Clear)
    W5500_ReadCommonSocketIR(drv, &ir); // Lê quais sockets interromperam
    W5500_ClearCommonSocketIR(drv, &ir); // Limpa as flags de socket imediatamente (Write 1 to Clear)
    uint8_t socket_idx = 0;

    // Varre todos os bits ativos no IR global
    while (ir && (socket_idx < W5500_MAX_SOCKETS)) {
        uint8_t mask = (1 << socket_idx);

        if (ir & mask) {
            uint8_t sn_ir = 0;
            // Lê o registrador de interrupção ESPECÍFICO do socket
            W5500_ReadSocketIR(drv, socket_idx, &sn_ir);

            // Limpa as flags no hardware imediatamente (Write 1 to Clear)
            // É mais seguro limpar tudo que lemos para evitar reentrância
            W5500_ClearSocketIR(drv, socket_idx, &sn_ir);

            // Processa as flags
            if (sn_ir & W5500_Sn_IR_SEND_OK) {
                drv->sockets[socket_idx].tx_pending = false;
                osEventFlagsSet(drv->EventHandle, EVT_SOCKET_TX);
            }

            if (sn_ir & W5500_Sn_IR_TIMEOUT) {
                drv->sockets[socket_idx].state = SOCKET_ERROR;
                osEventFlagsSet(drv->EventHandle, EVT_SOCKET_TIMEOUT);
            }

            if (sn_ir & W5500_Sn_IR_RECV) {
                drv->sockets[socket_idx].rx_pending = true;
                osEventFlagsSet(drv->EventHandle, EVT_SOCKET_RX);
            }

            if (sn_ir & W5500_Sn_IR_DISCON) {
                drv->sockets[socket_idx].state = SOCKET_CLOSING;
                osEventFlagsSet(drv->EventHandle, EVT_SOCKET_DISCON);
            }

            if (sn_ir & W5500_Sn_IR_CON) {
                drv->sockets[socket_idx].state = SOCKET_READY;
                osEventFlagsSet(drv->EventHandle, EVT_SOCKET_CON);
            }

            // Remove este socket da lista de pendentes local
            ir &= ~mask;
        }
        socket_idx++;
    }
}

HAL_StatusTypeDef W5500_Init(W5500_Driver_t * drv) {
	W5500_Reset(drv);
	osDelay(pdMS_TO_TICKS(100));
	W5500_PHYReset(drv);
	osDelay(pdMS_TO_TICKS(100));
	W5500_ReadVERSIONR(drv);
	W5500_GenerateMAC(drv);
	W5500_SetMAC(drv);
	W5500_ReadMAC(drv, drv->MAC);
	W5500_SetIP(drv);
	W5500_SetGATEWAY(drv);
	W5500_SetSUBNET(drv);
	W5500_ConfigureCommonSocketIMR(drv, 0xFF, false);
	for (int i = 0; i < W5500_MAX_SOCKETS; i++) {
		W5500_SetSocketIMR(drv, i, 0xFF); // Habilita todas as interrupções de socket
	}
	W5500_ConfigureCommonIMR(drv, 0xFF, true);

	W5500_mDNS_Start(drv);

	return HAL_OK;
}

// Retorna o ID do socket (0 a 7) ou -1 se não houver sockets livres
uint8_t W5500_BindSocket(W5500_Driver_t *drv, uint8_t protocol, uint16_t port, void *context, Socket_RxCallback_t rx_cb) {

    uint8_t socket_id = W5500_OpenSocket(drv, protocol, port);
	if (socket_id != 0xFF) {
		drv->sockets[socket_id].app_context = context;
		drv->sockets[socket_id].rx_cb = rx_cb;
	}
    return socket_id;
}

// Função para liberar o socket quando o serviço fechar (ex: Telnet desconectou)
void W5500_UnbindSocket(W5500_Driver_t *drv, uint8_t socket_id) {
    if (socket_id < 8) {
        drv->sockets[socket_id].app_context = NULL;
        drv->sockets[socket_id].rx_cb = NULL;
        W5500_CloseSocket(drv, socket_id);
    }
}


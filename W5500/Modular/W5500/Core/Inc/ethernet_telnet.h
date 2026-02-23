/*
 * ethernet_telnet.h
 *
 *  Created on: Feb 22, 2026
 *      Author: Luccas
 */

#ifndef INC_ETHERNET_TELNET_H_
#define INC_ETHERNET_TELNET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// --- ESTADOS DO TELNET (TCP SERVER) ---
typedef enum {
    TELNET_IDLE = 0,
    TELNET_LISTEN,       // Aguardando alguém conectar na porta 23
    TELNET_CONNECTED,    // Alguém conectou!
    TELNET_DISCONNECTING // Fechando a conexão
} TELNET_State_t;

// --- ESTRUTURA DE CONTROLE ---
typedef struct {
    TELNET_State_t state;
    uint8_t socket;
    char rx_buffer[128]; // Buffer para acumular os comandos digitados
    uint16_t rx_index;   // Posição atual do buffer
} TELNET_Control_t;

typedef struct W5500_Driver_t W5500_Driver_t;

void TELNET_FSM(W5500_Driver_t *drv);
void TELNET_HandleRx(W5500_Driver_t *drv, uint8_t *buf, uint16_t len);
void TELNET_ProcessCommand(W5500_Driver_t *drv, char *cmd_buffer);

#ifdef __cplusplus
}
#endif

#endif /* INC_ETHERNET_TELNET_H_ */

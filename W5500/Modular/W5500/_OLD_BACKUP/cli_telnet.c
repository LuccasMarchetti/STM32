/*
 * cli_telnet.c
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */

#include "cli_telnet.h"
#include "w5500.h"

#include <stdio.h>
#include <string.h>

// 1. Declaração antecipada de todas as funções (Handlers)
void cmd_help_handler(W5500_Driver_t *drv, int argc, char **argv);
void cmd_info_handler(W5500_Driver_t *drv, int argc, char **argv);
void cmd_led_handler(W5500_Driver_t *drv, int argc, char **argv);

// 2. A nossa tabela mestra de comandos
const Telnet_Command_t cli_commands[] = {
    {"help", "Mostra esta lista automatica de comandos", cmd_help_handler},
    {"info", "Mostra o IP e status da placa",            cmd_info_handler},
    {"led",  "Controla o LED (ex: led on, led off, led toggle)",     cmd_led_handler},
    // Quer um comando novo? É só colocar a linha dele aqui e pronto!
};

// Variavel global para o tamanho da tabela
const int num_cli_commands = sizeof(cli_commands) / sizeof(Telnet_Command_t);


// 3. A implementação do super comando HELP
void cmd_help_handler(W5500_Driver_t *drv, int argc, char **argv) {

    // Cabeçalho
    char *header = "\r\n=== Comandos Disponiveis ===\r\n";
    W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)header, strlen(header));

    // Varre a tabela inteira e imprime linha por linha
    for (int i = 0; i < num_cli_commands; i++) {
        // O "%-10s" é o pulo do gato! Ele força a string a ocupar sempre 10 caracteres,
        // preenchendo com espaços em branco. Isso deixa tudo perfeitamente alinhado na tela!
        snprintf(drv->sockets[drv->telnet.socket].tx_buf, sizeof(drv->sockets[drv->telnet.socket].tx_buf), "  %-10s - %s\r\n",
                 cli_commands[i].cmd_name,
                 cli_commands[i].help_text);

        W5500_SocketSend(
        		drv,
				drv->telnet.socket,
				drv->sockets[drv->telnet.socket].tx_buf,
				strlen(drv->sockets[drv->telnet.socket].tx_buf)
				);
    }
}

// Comando: info
void cmd_info_handler(W5500_Driver_t *drv, int argc, char **argv) {
    char tx_buf[64];
    sprintf(tx_buf, "IP Atual: %d.%d.%d.%d\r\n",
            drv->IP[0], drv->IP[1], drv->IP[2], drv->IP[3]);
    W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)tx_buf, strlen(tx_buf));
}

// Comando: led <on|off>
void cmd_led_handler(W5500_Driver_t *drv, int argc, char **argv) {
    if (argc < 2) {
    	sprintf(drv->sockets[drv->telnet.socket].tx_buf, "Uso: led <on|off|toggle>\r\n");
        W5500_SocketSend(drv, drv->telnet.socket, drv->sockets[drv->telnet.socket].tx_buf, strlen(drv->sockets[drv->telnet.socket].tx_buf));
        return;
    }

    if (strcmp(argv[1], "on") == 0) {
        // LIGA LED AQUI
    	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_SET);
        W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)"LED Ligado!\r\n", 13);
    }
    else if (strcmp(argv[1], "off") == 0) {
        // DESLIGA LED AQUI
    	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, GPIO_PIN_RESET);
        W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)"LED Desligado!\r\n", 16);
    }
    else if (strcmp(argv[1], "toggle") == 0) {
		// TOGGLE LED AQUI
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_6);
		W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)"LED Toggle!\r\n", 13);
	}
	else {
		W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)"Erro: Argumento desconhecido. Use 'on', 'off' ou 'toggle'.\r\n", 70);
	}
}

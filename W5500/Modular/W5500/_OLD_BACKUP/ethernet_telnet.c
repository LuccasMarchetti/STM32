/*
 * ethernet_telnet.c
 *
 *  Created on: Feb 22, 2026
 *      Author: Luccas
 */
#include "ethernet_telnet.h"
#include "cli_telnet.h"
#include "w5500.h"

#include <string.h>
#include <stdio.h>

void TELNET_ProcessCommand(W5500_Driver_t *drv, char *cmd_buffer) {
    char *argv[MAX_ARGS];
    int argc = 0;

    // 1. Fatia a string recebida em argc e argv
    char *token = strtok(cmd_buffer, " ");
    while (token != NULL && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    // Se o usuário apertou Enter sem digitar nada
    if (argc == 0) {
        W5500_SocketSend(drv, drv->telnet.socket, (uint8_t*)"STM32> ", 7);
        return;
    }

    // 2. Varre a tabela procurando o comando
    bool command_found = false;
    for (int i = 0; i < num_cli_commands; i++) {
        if (strcmp(argv[0], cli_commands[i].cmd_name) == 0) {
            // AChou! Executa a função passando os argumentos
            cli_commands[i].handler(drv, argc, argv);
            command_found = true;
            break;
        }
    }

    // 3. Tratamento de comando não encontrado (Poderia ser o comando 'help' também)
    if (!command_found) {
        sprintf(drv->sockets[drv->telnet.socket].tx_buf, "Comando desconhecido: %s\r\n", argv[0]);
        W5500_SocketSend(
        		drv,
				drv->telnet.socket,
				drv->sockets[drv->telnet.socket].tx_buf,
				strlen(drv->sockets[drv->telnet.socket].tx_buf));
    }

    // 4. Imprime o prompt novamente
    sprintf(drv->sockets[drv->telnet.socket].tx_buf, "STM32: ", 7);
    W5500_SocketSend(drv, drv->telnet.socket, drv->sockets[drv->telnet.socket].tx_buf, 7);
}

void TELNET_FSM(W5500_Driver_t *drv) {
    TELNET_Control_t *telnet = &drv->telnet;

    // O Telnet só deve subir se a rede estiver pronta!
    if (drv->net_state != SYS_NET_READY) return;

    switch (telnet->state) {
        case TELNET_IDLE:
            if (telnet->socket == 0xFF) {
                // Abre socket TCP na porta 23 (Padrão Telnet)
                telnet->socket = W5500_OpenSocket(drv, W5500_Sn_MR_TCP, 23);
            }

            // Manda o W5500 entrar em modo de ESCUTA (TCP Server)
            W5500_WriteSnCR(drv, telnet->socket, W5500_Sn_CR_LISTEN);

            telnet->rx_index = 0;
            telnet->state = TELNET_LISTEN;
            break;

        case TELNET_LISTEN:
            // O Socket fica esperando.
            // Quando alguém conectar, o W5500 vai gerar a interrupção W5500_Sn_IR_CON.
            // O seu W5500_HandleIRQ já capta isso! Vamos tratar a mudança de estado
            // usando uma verificação simples do status do socket.
            {
                uint8_t sn_sr = 0;
                W5500_ReadSnSR(drv, telnet->socket, &sn_sr);
                if (sn_sr == W5500_SOCK_ESTABLISHED) {
                    telnet->state = TELNET_CONNECTED;

                    // Envia uma mensagem de boas-vindas pro usuário!
                    char *welcome = "\r\n=== STM32 Telnet Server ===\r\nDigite 'help': ";
                    W5500_SocketSend(drv, telnet->socket, (uint8_t*)welcome, strlen(welcome));
                }
            }
            break;

        case TELNET_CONNECTED:
            // Fica aqui processando comandos...
            // Se o usuário fechar o PuTTY, o W5500 gera W5500_Sn_IR_DISCON.
            {
                uint8_t sn_sr = 0;
                W5500_ReadSnSR(drv, telnet->socket, &sn_sr);

                // Se o status mudou para fechado ou fechando...
                if (sn_sr == W5500_SOCK_CLOSED || sn_sr == W5500_SOCK_CLOSEWAIT) {
                    telnet->state = TELNET_DISCONNECTING;
                }
            }
            break;

        case TELNET_DISCONNECTING:
        	W5500_WriteSnCR(drv, telnet->socket, W5500_Sn_CR_DISCON);
            W5500_CloseSocket(drv, telnet->socket);
            telnet->socket = 0xFF;
            telnet->state = TELNET_IDLE; // Volta pro início para aceitar nova conexão
            break;
    }
}

void TELNET_HandleRx(W5500_Driver_t *drv, uint8_t *buf, uint16_t len) {
    TELNET_Control_t *telnet = &drv->telnet;

    for (uint16_t i = 0; i < len; i++) {
        uint8_t c = buf[i];

        // LÓGICA DE FILTRO DO PROTOCOLO TELNET
        if (c == 0xFF) { // Se for um byte de comando (IAC)
            // Os comandos Telnet têm sempre 3 bytes de tamanho (ex: FF FB 01)
            // Então simplesmente pulamos os próximos 2 bytes!
            i += 2;
            continue;
        }

        // Se for um caractere normal, adicionamos ao nosso buffer
        if (c >= 32 && c <= 126) { // Tabela ASCII visível
            if (telnet->rx_index < sizeof(telnet->rx_buffer) - 1) {
                telnet->rx_buffer[telnet->rx_index++] = c;

                // Ecoa a letra de volta para o cliente ver o que está digitando
                W5500_SocketSend(drv, telnet->socket, &c, 1);
            }
        }
        // Se o usuário apertar ENTER (\r ou \n)
        else if (c == '\r' || c == '\n') {
            if (telnet->rx_index > 0) { // Se tem algo digitado
                telnet->rx_buffer[telnet->rx_index] = '\0'; // Finaliza a string

                // Manda pular linha no terminal do cliente
                W5500_SocketSend(drv, telnet->socket, (uint8_t*)"\r\n", 2);

                // 🔥 AQUI VOCÊ EXECUTA O COMANDO! 🔥
                TELNET_ProcessCommand(drv, telnet->rx_buffer);

                // Limpa o buffer para o próximo comando
                telnet->rx_index = 0;
            }
        }
        // Se o usuário apertar Backspace (Apagar)
        else if (c == 0x08 || c == 0x7F) {
            if (telnet->rx_index > 0) {
                telnet->rx_index--;
                // Manda comando pro terminal do cliente apagar a letra da tela
                W5500_SocketSend(drv, telnet->socket, (uint8_t*)"\x08 \x08", 3);
            }
        }
    }
}

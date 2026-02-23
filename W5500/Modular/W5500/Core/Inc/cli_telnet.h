/*
 * cli_telnet.h
 *
 *  Created on: Feb 23, 2026
 *      Author: Luccas
 */

#ifndef INC_CLI_TELNET_H_
#define INC_CLI_TELNET_H_

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ARGS 5 // Número máximo de argumentos por comando

typedef struct W5500_Driver_t W5500_Driver_t;
// A assinatura (molde) que TODAS as funções de comando devem ter
typedef void (*CmdHandler_t)(W5500_Driver_t *drv, int argc, char **argv);

// A estrutura que amarra o texto do comando à função
typedef struct {
    const char *cmd_name;
    const char *help_text;
    CmdHandler_t handler;
} Telnet_Command_t;

// Declarações externas para a tabela de comandos e seu tamanho
extern const Telnet_Command_t cli_commands[];
extern const int num_cli_commands;

#ifdef __cplusplus
}
#endif

#endif /* INC_CLI_TELNET_H_ */
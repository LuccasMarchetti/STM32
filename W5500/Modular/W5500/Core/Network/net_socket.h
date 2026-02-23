/*
 * net_socket.h
 *
 * API abstrata de sockets - INDEPENDENTE de implementação (W5500/lwIP/BSD)
 * 
 * Esta camada esconde os detalhes de hardware e fornece uma interface
 * genérica que pode ser usada por qualquer serviço/aplicação.
 */

#ifndef NETWORK_NET_SOCKET_H_
#define NETWORK_NET_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "net_types.h"

typedef struct W5500_HW W5500_HW_t;

/* =============================================================================
 * ESTRUTURA DE SOCKET
 * ============================================================================= */

#define NET_MAX_SOCKETS 8

typedef struct NetSocket {
    uint8_t id;
    uint8_t hw_socket;
    NetSocketType_t type;
    uint8_t protocol;
    NetSocketState_t state;
    uint16_t local_port;
    uint16_t remote_port;
    NetAddr_t local_addr;
    NetAddr_t remote_addr;
    NetSocketRxCallback_t rx_callback;
    void *user_data;
} NetSocket_t;

/* =============================================================================
 * INICIALIZACAO
 * ============================================================================= */

NetErr_t NetSocket_Init(W5500_HW_t *hw);
NetErr_t NetSocket_Deinit(void);

/* =============================================================================
 * FUNÇÕES DE CRIAÇÃO E DESTRUIÇÃO
 * ============================================================================= */

/**
 * @brief Cria um novo socket
 * 
 * @param type Tipo do socket (TCP, UDP, RAW)
 * @return Ponteiro para socket criado ou NULL se falhar
 * 
 * @note Socket criado começa no estado CLOSED
 * @note Deve ser liberado com NetSocket_Destroy() quando não for mais usado
 */
NetSocket_t* NetSocket_Create(NetSocketType_t type, uint8_t protocol);

/**
 * @brief Destrói um socket e libera recursos
 * 
 * @param sock Ponteiro para o socket
 * @return NET_OK ou código de erro
 * 
 * @note Fecha o socket se ainda estiver aberto
 * @note Define o ponteiro como NULL após destruir
 */
NetErr_t NetSocket_Destroy(NetSocket_t *sock);

/* =============================================================================
 * FUNÇÕES DE CONFIGURAÇÃO
 * ============================================================================= */

/**
 * @brief Associa (bind) socket a uma porta local
 * 
 * @param sock Socket
 * @param port Porta local (0 = escolher automaticamente)
 * @return NET_OK ou código de erro
 * 
 * @note Necessário antes de Listen() ou para UDP em porta específica
 */
NetErr_t NetSocket_Bind(NetSocket_t *sock, const NetAddr_t *addr);

/**
 * @brief Define callback para recepção de dados
 * 
 * @param sock Socket
 * @param callback Função a ser chamada quando dados chegarem
 * @param user_ctx Contexto do usuário (passado de volta no callback)
 * @return NET_OK ou código de erro
 * 
 * @note Callback é chamado em contexto de interrupção/task do manager!
 */
NetErr_t NetSocket_SetRxCallback(
    NetSocket_t *sock,
    NetSocketRxCallback_t callback,
    void *user_ctx
);

/**
 * @brief Define timeout para operações
 * 
 * @param sock Socket
 * @param timeout_ms Timeout em milissegundos (0 = sem timeout)
 * @return NET_OK ou código de erro
 */
/* =============================================================================
 * FUNÇÕES TCP SERVER
 * ============================================================================= */

/**
 * @brief Coloca socket TCP em modo de escuta (servidor)
 * 
 * @param sock Socket TCP
 * @param backlog Número máximo de conexões pendentes (ignorado em W5500)
 * @return NET_OK ou código de erro
 * 
 * @note Socket deve estar em bind() antes
 * @note W5500 aceita apenas 1 conexão por socket
 */
NetErr_t NetSocket_Listen(NetSocket_t *sock, uint8_t backlog);

/* =============================================================================
 * FUNÇÕES TCP CLIENT
 * ============================================================================= */

/**
 * @brief Conecta a um servidor remoto (TCP client)
 * 
 * @param sock Socket TCP
 * @param addr Endereço do servidor (IP + porta)
 * @return NET_OK ou código de erro
 * 
 * @note Operação pode ser assíncrona (retornar NET_ERR_IN_PROGRESS)
 * @note Verificar estado com NetSocket_GetState()
 */
NetErr_t NetSocket_Connect(NetSocket_t *sock, const NetAddr_t *addr);

/**
 * @brief Desconecta socket TCP
 * 
 * @param sock Socket TCP
 * @return NET_OK ou código de erro
 * 
 * @note Envia FIN e aguarda ACK
 */
NetErr_t NetSocket_Disconnect(NetSocket_t *sock);

/* =============================================================================
 * FUNÇÕES DE TRANSMISSÃO E RECEPÇÃO
 * ============================================================================= */

/**
 * @brief Envia dados através do socket
 * 
 * TCP: Envia para conexão estabelecida
 * UDP: Requer que remote_addr seja definido previamente ou passado em SendTo()
 * 
 * @param sock Socket
 * @param data Ponteiro para dados a enviar
 * @param len Tamanho dos dados
 * @return NET_OK ou código de erro
 * 
 * @note Para TCP, socket deve estar em ESTABLISHED
 * @note Pode retornar NET_ERR_WOULD_BLOCK se buffer estiver cheio
 */
int32_t NetSocket_Send(
    NetSocket_t *sock,
    const uint8_t *data,
    uint16_t len,
    uint32_t timeout_ms
);

/**
 * @brief Recebe dados do socket TCP (modo polling)
 *
 * @param sock Socket
 * @param buffer Buffer para armazenar dados
 * @param buffer_size Tamanho do buffer
 * @return NET_OK ou código de erro
 *
 * @note Alternativa ao callback para modo polling
 * @note Retorna NET_ERR_WOULD_BLOCK se não houver dados
 */
int32_t NetSocket_Recv(
    NetSocket_t *sock,
    uint8_t *buffer,
    uint16_t buffer_size,
    uint32_t timeout_ms
);

/**
 * @brief Envia dados UDP para um endereço específico
 * 
 * @param sock Socket UDP
 * @param data Ponteiro para dados
 * @param len Tamanho dos dados
 * @param remote_addr Endereço destino
 * @return NET_OK ou código de erro
 */
int32_t NetSocket_SendTo(
    NetSocket_t *sock,
    const uint8_t *data,
    uint16_t len,
    const NetAddr_t *remote_addr,
    uint32_t timeout_ms
);

/**
 * @brief Recebe dados do socket (modo polling)
 * 
 * @param sock Socket
 * @param buffer Buffer para armazenar dados
 * @param buffer_size Tamanho do buffer
 * @param received [out] Bytes efetivamente recebidos
 * @param remote_addr [out] Endereço de origem (pode ser NULL)
 * @return NET_OK ou código de erro
 * 
 * @note Alternativa ao callback para modo polling
 * @note Retorna NET_ERR_WOULD_BLOCK se não houver dados
 */
int32_t NetSocket_RecvFrom(
    NetSocket_t *sock,
    uint8_t *buffer,
    uint16_t buffer_size,
    NetAddr_t *remote_addr,
    uint32_t timeout_ms
);

/* =============================================================================
 * FUNÇÕES DE CONSULTA
 * ============================================================================= */

/**
 * @brief Obtém estado atual do socket
 * 
 * @param sock Socket
 * @return Estado atual
 */
NetSocketState_t NetSocket_GetState(const NetSocket_t *sock);

/**
 * @brief Verifica se socket está conectado
 * 
 * @param sock Socket
 * @return true se conectado (TCP ESTABLISHED)
 */
bool NetSocket_IsConnected(const NetSocket_t *sock);

/**
 * @brief Obtém porta local do socket
 * 
 * @param sock Socket
 * @return Porta local (0 se não bound)
 */
uint16_t NetSocket_GetLocalPort(const NetSocket_t *sock);

/**
 * @brief Obtém endereço remoto (TCP conectado ou último UDP recebido)
 * 
 * @param sock Socket
 * @param addr [out] Endereço remoto
 * @return NET_OK ou NET_ERR_NOT_CONNECTED
 */
NetErr_t NetSocket_GetRemoteAddr(const NetSocket_t *sock, NetAddr_t *addr);

/**
 * @brief Obtém quantidade de bytes disponíveis para leitura
 * 
 * @param sock Socket
 * @return Número de bytes disponíveis
 */
uint16_t NetSocket_GetAvailable(const NetSocket_t *sock);

/* =============================================================================
 * FUNÇÕES DE FECHAMENTO
 * ============================================================================= */

/**
 * @brief Fecha o socket (mas não o destrói)
 * 
 * @param sock Socket
 * @return NET_OK ou código de erro
 * 
 * @note Socket pode ser reaberto/reconfigurado após fechar
 * @note Para liberar completamente, usar NetSocket_Destroy()
 */
NetErr_t NetSocket_Close(NetSocket_t *sock);

/* =============================================================================
 * PROCESSAMENTO
 * ============================================================================= */

void NetSocket_ProcessAll(void);
NetSocket_t* NetSocket_GetByIndex(uint8_t index);
uint8_t NetSocket_GetCount(void);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_NET_SOCKET_H_ */

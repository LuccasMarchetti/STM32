/*
 * app_tcp_udp_example.c
 *
 *  Created on: Feb 23, 2026
 *      Author: System
 *
 * Exemplo de como implementar uma aplicação TCP/UDP geral
 * usando o stack de rede W5500
 */

#include "app_network.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ============================================================================= 
 * EXEMPLO 1: CLIENTE TCP (conecta a servidor e troca dados)
 * ============================================================================= */

/**
 * Exemplo: Conectar a um servidor TCP e enviar dados
 * 
 * int main(void) {
 *     Network_Init();
 *     Network_StartTask();
 *     
 *     // Aguardar até ter IP
 *     uint8_t ip[4];
 *     while (Network_GetIPAddress(ip) != 0 || ip[0] == 0) {
 *         osDelay(100);
 *     }
 *     
 *     // Alocarsocket TCP
 *     NetManager_t *mgr = Network_GetManager();
 *     NetSocket_t *sock = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_TCP);
 *     
 *     // Conectar a servidor (ex: Google DNS 8.8.8.8 porta 2323)
 *     NetAddr_t server_addr = {
 *         .ip = { .addr = {8, 8, 8, 8} },
 *         .port = 2323
 *     };
 *     
 *     if (NetSocket_Connect(sock, &server_addr) != NET_OK) {
 *         printf("Erro na conexão\n");
 *         return -1;
 *     }
 *     
 *     // Aguardar conexão estabelecer
 *     int retry = 100;
 *     while (!NetSocket_IsConnected(sock) && --retry > 0) {
 *         osDelay(100);
 *     }
 *     
 *     if (NetSocket_IsConnected(sock)) {
 *         // Enviar dados
 *         const char *msg = "Hello TCP Server!\r\n";
 *         NetSocket_Send(sock, (const uint8_t *)msg, strlen(msg), 0);
 *         
 *         // Receber resposta
 *         uint8_t rx_buf[256];
 *         int len = NetSocket_Recv(sock, rx_buf, sizeof(rx_buf), 0);
 *         if (len > 0) {
 *             printf("Recebido: %.*s\n", len, (char*)rx_buf);
 *         }
 *         
 *         // Fechar
 *         NetSocket_Close(sock);
 *         NetManager_FreeSocket(mgr, sock);
 *     }
 * }
 */

/* ============================================================================= 
 * EXEMPLO 2: SERVIDOR TCP (aguarda conexões e processa clientes)
 * ============================================================================= */

/**
 * Exemplo: Servidor TCP simples que responde a cliente
 * 
 * typedef struct {
 *     NetSocket_t *server_sock;
 *     NetSocket_t *client_sock;
 *     bool connected;
 * } SimpleTCPServer_t;
 * 
 * static SimpleTCPServer_t tcp_server = {0};
 * 
 * static void TCPServerTask(void) {
 *     NetManager_t *mgr = Network_GetManager();
 *     
 *     // Criar socket servidor (passivo)
 *     if (!tcp_server.server_sock) {
 *         tcp_server.server_sock = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_TCP);
 *         
 *         // Bind na porta 5000
 *         NetAddr_t server_addr = {
 *             .ip = { .addr = {0, 0, 0, 0} },  // Qualquer interface
 *             .port = 5000
 *         };
 *         
 *         if (NetSocket_Bind(tcp_server.server_sock, &server_addr) == NET_OK) {
 *             NetSocket_Listen(tcp_server.server_sock, 1);  // 1 cliente por vez
 *             printf("Servidor TCP aguardando na porta 5000...\n");
 *         }
 *         return;
 *     }
 *     
 *     // Verificar nova conexão
 *     if (!tcp_server.connected && 
 *         NetSocket_IsConnected(tcp_server.server_sock)) {
 *         
 *         tcp_server.client_sock = tcp_server.server_sock;  // Aceitar conexão
 *         tcp_server.connected = true;
 *         printf("Cliente conectado!\n");
 *         
 *         // Enviar mensagem de boas-vindas
 *         const char *welcome = "Welcome to STM32 TCP Server!\r\n";
 *         NetSocket_Send(tcp_server.client_sock, (const uint8_t *)welcome, 
 *                        strlen(welcome), 0);
 *     }
 *     
 *     // Processar dados do cliente
 *     if (tcp_server.connected && tcp_server.client_sock) {
 *         uint16_t available = NetSocket_GetAvailable(tcp_server.client_sock);
 *         
 *         if (available > 0) {
 *             uint8_t rx_buf[256];
 *             int len = NetSocket_Recv(tcp_server.client_sock, rx_buf, 
 *                                     sizeof(rx_buf) - 1, 0);
 *             if (len > 0) {
 *                 // Echo: enviar de volta
 *                 NetSocket_Send(tcp_server.client_sock, rx_buf, len, 0);
 *             }
 *         }
 *         
 *         // Verificar se cliente desconectou
 *         if (!NetSocket_IsConnected(tcp_server.client_sock)) {
 *             tcp_server.connected = false;
 *             NetSocket_Close(tcp_server.client_sock);
 *             printf("Cliente desconectado\n");
 *         }
 *     }
 * }
 */

/* ============================================================================= 
 * EXEMPLO 3: CLIENTE UDP (enviar/receber datagrams)
 * ============================================================================= */

/**
 * Exemplo: Cliente UDP simples (ex: servidor NTP modificado)
 * 
 * typedef struct {
 *     NetSocket_t *socket;
 *     NetAddr_t remote_addr;
 * } SimpleUDPClient_t;
 * 
 * static SimpleUDPClient_t udp_client = {0};
 * 
 * void UDPClient_Init(const char *hostname, uint16_t port) {
 *     NetManager_t *mgr = Network_GetManager();
 *     
 *     // Resolver hostname via DNS se necessário
 *     uint8_t remote_ip[4] = {0};
 *     
 *     if (hostname) {
 *         // Usar DNS para resolver
 *         DNS_Client_t *dns = Network_GetDNSClient();
 *         DNS_Query(dns, hostname, remote_ip, 5000);  // Resolver em 5 segundos
 *     }
 *     
 *     // Criar socket UDP
 *     udp_client.socket = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_UDP);
 *     
 *     // Bind em porta local (0 = qualquer)
 *     NetAddr_t local = { .ip = { .addr = {0,0,0,0} }, .port = 0 };
 *     NetSocket_Bind(udp_client.socket, &local);
 *     
 *     // Configurar endereço remoto
 *     udp_client.remote_addr.ip.addr[0] = remote_ip[0];
 *     udp_client.remote_addr.ip.addr[1] = remote_ip[1];
 *     udp_client.remote_addr.ip.addr[2] = remote_ip[2];
 *     udp_client.remote_addr.ip.addr[3] = remote_ip[3];
 *     udp_client.remote_addr.port = port;
 * }
 * 
 * int UDPClient_SendData(const uint8_t *data, uint16_t len) {
 *     if (!udp_client.socket) return -1;
 *     
 *     return NetSocket_SendTo(udp_client.socket, data, len, 
 *                            &udp_client.remote_addr, 0);
 * }
 * 
 * int UDPClient_RecvData(uint8_t *buffer, uint16_t max_len, 
 *                        NetAddr_t *src_addr) {
 *     if (!udp_client.socket) return -1;
 *     
 *     return NetSocket_RecvFrom(udp_client.socket, buffer, max_len, 
 *                              src_addr, 0);
 * }
 */

/* ============================================================================= 
 * EXEMPLO 4: SERVIDOR UDP (recebe e responde datagrams)
 * ============================================================================= */

/**
 * Exemplo: Servidor UDP simples (Echo server)
 * 
 * typedef struct {
 *     NetSocket_t *socket;
 *     uint16_t port;
 * } SimpleUDPServer_t;
 * 
 * static SimpleUDPServer_t udp_server = {0};
 * 
 * void UDPServer_Init(uint16_t port) {
 *     NetManager_t *mgr = Network_GetManager();
 *     
 *     // Criar socket UDP
 *     udp_server.socket = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_UDP);
 *     udp_server.port = port;
 *     
 *     // Bind na porta especificada
 *     NetAddr_t local = { 
 *         .ip = { .addr = {0,0,0,0} },  // Qualquer interface
 *         .port = port 
 *     };
 *     NetSocket_Bind(udp_server.socket, &local);
 *     
 *     printf("Servidor UDP aguardando na porta %u\n", port);
 * }
 * 
 * void UDPServer_Task(void) {
 *     if (!udp_server.socket) return;
 *     
 *     uint16_t available = NetSocket_GetAvailable(udp_server.socket);
 *     if (available == 0) return;
 *     
 *     // Receber datagram
 *     uint8_t rx_buf[512];
 *     NetAddr_t src_addr;
 *     int len = NetSocket_RecvFrom(udp_server.socket, rx_buf, 
 *                                 sizeof(rx_buf), &src_addr, 0);
 *     
 *     if (len > 0) {
 *         printf("Recebido %d bytes de %u.%u.%u.%u:%u\n", 
 *                len, src_addr.ip.addr[0], src_addr.ip.addr[1],
 *                src_addr.ip.addr[2], src_addr.ip.addr[3], src_addr.port);
 *         
 *         // Echo: enviar de volta para quem mandou
 *         NetSocket_SendTo(udp_server.socket, rx_buf, len, &src_addr, 0);
 *     }
 * }
 */

/* ============================================================================= 
 * EXEMPLO 5: USANDO CALLBACKS (processamento não-bloqueante)
 * ============================================================================= */

/**
 * Exemplo: Usar callbacks ao invés de polling
 * 
 * typedef struct {
 *     NetSocket_t *socket;
 *     char buffer[1024];
 *     bool data_ready;
 * } CallbackClient_t;
 * 
 * // Callback chamado quando dados chegam
 * static void RxCallback(NetSocket_t *sock, void *user_data) {
 *     CallbackClient_t *client = (CallbackClient_t *)user_data;
 *     
 *     uint16_t available = NetSocket_GetAvailable(sock);
 *     if (available > 0) {
 *         int len = NetSocket_Recv(sock, (uint8_t *)client->buffer, 
 *                                 sizeof(client->buffer) - 1, 0);
 *         if (len > 0) {
 *             client->buffer[len] = '\0';
 *             client->data_ready = true;
 *             printf("Dados recebidos via callback: %s\n", client->buffer);
 *         }
 *     }
 * }
 * 
 * // Inicializar com callback
 * void CallbackClient_Init(void) {
 *     CallbackClient_t *client = &(CallbackClient_t){0};
 *     NetManager_t *mgr = Network_GetManager();
 *     
 *     client->socket = NetManager_AllocSocket(mgr, NET_SOCKET_TYPE_TCP);
 *     
 *     // Registrar callback
 *     NetSocket_SetRxCallback(client->socket, RxCallback, client);
 *     
 *     // Conectar
 *     NetAddr_t addr = { .ip = { .addr = {8,8,8,8} }, .port = 80 };
 *     NetSocket_Connect(client->socket, &addr);
 * }
 */

/* ============================================================================= 
 * DICAS E BOAS PRÁTICAS
 * ============================================================================= */

/*
 * 1. POOL DE SOCKETS
 *    - NetManager aloca sockets dinamicamente (até 8 por padrão)
 *    - Sempre liberar sockets não usados com NetManager_FreeSocket()
 *    - Verificar retorno de NetManager_AllocSocket() para NULL
 *
 * 2. CICLOS DE CONEXÃO TCP
 *    a) NetSocket_Connect() - iniciar conexão
 *    b) Aguardar NetSocket_IsConnected() retornar true (pode levar alguns ms)
 *    c) Usar NetSocket_Send() / NetSocket_Recv()
 *    d) NetSocket_Close() quando terminar
 *
 * 3. OPERAÇÕES BLOQUEANTES vs NÃO-BLOQUEANTES
 *    - Por padrão, APIs usam flags=0 (bloqueante)
 *    - Pode usar callbacks com NetSocket_SetRxCallback() para não-bloqueante
 *    - Callbacks são chamados pela NetworkTask a cada 10ms
 *
 * 4. TRATAMENTO DE ERROS
 *    - Sempre verificar retorno de funções (NET_OK, NET_ERR_*)
 *    - NetSocket_GetAvailable() retorna 0 se nenhum dado
 *    - NetSocket_Recv() retorna -1 em erro
 *
 * 5. PERFORMANCE
 *    -  UDP é mais rápido que TCP (sem overhead de conexão)
 *    - Use UDP para dados não-críticos (NTP, DNS)
 *    - Use TCP para dados confiáveis que precisam llegar
 *
 * 6. MEMORY
 *    - Cada socket usa ~128 bytes
 *    - RX buffers são internos ao W5500 (2KB por socket)
 *    - Limitar número de conexões simultâneas
 *
 * 7. DNS RESOLUTION
 *    - Usar DNS_Client_t do stack para resolver hostnames
 *    - DNS é UDP, rápido (tipicamente <100ms)
 *    - Criar socket TCP após resolver DNS
 *
 * 8. TIMEOUT
 *    - Para conectar: aguardar IsConnected com timeout manual
 *    - Para dados: usar NetSocket_GetAvailable() em loop com delay
 *    - Evitar delays longos na NetworkTask (máx 10ms recomendado)
 */

/* Compilação: este arquivo é apenas referência. Para usar, copiar exemplos
 * para sua aplicação e adaptar conforme necessário.
 */

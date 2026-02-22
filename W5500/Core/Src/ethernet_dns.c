/*
 * ethernet_dns.c
 *
 *  Created on: Feb 21, 2026
 *      Author: Luccas
 */
#include "ethernet_dns.h"
#include "w5500.h"
#include <stdbool.h>

uint16_t build_dns_query(uint8_t *buffer, const char *domain) {
    uint8_t *ptr = buffer;

    // 1. Cabeçalho (12 bytes)
    *ptr++ = 0x12; *ptr++ = 0x34; // Transaction ID: Um número aleatório (ex: 0x1234)

    // Flags: 0x0100 (Standard Query, Recursion Desired)
    // "Recursion Desired" diz pro roteador: "Se você não souber o IP, pergunte pra outro servidor por mim"
    *ptr++ = 0x01; *ptr++ = 0x00;

    *ptr++ = 0x00; *ptr++ = 0x01; // Questions: 1
    *ptr++ = 0x00; *ptr++ = 0x00; // Answers: 0
    *ptr++ = 0x00; *ptr++ = 0x00; // Authority RRs: 0
    *ptr++ = 0x00; *ptr++ = 0x00; // Additional RRs: 0

    // 2. O Nome que estamos procurando (QNAME)
    ptr = format_dns_name(ptr, domain);

    // 3. Tipo e Classe
    *ptr++ = 0x00; *ptr++ = 0x01; // QTYPE: Tipo A (Queremos o IP)
    *ptr++ = 0x00; *ptr++ = 0x01; // QCLASS: IN (Internet)

    // Retorna o tamanho da mensagem para você enviar via W5500
    return (uint16_t)(ptr - buffer);
}

void DNS_FSM(W5500_Driver_t *drv) {
    DNS_Control_t *dns = &drv->dns;

    switch (dns->state) {
        case DNS_IDLE:
            // Configurações iniciais feitas na transição do DHCP
            break;

        case DNS_SEND_QUERY:
            if (dns->socket == 0xFF) {
                // Abre um socket UDP dinâmico (ex: porta 50053)
                dns->socket = W5500_OpenSocket(drv, W5500_Sn_MR_UDP, 50053);
            }

            // Incrementa o ID da transação
            dns->transaction_id++;

            // Monta e envia o pacote DNS (usando aquela função build_dns_query)
            uint16_t len = build_dns_query(drv->sockets[dns->socket].tx_buf, dns->target_domain);
            W5500_SetSocketDestIP(drv, dns->socket, dns->dns_server);
            W5500_SetSocketDestPort(drv, dns->socket, 53);
            W5500_SocketSend(drv, dns->socket, drv->sockets[dns->socket].tx_buf, len);

            dns->t_start = HAL_GetTick();
            dns->state = DNS_WAIT_RESPONSE;
            break;

        case DNS_WAIT_RESPONSE:
            if ((HAL_GetTick() - dns->t_start) >= DNS_TIMEOUT_MS) {
                dns->retries++;
                if (dns->retries >= DNS_MAX_RETRIES) {
                    dns->state = DNS_ERROR; // Falhou de vez
                    W5500_CloseSocket(drv, dns->socket);
                    dns->socket = 0xFF;
                } else {
                    dns->state = DNS_SEND_QUERY; // Tenta de novo
                }
            }
            break;

        case DNS_DONE:
            // Fica aqui aguardando. O RX_Handle que joga pra cá!
            break;

        case DNS_ERROR:
            // Tratamento de erro (ex: tentar outro IP de DNS)
            break;
    }
}

bool DNS_ParseResponse(uint8_t *buf, uint16_t len, uint8_t *resolved_ip, uint16_t expected_tx_id) {
    if (len < 12) return false; // Pacote muito pequeno para ser DNS

    // 1. Verifica Transaction ID (A resposta tem que bater com a pergunta que fizemos)
    uint16_t rx_tx_id = (buf[0] << 8) | buf[1];
    if (rx_tx_id != expected_tx_id) return false;

    // 2. Verifica Flags (Byte 2 e 3)
    // buf[2] & 0x80 deve ser 1 (Indica que é uma Resposta/Response)
    // buf[3] & 0x0F deve ser 0 (Indica que não houve erros - No Error)
    if ((buf[2] & 0x80) == 0 || (buf[3] & 0x0F) != 0) return false;

    // 3. Pega a quantidade de Perguntas e Respostas
    uint16_t qdcount = (buf[4] << 8) | buf[5];
    uint16_t ancount = (buf[6] << 8) | buf[7];

    if (ancount == 0) return false; // O servidor respondeu, mas não achou nenhum IP

    uint16_t idx = 12; // O ponteiro de leitura começa logo após os 12 bytes de cabeçalho

    // 4. PULA a seção de Perguntas (Echo)
    for (uint16_t i = 0; i < qdcount; i++) {
        // Pula o nome (salta palavra por palavra até achar o byte 0x00)
        while (idx < len && buf[idx] != 0) {
            idx += buf[idx] + 1; // Pula o byte de tamanho + os caracteres
        }
        idx += 1; // Pula o próprio byte 0x00
        idx += 4; // Pula QTYPE (2 bytes) e QCLASS (2 bytes)
    }

    // 5. Analisa a seção de Respostas (Answer Section)
    for (uint16_t i = 0; i < ancount; i++) {
        if (idx >= len) return false;

        // Pula o NOME da resposta (Lidando com a Compressão DNS)
        // Se os 2 bits mais significativos forem 11 (0xC0 em Hexa), é um ponteiro de 2 bytes!
        if ((buf[idx] & 0xC0) == 0xC0) {
            idx += 2; // Pula os 2 bytes do ponteiro
        } else {
            // Se não for ponteiro, é uma string normal (raro em respostas, mas seguimos a regra)
            while (idx < len && buf[idx] != 0) {
                idx += buf[idx] + 1;
            }
            idx += 1;
        }

        // Garante que não vamos ler fora da memória
        if (idx + 10 > len) return false;

        // Lê os detalhes do Registro
        uint16_t type = (buf[idx] << 8) | buf[idx+1];
        idx += 2; // Pula Type
        idx += 2; // Pula Class
        idx += 4; // Pula TTL (Tempo de Vida)

        uint16_t data_len = (buf[idx] << 8) | buf[idx+1];
        idx += 2; // Pula Data Length

        // LÓGICA FINAL: É do tipo A (0x0001 = IPv4) e tem exatos 4 bytes?
        if (type == 0x0001 && data_len == 4) {
            resolved_ip[0] = buf[idx];
            resolved_ip[1] = buf[idx+1];
            resolved_ip[2] = buf[idx+2];
            resolved_ip[3] = buf[idx+3];
            return true; // SUCESSO! Pegamos o IP do Servidor NTP!
        }

        // Se a resposta for de outro tipo (ex: CNAME - Apelido),
        // nós pulamos os dados dela e o loop tenta ler a próxima resposta (ancount).
        idx += data_len;
    }

    return false; // Varremos todas as respostas e não achamos nenhum IPv4
}

void DNS_HandleRx(W5500_Driver_t *drv, uint8_t *buf, uint16_t len) {
    DNS_Control_t *dns = &drv->dns;

    // Verifica se o Transaction ID bate e faz o parse da resposta
    // (Vamos escrever a lógica de parse do DNS na próxima etapa)
    bool sucesso = DNS_ParseResponse(buf, len, dns->resolved_ip, 0x1234);

    if (sucesso) {
        // 1. Encerra o DNS
        dns->state = DNS_DONE;
        W5500_CloseSocket(drv, dns->socket);
        dns->socket = 0xFF;

        // 2. Prepara e Acorda o NTP!
        drv->net_state = SYS_NET_NTP_RUNNING;

        // Copia o IP que acabamos de descobrir para a FSM do NTP
        memcpy(drv->ntp.server_ip, dns->resolved_ip, 4);

        drv->ntp.retries = 0;
        drv->ntp.state = NTP_SEND_REQUEST;
    }
}

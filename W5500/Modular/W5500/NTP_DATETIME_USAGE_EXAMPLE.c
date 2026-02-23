/*
 * NTP_DATETIME_USAGE_EXAMPLE.c
 * 
 * Exemplo de como usar as novas funções de data/hora do NTP
 * 
 * Problemas que foram corrigidos:
 * 1. ❌ Campo "microseconds" sempre era 0 (NTP não fornece)
 * 2. ❌ Segundos crescendo 20000x mais rápido (atualização duplicada)
 * 3. ❌ Sem formato legível de data/hora (só tinha timestamp Unix)
 * 
 * Soluções adicionadas:
 * ✅ Removido campo microseconds
 * ✅ Corrigido incremento de tempo (1 segundo por vez)
 * ✅ Adicionadas funções de conversão para data/hora
 * ✅ Função para formatação legível (YYYY-MM-DD HH:MM:SS)
 */

#include "app_network.h"
#include "ntp_service.h"
#include <stdio.h>
#include <string.h>

/* =============================================================================
 * EXEMPLO 1: Obter hora atual em formato legível (GMT)
 * ============================================================================= */

void Example_GetCurrentTime_GMT(void) {
    NTP_Client_t *ntp = Network_GetNTPClient();
    
    if (!NTP_IsSynchronized(ntp)) {
        printf("NTP não sincronizado ainda\n");
        return;
    }
    
    // Obter data/hora em GMT
    NTP_DateTime_t datetime;
    if (NTP_GetDateTime(ntp, &datetime, 0) == NET_OK) {
        // Opção 1: Usar função de formatação
        char time_str[30];
        NTP_DateTimeToString(&datetime, time_str, sizeof(time_str));
        printf("Hora (GMT): %s\n", time_str);
        
        // Opção 2: Exibir campo a campo
        printf("Data: %04d-%02d-%02d, Hora: %02d:%02d:%02d\n",
               datetime.year, datetime.month, datetime.day,
               datetime.hour, datetime.minute, datetime.second);
    }
}

/* =============================================================================
 * EXEMPLO 2: Obter hora com offset de timezone (ex: GMT-3 Brasil)
 * ============================================================================= */

void Example_GetCurrentTime_BrasilGMT3(void) {
    NTP_Client_t *ntp = Network_GetNTPClient();
    
    if (!NTP_IsSynchronized(ntp)) {
        printf("NTP não sincronizado\n");
        return;
    }
    
    // Timezone: GMT-3 = -3 * 3600 = -10800 segundos
    NTP_DateTime_t datetime;
    if (NTP_GetDateTime(ntp, &datetime, -3 * 3600) == NET_OK) {
        char time_str[30];
        NTP_DateTimeToString(&datetime, time_str, sizeof(time_str));
        printf("Hora (GMT-3 Brasil): %s\n", time_str);
    }
}

/* =============================================================================
 * EXEMPLO 3: Verificar dia da semana
 * ============================================================================= */

void Example_GetDayOfWeek(void) {
    NTP_Client_t *ntp = Network_GetNTPClient();
    
    if (!NTP_IsSynchronized(ntp)) {
        printf("NTP não sincronizado\n");
        return;
    }
    
    NTP_DateTime_t datetime;
    if (NTP_GetDateTime(ntp, &datetime, 0) == NET_OK) {
        const char *days[] = {"Domingo", "Segunda", "Terça", "Quarta", "Quinta", "Sexta", "Sábado"};
        printf("Hoje é %s\n", days[datetime.dow]);
    }
}

/* =============================================================================
 * EXEMPLO 4: Usar em Telnet (CLI) para exibir hora ao user
 * ============================================================================= */

// Função para integrar em seu handler de telnet/CLI
void CLI_Command_Time(void) {
    NTP_Client_t *ntp = Network_GetNTPClient();
    
    if (!NTP_IsSynchronized(ntp)) {
        printf("ERRO: NTP não sincronizado\n");
        printf("  Aguarde %d segundos e tente novamente\n", NTP_SYNC_INTERVAL_MS / 1000);
        return;
    }
    
    // Obter em GMT
    NTP_DateTime_t datetime_gmt;
    NTP_GetDateTime(ntp, &datetime_gmt, 0);
    
    // Obter em GMT-3 (Brasil)
    NTP_DateTime_t datetime_br;
    NTP_GetDateTime(ntp, &datetime_br, -3 * 3600);
    
    char gmt_str[30], br_str[30];
    NTP_DateTimeToString(&datetime_gmt, gmt_str, sizeof(gmt_str));
    NTP_DateTimeToString(&datetime_br, br_str, sizeof(br_str));
    
    printf("Hora do Sistema:\n");
    printf("  GMT:     %s\n", gmt_str);
    printf("  Brasil:  %s\n", br_str);
    
    // Mostrar info adicional
    NTP_Timestamp_t ts;
    if (NTP_GetTime(ntp, &ts) == NET_OK) {
        printf("  Unix:    %lu segundos desde 1970\n", ts.seconds);
    }
}

/* =============================================================================
 * EXEMPLO 5: Função para uptime/tempo online
 * ============================================================================= */

void Example_ShowUptime(uint32_t startup_unix_time) {
    NTP_Client_t *ntp = Network_GetNTPClient();
    
    if (!NTP_IsSynchronized(ntp)) {
        printf("NTP não sincronizado\n");
        return;
    }
    
    NTP_Timestamp_t now;
    if (NTP_GetTime(ntp, &now) == NET_OK) {
        uint32_t uptime_seconds = now.seconds - startup_unix_time;
        
        uint32_t days = uptime_seconds / 86400;
        uint32_t hours = (uptime_seconds % 86400) / 3600;
        uint32_t mins = (uptime_seconds % 3600) / 60;
        uint32_t secs = uptime_seconds % 60;
        
        printf("Uptime: %lu dias, %u horas, %u minutos, %u segundos\n",
               days, hours, mins, secs);
    }
}

/* =============================================================================
 * EXEMPLO 6: Estrutura de inicialização com timezone configurável
 * ============================================================================= */

typedef struct {
    NTP_Client_t *ntp;
    int32_t timezone_offset_sec;  // ex: -3*3600 para GMT-3
    const char *timezone_name;    // ex: "America/Sao_Paulo"
} SystemTime_t;

// Instância global
static SystemTime_t g_system_time = {
    .timezone_offset_sec = -3 * 3600,  // Brasil GMT-3
    .timezone_name = "America/Sao_Paulo"
};

// Função para obter hora local em qualquer lugar do código
void GetLocalTime(NTP_DateTime_t *datetime) {
    if (g_system_time.ntp && NTP_IsSynchronized(g_system_time.ntp)) {
        NTP_GetDateTime(g_system_time.ntp, datetime, g_system_time.timezone_offset_sec);
    } else {
        memset(datetime, 0, sizeof(NTP_DateTime_t));
    }
}

// Função para exibir hora local
void PrintLocalTime(void) {
    NTP_DateTime_t datetime;
    GetLocalTime(&datetime);
    
    if (datetime.year == 0) {
        printf("[SEM HORA]\n");
    } else {
        char buf[30];
        NTP_DateTimeToString(&datetime, buf, sizeof(buf));
        printf("[%s %s]\n", buf, g_system_time.timezone_name);
    }
}

/* =============================================================================
 * RESUMO DE MUDANÇAS NA API
 * ============================================================================= */

/*
ANTES (❌ Problemas):
- NTP_Timestamp_t tinha campo unused "microseconds" (sempre 0)
- Segundos cresciam muito rápido (acúmulo de atualizações)
- Sem funcionalidade de data/hora - só tinha Unix timestamp

DEPOIS (✅ Soluções):
1. Removido campo microseconds
   - Campo agora é apenas uint32_t seconds
   
2. Corrigido incremento de tempo
   - Agora incrementa apenas 1 segundo por ciclo da task
   - Usa campo "last_second_update" para rastrear
   
3. Novas funções de conversão:
   - NTP_UnixToDateTime() - converte timestamp para data/hora
   - NTP_GetDateTime() - obtém data/hora com timezone
   - NTP_DateTimeToString() - formata como "YYYY-MM-DD HH:MM:SS"

USO BÁSICO:

    NTP_DateTime_t dt;
    NTP_GetDateTime(ntp, &dt, -3*3600);  // GMT-3
    printf("%04d-%02d-%02d %02d:%02d:%02d\n", 
           dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);

*/


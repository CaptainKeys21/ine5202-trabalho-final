#define _POSIX_C_SOURCE 199309L // importante para CLOCK_REALTIME em time.h
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>


// Tamanho máximo do timestamp formatado.
#define TIMESTAMP_SIZE 20 
// Tamanho máximo do buffer para a mensagem formatada.
#define BUFFER_SIZE 512

void printf_timestamped(const char* format, ...) {
    time_t rawtime;
    struct tm *info;
    char timestamp_buffer[TIMESTAMP_SIZE];
    char message_buffer[BUFFER_SIZE];
    
    // 1. Obter o tempo atual em segundos desde a época (epoch)
    time(&rawtime);
    
    // 2. Converter para a estrutura de tempo local
    info = localtime(&rawtime);

    // 3. Formatar o tempo em uma string (exemplo: YYYY-MM-DD HH:MM:SS)
    // %Y: ano com século | %m: mês (01-12) | %d: dia do mês (01-31)
    // %H: hora (00-23) | %M: minuto (00-59) | %S: segundo (00-60)
    strftime(timestamp_buffer, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", info);

    // Formatar a mensagem original (como um printf normal)
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, BUFFER_SIZE, format, args);
    va_end(args);

    // Imprimir o timestamp e a mensagem juntos
    printf("[%s] %s", timestamp_buffer, message_buffer);
}

struct timespec get_abs_timeout(int seconds) {
    struct timespec ts;

    // Obter o tempo atual
    clock_gettime(CLOCK_REALTIME, &ts);

    // Calcular o tempo absoluto para o timeout
    ts.tv_sec += seconds;

    return ts;
}

char* create_id(char prefix, int index) {
    if (index < 0) return NULL;

    // Determina o número de dígitos do índice.
    // Usamos snprintf para determinar o comprimento sem realmente escrever a string.
    // O buffer temporário 'temp' precisa ser grande o suficiente para o número máximo de dígitos de 'int'.
    char temp[32]; 
    
    // snprintf retorna o número de caracteres que SERIAM escritos (excluindo o '\0').
    // Se index = 123, snprintf(temp, 32, "%d", 123) retorna 3.
    int num_digits = snprintf(temp, 32, "%d", index);

    // Calcula o tamanho total necessário.
    // O comprimento final da string será:
    // 2 (para "{prefix}-") 
    // + num_digits (para o valor do índice)
    // + 1 (para o terminador nulo '\0')
    int total_len = 2 + num_digits + 1; 

    // Aloca a memória exata.
    char* res = (char*)malloc(total_len);
    
    if (res == NULL) return NULL; 

    // Formata a string e retorna.
    // Usamos o tamanho exato no snprintf para garantir que não haja estouro de buffer,
    // embora `total_len` seja o tamanho exato da alocação.
    snprintf(res, total_len, "%c-%d", (int)prefix, index);

    return res;
}
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
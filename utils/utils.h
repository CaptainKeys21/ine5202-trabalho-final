#ifndef UTILS_H
#define UTILS_H

/**
 * @brief Cria um id com um prefixo determinado no formato {prefix}-{index}
 * 
 * @param prefix 
 * @param index 
 * @return char* 
 */
char* create_id(char prefix, int index);

/**
 * @brief Imprime uma mensagem no formato timestampado [HH:MM:SS.mmm] mensagem
 * 
 * @param format 
 */
void printf_timestamped(const char* format, ...);

/**
 * @brief Obtém um tempo absoluto para timeout baseado no tempo atual + segundos fornecidos
 * 
 * @param seconds 
 * @return struct timespec 
 */
struct timespec get_abs_timeout(int seconds);

#endif

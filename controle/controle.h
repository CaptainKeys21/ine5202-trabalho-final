#ifndef CONTROLE_H
#define CONTROLE_H

#include "setor.h"

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>


typedef struct controle {
    size_t num_aeronaves;
    size_t num_setores;
    
    // Matrizes do Banqueiro (alocadas dinamicamente)
    int **max;         
    int **allocation;  
    int **need;        
    int *available;   // 1 para disponível, 0 para alocado

    pthread_mutex_t banker_lock; // Protege as matrizes do Banqueiro
} controle_t;

void init_controle(controle_t* controle, size_t num_aeronaves, size_t num_setores);

void destroy_controle(controle_t* controle);

/**
 * @brief Algoritimo de segurança do banqueiro (Executado SOMENTE sob banker_lock)
 * 
 * @param ctrl a struct do banqueiro
 * @return true 
 * @return false 
 */
bool is_safe(controle_t* ctrl);

/**
 * @brief Set the or tenta conceder seguro object
 * 
 * @param ctrl 
 * @param aero_id 
 * @param setor_idx 
 * @return true 
 * @return false 
 */
bool setor_tenta_conceder_seguro(controle_t* ctrl, int aero_id, int setor_idx);

/**
 * @brief Libera o recurso alocado para a aeronave no setor
 * 
 * @param ctrl 
 * @param aero_id 
 * @param setor_idx 
 */
void liberar_recurso_banqueiro(controle_t* ctrl, int aero_id, int setor_idx);

#endif
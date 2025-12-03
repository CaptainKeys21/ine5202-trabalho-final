#ifndef CONTROLE_H
#define CONTROLE_H

#include "setor.h"

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>

typedef struct setor setor_t;
typedef struct aeronave aeronave_t;

typedef struct controle {
    size_t num_aeronaves;
    aeronave_t* aeronaves; // Ponteiro para as aeronaves gerenciadas

    size_t num_setores;
    setor_t* setores; // Ponteiro para os setores gerenciados
    
    // Matrizes do Banqueiro (alocadas dinamicamente)
    int** max;         
    int** allocation;  
    int** need;        
    int* available;   // 1 para disponível, 0 para alocado

    pthread_mutex_t banker_lock; // Protege as matrizes do Banqueiro

    pthread_cond_t new_request_cond; // Condição para novas solicitações
} controle_t;

void init_controle(controle_t* controle, size_t num_aeronaves, size_t num_setores);

void destroy_controle(controle_t* controle);

/**
 * @brief Thread de controle do banqueiro
 * 
 * @param arg ponteiro para a struct controle_t
 * @return void* 
 */
void* banqueiro_thread(void* arg);

/**
 * @brief Algoritimo de segurança do banqueiro (Executado SOMENTE sob banker_lock)
 * 
 * @param ctrl a struct do banqueiro
 * @param temp_available vetor temporário de recursos disponíveis
 * @param temp_allocation matriz temporária de alocações
 * @param temp_need matriz temporária de necessidades
 * @return true 
 * @return false 
 */
bool is_safe(controle_t* ctrl, int temp_available[], int temp_allocation[][ctrl->num_setores], int temp_need[][ctrl->num_setores]);

/**
 * @brief Set the or tenta conceder seguro object
 * 
 * @param ctrl 
 * @param aero_id 
 * @param setor_idx 
 * @return true 
 * @return false 
 */
bool setor_tenta_conceder_seguro(controle_t* ctrl, int aero_idx, int setor_destino_idx, int setor_origem_idx);

/**
 * @brief Libera o recurso alocado para a aeronave no setor
 * 
 * @param ctrl 
 * @param aero_id 
 * @param setor_idx 
 */
void liberar_recurso_banqueiro(controle_t* ctrl, int aero_id, int setor_idx);

/**
 * @brief Realiza um rollback forçado para liberar recursos e evitar deadlock
 * 
 * @param ctrl 
 * @param aeronave_solicitante 
 */
void realizar_rollback_banqueiro(controle_t* ctrl, aeronave_t* aeronave_solicitante);

/**
 * @brief Verifica se ainda existe alguma aerothread viva
 * 
 * @param ctrl 
 * @return true 
 * @return false 
 */
bool existe_aerothread_alive(controle_t* ctrl);

/**
 * @brief Força a liberação de um setor por uma aeronave, colocando-a de volta na fila
 * 
 * @param ctrl 
 * @param aeronave 
 * @param setor 
 */
void forcar_liberacao_setor(controle_t* ctrl, aeronave_t* aeronave, setor_t* setor);

/**
 * @brief Verifica se o sistema estaria em um estado seguro após a liberação total forçada de A_alocada e concessão para A_solicitante
 * 
 * @param ctrl 
 * @param A_alocada Aeronave que irá liberar todos os seus recursos
 * @param setor_solicitado Setor que A_solicitante deseja alocar
 * @param A_solicitante Aeronave que deseja alocar o setor
 * @return true 
 * @return false
 */
bool is_safe_after_release_total(controle_t* ctrl, aeronave_t* A_alocada, setor_t* setor_solicitado, aeronave_t* A_solicitante);

/**
 * @brief Imprime o estado atual das matrizes do banqueiro
 * 
 * @param ctrl 
 */
void imprimir_estado_banqueiro(controle_t* ctrl);

#endif
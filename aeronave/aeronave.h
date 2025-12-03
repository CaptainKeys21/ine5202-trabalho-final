#ifndef AERONAVE_H
#define AERONAVE_H

#include "rota.h"
#include "setor.h"
#include <pthread.h>

typedef struct rota rota_t;

/**
 * @brief Representa uma aeronave
 * 
 * @param id Identificação da nave
 * @param prioridade Prioridade da nave no setor, quanto maior mais prioridade
 * @param rota A rota que a nave deve percorrer
 * @param aero_index O ID da aeronave na matriz do banqueiro
 * @param current_setor Ponteiro para o setor onde a aeronave está atualmente
 * @param finished Indica se a aeronave já concluiu sua rota
 * @param lock Mutex para proteger o acesso à variável finished e current_setor
 */
typedef struct aeronave {
    char* id;
    unsigned int prioridade;
    rota_t rota;
    int aero_index;
    bool finished;
    long long espera_total_ns;
    setor_t* current_setor;
    pthread_mutex_t lock;
} aeronave_t;

/**
 * @brief Representa o resultado da simulação para uma aeronave
 * 
 * @param id Identificação da nave
 * @param media_espera_ms Média de tempo de espera em milissegundos
 */
typedef struct {
    char* id;
    double media_espera;
} resultado_aeronave_t;

/**
 * @brief Inicializa os aeronaves em uma lista dinâmica
 * 
 * @param aeronaves lista para ser inicializada
 * @param aeronaves_len tamanho da lista
 */
void init_aeronaves(aeronave_t* aeronaves, size_t aeronaves_len);

/**
 * @brief Libera todos os recursos internos 
 * de cada setor em um array e, em seguida, libera o próprio array.
 *
 * @param aeronaves Ponteiro para o array dinâmico de estruturas aeronave_t.
 * @param aeronaves_len O número de elementos (aeronaves) no array.
 */
void destroy_aeronaves(aeronave_t* aeronaves, size_t aeronaves_len);

/**
 * @brief Função que será executado em thread onde a aeronave irá executar suas rotinas
 * 
 * Rotina de execução:
 * 1. *Solicitar* acesso ao próximo setor de sua rota (inserir a aeronave na fila do setor);
 * 2. *Aguardar* a liberação do setor de destino;
 * 3. *Adquirir* o acesso ao setor de destino;
 * 4. *Liberar* o acesso do setor em que se encontrava anteriormente (setor de origem);
 * 5. *Simular vôo no setor* utilizando sleep() por um tempo aleatório;
 * 6. *Repetir* os passos até o final da rota
 * 
 * @param arg 
 * @return void* 
 */
void* aeronave_thread(void* arg);
void usar_setor(aeronave_t* aeronave);
#endif
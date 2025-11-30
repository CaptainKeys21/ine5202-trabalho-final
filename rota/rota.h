#ifndef ROTA_H
#define ROTA_H

#include "setor.h"
#include <unistd.h>

typedef struct setor setor_t;

/**
 * @brief Representa um nó da rota
 * @param setor Ponteiro para o setor desse nó
 * @param next Ponteiro para o próximo nó
 */
typedef struct rota_node {
    setor_t* setor;
    struct rota_node* next;
} rota_node_t;

/**
 * @brief Representa uma lista encadeada simples que possui o caminho que o avião fará
 * 
 * @param head Ponteiro para o primeiro da fila
 * @param tail Ponteiro para o ultimo da fila
 * @param curr É um ponteiro que aponta para um item arbitrário da lista (é inicializado com `head`)
 */
typedef struct rota {
    rota_node_t* curr;
    rota_node_t* head;
    rota_node_t* tail;
    size_t len;
} rota_t;

/**
 * @brief Inicializa uma rota com setores randômicos.
 * 
 * @param setores Lista de setores disponíveis para montar as rotas
 * @param setores_len Tamanho da lista de setores disponíveis
 * @param rota_len Tamanho desejado para a rota
 * 
 * @return rota_t com setores
 */
rota_t criar_rota(setor_t* setores, size_t setores_len, size_t rota_len);

/**
 * @brief Deleta rota e libera o espaço em memória usado
 * 
 * @param rota Rota a ser destruída
 */
void destruir_rota(rota_t rota);

/** 
 * @brief Retorna o próximo setor da rota e avança o ponteiro interno `curr`
 * 
 * @param rota Rota alvo
 * @return setor_t* Próximo setor da rota ou NULL se não houver mais setores
 */
setor_t* rota_next_setor(rota_t *rota);
#endif
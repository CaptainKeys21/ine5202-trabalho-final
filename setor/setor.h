#ifndef SETOR_H
#define SETOR_H

#include "controle.h"

#include <pthread.h>
#include <semaphore.h>

typedef struct aeronave aeronave_t;
typedef struct controle controle_t;

/**
 * @brief Representação de um setor que será usado por uma aeronave (recurso compartilhado)
 * 
 * @param id identificação unica do setor
 * @param sem semáforo do setor
 * @param lock lock do setor
 * @param fila fila de aeronaves para entrarem no setor
 * @param fila_len tamanho da fila de aeronaves
 */
typedef struct setor {
    char* id;
    sem_t sem;
    pthread_mutex_t lock;
    aeronave_t* fila;
    size_t fila_len;

    controle_t* controle; // Ponteiro para o controle global
    // Usaremos a COND_T para acordar a aeronave na fila quando um setor for liberado
    pthread_cond_t setor_disponivel_cond; 
    
    // O ID do setor na matriz do banqueiro (0 a N-1)
    int setor_index;
} setor_t;

/**
 * @brief Inicializa os setores em uma lista dinâmica
 * 
 * @param setores lista para ser inicializada
 * @param setores_len tamanho da lista
 */
void init_setores(setor_t* setores, size_t setores_len, controle_t* controle);

/**
 * @brief Libera todos os recursos internos (ID, semáforo, mutex, fila) 
 * de cada setor em um array e, em seguida, libera o próprio array.
 *
 * @param setores Ponteiro para o array dinâmico de estruturas setor_t.
 * @param setores_len O número de elementos (setores) no array.
 */
void destroy_setores(setor_t* setores, size_t setores_len);

/**
 * @brief Set the or solicitar entrada object
 * 
 * @param setor 
 * @param aeronave 
 */
void setor_solicitar_entrada(setor_t *setor, aeronave_t *aeronave);

/**
 * @brief Set the or liberar saida object
 * 
 * @param setor 
 * @param aeronave 
 */
void setor_liberar_saida(setor_t *setor, aeronave_t *aeronave);

/**
 * @brief Adiciona uma aeronave a fila do setor
 * 
 * @param setor setor alvo
 * @param aeronave aeronave a ser adicionado
 */
void entrar_fila(setor_t* setor, aeronave_t* aeronave);

/**
 * @brief Remove uma aeronave da fila do setor
 * 
 * @param setor setor alvo
 * @param aeronave aeronave a ser removido
 */
void sair_fila(setor_t* setor, aeronave_t* aeronave);

/**
 * @brief Pega a próxima aeronave da fila com base na prioridade
 * 
 * @param setor setor a ser retirado a aeronave
 * @return aeronave_t* aeronave que entrará no setor
 */
aeronave_t* proximo(setor_t* setor);

#endif

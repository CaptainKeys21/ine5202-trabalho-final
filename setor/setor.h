#ifndef SETOR_H
#define SETOR_H

#include <aeronave.h>
#include <semaphore.h>

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
} setor_t;

/**
 * @brief Inicializa os setores em uma lista dinâmica
 * 
 * @param setores lista para ser inicializada
 * @param setores_len tamanho da lista
 */
void init_setores(setor_t* setores, size_t setores_len);

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

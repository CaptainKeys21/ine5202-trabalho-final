#include "setor.h"
#include "aeronave.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void init_setores(setor_t* setores, size_t setores_len) {
    for (size_t i = 0; i < setores_len; i++) {
        setores[i].id = gerar_setor_id(i);
        sem_init(&setores[i].sem, 0, 1);
        setores[i].fila = NULL;
        setores[i].fila_len = 0;
        pthread_mutex_init(&setores[i].lock, NULL);
    }
}

void entrar_fila(setor_t* setor, aeronave_t* aeronave) {
    printf("Tentando adicionar aeronave %s na fila do setor %s", aeronave->id, setor->id);
    pthread_mutex_lock(&setor->lock);
    size_t new_len = setor->fila_len + 1;
    realloc(setor->fila, new_len * sizeof(aeronave));
    if (setor->fila == NULL) {
        printf("Erro de realloc ao adicionar aeronave %s na fila do setor %s", aeronave->id, setor->id);
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    setor->fila_len = new_len;
    printf("Nova aeronave %s adicionada a fila do setor %s", aeronave->id, setor->id);

    pthread_mutex_unlock(&setor->lock);
}

void sair_fila(setor_t* setor, aeronave_t* aeronave) {
    pthread_mutex_lock(&setor->lock);

    size_t aero_index = -1;
    for (size_t i = 0; i < setor->fila_len; i++) {
        if(setor->fila[i].id == aeronave->id) {
            aero_index = i;
        }
    }

    if (aero_index == -1) {
        printf("Aeronave %s não encontrada na fila do setor %s", aeronave->id, setor->id);
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    for (size_t i = aero_index; i < setor->fila_len - 1; i++) {
        setor->fila[i] = setor->fila[i + 1];
    }

    size_t new_len = setor->fila_len - 1;
    setor_t* tmp = setor->fila;
    realloc(setor->fila, new_len * sizeof(aeronave));
    if (setor->fila == NULL && new_len > 0) {
        printf("Erro ao remover aeronave %s da fila do setor %s", aeronave->id, setor->id);
        setor->fila = tmp;
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    setor->fila_len = new_len;
    printf("Aeronave %s removida da fila do setor %s", aeronave->id, setor->id);

    pthread_mutex_unlock(&setor->lock);
}

aeronave_t* proximo(setor_t* setor) {
    pthread_mutex_lock(&setor->lock);

    aeronave_t* escolhida = NULL;
    unsigned int maior_prioridade = 0;

    for (size_t i = 0; i < setor->fila_len; i++) {
        if (setor->fila[i].prioridade > maior_prioridade) {
            maior_prioridade = setor->fila[i].prioridade;
            escolhida = &setor->fila[i];
        }
    }

    pthread_mutex_unlock(&setor->lock);
    return escolhida;
}

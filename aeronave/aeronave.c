#include "aeronave.h"
#include "rota.h"
#include "setor.h"
#include "utils.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void* aeronave_thread(void* arg) {
    aeronave_t* aero = (aeronave_t *)arg;
    
    setor_t* setor_alvo = NULL;
    setor_t* setor_prev = NULL;
    
    // O loop continua enquanto houver um próximo setor na rota
    while ((setor_alvo = rota_next_setor(&aero->rota)) != NULL) {
        // Solicita o próximo setor
        // A aeronave espera DENTRO desta função se for negada.
        printf_timestamped("[AERONAVE %s]: SOLICITANDO ENTRADA no Setor %s. Prioridade: %u\n", aero->id, setor_alvo->id, aero->prioridade);
        setor_solicitar_entrada(setor_alvo, aero); 
        printf_timestamped("[AERONAVE %s]: ENTRANDO no Setor %s. Prioridade: %u\n", aero->id, setor_alvo->id, aero->prioridade);

        if (setor_prev != NULL) {
            // Libera o setor anterior
            printf_timestamped("[AERONAVE %s] SAINDO do Setor %s.\n", aero->id, setor_prev->id);
            setor_liberar_saida(setor_prev, aero);
        }
        
        // Atualiza o setor anterior para o próximo ciclo
        setor_prev = setor_alvo;

        // Atualiza o setor atual da aeronave
        pthread_mutex_lock(&aero->lock);
        aero->current_setor = setor_alvo;
        pthread_mutex_unlock(&aero->lock);

        // Simulação de uso do recurso (Voo no setor)
        usar_setor(aero);
    }
    
    // Ao finalizar, libera o último setor caso exista
    if (setor_prev != NULL) {
        printf_timestamped("[AERONAVE %s] SAINDO do Setor %s.\n", aero->id, setor_prev->id);
        setor_liberar_saida(setor_prev, aero);
    }

    pthread_mutex_lock(&aero->lock);
    aero->finished = true;
    printf_timestamped("[AERONAVE %s] ROTA CONCLUIDA E LIBERADA.\n", aero->id);

    resultado_aeronave_t* resultado = (resultado_aeronave_t*)malloc(sizeof(resultado_aeronave_t));
    if (resultado == NULL) return NULL;
    long long espera_ns = aero->espera_total_ns;
    pthread_mutex_unlock(&aero->lock);

    resultado->id = aero->id;
    resultado->media_espera_ms = (double)espera_ns / (double)(aero->rota.len * 1000000LL);
    
    
    return (void*)resultado;

}

void init_aeronaves(aeronave_t* aeronaves, size_t aeronaves_len) {
    for (size_t i = 0; i < aeronaves_len; i++) {
        aeronaves[i].id = create_id('A',i);
        aeronaves[i].prioridade = rand() % 1001;
        aeronaves[i].aero_index = i;
        aeronaves[i].current_setor = NULL;
        aeronaves[i].finished = false;
        aeronaves[i].espera_total_ns = 0;
        pthread_mutex_init(&aeronaves[i].lock, NULL);
    }
}

void destroy_aeronaves(aeronave_t* aeronaves, size_t aeronaves_len) {
    if (aeronaves == NULL || aeronaves_len == 0) {
        return; // Nada para liberar
    }

    // Iterar sobre o array e liberar os recursos internos de CADA setor
    for (size_t i = 0; i < aeronaves_len; i++) {
        aeronave_t* aeronave = &(aeronaves[i]);

        // Liberar a string 'id'
        if (aeronave->id != NULL) {
            free(aeronave->id);
            aeronave->id = NULL;
        }

        // Libera rota
        destruir_rota(aeronave->rota);
        pthread_mutex_destroy(&aeronave->lock);
    }

    // Liberar a memória do array principal
    // Esta chamada libera o bloco contínuo de memória que contém todas as estruturas setor_t.
    free(aeronaves);
}

void usar_setor(aeronave_t* aeronave) {
    printf_timestamped("[AERONAVE %s]: USANDO SETOR...\n", aeronave->id);
    usleep(300000 + rand() % 500000);
}
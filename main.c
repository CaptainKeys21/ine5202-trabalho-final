#include "setor.h"
#include "aeronave.h"
#include "controle.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char** argv) {
    srand(time(NULL));

    if (argc < 3) {
        fprintf(stderr, "Uso: %s <num_aeronaves> <num_setores>\n", argv[0]);
        return 1;
    }
    
    size_t num_aero = (size_t)atoi(argv[1]);
    size_t num_set = (size_t)atoi(argv[2]);

    printf("Iniciando simulação com %zu aeronaves e %zu setores...\n", num_aero, num_set);

    controle_t ctrl_data;
    init_controle(&ctrl_data, num_aero, num_set);

    setor_t* setores = (setor_t*)malloc(num_set * sizeof(setor_t));
    init_setores(setores, num_set, &ctrl_data);
    
    aeronave_t* aeronaves = (aeronave_t*)malloc(num_aero * sizeof(aeronave_t));
    init_aeronaves(aeronaves, num_aero);

    for (int i = 0; i < num_aero; i++) {
        aeronaves[i].rota = criar_rota(setores, num_set, rand() % num_set + 1);

        // Alocações para o banqueiro
        for (rota_node_t* curr = aeronaves[i].rota.head; curr != NULL; curr = curr->next) {
            ctrl_data.max[i][curr->setor->setor_index] = 1;
            ctrl_data.need[i][curr->setor->setor_index] = 1;
        }
    }

    ctrl_data.aeronaves = aeronaves; // Registra o vetor de aeronaves no controle

    pthread_t aero_threads[num_aero];
    pthread_t ctrl_thread;

    //imprimir_estado_banqueiro(&ctrl_data);
    // A thread de controle do banqueiro, tem que ser criada antes das aeronaves (percebemos isso da pior maneira)
    int res = pthread_create(&ctrl_thread, NULL, banqueiro_thread, (void *)&ctrl_data);
    if (res != 0) {
        fprintf(stderr, "Erro ao criar thread de controle: %d\n", res);
    }

    for (int i = 0; i < num_aero; i++) {
        int res = pthread_create(&aero_threads[i], NULL, aeronave_thread, (void *)&aeronaves[i]);

        if (res != 0) {
            fprintf(stderr, "Erro ao criar thread: %d\n", res);
            return 1;
        }
    }

    resultado_aeronave_t* resultados[num_aero];
    for (int i = 0; i < num_aero; i++) {
        pthread_join(aero_threads[i], (void**)&resultados[i]);
    }

    pthread_join(ctrl_thread, NULL);

    printf("\n=== RESULTADOS DA SIMULAÇÃO ===\n");
    double soma_total = 0;
    for (int i = 0; i < num_aero; i++) {
        soma_total += resultados[i]->media_espera;
        printf("Aeronave %s - Média de espera: %.2f ms\n", resultados[i]->id, resultados[i]->media_espera);
    }
    printf("Média geral de espera: %.2f ms\n", (double)soma_total / (double)num_aero);


    // Liberação de Recursos
    destroy_setores(setores, num_set);
    destroy_aeronaves(aeronaves, num_aero);
    destroy_controle(&ctrl_data);
}
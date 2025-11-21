#ifndef AERONAVE_H
#define AERONAVE_H

#include <setor.h>
#include <rota.h>
#include <pthread.h>

/**
 * @brief Representa uma aeronave
 * 
 * @param id Identificação da nave
 * @param prioridade Prioridade da nave no setor, quanto menor mais prioridade
 * @param rota A rota que a nave deve percorrer
 */
typedef struct aeronave {
    char* id;
    unsigned int prioridade;
    rota_t rota;
} aeronave_t;

/**
 * @brief Função que será executado em thread onde a aeronave irá executar suas rotinas
 * 
 * Rotina de execução:
 * s1. *Solicitar* acesso ao próximo setor de sua rota (inserir a aeronave na fila do setor);
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
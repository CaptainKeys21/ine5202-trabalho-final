#include "setor.h"
#include "aeronave.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void init_setores(setor_t* setores, size_t setores_len, controle_t* controle) {
    for (size_t i = 0; i < setores_len; i++) {
        setores[i].id = create_id('S',i);

        sem_init(&setores[i].sem, 0, 1);
        pthread_mutex_init(&setores[i].lock, NULL);
        pthread_cond_init(&setores[i].setor_disponivel_cond, NULL);

        setores[i].fila = NULL;
        setores[i].fila_len = 0;

        setores[i].setor_index = i; // Para localizar no banqueiro
        setores[i].controle = controle;
    }
}

void destroy_setores(setor_t* setores, size_t setores_len) {
    if (setores == NULL || setores_len == 0) {
        return; // Nada para liberar
    }

    // 1. Iterar sobre o array e liberar os recursos internos de CADA setor
    for (size_t i = 0; i < setores_len; i++) {
        setor_t* setor = &(setores[i]);
        
        // a. Destruir Mutex, Semáforo e Variável de Condição
        pthread_mutex_destroy(&(setor->lock));
        pthread_cond_destroy(&(setor->setor_disponivel_cond));
        sem_destroy(&(setor->sem));

        // b. Liberar a string 'id'
        if (setor->id != NULL) {
            free(setor->id);
            setor->id = NULL;
        }

        // c. Liberar a fila de aeronaves
        // (Lembre-se: se 'aeronave_t' tem recursos internos, eles DEVEM ser liberados aqui)
        if (setor->fila != NULL) {
            free(setor->fila);
            setor->fila = NULL;
            setor->fila_len = 0;
        }

        setor->controle = NULL; // Apenas para segurança
    }

    // 2. Liberar a memória do array principal
    // Esta chamada libera o bloco contínuo de memória que contém todas as estruturas setor_t.
    free(setores);
}

void setor_solicitar_entrada(setor_t *setor, aeronave_t *aeronave) {
    // 1. Tenta obter o recurso (semáforo) localmente (Bloqueia se já estiver ocupado)
    sem_wait(&setor->sem); 
    
    // Coloca a aeronave na fila do setor (para controle de prioridade e visibilidade)
    // Usaremos a fila para esperar o Banqueiro se o estado for inseguro
    // Lógica de adicionar à fila (deve ser baseada em aeronave->prioridade)
    entrar_fila(setor, aeronave);

    bool concedido = false;
    
    while (!concedido) {
        
        // 3. Tenta obter a permissão do Banqueiro (Bloqueio Global)
        pthread_mutex_lock(&setor->controle->banker_lock); 
        
        // ** CHAMADA AO CORAÇÃO DO BANQUEIRO **
        if (setor_tenta_conceder_seguro(setor->controle, aeronave->aero_index, setor->setor_index)) {
            // Permissão do banqueiro CONCEDIDA (Alocação provisória mantida)
            concedido = true;
            printf("[P%s/S%s]: Autorizado pelo Banqueiro. Alocação OK.\n", aeronave->id, setor->id);
            
            // Remove da fila de espera do Setor
            sair_fila(setor, aeronave);
            
        } else {
            // Permissão NEGADA (Alocação provisória revertida)
            printf("[P%s/S%s]: NEGADO. Estado Inseguro. Esperando...\n", aeronave->id, setor->id);
            
            // 4. Espera na Variável de Condição (Bloqueia a thread, libera o lock)
            // Deve liberar o banker_lock ANTES de esperar no cond_wait
            pthread_mutex_unlock(&setor->controle->banker_lock); 
            
            // Espera até que um setor seja liberado ou a prioridade mude
            pthread_cond_wait(&setor->setor_disponivel_cond, &setor->lock);
            
            // Tenta novamente (volta ao início do while, rebloqueia o setor->lock)
            continue; 
        }

        pthread_mutex_unlock(&setor->controle->banker_lock);
    }
}

void setor_liberar_saida(setor_t *setor, aeronave_t *aeronave) {
    
    // 1. Bloqueia as matrizes do banqueiro para liberar o recurso
    pthread_mutex_lock(&setor->controle->banker_lock);
    
    // ** CHAMADA AO CORAÇÃO DO BANQUEIRO **
    liberar_recurso_banqueiro(setor->controle, aeronave->aero_index, setor->setor_index);
    
    pthread_mutex_unlock(&setor->controle->banker_lock);
    
    // 2. Libera o semáforo do setor (recurso) para que o próximo possa tentar
    sem_post(&setor->sem); 
    
    // 3. Sinaliza a Variável de Condição para acordar as aeronaves esperando na fila
    // (Pode ser que a liberação tenha criado um Estado Seguro para elas)
    pthread_mutex_lock(&setor->lock);
    pthread_cond_broadcast(&setor->setor_disponivel_cond); // Acorda todos para reavaliação
    pthread_mutex_unlock(&setor->lock);
}

void entrar_fila(setor_t* setor, aeronave_t* aeronave) {
    printf("Tentando adicionar aeronave %s na fila do setor %s\n", aeronave->id, setor->id);
    pthread_mutex_lock(&setor->lock);
    
    size_t new_len = setor->fila_len + 1;
    aeronave_t* nova_fila = (aeronave_t*)realloc(setor->fila, new_len * sizeof(aeronave_t));
    if (nova_fila == NULL) {
        printf("Erro de realloc ao adicionar aeronave %s na fila do setor %s\n", aeronave->id, setor->id);
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    setor->fila = nova_fila;
    setor->fila[setor->fila_len] = *aeronave; // Adiciona no final da fila

    setor->fila_len = new_len;
    printf("Nova aeronave %s adicionada a fila do setor %s\n", aeronave->id, setor->id);

    pthread_mutex_unlock(&setor->lock);
}

void sair_fila(setor_t* setor, aeronave_t* aeronave) {
    pthread_mutex_lock(&setor->lock);

    size_t aero_index = -1;

    // 1. LOCALIZAÇÃO E OTIMIZAÇÃO: Usar strcmp e break
    for (size_t i = 0; i < setor->fila_len; i++) {
        // CORREÇÃO CRÍTICA: Comparar o conteúdo das strings
        if (strcmp(setor->fila[i].id, aeronave->id) == 0) { 
            aero_index = i;
            break; 
        }
    }

    if (aero_index == (size_t)-1) { // -1 é promovido a (size_t)-1
        printf("Aeronave %s não encontrada na fila do setor %s\n", aeronave->id, setor->id);
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    // 2. DESLOCAMENTO DOS ELEMENTOS (Remove o elemento no índice)
    // Move todos os elementos após o índice 'aero_index' uma posição para trás
    for (size_t i = aero_index; i < setor->fila_len - 1; i++) {
        setor->fila[i] = setor->fila[i + 1];
    }

    // 3. REALOCAÇÃO E ATRIBUIÇÃO SEGURA
    size_t new_len = setor->fila_len - 1;
    aeronave_t* nova_fila;
    
    // Se a fila esvaziou, new_len será 0. realloc(ptr, 0) == free(ptr), retorna NULL.
    nova_fila = (aeronave_t*)realloc(setor->fila, new_len * sizeof(aeronave_t));
    
    // Checagem de falha de alocação: 
    // Se nova_fila é NULL E a fila NÃO esvaziou (new_len > 0), é um erro.
    if (nova_fila == NULL && new_len > 0) {
        printf("Erro de realloc ao remover aeronave %s. Memória original preservada.\n", aeronave->id);
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    // 4. ATRIBUIÇÃO: Atualiza o ponteiro e o tamanho
    setor->fila = nova_fila; // Se new_len=0, nova_fila é NULL, e setor->fila aponta para NULL (correto).
    setor->fila_len = new_len;
    
    printf("Aeronave %s removida da fila do setor %s (Tamanho: %zu)\n", aeronave->id, setor->id, setor->fila_len);

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

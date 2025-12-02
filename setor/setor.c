#define _POSIX_C_SOURCE 199309L // importante para CLOCK_MONOTONIC em time.h
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

        pthread_mutex_init(&setores[i].lock, NULL);
        pthread_cond_init(&setores[i].setor_disponivel_cond, NULL);

        setores[i].fila = NULL;
        setores[i].fila_len = 0;

        setores[i].setor_index = i; // Para localizar no banqueiro
        setores[i].controle = controle;
    }

    controle->setores = setores;
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

void setor_solicitar_entrada(setor_t* setor, aeronave_t *aeronave) {
    // Coloca a aeronave na fila do setor (para controle de prioridade e visibilidade)
    // Usaremos a fila para esperar o Banqueiro se o estado for inseguro
    // Lógica de adicionar à fila (deve ser baseada em aeronave->prioridade)
    entrar_fila(setor, aeronave);
    
    // Sinaliza ao controle que há uma nova solicitação
    pthread_mutex_lock(&setor->controle->banker_lock);
    pthread_cond_signal(&setor->controle->new_request_cond);
    pthread_mutex_unlock(&setor->controle->banker_lock);

    struct timespec tempo_inicio; // Inicio do tempo de espera
    clock_gettime(CLOCK_MONOTONIC, &tempo_inicio);
    
    pthread_mutex_lock(&setor->lock);

    printf_timestamped("[AERONAVE %s] ESPERANDO concessão do BANQUEIRO para setor %s...\n", aeronave->id, setor->id);

    while (setor->controle->allocation[aeronave->aero_index][setor->setor_index] <= 0) {
        // Espera até ser acordada quando o setor estiver disponível
        pthread_cond_wait(&setor->setor_disponivel_cond, &setor->lock);
    }
    // Remove a aeronave da fila agora que ela adquiriu o recurso
    sair_fila(setor, aeronave);

    struct timespec tempo_fim; // Fim do tempo de espera
    clock_gettime(CLOCK_MONOTONIC, &tempo_fim);
    
    long long delta_ns = (tempo_fim.tv_sec - tempo_inicio.tv_sec) * 1000000000LL + (tempo_fim.tv_nsec - tempo_inicio.tv_nsec);

    pthread_mutex_lock(&aeronave->lock);
    aeronave->espera_total_ns += delta_ns;
    pthread_mutex_unlock(&aeronave->lock);

    printf_timestamped("[AERONAVE %s] ADQUIRIU ACESSO ao setor %s.\n", aeronave->id, setor->id);
    pthread_mutex_unlock(&setor->lock);
}

void setor_liberar_saida(setor_t *setor, aeronave_t *aeronave) {
    // 1. Bloqueia as matrizes do banqueiro para liberar o recurso
    pthread_mutex_lock(&setor->controle->banker_lock);
    printf_timestamped("[AERONAVE %s] LIBERANDO setor %s...\n", aeronave->id, setor->id);
    
    // ** CHAMADA AO CORAÇÃO DO BANQUEIRO **
    liberar_recurso_banqueiro(setor->controle, aeronave->aero_index, setor->setor_index);

    pthread_cond_signal(&setor->controle->new_request_cond);
    
    pthread_mutex_unlock(&setor->controle->banker_lock);
}

void entrar_fila(setor_t* setor, aeronave_t* aeronave) {
    printf_timestamped("[AERONAVE %s] TENTANDO ADICIONAR na fila de ESPERA do setor %s (Prioridade: %u)\n", 
           aeronave->id, setor->id, aeronave->prioridade);
    
    pthread_mutex_lock(&setor->lock);
    
    size_t new_len = setor->fila_len + 1;
    size_t insert_index = 0;
    
    // --- 1. Realocação Segura ---
    
    // Aloca espaço para o novo elemento.
    aeronave_t* nova_fila = (aeronave_t*)realloc(setor->fila, new_len * sizeof(aeronave_t));
    
    if (nova_fila == NULL) {
        fprintf(stderr, "Erro de realloc ao adicionar aeronave %s na fila do setor %s\n", 
                aeronave->id, setor->id);
        pthread_mutex_unlock(&setor->lock);
        return;
    }

    setor->fila = nova_fila; // Atualiza o ponteiro para a nova memória

    // --- 2. Encontrar a Posição de Inserção ---
    
    // Percorre a fila para encontrar a primeira aeronave com prioridade MENOR
    // do que a aeronave que está entrando.
    // Isso define o índice onde a nova aeronave deve ser inserida.
    for (size_t i = 0; i < setor->fila_len; i++) {
        if (aeronave->prioridade > setor->fila[i].prioridade) {
            insert_index = i;
            break; 
        }
        // Se a nova aeronave tem prioridade menor ou igual a todos, ela vai para o final.
        insert_index = new_len - 1; 
    }
    
    // Caso especial: Fila Vazia (setor->fila_len == 0), insert_index será 0.
    if (setor->fila_len == 0) {
        insert_index = 0;
    }
    
    // --- 3. Deslocar Elementos ---
    
    // Desloca todos os elementos a partir do insert_index uma posição para frente
    // (do final da fila em direção ao índice de inserção) para abrir espaço.
    for (size_t i = setor->fila_len; i > insert_index; i--) {
        setor->fila[i] = setor->fila[i - 1];
    }
    
    // --- 4. Inserir e Atualizar ---
    
    // Insere a nova aeronave no espaço vazio.
    setor->fila[insert_index] = *aeronave; 

    setor->fila_len = new_len;
    
    printf_timestamped("[AERONAVE %s] Nova aeronave ADICIONADA a fila de ESPERA do setor %s no índice %zu (Prioridade: %u)\n", 
           aeronave->id, setor->id, insert_index, aeronave->prioridade);


    pthread_mutex_unlock(&setor->lock);
}

void sair_fila(setor_t* setor, aeronave_t* aeronave) {
    printf_timestamped("[AERONAVE %s] TENTANDO REMOVER da fila de ESPERA do setor %s\n", aeronave->id, setor->id);

    size_t aero_index = -1;

    // 1. LOCALIZAÇÃO E OTIMIZAÇÃO: Usar strcmp e break
    for (size_t i = 0; i < setor->fila_len; i++) {
        // Comparar o conteúdo das strings
        if (strcmp(setor->fila[i].id, aeronave->id) == 0) { 
            aero_index = i;
            break; 
        }
    }

    if (aero_index == (size_t)-1) { // -1 é promovido a (size_t)-1
        printf("Aeronave %s não encontrada na fila do setor %s\n", aeronave->id, setor->id);
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
        return;
    }

    // 4. ATRIBUIÇÃO: Atualiza o ponteiro e o tamanho
    setor->fila = nova_fila; // Se new_len=0, nova_fila é NULL, e setor->fila aponta para NULL (correto).
    setor->fila_len = new_len;
    
    printf_timestamped("[AERONAVE %s] REMOVIDA da fila de ESPERA do setor %s (Tamanho: %zu)\n", aeronave->id, setor->id, setor->fila_len);

    pthread_mutex_unlock(&setor->lock);
}

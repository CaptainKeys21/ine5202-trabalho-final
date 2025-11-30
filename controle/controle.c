#include "controle.h"
#include "aeronave.h"

void init_controle(controle_t* controle, size_t num_aeronaves, size_t num_setores) {
    if (num_aeronaves == 0 || num_setores == 0) return;

    controle->num_aeronaves = num_aeronaves;
    controle->num_setores = num_setores;

    // Alocação dos vetores (Available e Finish)
    controle->available = (int *)calloc(num_setores, sizeof(int));
    controle->finish = (bool *)calloc(num_aeronaves, sizeof(bool));
    if (!controle->available || !controle->finish) return;
    
    // Inicializa todos os recursos como disponíveis (1 instância por setor)
    for (size_t j = 0; j < num_setores; j++) {
        controle->available[j] = 1; 
    }

    // 2. Alocação das matrizes (Max, Allocation, Need)
    // Alocação das linhas (num_aero) e, em seguida, das colunas (num_set)
    controle->max        = (int **)calloc(num_aeronaves, sizeof(int *));
    controle->allocation = (int **)calloc(num_aeronaves, sizeof(int *));
    controle->need       = (int **)calloc(num_aeronaves, sizeof(int *));

    if (!controle->max || !controle->allocation || !controle->need) return;
    for (size_t i = 0; i < num_aeronaves; i++) {
        controle->max[i]        = (int *)calloc(num_setores, sizeof(int));
        controle->allocation[i] = (int *)calloc(num_setores, sizeof(int));
        controle->need[i]       = (int *)calloc(num_setores, sizeof(int));
        
        if (!controle->max[i] || !controle->allocation[i] || !controle->need[i]) return;
    }

    // Inicializa o Mutex e Condição
    pthread_mutex_init(&controle->banker_lock, NULL);
    pthread_cond_init(&controle->new_request_cond, NULL);
}

void destroy_controle(controle_t* controle) {
    if (controle == NULL) return;

    // 1. Liberação das matrizes
    for (size_t i = 0; i < controle->num_aeronaves; i++) {
        free(controle->max[i]);
        free(controle->allocation[i]);
        free(controle->need[i]);
    }
    free(controle->max);
    free(controle->allocation);
    free(controle->need);

    // 2. Liberação dos vetores
    free(controle->available);
    // free(controle->setores); // Se você alocou setores, libere aqui

    // 3. Destruição do Mutex
    pthread_mutex_destroy(&controle->banker_lock);
}

void* banqueiro_thread(void* arg) {
    controle_t* ctrl = (controle_t*)arg;

    // Loop para monitorar as solicitações
    while (true) {

        pthread_mutex_lock(&ctrl->banker_lock);
        
        pthread_cond_wait(&ctrl->new_request_cond, &ctrl->banker_lock);

        for (size_t i = 0; i < ctrl->num_setores; i++) {
            printf("Banqueiro verificando setor %zu... (fila_len: %zu)\n", i, ctrl->setores[i].fila_len);
            setor_t* setor = &ctrl->setores[i];
            pthread_mutex_lock(&setor->lock);

            if (setor->fila_len > 0) {
                aeronave_t* aeronave = &setor->fila[0];
                if (setor_tenta_conceder_seguro(ctrl, aeronave->aero_index, setor->setor_index)) {
                    printf("Banqueiro concedeu setor %s para aeronave %s.\n", setor->id, aeronave->id);
                    pthread_cond_broadcast(&setor->setor_disponivel_cond);
                }
            }

            pthread_mutex_unlock(&setor->lock);
        }

        pthread_mutex_unlock(&ctrl->banker_lock);
    }

    return NULL;
}

// --- Algoritmo de Segurança (Executado SOMENTE sob banker_lock) ---
bool is_safe(controle_t* ctrl) {
    int work[ctrl->num_setores];
    bool finish[ctrl->num_aeronaves];
    size_t count = 0;
    
    // Inicialização
    for (size_t j = 0; j < ctrl->num_setores; j++) work[j] = ctrl->available[j];

    // Busca por sequência segura
    while (count < ctrl->num_aeronaves) {
        bool found = false;
        for (size_t p = 0; p < ctrl->num_aeronaves; p++) {
            if (finish[p] == false) {
                size_t j;
                // Verifica se Need[p] <= Work
                for (j = 0; j < ctrl->num_setores; j++) {
                    if (ctrl->need[p][j] > work[j]) break;
                }
                
                // Se Need[p] <= Work para todos os recursos, então encontrou um processo p seguro
                if (j == ctrl->num_setores) {
                    // Simula a conclusão: Work = Work + Allocation[p]
                    for (size_t k = 0; k < ctrl->num_setores; k++) {
                        work[k] += ctrl->allocation[p][k];
                    }
                    finish[p] = true;
                    found = true;
                    count++;
                }
            }
        }
        if (found == false && count < ctrl->num_aeronaves) return false;
    }
    return true;
}

// --- Funções de Decisão do Banqueiro (Chamadas pelo Setor) ---

// Tenta a alocação provisória e verifica a segurança
bool setor_tenta_conceder_seguro(controle_t* ctrl, int aero_id, int setor_idx) {
    printf("Banqueiro: Tentando conceder setor %d para aeronave %d...\n", setor_idx, aero_id);
    bool concedido = false;

    //imprimir_estado_banqueiro(ctrl);
    
    // // 1. ADQUIRIR LOCK GLOBAL
    // pthread_mutex_lock(&ctrl->banker_lock);
    // printf("Banqueiro: Lock adquirido para aeronave %d e setor %d.\n", aero_id, setor_idx);

    // 2. Checagem rápida de necessidade e disponibilidade
    if (ctrl->need[aero_id][setor_idx] > 0 && ctrl->available[setor_idx] >= 1) {
        
        // 3. Simular a alocação (temporariamente, na memória do ctrl)
        ctrl->available[setor_idx] -= 1;
        ctrl->allocation[aero_id][setor_idx] += 1;
        ctrl->need[aero_id][setor_idx] -= 1;

        // 4. Executar a checagem de segurança (is_safe)
        if (is_safe(ctrl)) {
            // Estado é seguro: manter a alocação e retornar TRUE
            concedido = true;
        } else {
            // Estado é inseguro: Reverter a alocação
            ctrl->available[setor_idx] += 1;
            ctrl->allocation[aero_id][setor_idx] -= 1;
            ctrl->need[aero_id][setor_idx] += 1;
        }
    }
    
    // // 5. LIBERAR LOCK GLOBAL
    // pthread_mutex_unlock(&ctrl->banker_lock);
    
    return concedido;
}

// Libera o recurso e atualiza as matrizes
void liberar_recurso_banqueiro(controle_t* ctrl, int aero_id, int setor_idx) {
    // Se o setor estava alocado, libera
    if (ctrl->allocation[aero_id][setor_idx] > 0) {
        ctrl->available[setor_idx] += ctrl->allocation[aero_id][setor_idx];
        ctrl->allocation[aero_id][setor_idx] = 0;
        // Restaura a necessidade (necessário se for usar o algoritmo de solicitação completo)
        //ctrl->need[aero_id][setor_idx] = ctrl->max[aero_id][setor_idx]; 
    }
}

bool existe_aerothread_alive(controle_t* ctrl) {
    for (size_t i = 0; i < ctrl->num_aeronaves; i++) {
        if (!ctrl->finish[i]) {
            printf("Aeronave %zu ainda está ativa.\n", i);
            return true;
        }
    }
    printf("Todas as aeronaves finalizaram suas rotinas.\n");
    return false;
}

void imprimir_estado_banqueiro(controle_t* ctrl) {
    if (!ctrl) return;

    // Adquire o lock para garantir que a leitura das matrizes seja atômica
    //pthread_mutex_lock(&ctrl->banker_lock); 
    
    size_t R = ctrl->num_setores;    // R = Número de Recursos (Setores)
    size_t P = ctrl->num_aeronaves; // P = Número de Processos (Aeronaves)

    // --- Título e Legenda ---
    printf("\n"
           "===========================================================\n"
           "                  ESTADO ATUAL DO BANQUEIRO\n"
           "===========================================================\n");

    // --- Imprime o Vetor AVAILABLE ---
    printf("Vetor AVAILABLE (Recursos Disponíveis):\n");
    printf("  [");
    for (size_t j = 0; j < R; j++) {
        printf(" S-%zu: %d%s", j, ctrl->available[j], (j < R - 1) ? " |" : "");
    }
    printf(" ]\n\n");

    // --- Imprime o Cabeçalho das Matrizes (Max, Allocation, Need) ---
    printf("Matrizes (P=%zu Aeronaves | R=%zu Setores):\n", P, R);
    printf("Aeronave |    MAX (Máx. Necessidade)   | ALLOCATION (Alocado)  |      NEED (Falta)\n");
    
    // Imprime um separador
    printf("---------|-----------------------------|-----------------------|----------------------\n");

    // --- Imprime as Matrizes Linha por Linha ---
    for (size_t i = 0; i < P; i++) {
        printf("  A-%zu    |", i);

        // Coluna MAX
        for (size_t j = 0; j < R; j++) {
            printf(" %d%s", ctrl->max[i][j], (j < R - 1) ? "" : " |");
        }
        printf("\t|");
        
        // Coluna ALLOCATION
        for (size_t j = 0; j < R; j++) {
            printf(" %d%s", ctrl->allocation[i][j], (j < R - 1) ? "" : " |");
        }
        printf("\t|");

        // Coluna NEED
        for (size_t j = 0; j < R; j++) {
            printf(" %d%s", ctrl->need[i][j], (j < R - 1) ? "" : " ");
        }
        printf("\n");
    }

    printf("===========================================================\n");
    
    // Libera o lock após o print
    //pthread_mutex_unlock(&ctrl->banker_lock);
}
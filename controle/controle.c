#include <errno.h> // para ETIMEDOUT
#include "controle.h"
#include "aeronave.h"
#include "utils.h"

void init_controle(controle_t* controle, size_t num_aeronaves, size_t num_setores) {
    if (num_aeronaves == 0 || num_setores == 0) return;

    controle->num_aeronaves = num_aeronaves;
    controle->num_setores = num_setores;

    //controle->setores = NULL;   // Inicializa o ponteiro para setores como NULL
    //controle->aeronaves = NULL; // Inicializa o ponteiro para aeronaves como NULL

    // Alocação dos vetores (Available e Finish)
    controle->available = (int *)calloc(num_setores, sizeof(int));
    if (!controle->available) return;
    
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

    // 3. Destruição do Mutex
    pthread_mutex_destroy(&controle->banker_lock);
    pthread_cond_destroy(&controle->new_request_cond);
}

void* banqueiro_thread(void* arg) {
    controle_t* ctrl = (controle_t*)arg;

    // Loop para monitorar as solicitações
    while (existe_aerothread_alive(ctrl)) {

        printf_timestamped("[BANQUEIRO] Aguardando novas solicitações...\n");
        pthread_mutex_lock(&ctrl->banker_lock);

        struct timespec ts = get_abs_timeout(5);
        int wait_res = pthread_cond_timedwait(&ctrl->new_request_cond, &ctrl->banker_lock, &ts);
        if (wait_res == ETIMEDOUT) {
            printf_timestamped("[BANQUEIRO] Timeout de espera atingido. Verificando novamente...\n");
        }

        printf_timestamped("[BANQUEIRO] Acordado para processar solicitações.\n");
        for (size_t i = 0; i < ctrl->num_setores; i++) {
            setor_t* setor = &ctrl->setores[i];
            pthread_mutex_lock(&setor->lock);
            printf_timestamped("[BANQUEIRO] Verificando setor %zu... (fila_len: %zu)\n", i, ctrl->setores[i].fila_len);
            if (setor->fila_len > 0) {
                aeronave_t* aeronave = &setor->fila[0];
                pthread_mutex_lock(&aeronave->lock);
                int setor_origem_idx = aeronave->current_setor ? aeronave->current_setor->setor_index : -1;
                if (setor_tenta_conceder_seguro(ctrl, aeronave->aero_index, setor->setor_index, setor_origem_idx)) {
                    printf_timestamped("[BANQUEIRO] Concedeu setor %s para aeronave %s.\n", setor->id, aeronave->id);
                    pthread_cond_broadcast(&setor->setor_disponivel_cond);
                }
                pthread_mutex_unlock(&aeronave->lock);
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
    for (size_t p = 0; p < ctrl->num_aeronaves; p++) finish[p] = false;

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
        // Se nenhum processo foi encontrado nesta iteração, o sistema não está em um estado seguro
        if (found == false && count < ctrl->num_aeronaves){
            for (size_t i = 0; i < ctrl->num_setores; i++) {
                printf(" %d", work[i]);
            }
            printf("\n");
            imprimir_estado_banqueiro(ctrl);
            return false;
        }
    }
    return true;
}

// Tenta a alocação provisória e verifica a segurança
bool setor_tenta_conceder_seguro(controle_t* ctrl, int aero_idx, int setor_destino_idx, int setor_origem_idx) {
    printf_timestamped("[BANQUEIRO] Tentando conceder setor %d para aeronave %d...\n", setor_destino_idx, aero_idx);
    bool concedido = false;


    // Checagem rápida de necessidade e disponibilidade
    if (ctrl->need[aero_idx][setor_destino_idx] > 0 && ctrl->available[setor_destino_idx] >= 1) {

        // Libera o recurso do setor de origem, se aplicável
        if (setor_origem_idx != -1) {
            ctrl->available[setor_origem_idx] += 1;
            ctrl->allocation[aero_idx][setor_origem_idx] -= 1;
        }
        
        // 3. Simular a alocação (temporariamente, na memória do ctrl)
        ctrl->available[setor_destino_idx] -= 1;
        ctrl->allocation[aero_idx][setor_destino_idx] += 1;
        ctrl->need[aero_idx][setor_destino_idx] -= 1;

        // 4. Executar a checagem de segurança (is_safe)
        if (is_safe(ctrl)) {
            // Estado é seguro: manter a alocação e retornar TRUE
            concedido = true;
        } else {
            // Estado é inseguro: Reverter a alocação
            ctrl->available[setor_destino_idx] += 1;
            ctrl->allocation[aero_idx][setor_destino_idx] -= 1;
            ctrl->need[aero_idx][setor_destino_idx] += 1;

            if (setor_origem_idx != -1) {
                ctrl->available[setor_origem_idx] -= 1;
                ctrl->allocation[aero_idx][setor_origem_idx] += 1;
            }
        }
    }
    
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
    if (ctrl == NULL) return false;
    if (ctrl->aeronaves == NULL) {
        return false;
    }


    for (size_t i = 0; i < ctrl->num_aeronaves; i++) {
        if (!ctrl->aeronaves[i].finished) {
            return true;
        }
    }
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
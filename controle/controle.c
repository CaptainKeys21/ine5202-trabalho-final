#include <errno.h> // para ETIMEDOUT
#include <string.h>
#include "controle.h"
#include "aeronave.h"
#include "utils.h"

void init_controle(controle_t* controle, size_t num_aeronaves, size_t num_setores) {
    if (num_aeronaves == 0 || num_setores == 0) return;

    controle->num_aeronaves = num_aeronaves;
    controle->num_setores = num_setores;

    // Alocação dos vetores (Available e Finish)
    controle->available = (int *)calloc(num_setores, sizeof(int));
    if (!controle->available) return;
    
    // Inicializa todos os recursos como disponíveis (1 instância por setor)
    for (size_t j = 0; j < num_setores; j++) {
        controle->available[j] = 1; 
    }

    // Alocação das matrizes (Max, Allocation, Need)
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

    // Liberação das matrizes
    for (size_t i = 0; i < controle->num_aeronaves; i++) {
        free(controle->max[i]);
        free(controle->allocation[i]);
        free(controle->need[i]);
    }
    free(controle->max);
    free(controle->allocation);
    free(controle->need);

    // Liberação dos vetores
    free(controle->available);

    // Destruição do Mutex
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
            if (setor->fila_len > 0) {
                bool setor_concedido = false;

                for (size_t fila_i = 0; fila_i < setor->fila_len && !setor_concedido; fila_i++) {
                    aeronave_t* aeronave = &setor->fila[fila_i];
                    int setor_origem_idx = aeronave->current_setor ? aeronave->current_setor->setor_index : -1;
                    if ((setor_concedido = setor_tenta_conceder_seguro(ctrl, aeronave->aero_index, setor->setor_index, setor_origem_idx))) {
                        printf_timestamped("[BANQUEIRO] Concedeu setor %s para aeronave %s.\n", setor->id, aeronave->id);
                        pthread_cond_broadcast(&setor->setor_disponivel_cond);
                    } 
                }
            }

            pthread_mutex_unlock(&setor->lock);
        }

        pthread_mutex_unlock(&ctrl->banker_lock);
    }

    printf_timestamped("[BANQUEIRO] Todas as aeronaves finalizaram. Encerrando thread do banqueiro.\n");

    return NULL;
}

bool is_safe(controle_t* ctrl, int temp_available[], int temp_allocation[][ctrl->num_setores], int temp_need[][ctrl->num_setores]) {
    int work[ctrl->num_setores];
    bool finish[ctrl->num_aeronaves];
    size_t count = 0;
    
    // Inicialização
    for (size_t j = 0; j < ctrl->num_setores; j++) work[j] = temp_available[j];
    for (size_t p = 0; p < ctrl->num_aeronaves; p++) finish[p] = false;

    // Busca por sequência segura
    while (count < ctrl->num_aeronaves) {
        bool found = false;
        for (size_t p = 0; p < ctrl->num_aeronaves; p++) {
            if (finish[p] == false) {
                size_t j;
                // Verifica se Need[p] <= Work
                for (j = 0; j < ctrl->num_setores; j++) {
                    if (temp_need[p][j] > work[j]) break;
                }
                
                // Se Need[p] <= Work para todos os recursos, então encontrou um processo p seguro
                if (j == ctrl->num_setores) {
                    // Simula a conclusão: Work = Work + Allocation[p]
                    for (size_t k = 0; k < ctrl->num_setores; k++) {
                        work[k] += temp_allocation[p][k];
                    }
                    finish[p] = true;
                    found = true;
                    count++;
                }
            }
        }
        // Se nenhum processo foi encontrado nesta iteração, o sistema não está em um estado seguro
        if (found == false && count < ctrl->num_aeronaves){
            return false;
        }
    }
    return true;
}

// Tenta a alocação provisória e verifica a segurança
bool setor_tenta_conceder_seguro(controle_t* ctrl, int aero_idx, int setor_destino_idx, int setor_origem_idx) {
    printf_timestamped("[BANQUEIRO] Tentando conceder setor %d para aeronave %d...\n", setor_destino_idx, aero_idx);
    // Checagem rápida de necessidade e disponibilidade
    if (ctrl->need[aero_idx][setor_destino_idx] <= 0 || ctrl->available[setor_destino_idx] < 1) return false;

    size_t num_aeronaves = ctrl->num_aeronaves;
    size_t num_setores = ctrl->num_setores;

    // 1. Criação das Matrizes Temporárias (Cópia do estado ATUAL)
    int temp_available[num_setores];
    int temp_allocation[num_aeronaves][num_setores];
    int temp_need[num_aeronaves][num_setores];

    memcpy(temp_available, ctrl->available, num_setores * sizeof(int));
    for (size_t i = 0; i < num_aeronaves; i++) {
        // Copia a linha [i] da matriz global para a linha [i] da matriz temporária local
        memcpy(temp_allocation[i], ctrl->allocation[i], num_setores * sizeof(int));
        memcpy(temp_need[i], ctrl->need[i], num_setores * sizeof(int));
    }


    // Libera o recurso do setor de origem, se aplicável
    if (setor_origem_idx != -1) {
        temp_available[setor_origem_idx] += 1;
        temp_allocation[aero_idx][setor_origem_idx] -= 1;
    }
    
    // Simular a alocação (temporariamente, na memória do ctrl)
    temp_available[setor_destino_idx] -= 1;
    temp_allocation[aero_idx][setor_destino_idx] += 1;
    temp_need[aero_idx][setor_destino_idx] -= 1;

    // Executar a checagem de segurança (is_safe)
    bool res = is_safe(ctrl, temp_available, temp_allocation, temp_need);
    if (res) {
        // Se seguro, efetiva a alocação nas matrizes reais
        if (setor_origem_idx != -1) {
            ctrl->available[setor_origem_idx] += 1;
            ctrl->allocation[aero_idx][setor_origem_idx] -= 1;
        }
        ctrl->available[setor_destino_idx] -= 1;
        ctrl->allocation[aero_idx][setor_destino_idx] += 1;
        ctrl->need[aero_idx][setor_destino_idx] -= 1;
    }

    return res;
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
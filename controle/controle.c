#include "controle.h"

void init_controle(controle_t* controle, size_t num_aeronaves, size_t num_setores) {
    if (num_aeronaves == 0 || num_setores == 0) return;

    controle->num_aeronaves = num_aeronaves;
    controle->num_setores = num_setores;

    // 1. Alocação dos vetores (Available e setores)
    controle->available = (int *)calloc(num_setores, sizeof(int));
    // controle->setores = (setor_t *)calloc(num_setores, sizeof(setor_t)); // Você deve inicializar setores
    if (!controle->available /* || !controle->setores */) return;
    
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
        
        // Aqui você definiria os valores iniciais de MAX e NEED
    }

    // Inicializa o Mutex
    pthread_mutex_init(&controle->banker_lock, NULL);
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

// --- Algoritmo de Segurança (Executado SOMENTE sob banker_lock) ---
bool is_safe(controle_t* ctrl) {
    int work[ctrl->num_setores];
    bool finish[ctrl->num_aeronaves];
    size_t count = 0;
    
    // Inicialização
    for (size_t j = 0; j < ctrl->num_setores; j++) work[j] = ctrl->available[j];
    for (size_t i = 0; i < ctrl->num_aeronaves; i++) finish[i] = false;

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
    // 1. Verificação preliminar: Recurso disponível?
    if (ctrl->available[setor_idx] == 0) return false; 
    
    // 2. Alocação provisória
    ctrl->available[setor_idx] -= 1;
    ctrl->allocation[aero_id][setor_idx] += 1;
    ctrl->need[aero_id][setor_idx] -= 1;

    // 3. Verifica se é seguro
    if (is_safe(ctrl)) {
        return true; // Alocação mantida
    } else {
        // 4. Reverte a alocação (rollback)
        ctrl->available[setor_idx] += 1;
        ctrl->allocation[aero_id][setor_idx] -= 1;
        ctrl->need[aero_id][setor_idx] += 1;
        return false; // Negado
    }
}

// Libera o recurso e atualiza as matrizes
void liberar_recurso_banqueiro(controle_t* ctrl, int aero_id, int setor_idx) {
    // Se o setor estava alocado, libera
    if (ctrl->allocation[aero_id][setor_idx] > 0) {
        ctrl->available[setor_idx] += ctrl->allocation[aero_id][setor_idx];
        ctrl->allocation[aero_id][setor_idx] = 0;
        // Restaura a necessidade (necessário se for usar o algoritmo de solicitação completo)
        ctrl->need[aero_id][setor_idx] = ctrl->max[aero_id][setor_idx]; 
    }
}
#include <errno.h> // para ETIMEDOUT
#include <string.h>
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
            //printf_timestamped("[BANQUEIRO] Verificando setor %zu... (fila_len: %zu)\n", i, ctrl->setores[i].fila_len);
            if (setor->fila_len > 0) {
                aeronave_t* aeronave = &setor->fila[0];
                int setor_origem_idx = aeronave->current_setor ? aeronave->current_setor->setor_index : -1;
                if (setor_tenta_conceder_seguro(ctrl, aeronave->aero_index, setor->setor_index, setor_origem_idx)) {
                    printf_timestamped("[BANQUEIRO] Concedeu setor %s para aeronave %s.\n", setor->id, aeronave->id);
                    pthread_cond_broadcast(&setor->setor_disponivel_cond);
                } else {
                    printf_timestamped("[BANQUEIRO] Realizando rollback para evitar deadlock.\n");
                    realizar_rollback_banqueiro(ctrl, aeronave);
                }
            }

            pthread_mutex_unlock(&setor->lock);
        }

        pthread_mutex_unlock(&ctrl->banker_lock);
    }

    printf_timestamped("[BANQUEIRO] Todas as aeronaves finalizaram. Encerrando thread do banqueiro.\n");

    return NULL;
}

// --- Algoritmo de Segurança (Executado SOMENTE sob banker_lock) ---
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
    // Checagem rápida de necessidade e disponibilidade
    if (ctrl->need[aero_idx][setor_destino_idx] <= 0 || ctrl->available[setor_destino_idx] < 1) return false;

    size_t num_aeronaves = ctrl->num_aeronaves;
    size_t num_setores = ctrl->num_setores;

    // --- 1. Criação das Matrizes Temporárias (Cópia do estado ATUAL) ---
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
    
    // 3. Simular a alocação (temporariamente, na memória do ctrl)
    temp_available[setor_destino_idx] -= 1;
    temp_allocation[aero_idx][setor_destino_idx] += 1;
    temp_need[aero_idx][setor_destino_idx] -= 1;

    // 4. Executar a checagem de segurança (is_safe)
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

void realizar_rollback_banqueiro(controle_t* ctrl, aeronave_t* aeronave_solicitante) {
    aeronave_t* alvo_rollback = NULL;
    int prioridade_minima = 1001; // Variável para rastrear a prioridade mais baixa
    
    // Encontre a aeronave ativa de mais baixa prioridade que possui algum recurso
    for (size_t p = 0; p < ctrl->num_aeronaves; p++) {
        aeronave_t* aero_p = &ctrl->aeronaves[p];
        
        // Critérios: Deve ter menor prioridade E estar alocada
        if (aero_p->prioridade < prioridade_minima && aero_p->current_setor != NULL) {
            
            // Simular se o estado se torna seguro após A-p liberar seu setor
            if (is_safe_after_release_total(ctrl, aero_p, aero_p->current_setor, aeronave_solicitante)) {
                 alvo_rollback = aero_p;
                 prioridade_minima = aero_p->prioridade;
            }
        }
    }

    if (alvo_rollback != NULL) {
        // Encontramos o alvo: Forçar liberação
        setor_t* setor_liberado = alvo_rollback->current_setor;
        forcar_liberacao_setor(ctrl, alvo_rollback, setor_liberado);
        
        printf_timestamped("[BANQUEIRO] Rollback Forçado: %s (Prioridade %d) liberou %s para resolver impasse.\n", 
                           alvo_rollback->id, alvo_rollback->prioridade, setor_liberado->id);

    }
}

void forcar_liberacao_setor(controle_t* ctrl, aeronave_t* aeronave, setor_t* setor) {
    ctrl->allocation[aeronave->aero_index][setor->setor_index] -= 1;
    ctrl->available[setor->setor_index] += 1;
    entrar_fila(setor, aeronave);
    aeronave->current_setor = NULL;
}

bool is_safe_after_release_total(controle_t* ctrl, aeronave_t* A_alocada, setor_t* setor_solicitado, aeronave_t* A_solicitante) {
    size_t num_aeronaves = ctrl->num_aeronaves;
    size_t num_setores = ctrl->num_setores;
    
    // --- 1. Criação das Matrizes Temporárias (Cópia do estado ATUAL) ---
    int temp_available[num_setores];
    int temp_allocation[num_aeronaves][num_setores];
    int temp_need[num_aeronaves][num_setores];

    memcpy(temp_available, ctrl->available, num_setores * sizeof(int));
    for (size_t i = 0; i < num_aeronaves; i++) {
        // Copia a linha [i] da matriz global para a linha [i] da matriz temporária local
        memcpy(temp_allocation[i], ctrl->allocation[i], num_setores * sizeof(int));
        memcpy(temp_need[i], ctrl->need[i], num_setores * sizeof(int));
    }

    // Identificadores
    size_t p_alocada_id = A_alocada->aero_index;
    size_t p_solicitante_id = A_solicitante->aero_index;
    size_t r_solicitado_id = setor_solicitado->setor_index;

    // --- 2. Simular a Liberação TOTAL Forçada (Rollback) ---
    
    // Itera sobre todos os setores que A_alocada detém
    for (size_t r = 0; r < num_setores; r++) {
        int recurso_alocado = temp_allocation[p_alocada_id][r];

        if (recurso_alocado > 0) {
            // Aumenta temporariamente o AVAILABLE com o recurso liberado
            temp_available[r] += recurso_alocado;
            
            // Simula a remoção da alocação de A_alocada (Zera a alocação)
            temp_allocation[p_alocada_id][r] = 0;
            
            // Recalcula o NEED temporário de A_alocada (Need = Max - 0)
            temp_need[p_alocada_id][r] = ctrl->max[p_alocada_id][r];
        }
    }

    // --- 3. Simular a Concessão para A_solicitante ---
    
    // Condição de Segurança: Checa se há recurso no AVAILABLE temporário (após a liberação)
    if (temp_available[r_solicitado_id] >= 1) { 
        
        // Simula a concessão: Work = Work - 1
        temp_available[r_solicitado_id] -= 1;
        
        // Simula a alocação: Allocation = Allocation + 1
        temp_allocation[p_solicitante_id][r_solicitado_id] += 1;
        
        // Recalcula o NEED: Need = Max - Allocation
        temp_need[p_solicitante_id][r_solicitado_id] = 
            ctrl->max[p_solicitante_id][r_solicitado_id] - temp_allocation[p_solicitante_id][r_solicitado_id];

        // --- 4. Checagem de Segurança Final ---
        // Chama o Algoritmo de Segurança com as matrizes simuladas.
        return is_safe(ctrl, temp_available, (int (*)[num_setores])temp_allocation, (int (*)[num_setores])temp_need);

    } else {
        // Mesmo após a liberação de recursos totais, o setor solicitado não ficou disponível.
        return false;
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
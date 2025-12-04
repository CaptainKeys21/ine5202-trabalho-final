#include "rota.h"

// Função auxiliar para checar se um setor (índice) já existe na rota.
bool setor_existe_na_rota(rota_node_t* head, setor_t* novo_setor) {
    rota_node_t* current = head;
    while (current != NULL) {
        if (current->setor == novo_setor) {
            return true;
        }
        current = current->next;
    }
    return false;
}

rota_t criar_rota(setor_t* setores, size_t setores_len, size_t rota_len) {
    rota_t rota;
    rota.head = NULL;
    rota.tail = NULL;
    rota.curr = NULL;
    size_t rota_atual_len = 0;

    // Se o tamanho da rota for maior que o número de setores disponíveis, 
    // não é possível criar uma rota com setores distintos.
    if (rota_len > setores_len) {
        fprintf(stderr, "ERRO: Rota de tamanho %zu solicitada, mas apenas %zu setores únicos disponíveis.\n", rota_len, setores_len);
        return rota; // Retorna rota vazia
    }

    // O loop continua até que o comprimento desejado da rota seja alcançado.
    while (rota_atual_len < rota_len) {
        
        size_t sel = rand() % setores_len;
        setor_t* novo_setor = &setores[sel];

        // Checa se o setor sorteado já existe na rota.
        if (!setor_existe_na_rota(rota.head, novo_setor)) {
            
            // Cria o novo nó
            rota_node_t* node = (rota_node_t*)malloc(sizeof(rota_node_t));
            if (node == NULL) {
                perror("Falha na alocação de memória para rota_node_t");
                break; 
            }
            
            node->setor = novo_setor;
            node->next = NULL;

            // Adiciona à lista encadeada
            if (rota.head == NULL) {
                rota.head = rota.tail = rota.curr = node;
            } else {
                rota.tail->next = node;
                rota.tail = node;
            }
            
            rota_atual_len++; // Incrementa o contador de nós válidos adicionados
        }
    }

    rota.len = rota_atual_len;

    return rota;
}

void destruir_rota(rota_t rota) {
    rota_node_t* curr = rota.head;

    while (curr) {
        rota_node_t* next = curr->next;
        free(curr);
        curr = next;
    }
}

setor_t* rota_next_setor(rota_t *rota) {
    if (rota->curr == NULL) return NULL;
    
    setor_t* setor = rota->curr->setor;
    rota->curr = rota->curr->next;
    
    return setor;
}
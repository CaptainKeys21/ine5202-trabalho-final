#include "rota.h"

rota_t criar_rota(setor_t* setores, size_t setores_len, size_t rota_len) {
    rota_t rota;
    rota.head = NULL;
    rota.tail = NULL;

    for (size_t i = 0; i < rota_len; i++) {
        size_t sel = rand() % setores_len;

        rota_node_t* node = malloc(sizeof(rota_node_t));
        node->setor = &setores[sel];
        node->next = NULL;

        if (rota.head == NULL) {
            rota.head = rota.tail = rota.curr = node;
        } else {
            rota.tail->next = node;
            node->prev = rota.tail;
            rota.tail = node;
        }
    }

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
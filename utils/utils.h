#ifndef UTILS_H
#define UTILS_H

#include <setor.h>

typedef struct {
    setor_t* setores;
    size_t len;
} setor_list_t;

setor_list_t* criar_rota(setor_list_t* setores, size_t len);
char* gerar_setor_id(int index);

#endif

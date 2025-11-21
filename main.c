#include <setor.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    setor_t* setores;
    size_t num_setores;
    
} controle_t;

void* thread_controle(void* arg) {
    controle_t* ctrl = (controle_t*)arg;
    
}

int main() {
    srand(time(NULL));

    setor_t* setores;

    init_setores(setores, 3);
}
#include "aeronave.h"
#include "setor.h"
#include <stdlib.h>

void* aeronave_thread(void* arg) {
    aeronave_t* a = (aeronave_t*)arg;

}

void usar_setor(aeronave_t* aeronave) {
    usleep(300000 + rand() % 500000);
}
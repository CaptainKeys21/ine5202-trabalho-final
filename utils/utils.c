#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* gerar_setor_id(int index) {
    if (index < 0) return NULL;

    char buffer[32];
    int pos = 0;

    int n = index + 1;
    while (n > 0) {
        int rem = (n - 1) % 26;
        buffer[pos++] = 'A' + rem;
        n = (n - 1) / 26;
    }
    
    buffer[pos] = '\0';

    for (int i = 0; i < pos / 2; i++) {
        char tmp = buffer[i];
        buffer[i] = buffer[pos - 1 - i];
        buffer[pos - 1 - i] = tmp;
    }

    char* res = malloc(pos + 1);
    strcpy(res, buffer);

    return res;
}
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

// Estrutura que representa uma conta bancária
struct conta {
    float saldo;
};

// Função de saque original (vulnerável a race condition)
bool saque(struct conta *c, float valor) {
    float saldo = c->saldo;
    if (valor > saldo) {
        return false;
    }
    // Simula trabalho e troca de contexto
    // (não é necessário, mas ajuda a reproduzir o problema)
    // O tempo de espera é apenas para simular uma operação mais longa
    for (volatile int i = 0; i < 50000; ++i);

    c->saldo = saldo - valor;
    return true;
}

// Parâmetros para as threads
struct args_saque {
    struct conta *c;
    float valor;
    char *sacador_nome;
};

void *thread_saque(void *a) {
    struct args_saque *args = (struct args_saque *)a;
    if (saque(args->c, args->valor)) {
        printf("[%s]: Saque de %.2f realizado com sucesso.\n", args->sacador_nome, args->valor);
    } else {
        printf("[%s]: Falha no saque de %.2f: saldo insuficiente.\n", args->sacador_nome, args->valor);
    }
    return NULL;
}

int main() {
    struct conta c = { .saldo = 100.0f };
    pthread_t t1, t2;
    struct args_saque a1 = { .c = &c, .valor = 80.0f, .sacador_nome = "Thread 1" };
    struct args_saque a2 = { .c = &c, .valor = 80.0f, .sacador_nome = "Thread 2" };

    // Cria duas threads que fazem saque ao mesmo tempo
    pthread_create(&t1, NULL, thread_saque, &a1);
    pthread_create(&t2, NULL, thread_saque, &a2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Saldo final: %.2f\n", c.saldo);
    return 0;
}
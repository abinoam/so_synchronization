#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

uint64_t valor = 0;
_Atomic uint32_t trava = 0;

#define TAMANHO 10
volatile int dados[TAMANHO];
volatile size_t inserir = 0;
volatile size_t remover = 0;

void enter_region(void)
{
    uint32_t v = 0;
    if (atomic_compare_exchange_strong_explicit(&trava, &v, 1,
                                                memory_order_acquire, memory_order_relaxed))
    {
        return;
    }

    do
    {

        if (v == 2 || atomic_compare_exchange_weak_explicit(&trava, &v, 2,
                                                            memory_order_relaxed, memory_order_relaxed))
        {
            syscall(SYS_futex, &trava, FUTEX_WAIT, 2);
        }
        v = 0;
    } while (!atomic_compare_exchange_weak_explicit(&trava, &v, 2,
                                                    memory_order_acquire, memory_order_relaxed));
}

void leave_region(void)
{
    uint32_t v = atomic_fetch_sub_explicit(&trava, 1, memory_order_release);
    if (v != 1)
    {
        atomic_store_explicit(&trava, 0, memory_order_relaxed);
        syscall(SYS_futex, &trava, FUTEX_WAKE, 1);
    }
}

void *produtor(void *arg)
{
    int v;
    for (v = 1;; v++)
    {
        while (((inserir + 1) % TAMANHO) == remover)
            ;
        printf("Produzindo %d\n", v);
        dados[inserir] = v;
        inserir = (inserir + 1) % TAMANHO;
        usleep(500000);
    }
    return NULL;
}

void *consumidor(void *arg)
{
    for (;;)
    {
        while (inserir == remover)
            ;
        printf("%zu: Consumindo %d\n", (size_t)arg, dados[remover]);
        remover = (remover + 1) % TAMANHO;
    }
    return NULL;
}

int main(void)
{
    pthread_t t1, t2, t3;

    pthread_create(&t1, NULL, produtor, (void *)0);
    pthread_create(&t2, NULL, consumidor, (void *)1);
    pthread_create(&t3, NULL, consumidor, (void *)2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return 0;
}

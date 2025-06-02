#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

#define TAMANHO 10
volatile int dados[TAMANHO];
volatile size_t inserir = 0;
volatile size_t remover = 0;

struct mutex
{
    _Atomic uint32_t trava;
};

struct mutex mutex_fila;

struct esperando
{
    struct mutex m;
    struct esperando *prox;
};

struct semaforo
{
    struct mutex trava;
    size_t valor;
    struct esperando *cabeca;
    struct esperando *cauda;
};

struct semaforo sem_fila;

void enter_region(_Atomic uint32_t *trava)
{
    uint32_t v = 0;
    if (atomic_compare_exchange_strong_explicit(trava, &v, 1,
                                                memory_order_acquire, memory_order_relaxed))
    {
        return;
    }

    do
    {

        if (v == 2 || atomic_compare_exchange_weak_explicit(trava, &v, 2,
                                                            memory_order_relaxed, memory_order_relaxed))
        {
            syscall(SYS_futex, trava, FUTEX_WAIT, 2);
        }
        v = 0;
    } while (!atomic_compare_exchange_weak_explicit(trava, &v, 2,
                                                    memory_order_acquire, memory_order_relaxed));
}

void leave_region(_Atomic uint32_t *trava)
{
    uint32_t v = atomic_fetch_sub_explicit(trava, 1, memory_order_release);
    if (v != 1)
    {
        atomic_store_explicit(trava, 0, memory_order_relaxed);
        syscall(SYS_futex, trava, FUTEX_WAKE, 1);
    }
}

void inicializar(struct mutex *m)
{
    atomic_store(&m->trava, 0);
}

void travar(struct mutex *m)
{
    enter_region(&m->trava);
}

void destravar(struct mutex *m)
{
    leave_region(&m->trava);
}

void sem_inicializar(struct semaforo *s)
{
    inicializar(&s->trava);
    s->valor = 0;
    s->cabeca = NULL;
    s->cauda = NULL;
}

void sem_incrementar(struct semaforo *s)
{
    struct esperando *esp;

    travar(&s->trava);
    esp = s->cabeca;
    if (esp != NULL)
    {
        s->cabeca = esp->prox;
        if (!s->cabeca)
        {
            s->cauda = NULL;
        }
    }
    s->valor++;
    destravar(&s->trava);
    if (esp != NULL)
    {
        destravar(&esp->m);
    }
}

void sem_decrementar(struct semaforo *s)
{
    struct esperando esp;
    for (;;)
    {
        travar(&s->trava);
        if (s->valor > 0)
        {
            s->valor--;
            destravar(&s->trava);
            return;
        }
        inicializar(&esp.m);
        travar(&esp.m);
        esp.prox = NULL;
        if (s->cauda)
        {
            s->cauda->prox = &esp;
        }
        else
        {
            s->cabeca = &esp;
        }
        s->cauda = &esp;
        destravar(&s->trava);
        travar(&esp.m);
    }
}

void *produtor(void *arg)
{
    int v;
    for (v = 1;; v++)
    {
        travar(&mutex_fila);
        while (((inserir + 1) % TAMANHO) == remover)
        {
            destravar(&mutex_fila);
            travar(&mutex_fila);
        }
        printf("Produzindo %d\n", v);
        fflush(stdout);
        dados[inserir] = v;
        inserir = (inserir + 1) % TAMANHO;
        destravar(&mutex_fila);
        sem_incrementar(&sem_fila);
        usleep(500000);
    }
    return NULL;
}

void *consumidor(void *arg)
{
    size_t id = (size_t)arg;
    for (;;)
    {
        sem_decrementar(&sem_fila);
        travar(&mutex_fila);
        printf("%zu: Consumindo %d\n", (size_t)arg, dados[remover]);
        fflush(stdout);
        remover = (remover + 1) % TAMANHO;
        destravar(&mutex_fila);
        // Para simular threads com diferentes tempos de consumo.
        //
        //  if (id == 1) usleep(1500000);
        //  else if (id == 2) usleep(2500000);
    }
    return NULL;
}

int main(void)
{
    pthread_t t1, t2, t3;

    inicializar(&mutex_fila);
    sem_inicializar(&sem_fila);

    pthread_create(&t1, NULL, produtor, (void *)0);
    pthread_create(&t2, NULL, consumidor, (void *)1);
    pthread_create(&t3, NULL, consumidor, (void *)2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return 0;
}

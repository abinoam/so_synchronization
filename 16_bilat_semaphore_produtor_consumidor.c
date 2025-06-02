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
    struct mutex trava; // protege a fila de espera + contador
    size_t valor;       // contador do semáforo
    struct esperando *cabeca;
    struct esperando *cauda;
};

struct semaforo sem_fila;  // semáforo que conta itens disponíveis
struct semaforo sem_vazio; // semáforo que conta vagas livres

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
        /* libera exatamente um thread que estivesse dormindo */
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
        /* se não houver “valor” (contador == 0), enfileira e dorme */
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
        travar(&esp.m); // aqui o thread efetivamente dorme até alguém fazer sem_incrementar
    }
}

void *produtor(void *arg)
{
    int v;
    for (v = 1;; v++)
    {
        /* 1) Esperar vaga livre: */
        sem_decrementar(&sem_vazio);

        /* 2) Inserir na fila com exclusão mútua */
       travar(&mutex_fila);
        printf("Produzindo %d\n", v);
        fflush(stdout);
        dados[inserir] = v;
        inserir = (inserir + 1) % TAMANHO;
        destravar(&mutex_fila);

        /* 3) Sinalizar que há mais um item disponível */
        sem_incrementar(&sem_fila);

        /* para desacelerar um pouco */
        // usleep(500000);
    }
    return NULL;
}

void *consumidor(void *arg)
{
    size_t id = (size_t)arg;
    for (;;)
    {
        /* 1) Esperar que haja ao menos um item disponível */
        sem_decrementar(&sem_fila);

        /* 2) Remover da fila com exclusão mútua */
        travar(&mutex_fila);
        printf("%zu: Consumindo %d\n", id, dados[remover]);
        fflush(stdout);
        remover = (remover + 1) % TAMANHO;
        destravar(&mutex_fila);

        /* 3) Sinalizar que ficou livre mais uma vaga */
        sem_incrementar(&sem_vazio);

        // Para simular threads com diferentes tempos de consumo.
        //  if (id == 1) usleep(150000);
        //  else if (id == 2) usleep(250000);

    }
    return NULL;
}

int main(void)
{
    pthread_t t1, t2, t3;

    /* Inicializa o mutex */
    inicializar(&mutex_fila);

    /* Inicializa semáforos */
    sem_inicializar(&sem_fila);
    sem_inicializar(&sem_vazio);

    /* Como a fila comporta até (TAMANHO-1) itens, definimos sem_vazio.valor = TAMANHO-1 */
    sem_vazio.valor = TAMANHO - 1;

    /* sem_fila já começa em zero (nenhum item disponível) */

    /* Cria threads */
    pthread_create(&t1, NULL, produtor, (void *)0);
    pthread_create(&t2, NULL, consumidor, (void *)1);
    pthread_create(&t3, NULL, consumidor, (void *)2);

    /* Aguarda término (na prática, nunca termina) */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return 0;
}

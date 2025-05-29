#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

uint64_t valor = 0;        /* shared variable */
atomic_bool trava = false; /* lock variable */

void enter_region(void)
{
    bool v;
    do
    {
        v = false;
    } while (!atomic_compare_exchange_strong(&trava, &v, true));
}

void leave_region(void)
{
    atomic_store(&trava, false);
}

void *thread(void *arg)
{
    size_t i = 1e8;
    while (i--)
    {
        enter_region();
        valor++;
        leave_region();
    }
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    // Cria duas threads que incrementam o contador global
    pthread_create(&t1, NULL, thread, (void *)0);
    pthread_create(&t2, NULL, thread, (void *)1);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final counter value: %ld\n", valor);
    return 0;
}

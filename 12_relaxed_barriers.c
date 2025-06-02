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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

uint64_t valor = 0;        /* shared variable */
atomic_bool trava = false; /* lock variable */

// Como as syscalls estão sendo usadas frequentemente,
// há queda de performance é maior que o uso de atomics.
void enter_region(void)
{
    uint32_t v;
    do
    {
        syscall(SYS_futex, &trava, FUTEX_WAIT, 1);
        v = atomic_exchange(&trava, 1);
    } while (v);
}
void leave_region(void)
{
    atomic_store(&trava, 0);
    syscall(SYS_futex, &trava, FUTEX_WAKE, 1);
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

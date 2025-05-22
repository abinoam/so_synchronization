#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define FALSE 0
#define TRUE 1
#define N 2 /* number of processes */

volatile int turn;          /* whose turn is it? */
volatile int interested[N]; /* all values initially 0 (FALSE) */
int valor = 0;              /* shared variable */

void enter_region(int process) /* process is 0 or 1 */
{
    int other;                                           /* number of the other process */
    other = 1 - process;                                 /* the opposite of process */
    interested[process] = TRUE;                          /* show that you are interested */
    atomic_thread_fence(memory_order_seq_cst);           /* cpu memory fence */
    turn = process;                                      /* set flag */
    atomic_thread_fence(memory_order_seq_cst);           /* cpu memory fence */
    while (turn == process && interested[other] == TRUE) /* null statement */
        ;
    atomic_thread_fence(memory_order_seq_cst);           /* cpu memory fence */
}

void leave_region(int process) /* process: who is leaving */
{
    interested[process] = FALSE; /* indicate departure from critical region */
}

void *thread(void *arg)
{
    int proc = (size_t)arg;
    size_t i = 1e8;
    while (i--)
    {
        enter_region(proc);
        valor++;
        leave_region(proc);
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

    printf("Final counter value: %d\n", valor);
    return 0;
}

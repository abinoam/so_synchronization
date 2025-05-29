#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

long counter = 0;

void *thread(void *arg)
{
    for (int i = 0; i < 1000000; i++) {
        counter++;
    }
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;

    // Cria duas threads que incrementam o contador global
    pthread_create(&t1, NULL, thread, NULL);
    pthread_create(&t2, NULL, thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final counter value: %ld\n", counter);
    return 0;
}
// ? Loc here: header modification to adapt pthread_setaffinity_np
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <utmpx.h>
#include <assert.h>

#include <sched.h>

cpu_set_t cpuset_0;
cpu_set_t cpuset_1;
pthread_barrier_t barrier;

void *thread1(void* dummy){
    pthread_barrier_wait(&barrier);
    assert(sched_getcpu() == 0);
    return NULL;
}

void *thread2(void* dummy){
    pthread_barrier_wait(&barrier);
    assert(sched_getcpu() == 1);
    return NULL;
}

int main(){
    pthread_t pid[2];
    int i;
    // ? LoC: Bind core here

    pthread_barrier_init(&barrier,NULL,3);

    CPU_ZERO(&cpuset_0);
    CPU_SET(0, &cpuset_0);
    
    CPU_ZERO(&cpuset_1);
    CPU_SET(1, &cpuset_1);

    for(i = 0; i < 2; ++i){
        // 1 Loc code here: create thread and save in pid[2]
        pthread_create(&pid[i], NULL, i == 0 ? thread1 : thread2, NULL);
    }

    pthread_setaffinity_np(pid[0],sizeof(cpu_set_t),&cpuset_0);
    pthread_setaffinity_np(pid[1],sizeof(cpu_set_t),&cpuset_1);

    pthread_barrier_wait(&barrier);

    for(i = 0; i < 2; ++i){
        // 1 Loc code here: join thread
        pthread_join(pid[i], NULL);
    }
    return 0;
}

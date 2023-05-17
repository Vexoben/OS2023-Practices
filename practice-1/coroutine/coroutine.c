/* YOUR CODE HERE */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include "coroutine.h"

#define THREAD_MAX 105

#define nex(x) ((x) + 1 == MAXCO ? 0 : (x) + 1)

int thread_cnt;
pthread_mutex_t mutex;

struct thread {
   int used;
   pthread_t pid;
   struct CoArray * coarray;
   int switch_id; // init: -1 
   int current_running_id; // init: MAXCO - 1
   int las_co;
} threads[THREAD_MAX];

__attribute__((constructor)) static void co_initial() {
   for (int i = 0; i < THREAD_MAX; ++i) {
      threads[i].used = 0;
      threads[i].coarray = CoArrayNil();
      threads[i].las_co = 0;
      struct Coroutine * ret = (struct Coroutine *) malloc(sizeof(struct Coroutine));
      ret->func = NULL;
      ret->parent = -1;
      ret->status = CO_RUNNING; 
      ret->id = MAXCO - 1;
      AddCoroutine(threads[i].coarray, ret);
   }
}

__attribute__((destructor)) static void co_destruct() {
   for (int i = 0; i < THREAD_MAX; ++i) {
      DeleteCoArray(threads[i].coarray);
   }
}

int getPID() {
   pthread_mutex_lock(&mutex);
   pthread_t cur_pid = pthread_self();
   for (int i = 0; i < thread_cnt; ++i) { 
      if (threads[i].pid == cur_pid) {
         pthread_mutex_unlock(&mutex);
         return i;
      }
   }
   int ret = thread_cnt;
   ++thread_cnt;
   threads[ret].used = 1;
   threads[ret].pid = cur_pid;
   threads[ret].current_running_id = MAXCO - 1;
   threads[ret].switch_id = -1;
   pthread_mutex_unlock(&mutex);
   return ret;
}

int co_start(int (*routine)(void)) {
   int pid = getPID();
   int id = CreateCoroutine(routine, threads[pid].current_running_id);
   threads[pid].switch_id = id;
   co_yield();
   return id;
}

int co_getid() {
   int pid = getPID();
   return threads[pid].current_running_id;
}

int co_getret(int cid) {
   int pid = getPID();
   return GetCoroutine(threads[pid].coarray, cid)->retval;
}

int co_yield() {
   // static int cnt = 0;
   // if (++cnt == 100) {
   //    puts("too much co_yield");
   //    exit(0);
   // }
   int pid = getPID();
   int t = setjmp(GetCoroutine(threads[pid].coarray, threads[pid].current_running_id)->context);
   // printf("setjmp: %d %d\n", threads[pid].current_running_id, t);
   if (!t) { // switch to another coroutine
      int id = threads[pid].switch_id == -1 ? GetRunningCo() : threads[pid].switch_id;
      // printf("%d switch to: %d\n", threads[pid].current_running_id, id);
      if (id == threads[pid].current_running_id) return 0;
      threads[pid].switch_id = -1;
      if (GetCoroutine(threads[pid].coarray, id)->status == CO_NEW) { // sub-coroutine
         int retval = 0;
         unsigned char * sp = (GetCoroutine(threads[pid].coarray, id)->stack + STACK_SIZE);
         threads[pid].current_running_id = id;
         SetCoStaues(threads[pid].coarray, id, CO_RUNNING);
         asm volatile (
            "movq %1, %%rsp; call *%2; movl %%eax, %0;"
            : "=m"(retval)
            : "r"(sp), "r"((void*) GetCoroutine(threads[pid].coarray, id)->func)
            : "eax", "memory"
         );
         pid = getPID();
         // printf("%d\n", pid);
         id = threads[pid].current_running_id;
         // printf("coroutine %d set to CO_DEAD\n", id);
         SetCoStaues(threads[pid].coarray, id, CO_DEAD);
         SetRetValue(threads[pid].coarray, id, retval);
         threads[pid].current_running_id = GetCoroutine(threads[pid].coarray, id)->parent;
         // printf("longjmpA: %d to %d\n", id, GetCoroutine(threads[pid].coarray, id)->parent);
         longjmp(GetCoroutine(threads[pid].coarray, GetCoroutine(threads[pid].coarray, id)->parent)->context, 1);
      } else {
         // printf("longjmpB: %d to %d\n", threads[pid].current_running_id, id);
         threads[pid].current_running_id = id;
         longjmp(GetCoroutine(threads[pid].coarray, id)->context, 1);
      }
      return 0;
   } else { // switch back
      return 0;
   }
}

int co_waitall() {
   int pid = getPID();
   int flag = 0;
   while (1) {
      flag = 1;
      for (int i = 0; i < MAXCO; ++i) {
         if (i != threads[pid].current_running_id && GetCoroutine(threads[pid].coarray, i) != NULL && GetCoroutine(threads[pid].coarray, i)->status != CO_DEAD) {
            flag = 0;
         }
      }
      if (flag == 1) break;
      co_yield();
   }
   return 0;
}

int co_wait(int cid) {
   int pid = getPID();
   while (GetCoroutine(threads[pid].coarray, cid)->status != CO_DEAD) {
      co_yield();
   }
   return 0;
}

int checkAuthrize(int child, int ancestor) {
   int pid = getPID();
   if (GetCoroutine(threads[pid].coarray, child) == NULL) return 0;
   while (child != -1) {
      if (child == ancestor) return 1;
      child = GetCoroutine(threads[pid].coarray, child)->parent;
   }
   return 0;
}

int co_status(int cid) {
   // printf("checkAuthrize: %d %d\n", cid, current_running_id);
   // printf("co_status: %d %d %d\n", cid, GetCoroutine(coarray, cid)->status, checkAuthrize(current_running_id, cid));
   int pid = getPID();
   if (checkAuthrize(cid, threads[pid].current_running_id)) {
      if (GetCoroutine(threads[pid].coarray, cid)->status == CO_DEAD) {
         return FINISHED;
      } else {
         return RUNNING;
      }
   } else {
      return UNAUTHORIZED;  
   }
}

int GetRunningCo() {
   int pid = getPID();
   int i, flag = 0;
   for (i = nex(threads[pid].las_co); i != threads[pid].las_co; i = nex(i)) {
      if (i != threads[pid].current_running_id &&
          GetCoroutine(threads[pid].coarray, i) != NULL &&
          GetCoroutine(threads[pid].coarray, i)->status != CO_DEAD) {
            flag = 1;
            break;
         }
   }
   if (flag == 0) {
      return threads[pid].current_running_id; // no other coroutine
   }
   threads[pid].las_co = i;
   return threads[pid].las_co;
}

int GetNewID() {
   int pid = getPID();
   for (int i = 0; i < MAXCO; ++i) {
      if (GetCoroutine(threads[pid].coarray, i) == NULL) {
         return i;
      }
   }
   fprintf(stderr, "No available coroutine space.");
   exit(-1);
}

int CreateCoroutine(int (*routine)(void), int parent) {
   int pid = getPID();
   int id = GetNewID();
   struct Coroutine * ret = (struct Coroutine *) malloc(sizeof(struct Coroutine));
   ret->id = id;
   ret->func = routine;
   ret->parent = parent;
   ret->status = CO_NEW;
   AddCoroutine(threads[pid].coarray, ret);
   return id;
}

void SetRetValue(struct CoArray * arr, int id, int value) {
   arr->head[id]->retval = value;
}

void SetCoStaues(struct CoArray * arr, int id, enum co_status status) {
   arr->head[id]->status = status;
}

struct CoArray * CoArrayNil() {
   struct CoArray * ret = (struct CoArray *) malloc(sizeof(struct CoArray));
   ret->length = 0;
   ret->head = (struct Coroutine **) malloc(sizeof(struct Coroutine *) * MAXCO);
   for (int i = 0; i < MAXCO; ++i) {
      ret->head[i] = NULL;
   }
   return ret;
}

void AddCoroutine(struct CoArray * arr, struct Coroutine * co) {
   arr->head[co->id] = co;
}

struct Coroutine * GetCoroutine(struct CoArray * arr, int index) {
   return arr->head[index];
}

void DeleteCoArray(struct CoArray * arr) {
   for (int i = 0; i < MAXCO; ++i) {
      if (arr->head[i] != NULL) {
         free(arr->head[i]);
      }
   }
   free(arr);
}

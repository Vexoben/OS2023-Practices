/* YOUR CODE HERE */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "coroutine.h"

#define nex(x) ((x) + 1 == MAXCO ? 0 : (x) + 1)

struct CoArray * coarray;
int switch_id = -1;
int current_running_id = MAXCO - 1;

__attribute__((constructor)) static void co_initial() {
   coarray = CoArrayNil();
   struct Coroutine * ret = (struct Coroutine *) malloc(sizeof(struct Coroutine));
   ret->func = NULL;
   ret->parent = -1;
   ret->status = CO_RUNNING; 
   ret->id = MAXCO - 1;
   AddCoroutine(coarray, ret);
}

__attribute__((destructor)) static void co_destruct() {
   DeleteCoArray(coarray);
}

int co_start(int (*routine)(void)) {
   int id = CreateCoroutine(routine, current_running_id);
   switch_id = id;
   co_yield();
   return id;
}

int co_getid() {
   return current_running_id;
}

int co_getret(int cid) {
   return GetCoroutine(coarray, cid)->retval;
}

int co_yield() {
   // static int cnt = 0;
   // if (++cnt == 100) {
   //    puts("too much co_yield");
   //    exit(0);
   // }
   
   int t = setjmp(GetCoroutine(coarray, current_running_id)->context);
   // printf("setjmp: %d %d\n", current_running_id, t);
   if (!t) { // switch to another coroutine
      int id = switch_id == -1 ? GetRunningCo() : switch_id;
      // printf("%d switch to: %d\n", current_running_id, id);
      if (id == current_running_id) return 0;
      switch_id = -1;
      if (GetCoroutine(coarray, id)->status == CO_NEW) { // sub-coroutine
         int retval = 0;
         unsigned char * sp = (GetCoroutine(coarray, id)->stack + STACK_SIZE);
         current_running_id = id;
         SetCoStaues(coarray, id, CO_RUNNING);
         asm volatile (
            "movq %1, %%rsp; call *%2; movl %%eax, %0;"
            : "=m"(retval)
            : "r"(sp), "r"((void*) GetCoroutine(coarray, id)->func)
            : "eax", "memory"
         );
         id = current_running_id;
         // printf("coroutine %d set to CO_DEAD\n", id);
         SetCoStaues(coarray, id, CO_DEAD);
         SetRetValue(coarray, id, retval);
         current_running_id = GetCoroutine(coarray, id)->parent;
         // printf("longjmpA: %d to %d\n", id, GetCoroutine(coarray, id)->parent);
         longjmp(GetCoroutine(coarray, GetCoroutine(coarray, id)->parent)->context, 1);
      } else {
         // printf("longjmpB: %d to %d\n", current_running_id, id);
         current_running_id = id;
         longjmp(GetCoroutine(coarray, id)->context, 1);
      }
      return 0;
   } else { // switch back
      return 0;
   }
}

int co_waitall() {
   int flag = 0;
   while (1) {
      flag = 1;
      for (int i = 0; i < MAXCO; ++i) {
         if (i != current_running_id && GetCoroutine(coarray, i) != NULL && GetCoroutine(coarray, i)->status != CO_DEAD) {
            flag = 0;
         }
      }
      if (flag == 1) break;
      co_yield();
   }
   return 0;
}

int co_wait(int cid) {
   while (GetCoroutine(coarray, cid)->status != CO_DEAD) {
      co_yield();
   }
   return 0;
}

int checkAuthrize(int child, int ancestor) {
   if (GetCoroutine(coarray, child) == NULL) return 0;
   while (child != -1) {
      if (child == ancestor) return 1;
      child = GetCoroutine(coarray, child)->parent;
   }
   return 0;
}

int co_status(int cid) {
   // printf("checkAuthrize: %d %d\n", cid, current_running_id);
   // printf("co_status: %d %d %d\n", cid, GetCoroutine(coarray, cid)->status, checkAuthrize(current_running_id, cid));
   if (checkAuthrize(cid, current_running_id)) {
      if (GetCoroutine(coarray, cid)->status == CO_DEAD) {
         return FINISHED;
      } else {
         return RUNNING;
      }
   } else {
      return UNAUTHORIZED;  
   }
}

int GetRunningCo() {
   static int las = 0;
   int i, flag = 0;
   for (i = nex(las); i != las; i = nex(i)) {
      if (i != current_running_id &&
          GetCoroutine(coarray, i) != NULL &&
          GetCoroutine(coarray, i)->status != CO_DEAD) {
            flag = 1;
            break;
         }
   }
   if (flag == 0) {
      return current_running_id; // no other coroutine
   }
   las = i;
   return las;
}

int GetNewID() {
   for (int i = 0; i < MAXCO; ++i) {
      if (GetCoroutine(coarray, i) == NULL) {
         return i;
      }
   }
   fprintf(stderr, "No available coroutine space.");
   exit(-1);
}

int CreateCoroutine(int (*routine)(void), int parent) {
   int id = GetNewID();
   struct Coroutine * ret = (struct Coroutine *) malloc(sizeof(struct Coroutine));
   ret->id = id;
   ret->func = routine;
   ret->parent = parent;
   ret->status = CO_NEW;
   AddCoroutine(coarray, ret);
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

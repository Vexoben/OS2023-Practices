/* YOUR CODE HERE */

#ifndef COROUTINE_H
#define COROUTINE_H

#include <setjmp.h>

typedef long long cid_t;
#define MAXN 1000
#define MAXCO 10005
#define UNAUTHORIZED -1
#define FINISHED 2
#define RUNNING 1

#define STACK_SIZE (1 << 16)

enum co_status {
  CO_NEW,
  CO_RUNNING,
  CO_DEAD 
};

struct Coroutine {
  int (*func)(void);
  int id, parent;
  int retval;
  enum co_status status;
  jmp_buf context;
  unsigned char stack[STACK_SIZE];
};

int co_start(int (*routine)(void));
int co_getid();
int co_getret(int cid);
int co_yield();
int co_waitall();
int co_wait(int cid);
int co_status(int cid);

int GetRunningCo();
int GetNewID();
int CreateCoroutine(int (*routine)(void), int parent);

struct CoArray {
  int length;
  struct Coroutine ** head;
};

void SetRetValue(struct CoArray * arr, int id, int value);
void SetCoStaues(struct CoArray * arr, int id, enum co_status status);

struct CoArray * CoArrayNil();
void AddCoroutine(struct CoArray * arr, struct Coroutine * co);
struct Coroutine * GetCoroutine(struct CoArray * arr, int index);
void DeleteCoArray(struct CoArray * arr);

#endif
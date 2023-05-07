#ifndef OS_MM_H
#define OS_MM_H
#define MAX_ERRNO 4095
#define PAGE_SIZE 4096

#define OK          0
#define EINVAL      22  /* Invalid argument */    
#define ENOSPC      28  /* No page left */  


#define IS_ERR_VALUE(x) ((x) >= (unsigned long)-MAX_ERRNO)
static inline void *ERR_PTR(long error) { return (void *)error; }
static inline long PTR_ERR(const void *ptr) { return (long)ptr; }
static inline long IS_ERR(const void *ptr) { return IS_ERR_VALUE((unsigned long)ptr); }

struct List;

struct BuddyNode {
   int used, rank;
   void *data;
   struct BuddyNode *buddy, *parent;
   struct List * parent_list;
};

struct List {
   struct BuddyNode * head;
   struct List *next, *prev;
};

struct List* ListNil();
struct List* ListCons(struct BuddyNode * data, struct List *list);
int ListFindAndDelete(struct BuddyNode* node, struct List *list, struct List ** head);

int init_page(void *p, int pgcount);
void *alloc_pages(int rank);
int return_pages(void *p);
int query_ranks(void *p);
int query_page_counts(int rank);

#endif
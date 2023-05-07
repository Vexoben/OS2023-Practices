#include "buddy.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define NULL ((void *)0)
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

int maxrank;
void *page_front, *page_end;
struct List *used[30], *unused[30];
struct List *page_map[1 << 16 | 3];

int pageID(void *x) {
    return (x - page_front) / PAGE_SIZE;
}

struct List* ListNil() {
    return NULL;
}

struct List* ListCons(struct BuddyNode * data, struct List *list) {
    assert(data != NULL);
    struct List* ret = (struct List*) malloc(sizeof(struct List));
    if (list != NULL) list->prev = ret;
    ret->prev = NULL;
    ret->head = data;
    ret->next = list;
    return ret;
}

struct BuddyNode* ListFindPage(void * data, struct List *list) {
    // assert(data != NULL);
    // struct List* ret = list;
    // while (ret != NULL && ret->head->data != data) ret = ret->next;
    // if (ret == NULL) return NULL;
    // else return ret->head;
    assert(data != NULL);
    if (list == NULL) return NULL;
    struct List* ret = page_map[pageID(data)];
    if (list->head->rank != ret->head->rank || list->head->used != ret->head->used) {
        return NULL;
    } else {
        return ret->head;
    }
}

int ListFindAndDelete(struct BuddyNode* node, struct List *list, struct List ** head) {
    // assert(node != NULL);
    // struct List *prev = NULL, *cur = list;
    // while (cur != NULL && cur->head != node) {
    //     prev = cur;
    //     cur = cur->next;
    // }
    // if (cur == NULL) return 0;
    // else {
    //     if (prev != NULL) prev->next = cur->next;
    //     else (*head) = cur->next;
    //     free(cur);
    //     return 1;
    // }
    assert(node != NULL);
    if (list == NULL) return 0;
    struct List * tmp = page_map[pageID(node->data)];
    assert(tmp != NULL);
    if (list->head->rank != tmp->head->rank || list->head->used != tmp->head->used) return 0;
    if (tmp->prev != NULL) tmp->prev->next = tmp->next;
    else (*head) = tmp->next;
    if (tmp->next != NULL) tmp->next->prev = tmp->prev;
    free(tmp);
    return 1;
}

int init_page(void *p, int pgcount){
    int c = 1;
    maxrank = 1;
    page_front = p;
    page_end = p + pgcount * PAGE_SIZE;
    while (c != pgcount) {
        c <<= 1;
        ++maxrank;
    }
    struct BuddyNode* node = (struct BuddyNode*) malloc(sizeof(struct BuddyNode));
    node->data = p;
    node->buddy = node->parent = NULL;
    node->rank = maxrank;
    node->used = 0;
    node->parent_list = NULL;
    unused[maxrank] = ListCons(node, unused[maxrank]);
    page_map[pageID(p)] = unused[maxrank];
    return OK;
}

void* split(int rank, int need) {
    struct List * node = unused[rank];
    while (rank > need) {
        unused[rank] = node->next;
        node->next = NULL;
        struct BuddyNode* t1 = (struct BuddyNode*) malloc(sizeof(struct BuddyNode));
        struct BuddyNode* t2 = (struct BuddyNode*) malloc(sizeof(struct BuddyNode));
        t1->buddy = t2;
        t2->buddy = t1;
        t1->parent = t2->parent = node->head;
        t1->data = node->head->data;
        t2->data = node->head->data + (PAGE_SIZE << (rank - 2));
        t1->used = t2->used = 0;
        t1->rank = t2->rank = rank - 1;
        node->head->used = 1;
        used[rank] = ListCons(node->head, used[rank]);
        t1->parent_list = t2->parent_list = used[rank];
        unused[rank - 1] = ListCons(t2, unused[rank - 1]);
        page_map[pageID(t2->data)] = unused[rank - 1];
        unused[rank - 1] = ListCons(t1, unused[rank - 1]);
        page_map[pageID(t1->data)] = unused[rank - 1];
        free(node);
        node = unused[rank - 1];
        --rank;
    }
    unused[rank] = node->next;
    node->next = NULL;
    node->head->used = 1;
    used[rank] = ListCons(node->head, used[rank]);
    page_map[pageID(node->head->data)] = used[rank];
    void* ret = node->head->data;
    free(node);
    return ret;
}

void *alloc_pages(int rank){
    if (rank < 1 || rank > EINVAL) {
        return (void*) -EINVAL;
    } else {
        for (int i = rank; i <= maxrank; ++i) {
            if (unused[i] != NULL) {
                return split(i, rank);
            }
        }
    }
    return (void*) -ENOSPC;
}

void merge(struct BuddyNode* node, int rank) {
    assert(node != NULL);
    struct BuddyNode* buddy = node->buddy;
    if (buddy != NULL && ListFindAndDelete(buddy, unused[rank], &unused[rank])) {
        ListFindAndDelete(node, used[rank], &used[rank]);
        page_map[pageID(MIN(node->data, buddy->data))] = node->parent_list;
        page_map[pageID(MAX(node->data, buddy->data))] = NULL;
        merge(buddy->parent, rank + 1);
        free(buddy);
        free(node);
    } else {
        ListFindAndDelete(node, used[rank], &used[rank]);
        unused[rank] = ListCons(node, unused[rank]);
        node->used = 0;
        page_map[pageID(node->data)] = unused[rank];
    }
}

int return_pages(void *p) {
    if (p < page_front || p >= page_end || (p - page_front) % PAGE_SIZE != 0) return -EINVAL;
    for (int i = 1; i <= maxrank; ++i) {
        struct BuddyNode* ret = ListFindPage(p, used[i]);
        if (ret != NULL) {
            merge(ret, i);
            return OK;
        }
    }
    return -EINVAL;
}

int query_ranks(void *p){
    if (p < page_front || p >= page_end || (p - page_front) % PAGE_SIZE != 0) return -EINVAL;
    for (int i = 1; i <= maxrank; ++i) {
        struct BuddyNode* ret = ListFindPage(p, used[i]);
        if (ret != NULL) {
            return i;
        }
    }
    for (int i = maxrank; i >= 1; --i) {
        struct BuddyNode* ret = ListFindPage(p, unused[i]);
        if (ret != NULL) {
            return i;
        }
    }
    return -1;
}

int query_page_counts(int rank){
    if (rank < 1 || rank > EINVAL) {
        return -EINVAL;
    }
    int cnt = 0;
    struct List * p = unused[rank];
    while (p != NULL) {
        ++cnt;
        p = p->next;
    }
    return cnt;
}

__attribute((destructor)) void destruct() {
    for (int i = 1; i <= maxrank; ++i) {
        struct List * list, *tmp;
        for (list = unused[i]; list != NULL; list = tmp) {
            tmp = list->next;
            free(list->head);
            free(list);
        }
        for (list = used[i]; list != NULL; list = tmp) {
            tmp = list->next;
            free(list->head);
            free(list);
        }
    }
}

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
// #define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define WSIZE 4
#define DSIZE 8
 
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (unsigned int) (val))

#define HDRP(bp) ((void*)((char*)(bp) - WSIZE))
#define GET_SIZE(bp) (GET(HDRP(bp)) & ~0x7)
#define GET_ALLOC(bp) (GET(HDRP(bp)) & 0x7)
#define FTRP(bp) ((void*)((char*)(bp) + GET_SIZE(bp) - DSIZE))
#define PREV_RP(bp) ((void*)(bp))
#define NEXT_RP(bp) ((void*)((char*)(bp) + WSIZE))

#define NEXT_BLKP_MEM(bp) ((void*)((char*)(bp) + GET_SIZE(bp)))
#define PREV_BLKP_MEM(bp) ((void*)((char*)(bp) - GET_SIZE(((void*)bp) - WSIZE)))

#define PREV_BLKP_LIST(bp) ((void*)(bottom + (GET(PREV_RP(bp)))))
#define NEXT_BLKP_LIST(bp) ((void*)(bottom + (GET(NEXT_RP(bp)))))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
  
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define SIZE_PTR(p)  ((size_t*)(((char*)(p)) - SIZE_T_SIZE))

/*

alloc: 1 for available, 0 for busy
the lowest bit denotes the state for current block
the second lowest bit denotes the state for previous(in memory, not in list) block
the third lowest bit is always 1 for debug

for assigned page:          for unsigned page:
------------                ------------ 
|size|alloc|                |size|alloc|
------------                ------------
|          |                |   prev   |
|          |                |   next   |
|   data   |                |          |
|          |                |   data   |
|          |                |          |
------------                ------------
|size|alloc|                |size|alloc|
------------                ------------
*/

void *bottom, *list_head, *list_tail, *mem_tail;

#ifdef DEBUG
int block_cnt;
#endif

/*
 * mm_init - Called when a new trace starts.
 */
int mm_init(void) {
  bottom = mem_sbrk(32 * WSIZE + WSIZE) + WSIZE;
  if ((long) bottom < 0) return -1;
  list_head = bottom + WSIZE;
  list_tail = mem_tail = list_head;
  PUT(PREV_RP(list_head), 0);
  PUT(NEXT_RP(list_head), 0);
  PUT(HDRP(list_head), PACK(32 * WSIZE, 5));
  PUT(FTRP(list_head), PACK(32 * WSIZE, 5));
#ifdef DEBUG 
  printf("%lld %lld %lld\n", (long long) bottom, (long long) list_head, (long long) list_tail);
  block_cnt = 1;
#endif
  return 0;
}

void* first_fit_front(size_t size) {
  void * pos = list_head;
  while (pos != bottom && GET_SIZE(pos) < size) {
    pos = NEXT_BLKP_LIST(pos);
  }
  return pos;
}

void* first_fit_back(size_t size) {
  void * pos = list_tail;
  while (pos != bottom && GET_SIZE(pos) < size) {
    pos = PREV_BLKP_LIST(pos);
  }
  return pos;
}

void* find_fit(size_t size) {
  // if (size <= 64) return first_fit_front(size);
  // else return first_fit_back(size);
  return first_fit_back(size);

  // best fit
  // void * pos = list_head, *min_pos = bottom;
  // unsigned int min_size = 0xffffffff, cnt = 0;
  // while (pos != bottom) {
  //   if (GET_SIZE(pos) >= size) {
  //     if (GET_SIZE(pos) < min_size) {
  //       min_pos = pos;
  //       min_size = GET_SIZE(pos);
  //       if (++cnt == 2) break;
  //     }
  //   }
  //   pos = NEXT_BLKP_LIST(pos);
  // }
  // return min_pos;
}

void list_delete(void *p) {
  void *prev = PREV_BLKP_LIST(p), *next = NEXT_BLKP_LIST(p);
  if (list_head == p) list_head = next;
  if (list_tail == p) list_tail = prev;
  if (prev != bottom) {
    PUT(NEXT_RP(prev), GET(NEXT_RP(p)));
  }
  if (next != bottom) {
    PUT(PREV_RP(next), GET(PREV_RP(p)));
  }
}

void list_push_front(void *p) {
  PUT(PREV_RP(p), 0);
  PUT(NEXT_RP(p), list_head - bottom);
  if (list_head != bottom) {
    PUT(PREV_RP(list_head), p - bottom);
  }
  if (list_tail == bottom) list_tail = p;
  list_head = p;
}

void list_push_back(void *p) {
  PUT(PREV_RP(p), list_tail - bottom);
  PUT(NEXT_RP(p), 0);
  if (list_tail != bottom) {
    PUT(NEXT_RP(list_tail), p - bottom);
  }
  if (list_head == bottom) list_head = p;
  list_tail = p;
}

void list_insert(void *p) {
#ifdef DEBUG
  printf("list_insert: %lld %lld %lld %lld\n", (long long) p, (long long) PREV_RP(p), (long long) NEXT_RP(p), (long long) PREV_RP(list_head));
#endif
  // if (GET_SIZE(p) <= 64) list_push_front(p);
  // else list_push_back(p);
  list_push_front(p);
}

void* split_block(void * bp, int size) {
  // If the remaining memory is less than 4 words, don't split it
#ifdef DEBUG
  printf("%lld\n", (long long) bp);
  printf("size need: %d\n", size);
  printf("%d\n", GET_SIZE(bp));
#endif
  if (GET_SIZE(bp) - size < 16) {
    unsigned int bp_alloc = PACK(GET_SIZE(bp), GET_ALLOC(bp) ^ 1);
    PUT(HDRP(bp), bp_alloc);
    PUT(FTRP(bp), bp_alloc);
    list_delete(bp);
    void * tmp = NEXT_BLKP_MEM(bp);
    if (tmp <= mem_heap_hi()) {
      unsigned int tmp_alloc = GET(HDRP(tmp)) ^ 2;
      PUT(HDRP(tmp), tmp_alloc);
      if (tmp_alloc & 1) PUT(FTRP(tmp), tmp_alloc);
    }
    return tmp;
  }
#ifdef DEBUG
  void * next_pos = NEXT_BLKP_MEM(bp);
  puts("--------");
  void * pos = bottom + WSIZE;
  int cnt = 0;
  while (pos <= mem_heap_hi()) {
    printf("%lld %d\n", (long long) pos, GET_SIZE(pos));
    pos = NEXT_BLKP_MEM(pos);
    ++cnt;
  }
  printf("------");
#endif 
  unsigned int remain_size = GET_SIZE(bp) - size;
  unsigned int bp_alloc = PACK(size, GET_ALLOC(bp) ^ 1);
  PUT(HDRP(bp), bp_alloc);
  PUT(FTRP(bp), bp_alloc);
  void* y = NEXT_BLKP_MEM(bp);
  unsigned int y_alloc = PACK(remain_size, 5);
  PUT(HDRP(y), y_alloc);
  PUT(FTRP(y), y_alloc);
  list_delete(bp);
  list_insert(y);
  if (bp == mem_tail) mem_tail = y;
#ifdef DEBUG
  ++block_cnt;
  assert(next_pos == NEXT_BLKP_MEM(y));
  mm_checkheap(__LINE__);
#endif
  return y;
}

/*
 * malloc - Allocate a block
 *      Always allocate a block whose size is a multiple of the alignment.
 */
void *malloc(size_t size) {
#ifdef DEBUG
  mm_checkheap(__LINE__);
  printf("malloc: %ld\n", size);
#endif  
  if (size == 0) return NULL;
  unsigned int new_size = DSIZE * ((size + WSIZE + DSIZE - 1) / DSIZE);
  if (new_size < 2 * DSIZE) new_size = 2 * DSIZE;
  void *bp = find_fit(new_size);
  #ifdef DEBUG
    printf("new size: %d\n", new_size);
    printf("find fit: %lld\n", (long long)bp);
  #endif
  if (bp != bottom) {
    split_block(bp, new_size);
    #ifdef DEBUG
      printf("ret0: %lld\n", (long long) bp);
      print_mem_blocks();
    #endif
    return bp;
  } else {
    bp = mem_tail;
    if ((GET_ALLOC(bp) & 1)) {
      // printf("mem_sbrk: %d %d\n", __LINE__, new_size - GET_SIZE(bp));
      // printf("%d %d\n", new_size, GET_SIZE(bp));
      mem_sbrk(new_size - GET_SIZE(bp));
      unsigned int bp_alloc = PACK(new_size, GET_ALLOC(bp) ^ 1);
      PUT(HDRP(bp), bp_alloc);
      PUT(FTRP(bp), bp_alloc);
      list_delete(bp);
      #ifdef DEBUG
        printf("ret1: %lld\n", (long long) bp);
        print_mem_blocks();
      #endif
      return bp;
    } else {
      // printf("mem_sbrk: %d %d\n", __LINE__, new_size);
      bp = mem_sbrk(new_size) + WSIZE;
      int tmp = GET_ALLOC(mem_tail) & 1 ? 2 : 0;
      mem_tail = bp;
      unsigned int bp_alloc = PACK(new_size, 4 ^ tmp);
      PUT(HDRP(bp), bp_alloc);
      PUT(FTRP(bp), bp_alloc);
      #ifdef DEBUG
        ++block_cnt;
        printf("ret2: %lld\n", (long long) bp);
        print_mem_blocks();
      #endif
      return bp;
    }
  }
}

/*
 * free - Free a block and coalesce adjecent available blocks
 */
void free(void *ptr) {
#ifdef DEBUG
  mm_checkheap(__LINE__);
  printf("free %lld\n", (long long) (ptr));
  if (ptr == NULL) return;
  assert(GET_ALLOC(ptr) & 4);
  assert((GET_ALLOC(ptr) & 1) == 0);
#endif
  if (ptr == NULL) return;
  void *prev_pos = (ptr - WSIZE == bottom || (GET_ALLOC(ptr) & 2) == 0) ? NULL : PREV_BLKP_MEM(ptr);
  void *next_pos = (NEXT_BLKP_MEM(ptr) > mem_heap_hi()) ? NULL : NEXT_BLKP_MEM(ptr);
  unsigned int prev_alloc = prev_pos == NULL ? 0 : GET(HDRP(prev_pos));
  unsigned int next_alloc = next_pos == NULL ? 0 : GET(HDRP(next_pos));
  if ((prev_alloc & 1) == 0 && (next_alloc & 1) == 0) { // no coalesce
    #ifdef DEBUG
      printf("free type 0\n");
    #endif
    unsigned int ptr_alloc = GET(HDRP(ptr)) ^ 1;
    PUT(HDRP(ptr), ptr_alloc);
    PUT(FTRP(ptr), ptr_alloc);
    list_insert(ptr);
    if (next_pos != NULL) {
      unsigned int new_alloc = next_alloc ^ 2;
      PUT(HDRP(next_pos), new_alloc);
    }
    #ifdef DEBUG
      print_mem_blocks();
    #endif    
  } else if ((prev_alloc & 1) == 0 && (next_alloc & 1) == 1) { // coalesce to next
    #ifdef DEBUG
      printf("free type 1\n");
    #endif
    unsigned int size = GET_SIZE(ptr) + GET_SIZE(next_pos);
    unsigned int ptr_alloc = PACK(size, GET_ALLOC(ptr) ^ 1);
    // void *nn_pos = NEXT_BLKP_MEM(next_pos) > mem_heap_hi() ? NULL : NEXT_BLKP_MEM(next_pos);
    PUT(HDRP(ptr), ptr_alloc);
    PUT(FTRP(ptr), ptr_alloc);
    list_delete(next_pos);
    list_insert(ptr);
    if (mem_tail == next_pos) mem_tail = ptr;
    // if (nn_pos != NULL) {
    //   unsigned int new_alloc = GET_ALLOC(nn_pos);
    //   PUT(HDRP(nn_pos), new_alloc); 
    //   if (new_alloc & 1) PUT(FTRP(nn_pos), new_alloc);
    //   assert((new_alloc & 2));
    //   assert(NEXT_BLKP_MEM(ptr) == nn_pos);
    // }
    #ifdef DEBUG
      print_mem_blocks();
      --block_cnt;
    #endif    
  } else if ((prev_alloc & 1) == 1 && (next_alloc & 1) == 0) { // coalesce to prev 
    #ifdef DEBUG
      printf("free type 2\n");
    #endif
    unsigned int size = GET_SIZE(prev_pos) + GET_SIZE(ptr);
    unsigned int ptr_alloc = PACK(size, GET_ALLOC(prev_pos));
    // printf("%lld\n", (long long) bottom);
    // printf("%lld %lld %lld %lld %lld\n", (long long) ptr, (long long) prev_pos, (long long) HDRP(prev_pos), (long long) FTRP(prev_pos), (long long) next_pos);
    // printf("%d\n", GET_ALLOC(prev_pos));
    // printf("%d %d %d\n", size, GET_SIZE(prev_pos), GET_SIZE(ptr));
    PUT(HDRP(prev_pos), ptr_alloc);
    PUT(FTRP(prev_pos), ptr_alloc);
    if (next_pos != NULL) {
      unsigned int new_alloc = next_alloc ^ 2;
      PUT(HDRP(next_pos), new_alloc);
    }
    if (mem_tail == ptr) mem_tail = prev_pos;
    #ifdef DEBUG
      print_mem_blocks();
      --block_cnt;
      mm_checkheap(__LINE__);
    #endif    
  } else {  // coalesce both next and prev
    #ifdef DEBUG
      printf("free type 3\n");
    #endif
    unsigned int size = GET_SIZE(prev_pos) + GET_SIZE(ptr) + GET_SIZE(next_pos);
    unsigned int ptr_alloc = PACK(size, GET_ALLOC(prev_pos));
    // void *nn_pos = NEXT_BLKP_MEM(next_pos) > mem_heap_hi() ? NULL : NEXT_BLKP_MEM(next_pos);
    PUT(HDRP(prev_pos), ptr_alloc);
    PUT(FTRP(prev_pos), ptr_alloc);
    list_delete(next_pos);
    if (mem_tail == next_pos) mem_tail = prev_pos;
    // if (nn_pos != NULL) {
    //   unsigned int new_alloc = GET(HDRP(nn_pos));
    //   PUT(HDRP(nn_pos), new_alloc);
    //   if (new_alloc & 1) PUT(FTRP(nn_pos), new_alloc);
    // }
    #ifdef DEBUG
      print_mem_blocks();
      block_cnt -= 2;
    #endif    
  }
}

/*
 * realloc - Change the size of the block by mallocing a new block,
 *      copying its data, and freeing the old block.  I'm too lazy
 *      to do better.
 */
void *realloc(void *oldptr, size_t size) {
#ifdef DEBUG
  printf("reallocc %lld %ld\n", (long long) (oldptr), size);
  mm_checkheap(__LINE__);
#endif
  size_t oldsize;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if(size == 0) {
    #ifdef DEBUG    
      printf("realloc: only free\n");
    #endif
    free(oldptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if(oldptr == NULL) {
    #ifdef DEBUG    
      printf("realloc: only malloc\n");
    #endif
    return malloc(size);
  }

  newptr = malloc(size);

  /* If realloc() fails the original block is left untouched  */
  if(!newptr) {
    return 0;
  }
  #ifdef DEBUG    
    printf("realloc: malloc and free\n");
  #endif
  /* Copy the old data. */
  oldsize = GET_SIZE(oldptr);
  if(size < oldsize) oldsize = size;
  memcpy(newptr, oldptr, oldsize);

  /* Free the old block. */
  free(oldptr);

  return newptr;
}

/*
 * calloc - Allocate the block and set it to zero.
 */
void *calloc (size_t nmemb, size_t size) {
#ifdef DEBUG
  mm_checkheap(__LINE__);
#endif  
  size_t bytes = nmemb * size;
  void *newptr;

  newptr = malloc(bytes);
  memset(newptr, 0, bytes);

  return newptr;
}



void print_mem_blocks() {
  printf("mem_blocks:\n");
  void * pos = bottom + WSIZE;
  while (pos <= mem_heap_hi()) {
    assert(GET_ALLOC(pos) & 4);
    printf("%lld(%d) ", (long long) pos, GET_ALLOC(pos) & 3);
    pos = NEXT_BLKP_MEM(pos);
  }
  printf("\n");
}

/*
 * mm_checkheap - check heap
 */
void mm_checkheap(int verbose) {
#ifdef DEBUG    
  printf("begin mm_checkheap %d\n", verbose);
  // check mem_block
  printf("memcheck_list_head: %lld\n", (long long) list_head);
  void * pos = bottom + WSIZE;
  int cnt = 0;
  while (pos <= mem_heap_hi()) {
    pos = NEXT_BLKP_MEM(pos);
    ++cnt;
  }
  printf("cnt/block_cnt: %d %d\n", cnt, block_cnt);
  if (cnt != block_cnt) {
    printf("cnt/block_cnt fail:\n");
    pos = bottom + WSIZE;
    int cnt = 0;
    while (pos <= mem_heap_hi()) {
      printf("%lld %d\n", (long long) pos, GET_SIZE(pos));
      pos = NEXT_BLKP_MEM(pos);
      ++cnt;
    }
    printf("end with: %lld\n", (long long) pos);
  }
  assert(cnt == block_cnt);

  pos = bottom + WSIZE;
  assert(pos >= mem_heap_lo() && pos <= mem_heap_hi());
  assert((long long) (pos) % 8 == 0);

  void *tmp;
  
  while (pos <= mem_heap_hi()) {
    tmp = NEXT_BLKP_MEM(pos);
    assert(tmp >= pos);
    assert(GET_ALLOC(pos) & 4);
    if (GET_ALLOC(pos) & 1) { // if available, header == footor, and the next block should be unavailable
      assert(GET(HDRP(pos)) == GET(FTRP(pos)));
      if (tmp <= mem_heap_hi()) {
        assert((GET_ALLOC(tmp) & 1) == 0);
        if ((GET_ALLOC(tmp) & 2) != 2) {
          printf("fail assert: %lld\n", (long long) pos);
        }
        assert((GET_ALLOC(tmp) & 2));
      }
    } else { // if not available, the next block's header should record it.
      if (tmp <= mem_heap_hi()) {
        assert((GET_ALLOC(tmp) & 2) == 0);
      }
    }
    if (NEXT_BLKP_MEM(pos) > mem_heap_hi()) {
      assert(pos == mem_tail);
    }
    pos = NEXT_BLKP_MEM(pos);
    assert(pos >= mem_heap_lo());
  }

  // check mem_list
  void * head = list_head;
  assert(list_head == bottom || PREV_BLKP_LIST(head) == bottom);
  if (list_head == bottom) assert(list_tail == bottom);
  while (head != bottom) {
    assert(GET_ALLOC(head) & 1);
    // void * prev = PREV_BLKP_LIST(head);
    void * next = NEXT_BLKP_LIST(head);
    if (next != bottom) {
      assert(PREV_BLKP_LIST(next) == head);
    } else {
      assert(head == list_tail);
    }
    head = next;
  }
#endif
}
 

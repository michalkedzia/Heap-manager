#ifndef __HEAP_H__
#define __HEAP_H__
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include "custom_unistd.h"

struct block_fence_t
{
    uint8_t pattern[8];
};

struct memblock_t
{
    struct block_fence_t head_fence;
    int check_sum;
    bool used;
    struct memblock_t *prev;
    struct memblock_t *next;

    const char *filaname;
    int line;
    size_t size;
    struct block_fence_t tail_fence;
};

struct heap_t
{
    bool init;

    struct memblock_t *phead;
    struct memblock_t *ptail;
    
    intptr_t start_brk;
    int check_sum;
};

enum pointer_type_t
{
    pointer_null,
    pointer_out_of_heap,
    pointer_control_block,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

#define MEM_BLCK_SIZE (sizeof(struct memblock_t))
#define FENCE_SIZE (sizeof(struct block_fence_t))
#define WORD (sizeof(void *))
#define SBRK_FAIL 	((void*)-1)
#define PAGE_SIZE       4096    
#define PAGE_FENCE      1       
#define PAGES_AVAILABLE 16384   
#define PAGES_TOTAL     (PAGES_AVAILABLE + 2 * PAGE_FENCE)
#define PATTERN_SUM 28
#define MB 1048576

int heap_setup(void);

void* heap_malloc(size_t count);
void* heap_calloc(size_t number, size_t size);
void heap_free(void* memblock);
void* heap_realloc(void* memblock, size_t size);

void* heap_malloc_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_debug(void* memblock, size_t size, int fileline,const char* filename);

void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);

void* heap_malloc_aligned_debug(size_t count, int fileline,const char* filename);
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline,const char* filename);
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline,const char* filename);

size_t heap_get_used_space(void);
size_t heap_get_largest_used_block_size(void);
uint64_t heap_get_used_blocks_count(void);
size_t heap_get_free_space(void);
size_t heap_get_largest_free_area(void);
uint64_t heap_get_free_gaps_count(void);


enum pointer_type_t get_pointer_type(const void* pointer);
void* heap_get_data_block_start(const void* pointer);
size_t heap_get_block_size(const void* memblock);
int heap_validate(void);
void heap_dump_debug_information(void);

void RESET_RESOURCES(void);
void UTEST(void);

#endif
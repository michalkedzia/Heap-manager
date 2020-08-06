#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>
#include "custom_unistd.h"
#include "heap.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct heap_t heap;

size_t heap_size (void)
{
    return ((intptr_t)custom_sbrk(0) - (intptr_t)heap.start_brk );
}

int calculate_check_sum_block(struct memblock_t * ptr)
{
    uint8_t * pointer = (uint8_t *)ptr;
    int temp=ptr->check_sum;
    ptr->check_sum=0;

    int checksum = 0;
    for(int i=0 ; i < MEM_BLCK_SIZE ;i++)
    {
        checksum+= *(pointer + i);
    }

    ptr->check_sum=temp;
    return checksum;
}


void set_check_sum_block(struct memblock_t * ptr)
{
    uint8_t * pointer = (uint8_t *)ptr;
    ptr->check_sum=0;

    int checksum = 0;
    for(int i=0 ; i < MEM_BLCK_SIZE ;i++)
    {
        checksum+= *(pointer + i);
    }

    ptr->check_sum=checksum;
    
    return ;
}


void set_fences(struct memblock_t * ptr)
{
    if(ptr==NULL) return;
    
    for(unsigned int i=0;i<sizeof(struct block_fence_t);i++)
    {
        ptr->head_fence.pattern[i]=ptr->tail_fence.pattern[i]=i;
    }
    
    return;
}


int heap_setup(void)
{
    pthread_mutex_lock(&mutex);

    if(heap.init)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    void * memory_sbrk = custom_sbrk(PAGE_SIZE * 1);

    if(memory_sbrk == SBRK_FAIL)
    {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    heap.start_brk=memory_sbrk;
    
    struct memblock_t * first = (struct memblock_t *)memory_sbrk;
    struct memblock_t * mid =(struct memblock_t*)(first + 1);
    struct memblock_t * last =(struct memblock_t*)(memory_sbrk + PAGE_SIZE - sizeof(struct memblock_t));

    first->prev=NULL;
    first->next=mid;
    first->size=0;
    set_fences(first);
    first->used=true;
    first->line=0;
    first->filaname=NULL;
    
    mid->prev=first;
    mid->next=last;
    mid->size=(PAGE_SIZE - 3*MEM_BLCK_SIZE);
    set_fences(mid);
    mid->used=false;
    mid->line=0;
    mid->filaname=NULL;

    last->prev=mid;
    last->next=NULL;
    last->size=0;
    set_fences(last);
    last->used=true;
    last->line=0;
    last->filaname=NULL;

    heap.phead=first;
    heap.ptail=last;
    heap.init=true;

    set_check_sum_block(first);
    set_check_sum_block(mid);
    set_check_sum_block(last);
    heap.check_sum=first->check_sum;

    pthread_mutex_unlock(&mutex);
    return 0;
}

size_t round_to_CPU_word(size_t count)
{
    size_t temp=count / WORD;
    if(count - (WORD * temp)) temp++;
    return temp * WORD;
}

struct memblock_t * find_memblock(size_t count)
{
    struct memblock_t * ptr=heap.phead;
    int size=0;
    bool init=false;
    struct memblock_t *min=NULL;

    while(ptr)
    {
        if(ptr->used==false)
        {
            if( ((int)ptr->size - (int)(count + sizeof(struct memblock_t)))>=0  )
            {
                if(init==false)
                {
                    size=ptr->size;
                    min=ptr;
                    init=true;
                }
                else
                {
                    if(((int)ptr->size - (count + sizeof(struct memblock_t)) )< size)
                    {
                        size=ptr->size;
                        min=ptr;
                    }
                }
            }
        }
        ptr=ptr->next;
    }
    return min;
}





int round_to_pages(int bytes)
{
    int temp=bytes / PAGE_SIZE;
    if(bytes - (PAGE_SIZE * temp)) temp++;
    return temp;
}

int add_pages_to_heap(int bytes)
{
    if(heap.init==false)
    {
        return 1;
    }

    void * mem = custom_sbrk(round_to_pages(bytes) * PAGE_SIZE);
    if(mem == SBRK_FAIL)
    {
        return 2;
    }

    struct memblock_t * new_blck = (struct memblock_t*)(mem + (round_to_pages(bytes)  * PAGE_SIZE - MEM_BLCK_SIZE));

    new_blck->prev=heap.ptail;
    new_blck->next=NULL;
    new_blck->used=true;
    set_fences(new_blck);
    new_blck->size=0;
    new_blck->line=0;
    new_blck->filaname=NULL;

    heap.ptail->next=new_blck;
    heap.ptail->size=(round_to_pages(bytes)  * PAGE_SIZE - MEM_BLCK_SIZE);
    heap.ptail->used=false;

    heap.ptail=new_blck;

    set_check_sum_block(new_blck);
    set_check_sum_block(heap.ptail);

    return 0;
}


void connect_blocks(void)
{
    struct memblock_t *start = heap.phead->next;
    bool conct=false;

    while(start->next!=heap.ptail)
    {
        struct memblock_t *temp = start;
        if(start->used == false && start->next->used == false)
        {
            start->size= start->size + MEM_BLCK_SIZE + start->next->size;
            start->used=false;
            temp=start->next;
            conct=true;

            start->next=temp->next;
            set_check_sum_block(start);
            temp->next->prev=temp->prev;
            set_check_sum_block(temp->next);
            temp->next=NULL;
            temp->prev=NULL;
            set_check_sum_block(temp);
        }

        if(start->next == heap.ptail)
        {
            if(conct==false) return;
            return connect_blocks();
        }
        start=start->next;
    }

    if(conct==false) return;
    return connect_blocks();
}

void* heap_malloc(size_t count)
{
    if(count <= 0) return NULL;

    size_t count_round = round_to_CPU_word(count);
    
    pthread_mutex_lock(&mutex);

    struct memblock_t *blck = find_memblock(count_round);
    if(blck==NULL)
    {
        if(add_pages_to_heap(count))
        {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }

        connect_blocks();
        pthread_mutex_unlock(&mutex);
        return heap_malloc(count);
    }


    struct memblock_t *new_memblock= (struct memblock_t *)((uint8_t*)blck + MEM_BLCK_SIZE + count_round);

    blck->next->prev=new_memblock;
    set_check_sum_block(blck->next);
    new_memblock->next=blck->next;
    new_memblock->prev=blck;
    blck->next=new_memblock;

    blck->line=0;
    blck->filaname=NULL;

    new_memblock->line=0;
    new_memblock->filaname=NULL;

    new_memblock->size= blck->size - (MEM_BLCK_SIZE + count_round);
    set_fences(new_memblock);
    new_memblock->used=false;

    blck->size=count_round;
    blck->used=true;

    set_check_sum_block(blck);
    set_check_sum_block(new_memblock);

    void * mem_to_user = (void *) ((uint8_t *)blck + MEM_BLCK_SIZE );
    pthread_mutex_unlock(&mutex);
    return mem_to_user;
}

void* heap_calloc(size_t number, size_t size)
{
    pthread_mutex_lock(&mutex);
    if(number == 0 || size == 0 || heap.init==false)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    pthread_mutex_unlock(&mutex);
    size_t bytes = number * size;

    if((bytes / number) != size)
    {
        return NULL;
    }

    void *mem = heap_malloc(bytes);
    if(mem == NULL) return NULL;
    memset(mem, 0, bytes);

    return mem;
}

void* heap_realloc(void* memblock, size_t size)
{
    void *mem=NULL;
    if(memblock==NULL)
    {
        mem=heap_malloc(size);
        if(mem==NULL) return NULL;
        return mem;
    }

    pthread_mutex_lock(&mutex);

    struct memblock_t * blck=(struct memblock_t*)((uint8_t*)memblock - MEM_BLCK_SIZE);

    if(size == 0 )
    {
        heap_free(memblock);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if(blck->size >= size)
    {
        pthread_mutex_unlock(&mutex);
        return memblock;
    }

    pthread_mutex_unlock(&mutex);

    mem = heap_malloc(size);
    if(mem == NULL) return NULL;


    pthread_mutex_lock(&mutex);

    memcpy(mem,memblock,blck->size);
    heap_free(memblock);
    pthread_mutex_unlock(&mutex);

    return mem;
}

void* heap_malloc_debug(size_t count, int fileline, const char* filename)
{
    if(count <= 0) return NULL;

    size_t count_round = round_to_CPU_word(count);

    pthread_mutex_lock(&mutex);
    struct memblock_t *blck = find_memblock(count_round);
    if(blck==NULL)
    {
        if(add_pages_to_heap(count))
        {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        connect_blocks();
        pthread_mutex_unlock(&mutex);

        return heap_malloc(count);
    }

    struct memblock_t *new_memblock= (struct memblock_t *)((uint8_t*)blck + MEM_BLCK_SIZE + count_round);

    blck->filaname=filename;
    blck->line=fileline;

    blck->next->prev=new_memblock;
    set_check_sum_block(blck->next);
    new_memblock->next=blck->next;
    new_memblock->prev=blck;
    blck->next=new_memblock;

    new_memblock->line=0;
    new_memblock->filaname=NULL;

    new_memblock->size= blck->size - (MEM_BLCK_SIZE + count_round);
    set_fences(new_memblock);
    new_memblock->used=false;

    blck->size=count_round;
    blck->used=true;

    void * mem_to_user = (void *) ((uint8_t *)blck + MEM_BLCK_SIZE );

    set_check_sum_block(blck);
    set_check_sum_block(new_memblock);

    pthread_mutex_unlock(&mutex);
    return mem_to_user;
}


void* heap_calloc_debug(size_t number, size_t size, int fileline,const char* filename)
{
    pthread_mutex_lock(&mutex);
    if(number == 0 || size == 0 || heap.init==false)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    pthread_mutex_unlock(&mutex);

    size_t bytes = number * size;

    if((bytes / number) != size)
    {
        return NULL;
    }


    void *mem = heap_malloc_debug(bytes, fileline,filename);
    if(mem == NULL) return NULL;
    memset(mem, 0, bytes);

    return mem;
}




void* heap_realloc_debug(void* memblock, size_t size, int fileline,const char* filename)
{
    void *mem=NULL;

    if(memblock==NULL)
    {
        mem=heap_malloc_debug(size, fileline,filename);
        if(mem==NULL) return NULL;
        return mem;
    }

    struct memblock_t * blck=(struct memblock_t*)((uint8_t*)memblock - MEM_BLCK_SIZE);
    pthread_mutex_lock(&mutex);

    if(size == 0 )
    {
        heap_free(memblock);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if(blck->size >= size)
    {
        pthread_mutex_unlock(&mutex);
        return memblock;
    }

    pthread_mutex_unlock(&mutex);

    mem = heap_malloc_debug(size, fileline,filename);
    if(mem == NULL) return NULL;
    memcpy(mem,memblock,blck->size);

    pthread_mutex_lock(&mutex);
    heap_free(memblock);
    pthread_mutex_unlock(&mutex);


    return mem;
}


void RESET_RESOURCES(void)
{
    int size = (intptr_t)heap.start_brk - (intptr_t)custom_sbrk(0);
    custom_sbrk(size);
    heap.init=false;
}

void heap_free(void* memblock) 
{
    if(memblock==NULL) return;
    struct memblock_t *blck = (struct memblock_t *)((uint8_t *)memblock - MEM_BLCK_SIZE);
    blck->used=false;
    set_check_sum_block(blck);
    connect_blocks();
    if(heap.phead->next->next == heap.ptail)
    {
        RESET_RESOURCES();
        heap_setup();
    }
}


struct memblock_t * find_memblock_aligned (size_t count) 
{
    struct memblock_t * ptr=heap.phead;
    
    while(ptr)
    {
        if(ptr->used==false)
        {
            intptr_t mem_position = (uint8_t*)ptr + MEM_BLCK_SIZE ;
            intptr_t page_border_position = (mem_position + (PAGE_SIZE-mem_position%PAGE_SIZE) % PAGE_SIZE);
            intptr_t size_to_border = page_border_position - mem_position;

            if(ptr->size - size_to_border >=  count && ptr->size >=size_to_border && (ptr->size - size_to_border - MEM_BLCK_SIZE) >= count )
            {
                return ptr;
            }
        }
        ptr=ptr->next;
    }

    return NULL;
}


enum pointer_type_t get_pointer_type(const const void* pointer)
{
    if(pointer == NULL) return pointer_null;

    pthread_mutex_lock(&mutex);

    if(heap.init == false || ( (uint8_t*)pointer < (uint8_t*)heap.start_brk || (uint8_t*)pointer > (uint8_t*)custom_sbrk(0) ))
    {
        pthread_mutex_unlock(&mutex);
        return pointer_out_of_heap;
    }

    struct memblock_t * ptr=heap.phead;

    while(ptr)
    {
        uint8_t *pointer_ = (uint8_t *)pointer;
        uint8_t * ptr_heap = (uint8_t *)ptr;

        if(pointer_>= ptr_heap && pointer_ < (ptr_heap + MEM_BLCK_SIZE))
        {
            pthread_mutex_unlock(&mutex);
            return pointer_control_block;
        }

        if( (pointer_ >(ptr_heap + MEM_BLCK_SIZE)  && pointer_ < (ptr_heap + MEM_BLCK_SIZE + (intptr_t)ptr->size)) && ptr->used==true )
        {
            pthread_mutex_unlock(&mutex);
            return pointer_inside_data_block;
        }

        if( (pointer_ >=(ptr_heap + MEM_BLCK_SIZE)  && pointer_ < (ptr_heap + MEM_BLCK_SIZE + (intptr_t)ptr->size)) && ptr->used==false )
        {
            pthread_mutex_unlock(&mutex);
            return pointer_unallocated;
        }

        if( (pointer_ == (ptr_heap + MEM_BLCK_SIZE) ) && ptr->used==true )
        {
            pthread_mutex_unlock(&mutex);
            return pointer_valid;
        }
        ptr=ptr->next;
    }
    pthread_mutex_unlock(&mutex);
    return pointer_out_of_heap;
}

void PRINT_POINTER_TYPE(const const void* pointer)
{
    enum pointer_type_t type= get_pointer_type(pointer);
    if(type == pointer_null) printf("\n pointer_null \n");
    if(type == pointer_out_of_heap) printf("\n pointer_out_of_heap \n");
    if(type == pointer_control_block) printf("\n pointer_control_block \n");
    if(type == pointer_inside_data_block) printf("\n pointer_inside_data_block \n");
    if(type == pointer_unallocated) printf("\n pointer_unallocated \n");
    if(type == pointer_valid) printf("\n pointer_valid \n");
}

void* heap_malloc_aligned_debug(size_t count, int fileline,const char* filename)
{
    if(count <= 0) return NULL;

    size_t count_round =round_to_pages(count) * PAGE_SIZE;

    pthread_mutex_lock(&mutex);

    struct memblock_t *blck = find_memblock_aligned (count_round);
    if(blck==NULL)
    {
        if(add_pages_to_heap(count_round))
        {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        connect_blocks();
        pthread_mutex_unlock(&mutex);
        return heap_malloc_aligned_debug(count, fileline,filename);
    }

    intptr_t mem_position = (uint8_t*)blck + MEM_BLCK_SIZE ;
    intptr_t page_border_position = (mem_position + (PAGE_SIZE-mem_position%PAGE_SIZE) % PAGE_SIZE);
    intptr_t size_to_border = page_border_position - mem_position;

    struct memblock_t *new_memblock= (struct memblock_t *)((uint8_t*)page_border_position - MEM_BLCK_SIZE);

    blck->next->prev=new_memblock;
    set_check_sum_block(blck->next);
    new_memblock->next=blck->next;
    new_memblock->prev=blck;
    blck->next=new_memblock;

    new_memblock->size= blck->size - size_to_border;
    set_fences(new_memblock);
    new_memblock->used=true;

    blck->size=size_to_border - MEM_BLCK_SIZE;
    blck->used=false;

    new_memblock->filaname=filename;
    new_memblock->line=fileline;

    struct memblock_t *new_memblock_end= (struct memblock_t *)((uint8_t*)new_memblock + MEM_BLCK_SIZE + count_round);
    new_memblock_end->next=new_memblock->next;
    new_memblock_end->prev=new_memblock;
    new_memblock->next->prev=new_memblock_end;
    set_check_sum_block(new_memblock->next);
    new_memblock->next=new_memblock_end;

    new_memblock_end->used=false;
    new_memblock_end->size=new_memblock->size - MEM_BLCK_SIZE - count_round;
    set_fences(new_memblock_end);

    new_memblock->size=count_round;

    set_check_sum_block(blck);
    set_check_sum_block(new_memblock);
    set_check_sum_block(new_memblock_end);
    
    void * mem_to_user = (void *) ((uint8_t *)new_memblock + MEM_BLCK_SIZE );
    pthread_mutex_unlock(&mutex);
    return mem_to_user;
}

void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline,const char* filename)
{
    pthread_mutex_lock(&mutex);
    if(number == 0 || size == 0 || heap.init==false)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    pthread_mutex_unlock(&mutex);

    size_t bytes = number * size;

    if((bytes / number) != size)
    {
        return NULL;
    }

    void *mem = heap_malloc_aligned_debug(bytes, fileline,filename);
    if(mem == NULL) return NULL;
    memset(mem, 0, bytes);

    return mem;
}

void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline,const char* filename)
{
    void *mem=NULL;
    
    if(memblock==NULL)
    {
        mem=heap_malloc_aligned_debug(size, fileline,filename);
        if(mem==NULL) return NULL;
        return mem;
    }

    struct memblock_t * blck=(struct memblock_t*)((uint8_t*)memblock - MEM_BLCK_SIZE);

    pthread_mutex_lock(&mutex);

    if(size == 0 )
    {
        heap_free(memblock);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    if(blck->size >= size)
    {
        pthread_mutex_unlock(&mutex);
        return memblock;
    }

    pthread_mutex_unlock(&mutex);

    mem = heap_malloc_aligned_debug(size, fileline,filename);
    if(mem == NULL) return NULL;
    memcpy(mem,memblock,blck->size);

    pthread_mutex_lock(&mutex);
    heap_free(memblock);
    pthread_mutex_unlock(&mutex);

    return mem;

}

void* heap_malloc_aligned(size_t count)
{
    return heap_malloc_aligned_debug(count,0,NULL);
}

void* heap_calloc_aligned(size_t number, size_t size)
{
    return heap_calloc_aligned_debug(number, size, 0,NULL);
}

void* heap_realloc_aligned(void* memblock, size_t size)
{
    return heap_realloc_aligned_debug( memblock, size, 0,NULL);
}


size_t heap_get_used_space(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.init == false)
    {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    struct memblock_t * ptr=heap.phead;
    size_t size= 0;

    while(ptr)
    {
        if(ptr->used == false) size+=ptr->size;
        ptr=ptr->next;
    }

    size=heap_size() - size;

    pthread_mutex_unlock(&mutex);
    return size;
}


size_t heap_get_largest_used_block_size(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.init == false)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    struct memblock_t * ptr=heap.phead;
    size_t max= 0;

    while(ptr)
    {
        if(ptr->used == true)
        {
            if(ptr->size > max) max= ptr->size;
        }
        ptr=ptr->next;
    }

    pthread_mutex_unlock(&mutex);
    return max;
}

uint64_t heap_get_used_blocks_count(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.init == false)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    struct memblock_t * ptr=heap.phead;
    size_t count= 0;

    while(ptr)
    {
        if(ptr->used == true && ptr != heap.phead && ptr!= heap.ptail)
        {
            count++;
        }
        ptr=ptr->next;
    }

    pthread_mutex_unlock(&mutex);
    return count;
}

size_t heap_get_free_space(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.init == false)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    struct memblock_t * ptr=heap.phead;
    size_t size= 0;

    while(ptr)
    {
        if(ptr->used == false)
        {
            size+=ptr->size;
        }
        ptr=ptr->next;
    }

    pthread_mutex_unlock(&mutex);
    return size;
}

size_t heap_get_largest_free_area(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.init == false)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    struct memblock_t * ptr=heap.phead;
    size_t max= 0;

    while(ptr)
    {
        if(ptr->used == false && ptr->size > max)
        {
            max=ptr->size;
        }
        ptr=ptr->next;
    }

    pthread_mutex_unlock(&mutex);
    return max;
}

uint64_t heap_get_free_gaps_count(void)
{
    pthread_mutex_lock(&mutex);
    if(heap.init == false)
    {
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    struct memblock_t * ptr=heap.phead;
    size_t count= 0;

    while(ptr)
    {
        if(ptr->used == false && ptr->size >= (WORD + MEM_BLCK_SIZE))
        {
            count++;
        }
        ptr=ptr->next;
    }

    pthread_mutex_unlock(&mutex);
    return count;
}

void* heap_get_data_block_start(const void* pointer)
{
    enum pointer_type_t type= get_pointer_type(pointer);
    if(type != pointer_inside_data_block && type!=pointer_valid) return NULL;
    if(type == pointer_valid) return pointer;

    struct memblock_t * ptr=heap.phead;

    while(ptr)
    {
        uint8_t *pointer_ = (uint8_t *)pointer;
        uint8_t * ptr_heap = (uint8_t *)ptr;

        if( (pointer_ >(ptr_heap + MEM_BLCK_SIZE)  && pointer_ < (ptr_heap + MEM_BLCK_SIZE + (intptr_t)ptr->size)) && ptr->used==true )
        {
            return (ptr_heap + MEM_BLCK_SIZE);
        }
        ptr=ptr->next;
    }
    return NULL;
}

size_t heap_get_block_size(const const void* memblock)
{
    enum pointer_type_t type= get_pointer_type(memblock);
    if(type!=pointer_valid) return 0;
    struct memblock_t *blck= ((uint8_t *)(memblock) - MEM_BLCK_SIZE );
    return blck->size;
}

int valid_fences(struct memblock_t * ptr)
{
    if(ptr==NULL) return -1;

    int sum1=0,sum2=0;

    for(unsigned int i=0;i<sizeof(struct block_fence_t);i++)
    {
        sum1+=ptr->head_fence.pattern[i];
        sum2+=ptr->tail_fence.pattern[i];
    }

    if( (sum1 != sum2) && (sum1!= PATTERN_SUM )) return 1;
    return 0;
}

int heap_validate(void)
{
    if(heap.init == false)
    {
        return -1;
    }

    if(calculate_check_sum_block(heap.phead)!= heap.check_sum || calculate_check_sum_block(heap.ptail) != heap.ptail->check_sum)
    {
        printf("Nieporane sumy kontrolne sterty\n");
        return -1;
    }

    if(valid_fences(heap.phead) || valid_fences(heap.ptail))
    {
        printf("Uszkodzone płotki kontrolne sterty\n");
        return -1;
    }

    struct memblock_t * pointer = heap.phead->next;

    while(pointer != heap.ptail)
    {
        if(calculate_check_sum_block(pointer)!= pointer->check_sum )
        {
            printf("Nieporawna suma kontrolna bloku\n");
            return -1;
        }

        if(valid_fences(pointer))
        {
            printf("Uszkodzone płotki kontrolne bloku\n");
            return -1;
        }

        if(((uint8_t *)pointer->next - (uint8_t *)pointer - MEM_BLCK_SIZE  ) != pointer->size)
        {
            printf("Nieprawidlowy rozmiar bloku\n");
            return -1;
        }

        pointer=pointer->next;
    }

    return 0;
}

void heap_dump_debug_information(void)
{
    if(heap_validate()) return;

    struct memblock_t * pointer = heap.phead;

    printf("################ HEAP DEBUG INFORMATION ################\n\n\n");

    printf("Wielkosc sterty [B] : %d\n",heap_size());
    printf("Liczba zajetych bajtow sterty [B] : %d\n",heap_get_used_space());
    printf("Liczba wolnych bajtow sterty [B] : %d\n",heap_get_free_space());
    printf("Wielkosc najwiekszego wolnego bloku [B] : %d\n\n\n",heap_get_largest_free_area());

    printf("Lista zaalokowanych blokow :\n\n");

    while(pointer)
    {
        if(pointer->used == true && pointer!= heap.phead && pointer != heap.ptail)
        {

            printf("Adres bloku : %p, Dlugosc bloku [B] : %d",(uint8_t *)pointer + MEM_BLCK_SIZE , pointer->size);
            if(pointer->line !=0 && pointer->filaname!=NULL)
            {
                printf(", Nazwa pliku zrodlowego %s, numer linii pliku %d\n",pointer->filaname,pointer->line);
            }
            else printf("\n");
        }
        pointer=pointer->next;
    }
    
    return;
}

void UTEST (void)
{
    // Test funkcji heap_setup :
    // przy braku braku pamieci na stercie,
    int er=0;
    int max_mem = PAGE_SIZE * PAGES_AVAILABLE -1;
    custom_sbrk(max_mem);

    er= heap_setup();

    if(er != -1)
    {
        printf("Funkcja powinna zwrocic %d, a zwrocila %d\n", -1 ,er);
        assert(er != -1);
    }

    custom_sbrk(-max_mem);

    // prawidlowe uzycie

    er= heap_setup();

    if(er != 0)
    {
        printf("Funkcja powinna zwrocic %d, a zwrocila %d\n", 0 ,er);
        assert(er != 0);
    }

    // wielokrotne uzycie
    er= heap_setup();

    if(er != 0)
    {
        printf("Funkcja powinna zwrocic %d, a zwrocila %d\n", 0 ,er);
        assert(er != 0);
    }

    RESET_RESOURCES();
    printf("UTEST PASSED\n");
    return;
}
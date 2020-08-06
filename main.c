#include <stdio.h>
#include "custom_unistd.h"
#include "heap.h"

int main(int argc, char **argv)
{
    UTEST();

    int status = heap_setup();
    assert(status == 0);

    // parametry pustej sterty
    size_t free_bytes = heap_get_free_space();
    size_t used_bytes = heap_get_used_space();


    // ******************  malloc i malloc_aligned    *************************

    void* p1 = heap_malloc(8 * MB); // 8MB
    void* p2 = heap_malloc(8 * MB); // 8MB
    void* p3 = heap_malloc(8 * MB); // 8MB
    void* p4 = heap_malloc(45 * MB); // 45MB
    assert(p1 != NULL); // malloc musi się udać
    assert(p2 != NULL); // malloc musi się udać
    assert(p3 != NULL); // malloc musi się udać
    assert(p4 == NULL); // nie ma prawa zadziałać
    // Ostatnia alokacja, na 45MB nie może się powieść,
    // ponieważ sterta nie może być aż tak 
    // wielka (brak pamięci w systemie operacyjnym).

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona

    // zaalokowano 3 bloki
    assert(heap_get_used_blocks_count() == 3);

    // zajęto 24MB sterty; te 2000 bajtów powinno
    // wystarczyć na wewnętrzne struktury sterty
    assert(
            heap_get_used_space() >= 24 * MB &&
            heap_get_used_space() <= 24 * MB + 2000
    );

    // zwolnij pamięć
    heap_free(p1);
    heap_free(p2);
    heap_free(p3);

    // wszystko powinno wrócić do normy
    assert(heap_get_free_space() == free_bytes);
    assert(heap_get_used_space() == used_bytes);

    // już nie ma bloków
    assert(heap_get_used_blocks_count() == 0);


    // *************************************************************************


    p1 = heap_malloc_aligned(PAGE_SIZE); // strona
    p2 = heap_malloc_aligned(PAGE_SIZE);// strona
    p3 = heap_malloc_aligned(PAGE_SIZE); // strona
    p4 = heap_malloc(PAGE_SIZE * PAGES_AVAILABLE); // brak pamieci
    assert(p1 != NULL); // malloc musi się udać
    assert(p2 != NULL); // malloc musi się udać
    assert(p3 != NULL); // malloc musi się udać
    assert(p4 == NULL); // nie ma prawa zadziałać


    // *_aligned musza zwracac zmienna spełniaca warunek
    assert( ((intptr_t)p1 & (intptr_t)(PAGE_SIZE - 1)) == 0 );
    assert( ((intptr_t)p2 & (intptr_t)(PAGE_SIZE - 1)) == 0 );
    assert( ((intptr_t)p3 & (intptr_t)(PAGE_SIZE - 1)) == 0 );

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona

    // zwolnij pamięć
    heap_free(p1);
    heap_free(p2);
    heap_free(p3);

    assert(heap_get_used_blocks_count() == 0);


// ******************  calloc i calloc_aligned    *************************

    p1 = heap_calloc(MB * 2, sizeof(int)); // 8MB
    p2 = heap_calloc(MB * 2, sizeof(int)); // 8MB
    p3 = heap_calloc(MB* 2, sizeof(int) ); // 8MB
    p4 = heap_calloc(MB * 10 , sizeof(int)); // 40MB
    assert(p1 != NULL); // calloc musi się udać
    assert(p2 != NULL); // calloc musi się udać
    assert(p3 != NULL); // calloc musi się udać
    assert(p4 == NULL); // nie ma prawa zadziałać

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona

    // zaalokowano 3 bloki
    assert(heap_get_used_blocks_count() == 3);

    // zajęto 24MB sterty; te 2000 bajtów powinno
    // wystarczyć na wewnętrzne struktury sterty
    assert(
            heap_get_used_space() >= MB * 24 &&
            heap_get_used_space() <= 24* MB + 2000
    );


    // sprawdzanie czy calloc wyzerowal zaalokowana pamiec 

    for(int i = 0; i < MB * 8 ;i++)
    {
        assert(*((uint8_t*)p1 + i) == 0);
        assert(*((uint8_t*)p2 + i) == 0);
        assert(*((uint8_t*)p3 + i) == 0);
    }

    // zwolnij pamięć
    heap_free(p1);
    heap_free(p2);
    heap_free(p3);

    // wszystko powinno wrócić do normy
    assert(heap_get_free_space() == free_bytes);
    assert(heap_get_used_space() == used_bytes);

    // już nie ma bloków
    assert(heap_get_used_blocks_count() == 0);




    p1 = heap_calloc_aligned(2, PAGE_SIZE); // 2 strony
    p2 = heap_calloc_aligned(2, PAGE_SIZE);  // 2 strony
    p3 = heap_calloc_aligned(2, PAGE_SIZE);  // 2 strony
    p4 = heap_calloc_aligned(PAGES_AVAILABLE, PAGE_SIZE); // brak pamieci
    assert(p1 != NULL); // calloc musi się udać
    assert(p2 != NULL); // calloc musi się udać
    assert(p3 != NULL); // calloc musi się udać
    assert(p4 == NULL); // nie ma prawa zadziałać

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona


    // *_aligned musza zwracac zmienna spełniaca warunek
    assert( ((intptr_t)p1 & (intptr_t)(PAGE_SIZE - 1)) == 0 );
    assert( ((intptr_t)p2 & (intptr_t)(PAGE_SIZE - 1)) == 0 );
    assert( ((intptr_t)p3 & (intptr_t)(PAGE_SIZE - 1)) == 0 );

    // sprawdzanie czy calloc wyzerowal zaalokowana pamiec 

    for(int i = 0; i < 2* PAGE_SIZE ;i++)
    {
        assert(*((uint8_t*)p1 + i) == 0);
        assert(*((uint8_t*)p2 + i) == 0);
        assert(*((uint8_t*)p3 + i) == 0);
    }

    // zwolnij pamięć
    heap_free(p1);
    heap_free(p2);
    heap_free(p3);

    // już nie ma bloków
    assert(heap_get_used_blocks_count() == 0);


    // ******************  realloc i realloc_aligned   *************************

    // alokowanie 
    p1 = heap_realloc(NULL, MB * 8); // 8MB
    assert(p1 != NULL); // realloc musi się udać

    // ustawianie wartosci w pamieci
    memset (p1, 'X', MB * 8 );

    p2 = heap_realloc(p1 , MB * 8 * 2); //zwiekaszamy obszar pamieci 2x
    assert(p2 != NULL); // realokacja musi się udać

    // sprawdzanie czy realloc przepisał warosci pamieci 
    assert(memcmp (p1, p2, 8 * MB )==0);

    // zwalnianie za pomoca realloc 
    p3  = heap_realloc(p2,0);
    assert(p3==NULL) ;

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona

    // wolna sterta
    assert(heap_get_used_blocks_count() == 0);




    p1 = heap_realloc_aligned(NULL, PAGE_SIZE  ); // 8MB
    assert(p1 != NULL); // realloc musi się ud *ać

    // *_aligned musza zwracac zmienna spełniaca warunek
    assert( ((intptr_t)p1 & (intptr_t)(PAGE_SIZE - 1)) == 0 );


    // ustawianie wartosci w pamieci
    memset (p1, 'X', PAGE_SIZE );

    p2 = heap_realloc_aligned(p1 , PAGE_SIZE *2); //zwiekaszamy obszar pamieci 2x
    assert(p2 != NULL); // realokacja musi się udać

    assert( ((intptr_t)p2 & (intptr_t)(PAGE_SIZE - 1)) == 0 );

    // sprawdzanie czy realloc przepisał warosci pamieci 
    assert(memcmp (p1, p2, PAGE_SIZE )==0);

    // zwalnianie za pomoca realloc 
    p3  = heap_realloc_aligned(p2,0);
    assert(p3==NULL) ;

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona

    // wolna sterta
    assert(heap_get_used_blocks_count() == 0);





    // *****************  pozostale funkcje  ***********************


    p1=heap_malloc(PAGE_SIZE);
    p2=heap_malloc(PAGE_SIZE  *2 );
    p3=heap_malloc(PAGE_SIZE *3);
    p4=heap_malloc(1000);
    assert(p1 != NULL);
    assert(p2 != NULL);
    assert(p3 != NULL);

    heap_free(p3);

    // sprawdzanie wartosci zwracanych prpzez get_pointer_type
    assert(get_pointer_type(NULL) == pointer_null );
    assert( get_pointer_type(p1 - 1) == pointer_control_block );
    assert(get_pointer_type(p1) != pointer_inside_data_block && get_pointer_type(p1 +1) == pointer_inside_data_block);
    assert(get_pointer_type(p1 - PAGE_SIZE) == pointer_out_of_heap && get_pointer_type(p3 - 4* PAGE_SIZE) == pointer_out_of_heap );
    assert(get_pointer_type(p3) == pointer_unallocated );
    assert(get_pointer_type(p1) == pointer_valid && get_pointer_type(p2) == pointer_valid );




    assert(p1 == heap_get_data_block_start(p1));
    assert(p2 == heap_get_data_block_start(p2));
    assert( heap_get_data_block_start(p1 +2) == p1 );
    assert( heap_get_data_block_start(p1 - 5) == NULL);



    // funckja moze zwrocic wartosc inna niz podana przy alaokacji poniewaz zwraca wielokrotnosc slowa 
    assert(heap_get_block_size(p1) == PAGE_SIZE);
    assert(heap_get_block_size(p3) == 0);
    assert(heap_get_block_size(p1 - 1 ) == 0);
    assert(heap_get_block_size(p4) % WORD == 0);

    status = heap_validate();
    assert(status == 0); // sterta nie może być uszkodzona

    heap_free(p1);
    heap_free(p2);
    heap_free(p4);



//    heap_dump_debug_information();
//    RESET_RESOURCES();

    return 0;
}
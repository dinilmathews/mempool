#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdalign.h>
#include <stdbool.h>
#include "mempool.h"

/* Mempool location */
#define STORAGE_SIZE 2000
uint8_t storage[STORAGE_SIZE];

#define MAX_ALIGN alignof(max_align_t)
#define ALIGN_DOWN(var) (((var) / MAX_ALIGN) * MAX_ALIGN)
/* 3 * HDR_SIZE is occupied for the HEAD/TAIL/Free block*/
#define HDR_SIZE 16
#define MAX_ALLOCATABLE ALIGN_DOWN(STORAGE_SIZE - 3*HDR_SIZE)

uint8_t* test_allocation(uint32_t size)
{
    uint8_t* ptr = mempool_alloc(size);
    if (ptr != NULL)
    {
        printf("Allocated offset 0x%lx size:%u\n", (uintptr_t)ptr - (uintptr_t)storage, size);
        /* make sure address is in the storage area and meets alignment requirement */
        assert(ptr >= &storage[0] && (ptr - storage + size) <= STORAGE_SIZE);
        assert(((uintptr_t)ptr % MAX_ALIGN) == 0);
    }
    else
    {
        printf("Error:Allocation of size %u failed\n", size);
        assert(false);
    }

    return ptr;
}

void test_free(uint8_t *ptr)
{
    mempool_status_t status;
    status = mempool_free(ptr);
    printf("mempool free: offset: 0x%lx status: %d\n", (uintptr_t)ptr - (uintptr_t)storage, status);
    assert(status == MEMPOOL_SUCCESS);
}

void test_sequential_alloc_free(void)
{
    uint8_t *m1, *m2, *m3, *m4, *m5, *m6, *m7;
    printf("\ntest_sequential_alloc_free\n");
    /* Due to additional headers in storage, full size cannot be allocated */
    m1 = mempool_alloc(sizeof(storage));
    assert(m1 == NULL);
    /* Allocate several blocks */
    m2 = test_allocation(101);
    m3 = test_allocation(202);
    m4 = test_allocation(303);
    m5 = test_allocation(404);
    m6 = test_allocation(808);
    /* Pool should be full now */
    m7 = mempool_alloc(100);
    assert(m7 == NULL);
    /* Free the blocks */
    test_free(m2);
    test_free(m3);
    test_free(m4);
    test_free(m5);
    test_free(m6);

    /* check if the whole storage has coalesced and is available again */
    m1 = test_allocation(MAX_ALLOCATABLE);
    test_free(m1);
}

void test_mixed_alloc_dealloc(void)
{
    uint8_t *m2, *m3, *m4;
    printf("\ntest_mixed_alloc_dealloc\n");
    m2 = test_allocation(101);
    m3 = test_allocation(202);
    test_free(m2);
    /* m4 should point now to same
    free space as m2 */
    m4 = test_allocation(30);
    assert(m2 == m4);
    test_free(m4);
    test_free(m3);
    /* check if the whole storage is free again */
    test_free(test_allocation(MAX_ALLOCATABLE));
}

void test_coalescing(void)
{
    uint8_t *m2, *m3, *m4;
    printf("\ntest_coalescing\n");
    m2 = test_allocation(101);
    m3 = test_allocation(202);
    m4 = test_allocation(30);

    /* Keep m3 but free m2 and m4 */
    test_free(m2);
    test_free(m4);
    /* now free m3, which should cause m2 and m4 to
    coalesce */
    test_free(m3);

    /* If coalescing worked whole memory is available again */
    test_free(test_allocation(MAX_ALLOCATABLE));
}

int main(void)
{
    /* Initialize mempool */
    assert(mempool_init(storage, sizeof(storage)) == MEMPOOL_SUCCESS);

    /* Run the tests */
    test_sequential_alloc_free();
    test_mixed_alloc_dealloc();
    test_coalescing();

    printf("Tests passed\n");
    return 0;
}

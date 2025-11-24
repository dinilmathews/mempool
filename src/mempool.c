#include <stdint.h>
#include <stddef.h>
#include <stdalign.h>
#include <stdbool.h>
#include "mempool.h"
#include "debug.h"

/*
  mempool uses a linked list to manage the free blocks.
  Node is inserted at the start of a free segment and points to next free segment.
  HEAD -> [Node|free space] -> [Node|free space] -> [Node|free space] -> TAIL
*/

typedef struct memblock_admin_s
{
    uint32_t size;
    struct memblock_admin_s *next;
} memblock_admin_t;

memblock_admin_t *HEAD;
memblock_admin_t *TAIL;

/* Alignment helpers: The returned addresses should be aligned to max requirement */
#define MAX_ALIGN alignof(max_align_t)
#define ALIGN_UP(var) ((((var) + (MAX_ALIGN - 1)) / MAX_ALIGN) * MAX_ALIGN)
#define ALIGN_DOWN(var) (((var) / MAX_ALIGN) * MAX_ALIGN)
#define ALIGN_ADDR_UP(addr) ALIGN_UP((uintptr_t)(addr))
#define ALIGN_ADDR_DOWN(addr) ALIGN_DOWN((uintptr_t)(addr))
#define MEM_BLOCK_ADMIN_SIZE ALIGN_UP(sizeof(memblock_admin_t))
/* Minimum space required in a memory block */
/* The memory block will contain the node structure +
   space fitting max alignment */
#define MIN_ALLOC_SIZE MAX_ALIGN
#define MIN_MEM_BLOCK_SIZE (MEM_BLOCK_ADMIN_SIZE + MIN_ALLOC_SIZE)

/* Create a memory block node starting at location "start", and having free space
"available space". The created free block node will point to "next" free block node */
static memblock_admin_t *init_mem_block_admin(uintptr_t start, uint32_t available_space)
{
    /* The free block has to start at a aligned location */
    uintptr_t start_aligned = ALIGN_ADDR_UP(start);
    /* Place the memblock node at start of the free space */
    memblock_admin_t *freeblock = (memblock_admin_t *)(start_aligned);
    uint32_t align_space_consumed = start_aligned - start;
    MY_ASSERT(available_space > align_space_consumed);
    freeblock->size = available_space - align_space_consumed;
    freeblock->next = NULL;
    return freeblock;
}

/* Find the first block that fits the requested size and split off the
remaining free space into a new node */
static memblock_admin_t *freememblocklist_get_memblock(uint32_t required_size)
{
    memblock_admin_t *current = HEAD->next;
    memblock_admin_t *previous = HEAD;

    while (current != NULL)
    {
        if (current->size >= required_size)
        {
            /* found the first free block that has enough space */
            uint32_t space_left = current->size - required_size;
            /* Split off remaining space and add as a new node in the linked list */
            if (space_left >= (MIN_MEM_BLOCK_SIZE))
            {
                /* create new block around the remaining free space and add it to the linked list */
                memblock_admin_t *block =
                    init_mem_block_admin(((uintptr_t)current + required_size), space_left);
                previous->next = block;
                block->next = current->next;

                /* update current selected node to show correct block size */
                current->size = required_size;
            }
            else
            {
                /* remove the current node */
                previous->next = current->next;
            }
            break;
        }
        /* goto the next node */
        previous = current;
        current = current->next;
    }

    return current;
}

/* Merge the second block into first block */
static void memblock_coalesce(memblock_admin_t *f1, memblock_admin_t *f2)
{
    MY_ASSERT(f1->next == f2);
    f1->next = f2->next;
    f1->size = (uint32_t)((uintptr_t)(f2) + f2->size - (uintptr_t)f1);
    /* Make sure the new merged memblock is still lies within free area */
    MY_ASSERT(((uintptr_t)f1 + f1->size) <= (uintptr_t)TAIL);
}

/* Check if the two memblocks are adjacent to each other */
static bool memblock_check_coalesce(memblock_admin_t *f1, memblock_admin_t *f2)
{
    MY_ASSERT(f1->next == f2);
    if (f2 == TAIL || f1 == HEAD)
    {
        /* HEAD/TAIL block is only for administrative purpose,
        do not coalesce */
        return false;
    }
    /* If f2 starts just after f1 ends, with not even enough space for the
    smallest mem block inbetween, then we can coalesce f1 and f2 */
    uintptr_t gap = ((uintptr_t)f2 - (uintptr_t)f1 - f1->size);
    return (gap < (MIN_MEM_BLOCK_SIZE));
}

/* check if the adjacent freeblocks can be coalesced */
static void coalesce_mem_blocks(memblock_admin_t *previous, memblock_admin_t *current, memblock_admin_t *next)
{
    memblock_admin_t *block;
    if (memblock_check_coalesce(previous, current))
    {
        memblock_coalesce(previous, current);
        block = previous;
    }
    else
    {
        block = current;
    }
    if (memblock_check_coalesce(block, next))
    {
        memblock_coalesce(block, next);
    }
}

static mempool_status_t freememblocklist_insert(memblock_admin_t *block)
{
    memblock_admin_t *current = HEAD->next;
    memblock_admin_t *previous = HEAD;
    /* The block that is freed must have minimum size */
    MY_ASSERT(block->size >= MIN_MEM_BLOCK_SIZE);

    while (current != NULL)
    {
        /* something is wrong if the address already exists in the free list */
        MY_ASSERT(current != block);

        /* Freeblock list has memblocks with increasing addresses.
        Insert the new memblock in correct place */
        if ((uintptr_t)current < (uintptr_t)block)
        {
            /* New memblock has higher address than current node, goto next node */
            previous = current;
            current = current->next;
            continue;
        }
        else
        {
            /* New memblock can be inserted before current node which has higher address */
            previous->next = block;
            block->next = current;
            /* Check if the new memblock can be coalesced with existing free nodes */
            coalesce_mem_blocks(previous, block, current);
            return MEMPOOL_SUCCESS;
        }
    }
    /* New memblock could not be inserted successfully */
    MY_ASSERT(false);
    return MEMPOOL_INVALID_PARAM;
}

mempool_status_t mempool_init(uint8_t *storage, uint32_t size)
{
    memblock_admin_t *freeblock;
    uintptr_t end;
    uint32_t available_space;
    /* min pool size must cover the HEAD + FREEBLOCK + TAIL */
    const uint32_t min_pool_size = 2 * MEM_BLOCK_ADMIN_SIZE + MIN_ALLOC_SIZE;

    /* if storage is NULL or not enough size in the storage block
    return error */
    if (storage == NULL || size < min_pool_size)
    {
        return MEMPOOL_INVALID_PARAM;
    }

    /* HEAD node is the first memblock which
     points to the free blocks list */
    HEAD = init_mem_block_admin((uintptr_t)storage, MEM_BLOCK_ADMIN_SIZE);
    end = ALIGN_ADDR_DOWN(((storage + size)) - MEM_BLOCK_ADMIN_SIZE);

    /* The last memblock is the TAIL */
    TAIL = init_mem_block_admin(end, MEM_BLOCK_ADMIN_SIZE);
    available_space = (uintptr_t)TAIL - ((uintptr_t)HEAD + HEAD->size);
    freeblock = init_mem_block_admin((uintptr_t)HEAD + MEM_BLOCK_ADMIN_SIZE, available_space);

    /* Insert the free block node between HEAD and TAIL */
    HEAD->next = freeblock;
    freeblock->next = TAIL;

    return MEMPOOL_SUCCESS;
}

mempool_status_t mempool_free(uint8_t *loc)
{
    mempool_status_t status;
    memblock_admin_t *block;

    /* validate input pointer */
    if (loc == NULL || ((uintptr_t)loc % MAX_ALIGN != 0))
    {
        return MEMPOOL_INVALID_PARAM;
    }

    block = (memblock_admin_t *)((uintptr_t)loc - MEM_BLOCK_ADMIN_SIZE);

    /* validate block is within boundaries */
    if ((uintptr_t)block < ((uintptr_t)HEAD + HEAD->size) ||
        ((uintptr_t)block + block->size) > (uintptr_t)TAIL)
    {
        return MEMPOOL_INVALID_PARAM;
    }
    /* add block back to the free list and coalesce to adjacent
    mem blocks */
    status = freememblocklist_insert(block);
    return status;
}

uint8_t *mempool_alloc(uint32_t size)
{
    uint8_t *alloc_location = NULL;

    if(size == 0)
    {
        return NULL;
    }

    /* Go over list of free blocks and find the first block that fits */
    uint32_t req_size = ALIGN_UP(size) + MEM_BLOCK_ADMIN_SIZE;
    memblock_admin_t *memblock = freememblocklist_get_memblock(req_size);
    if (memblock)
    {
        /* Actual space starts after the linked list header */
        alloc_location = (uint8_t *)memblock + MEM_BLOCK_ADMIN_SIZE;
    }

    return alloc_location;
}

/* Print the free blocks linked list */
void mempool_debug_print(void)
{
#ifdef DEBUG
    memblock_admin_t *current = HEAD->next;
    while (current != TAIL)
    {
        MY_PRINT("\t[LL]block:c:%lu, sz:%u, n:%lu\n",
                 (uintptr_t)current - (uintptr_t)HEAD,
                 current->size,
                 ((uintptr_t)current->next) ? ((uintptr_t)current->next - (uintptr_t)HEAD) : 0);
        current = current->next;
    }
#endif
}

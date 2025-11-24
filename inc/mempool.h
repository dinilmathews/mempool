#ifndef __MEM_POOL_H__
#define __MEM_POOL_H__

#include <stdint.h>

typedef enum {
    MEMPOOL_SUCCESS = 0,
    MEMPOOL_INVALID_PARAM = 1,
} mempool_status_t;
/**
 * @brief Initialize memory pool manager with storage location and size of storage
 *
 *  @param  storage   pointer to start of storage location
 *  @param  size      size of storage in bytes
 *
 *  @return  mempool_status_t
 */
mempool_status_t mempool_init(uint8_t *storage, uint32_t size);

/**
 * @brief Allocate memory from managed storage
 *
 *  @param  size     number of bytes to allocate
 *  @return          pointer to allocated memory, or NULL if allocation fails
 */
uint8_t *mempool_alloc(uint32_t size);

/**
 * @brief Free earlier allocated memory.
 *        Invalid pointer will lead to an assert.
 *
 *  @param  loc     Pointer to previously allocated memory
 *  @return mempool_status_t
 */
mempool_status_t mempool_free(uint8_t *loc);

/**
 * @brief Print internal linked list structure.
 */
void mempool_debug_print(void);

#endif /* __MEM_POOL_H__ */

#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
// Fixed block pools - preferred for embedded, no fragmentation
typedef struct mk_mem_pool mk_mem_pool_t;
mk_mem_pool_t* mk_mem_pool_create(size_t block_size, size_t block_count, const char* name);
void* mk_mem_pool_alloc(mk_mem_pool_t* pool, uint32_t timeout_ms);
mk_status_t mk_mem_pool_free(mk_mem_pool_t* pool, void* block);
size_t mk_mem_pool_free_count(mk_mem_pool_t* pool);

// General heap wrappers for diagnostics
void* mk_malloc(size_t size);
void mk_free(void* ptr);
size_t mk_get_free_heap(void);
size_t mk_get_min_free_heap(void);
#ifdef __cplusplus
}
#endif

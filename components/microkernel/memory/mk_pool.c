#include "mk_memory.h"
#include "mk_mutex.h"
#include "esp_heap_caps.h"
#include <stdlib.h>

struct mk_mem_pool {
    size_t block_size;
    size_t block_count;
    uint8_t* mem;
    bool* used;
    mk_mutex_t* lock;
};

mk_mem_pool_t* mk_mem_pool_create(size_t block_size, size_t block_count, const char* name){
    if (block_size == 0 || block_count == 0) return NULL;
    mk_mem_pool_t* p = (mk_mem_pool_t*)malloc(sizeof(mk_mem_pool_t));
    if(!p) return NULL;
    p->block_size = block_size;
    p->block_count = block_count;
    p->mem = (uint8_t*)heap_caps_malloc(block_size * block_count, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!p->mem) {
        p->mem = (uint8_t*)malloc(block_size * block_count);
    }
    p->used = (bool*)calloc(block_count, sizeof(bool));
    p->lock = mk_mutex_create(name ? name : "pool_lock");
    if(!p->mem || !p->used || !p->lock){
        if (p->mem) free(p->mem);
        if (p->used) free(p->used);
        if (p->lock) mk_mutex_delete(p->lock);
        free(p);
        return NULL;
    }
    return p;
}

void* mk_mem_pool_alloc(mk_mem_pool_t* pool, uint32_t timeout_ms){
    if(!pool) return NULL;
    if (mk_mutex_lock(pool->lock, timeout_ms) != MK_OK) return NULL;
    void* ptr = NULL;
    for(size_t i = 0; i < pool->block_count; i++){
        if(!pool->used[i]){
            pool->used[i] = true;
            ptr = pool->mem + i * pool->block_size;
            break;
        }
    }
    mk_mutex_unlock(pool->lock);
    return ptr;
}

mk_status_t mk_mem_pool_free(mk_mem_pool_t* pool, void* block){
    if(!pool || !block) return MK_ERR_INVALID;
    if (mk_mutex_lock(pool->lock, 1000) != MK_OK) return MK_ERR_TIMEOUT;
    size_t offset = (uint8_t*)block - pool->mem;
    size_t idx = offset / pool->block_size;
    if(idx >= pool->block_count || (offset % pool->block_size) != 0 || !pool->used[idx]){
        mk_mutex_unlock(pool->lock);
        return MK_ERR_INVALID;
    }
    pool->used[idx] = false;
    mk_mutex_unlock(pool->lock);
    return MK_OK;
}

size_t mk_mem_pool_free_count(mk_mem_pool_t* pool){
    if(!pool) return 0;
    if (mk_mutex_lock(pool->lock, 100) != MK_OK) return 0;
    size_t c = 0;
    for(size_t i = 0; i < pool->block_count; i++) {
        if(!pool->used[i]) c++;
    }
    mk_mutex_unlock(pool->lock);
    return c;
}

void* mk_malloc(size_t size){ return malloc(size); }
void mk_free(void* ptr){ free(ptr); }
size_t mk_get_free_heap(void){ return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
size_t mk_get_min_free_heap(void){ return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL); }


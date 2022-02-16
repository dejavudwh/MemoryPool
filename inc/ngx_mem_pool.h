#pragma once

#include <memory.h>
#include <stdlib.h>

#include <cstddef>
#include <functional>
#include <iostream>

using namespace std;

struct ngx_pool_s;

struct ngx_pool_data_s {
    unsigned char* last;
    unsigned char* end;
    ngx_pool_s* next;
    unsigned int failed;
};

struct ngx_pool_large_s {
    ngx_pool_large_s* next;
    void* alloc;
};

struct ngx_pool_cleanup_s {
    function<void(void*)> handler;
    void* data;
    ngx_pool_cleanup_s* next;
};

struct ngx_pool_s {
    ngx_pool_data_s d;
    size_t max;
    ngx_pool_s* current;
    ngx_pool_large_s* large;
    ngx_pool_cleanup_s* cleanup;
};

#define ngx_align(d, a) (((d) + (a - 1)) & ~(a - 1))

const int ngx_pagesize = 4096;
const int NGX_MAX_ALLOC_FROM_POOL = ngx_pagesize - 1;
const int NGX_DEFAULT_POOL_SIZE = 16 * 1024;
const int NGX_POOL_ALIGNMENT = 16;
const int NGX_MIN_POOL_SIZE = ngx_align((sizeof(ngx_pool_s) + 2 * sizeof(ngx_pool_large_s)), NGX_POOL_ALIGNMENT);

class ngx_mem_pool {
public:
    ngx_mem_pool();
    ~ngx_mem_pool();
    void* ngx_palloc(size_t size);
    void* ngx_pnalloc(size_t size);
    void* ngx_pcalloc(size_t size);
    void ngx_pfree(void* p);
    void ngx_reset_pool();
    void ngx_destroy_pool();
    ngx_pool_cleanup_s* ngx_pool_cleanup_add(size_t size);

private:
    ngx_pool_s* m_pool;
    size_t default_size;
    size_t unused_size;
    void* ngx_palloc_small(size_t size, unsigned int align);
    void* ngx_palloc_large(size_t size);
    void* ngx_palloc_block(size_t size);
};
#include "ngx_mem_pool.h"

#include <cstddef>
#include <cstdlib>
#include <new>

ngx_mem_pool::ngx_mem_pool() {
    default_size = 512;
    m_pool = ( ngx_pool_s* )malloc(512);
    if (m_pool == nullptr) {
        throw bad_alloc();
    }

    m_pool->d.last = ( unsigned char* )m_pool + sizeof(ngx_pool_s);
    m_pool->d.end = ( unsigned char* )m_pool + default_size;

    m_pool->d.next = nullptr;
    m_pool->d.failed = 0;

    unused_size = default_size - sizeof(ngx_pool_s);
    m_pool->max = (size < NGX_MAX_ALLOC_FROM_POOL) ? size : NGX_MAX_ALLOC_FROM_POOL;
    m_pool->current = m_pool;
    m_pool->large = nullptr;
    m_pool->cleanup = nullptr;
}

void* ngx_mem_pool::ngx_palloc(size_t size) {
    if (size <= m_pool->max) {
        return ngx_palloc_small(size, 1);
    }

    return ngx_palloc_large(size);
}

void* ngx_mem_pool::ngx_pnalloc(size_t size) {
    if (size <= m_pool->max) {
        return ngx_palloc_small(size, 0);
    }
    return ngx_palloc_large(size);
}

#define NGX_ALIGNMENT sizeof(unsigned long)
#define ngx_align_ptr(p, a) ( u_char* )((( uintptr_t )(p) + (( uintptr_t )a - 1)) & ~(( uintptr_t )a - 1))

void* ngx_mem_pool::ngx_palloc_small(size_t size, unsigned int align) {
    unsigned char* m;
    ngx_pool_s* p;

    p = m_pool->current;

    do {
        m = p->d.last;

        if (align) {
            m = ngx_align_ptr(m, NGX_ALIGNMENT);
        }

        if (( size_t )(p->d.end - m) >= size) {
            p->d.last = m + size;

            return m;
        }

        p = p->d.next;
    } while (p);

    return ngx_palloc_block(size);
}

void* ngx_mem_pool::ngx_palloc_block(size_t size) {
    unsigned char* m;
    size_t psize;
    ngx_pool_s *p, *ne;

    psize = ( size_t )(m_pool->d.end - ( u_char* )m_pool);
    m = ( unsigned char* )malloc(psize);
    if (m == nullptr) {
        return nullptr;
    }

    ne = ( ngx_pool_s* )m;

    ne->d.end = m + psize;
    ne->d.next = nullptr;
    ne->d.failed = 0;

    m += sizeof(ngx_pool_data_s);
    m = ngx_align_ptr(m, NGX_ALIGNMENT);

    for (p = m_pool->current; p->d.next; p = p->d.next) {
        if (p->d.failed++ > 4) {
            m_pool->current = p->d.next;
        }
    }

    p->d.next = ne;

    return m;
}

void* ngx_mem_pool::ngx_palloc_large(size_t size) {
    void* p;
    unsigned int n;
    ngx_pool_large_s* large;

    p = malloc(size);
    if (p == nullptr) {
        return nullptr;
    }

    n = 0;
    for (large = m_pool->large; large; large = large->next) {
        if (large->alloc == nullptr) {
            large->alloc = p;
            return p;
        }
        if (n++ > 3) {
            break;
        }
    }
    large = ( ngx_pool_large_s* )ngx_palloc_small(sizeof(ngx_pool_large_s), 1);
    if (large == nullptr) {
        free(p);
        return nullptr;
    }

    large->alloc = p;             // alloc记录大块内存的起始地址
    large->next = m_pool->large;  //头插法，内存池中中的large入口
    m_pool->large = large;

    return p;
}

void ngx_mem_pool::ngx_pfree(void* p) {
    ngx_pool_large_s* l;

    for (l = m_pool->large; l; l = l->next) {
        if (p == l->alloc) {
            free(l->alloc);
            l->alloc = nullptr;

            return;
        }
    }
}

#define ngx_memzero(buf, n) ( void )memset(buf, 0, n)

void* ngx_mem_pool::ngx_pcalloc(size_t size) {
    void* p;
    p = ngx_palloc(size);
    if (p) {
        ngx_memzero(p, size);
    }

    return p;
}

void ngx_mem_pool::ngx_reset_pool() {
    ngx_pool_s* p;
    ngx_pool_large_s* l;

    for (l = m_pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    p = m_pool;
    p->d.last = ( unsigned char* )p + sizeof(ngx_pool_s);
    p->d.failed = 0;

    m_pool->current = m_pool;
    m_pool->large = nullptr;  //在reset小块内存的时候大块内存的内存头就已经释放了
}

void ngx_mem_pool::ngx_destroy_pool() {
    ngx_pool_s *p, *n;
    ngx_pool_large_s* l;
    ngx_pool_cleanup_s* c;

    for (c = m_pool->cleanup; c; c = c->next) {
        if (c->handler) {
            c->handler(c->data);
        }
    }
    for (l = m_pool->large; l; l = l->next) {
        if (l->alloc) {
            free(l->alloc);
        }
    }

    for (p = m_pool, n = m_pool->d.next; /* void */; p = n, n = n->d.next) {
        free(p);
        if (n == nullptr) {
            break;
        }
    }
}

ngx_pool_cleanup_s* ngx_mem_pool::ngx_pool_cleanup_add(size_t size) {
    ngx_pool_cleanup_s* c;
    c = ( ngx_pool_cleanup_s* )ngx_palloc(sizeof(ngx_pool_cleanup_s));
    if (c == nullptr) {
        return nullptr;
    }

    if (size) {
        c->data = ngx_palloc(size);
        if (c->data == nullptr) {
            return nullptr;
        }

    } else {
        c->data = nullptr;
    }

    c->handler = nullptr;
    c->next = m_pool->cleanup;
    m_pool->cleanup = c;

    return c;
}
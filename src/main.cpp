#include <stdio.h>
#include <string.h>
#include "ngx_mem_pool.h"

typedef struct Data stData;
struct Data {
    char* ptr;
    FILE* pfile;
};

void func1(void* p1) {
    char* p = ( char* )p1;
    printf("free ptr mem!");
    free(p);
}

int main() {
    ngx_mem_pool mempool;

    void* p1 = mempool.ngx_palloc(128);   
    if (p1 == nullptr) {
        printf("ngx_palloc 128 bytes fail...");
        return -1;
    }

    stData* p2 = ( stData* )mempool.ngx_palloc(512);   
    if (p2 == nullptr) {
        printf("ngx_palloc 512 bytes fail...");
        return -1;
    }
    p2->ptr = ( char* )malloc(12);
    strcpy(p2->ptr, "hello world");

    ngx_pool_cleanup_s* c1 = mempool.ngx_pool_cleanup_add(sizeof(char*));
    c1->handler = func1;
    c1->data = p2->ptr;

    return 0;
}
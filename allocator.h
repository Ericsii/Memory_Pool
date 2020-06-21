#ifndef MEMORY_POOL_ALLOCATOR_H
#define MEMORY_POOL_ALLOCATOR_H


#include <cstdlib>

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void *pMem) noexcept ;
void operator delete[](void *pMem) noexcept ;
void* mem_alloc(size_t size);
void mem_free(void *pMem);


#endif //MEMORY_POOL_ALLOCATOR_H

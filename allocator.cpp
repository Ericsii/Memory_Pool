#include "allocator.h"
#include "memory_pool.hpp"

void* operator new(size_t size) {
    return MemoryMgr::Instance().allocMemory(size);
}

void* operator new[](size_t size) {
    return MemoryMgr::Instance().allocMemory(size);
}

void operator delete(void *pMem) noexcept {
    MemoryMgr::Instance().freeMemory(pMem);
}

void operator delete[](void *pMem) noexcept {
    MemoryMgr::Instance().freeMemory(pMem);
}

void *mem_alloc(size_t size) {
    return MemoryMgr::Instance().allocMemory(size);
}

void mem_free(void *pMem) {
    MemoryMgr::Instance().freeMemory(pMem);
}

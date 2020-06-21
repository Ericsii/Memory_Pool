#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

// 调试模式
#define _DEBUG

#include <cstdlib>
#include <cassert>
#include <cstdio>
#include <mutex>

// 最大内存大小
// 超过则采用分配自由空间的方式
#define MAX_MEMORY_SIZE 1024
// 块个数
#define BLOCKS_SIZE 10000

// 判断是否调试模式
#ifdef _DEBUG
#include <cstdarg>
#define mDebug(...) printf(__VA_ARGS__)
#else
#define mDebug(...)
#endif // _DEBUG

typedef unsigned int uint;

class MemoryAlloc;

// 内存块
struct MemoryBlock{
    MemoryAlloc* pAlloc;            // 所属池指针
    MemoryBlock* pPrev;             // 链表上一个内存块
    MemoryBlock* pNext;             // 链表下一内存块
    int mId;                        // 内存块编号
    int mRef;                       // 引用次数
    bool inPool;                    // 是否在池中
    bool allocPool;
    char c[2];                      // 占位(用于内存中字节对齐)
};

// 内存池
class MemoryAlloc {
protected:
    char* _pSelf;                   // 内存池地址
    MemoryBlock* _pHead;            // 空闲内存链表头
    MemoryBlock* _pHeadUsed;        // 已分配自由空间链表
    size_t _nSize;                  // 内存块大小
    size_t _nBlocks;                // 内存块个数
    std::mutex _mutex;              // 互斥对象(预备线程锁)

public:
    MemoryAlloc() {
        _pSelf = nullptr;
        _pHead = nullptr;
        _pHeadUsed = nullptr;
        _nSize = 0;
        _nBlocks = 0;
    }

    ~MemoryAlloc() {
        MemoryBlock *pT = _pHeadUsed;
        while(_pHeadUsed) {
            pT = _pHeadUsed;
            _pHeadUsed = _pHeadUsed->pNext;

            mDebug("Free Memory: %llx, id: %d, refs: %d\n", pT, pT->mId, pT->mRef);
            free(pT);
        }
        if(_pSelf) {
            mDebug("Free Allocator: %llx\n", _pSelf);
            free(_pSelf);
        }

    }

    // 初始化内存池
    // 申请内存池空间
    // 遍历初始化块链表
    void init() {
        // 加锁线程安全
        _mutex.lock();

        if(_pSelf)
            return;

        size_t realSize = sizeof(MemoryBlock) + _nSize; // 单个内存块的真实大小
        size_t blocksSize = _nBlocks * realSize; // 池总空间大小

        // 申请空间
        _pSelf = (char*)malloc(blocksSize);

        // 初始化内存块链表及参数
        _pHead = (MemoryBlock*)_pSelf;
        _pHead->inPool = true;
        _pHead->allocPool = true;
        _pHead->mId = 0;
        _pHead->pAlloc = this;
        _pHead->mRef = 0;
        _pHead->pPrev = nullptr;
        _pHead->pNext = nullptr;

        // 顺序遍历内存
        // 初始化内存分配链表
        MemoryBlock *pT_1 = _pHead, *pT_2 = nullptr;
        for(size_t i = 1; i < _nBlocks; i++) {
            pT_2 = (MemoryBlock*)((char*)pT_1 + realSize);
            pT_2->inPool = true;
            pT_2->allocPool = true;
            pT_2->mId = i;
            pT_2->pAlloc = this;
            pT_2->mRef = 0;
            pT_2->pNext = nullptr;
            pT_2->pPrev = pT_1;
            pT_1->pNext = pT_2;
            pT_1 = pT_2;
        }

        mDebug("Initialize Allocator: %llx, size: %d\n", _pSelf, _nSize);

        // 解锁
        _mutex.unlock();
    }

    // 申请内存操作(线程安全)
    // parameters:
    //      nSize: 申请内存大小
    // return: 申请内存指针
    void* allocMemory(size_t nSize) {

        // 没有初始化则初始化内存池
        if(!_pSelf) {
            init();
        }

        // 加锁
        _mutex.lock();

        MemoryBlock *pReturn = nullptr;
        if(_pHead == nullptr) {



            // 内存池空间分配完
            // 申请自由内存空间


            pReturn = (MemoryBlock*)malloc(sizeof(MemoryBlock) + nSize);
            pReturn->inPool = false;
            pReturn->allocPool = true;
            pReturn->mId = -1;
            pReturn->mRef = 1;
            pReturn->pPrev = nullptr;
            pReturn->pNext = nullptr;
            pReturn->pAlloc = this;

            // 添加进已使用自由空间链表头中
            if(_pHeadUsed == nullptr) {
                _pHeadUsed = pReturn;
            }
            else {
                pReturn->pNext = _pHeadUsed;
                _pHeadUsed->pPrev = pReturn;
                _pHeadUsed = pReturn;
            }


        }
        else {


            // 取链表头作为分配空间
            pReturn = _pHead;
            _pHead = _pHead->pNext;
            pReturn->mRef = 1;
            pReturn->pNext = pReturn->pPrev = nullptr;

        }

        mDebug("Alloc Memory: %llx, id: %d, size:%d\n", pReturn, pReturn->mId, nSize);
        // 通过块指针定位到所分配的对象指针地址

        // 解锁
        _mutex.unlock();
        return ((char*)pReturn + sizeof(MemoryBlock));
    }

    // 释放空间(线程安全)
    // 在池中的内存回收进池链表
    // 自由内存空间直接释放
    // parameters:
    //      pMemory: 所需释放的内存指针
    // return: 0 如果释放成功
    void freeMemory(void *pMemory) {
        // 线程锁(线程安全)


        // 通过对象指针定位到块指针
        auto *pBlock = (MemoryBlock*)((char*)pMemory - sizeof(MemoryBlock));

        // 每个内存引用次数计数
        if(--pBlock->mRef != 0)
            return;

        // 加锁
        _mutex.lock();
        // 判断是否处于内存池中
        if(pBlock->inPool) {
            // 将内存单元加入未分配链表中
            if(_pHead == nullptr) {
                pBlock->pPrev = nullptr;
                pBlock->pNext = nullptr;
                _pHead = pBlock;
            }
            else {
                pBlock->pPrev = nullptr;
                pBlock->pNext = _pHead;
                _pHead->pPrev = pBlock;
                _pHead = pBlock;
            }


            mDebug("Recycle Memory: %llx, id: %d\n", pBlock, pBlock->mId);
        }
        else {
            // 自由内存直接释放

            // 维护自由空间链表
            if(pBlock->pPrev) {
                pBlock->pPrev->pNext = pBlock->pNext;
            }
            else {
                // 断言自由空间链表头为此块
                assert(_pHeadUsed == pBlock);
                _pHeadUsed = pBlock->pNext;
            }
            if(pBlock->pNext) {
                pBlock->pNext->pPrev = pBlock->pPrev;
            }

            mDebug("Free Memory: %llx, id: %d\n", pBlock, pBlock->mId, pBlock->mRef);
            free(pBlock);
        }

        // 解锁
        _mutex.unlock();

    }

};

// 模板内存池类
// nSize: 内存单元大小
// nBlocks: 内存单元个数
template<size_t nSize, size_t nBlocks>
class MemoryAllocator: public MemoryAlloc {
public:
    MemoryAllocator() {
        // 保证对齐内存(避免出现泄露)
        const size_t ptrSize = sizeof(void*);
        _nSize = (nSize / ptrSize) * ptrSize + (nSize % ptrSize ? ptrSize : 0);
        _nBlocks = nBlocks;
    }
};

// 内存管理类
// 多个不同大小的内存池组成
// 便于分配不同大小的内存
// 单例，静态对象
class MemoryMgr {
private:
    // 创建不同大小的内存池
    MemoryAllocator<64, BLOCKS_SIZE> _mem_64;
    MemoryAllocator<128, BLOCKS_SIZE> _mem_128;
    MemoryAllocator<256, BLOCKS_SIZE> _mem_256;
    MemoryAllocator<512, BLOCKS_SIZE> _mem_512;
    MemoryAllocator<1024, BLOCKS_SIZE> _mem_1024;

    MemoryAlloc *_allocMap[MAX_MEMORY_SIZE + 1];    // 内存分配器映射

    MemoryBlock *_pHeadUsed;                        // 自由空间链表头

    std::mutex _mutex;                              // 线程互斥变量(用于线程锁)

    MemoryMgr() {
        // 配置映射
        MakeLink(0, 64, &_mem_64);
        MakeLink(65, 128, &_mem_128);
        MakeLink(129, 256, &_mem_256);
        MakeLink(257, 512, &_mem_512);
        MakeLink(513, 1024, &_mem_1024);
    }

    ~MemoryMgr() {
        MemoryBlock *pT = nullptr;
        while(_pHeadUsed) {
            pT = _pHeadUsed;
            _pHeadUsed = _pHeadUsed->pNext;

            mDebug("Free Memory: %llx, id: %d\n", pT, pT->mId);
            free(pT);
        }
    }

    // 内存分配映射
    // 不同内存大小使用不同的池进行分配
    // parameters:
    //      begin: 内存开始分配大小
    //      end: 内存分配截至大小
    void MakeLink(size_t begin, size_t end, MemoryAlloc * pMemAlloc) {
        for(size_t i = begin; i <= end; ++i) {
            _allocMap[i] = pMemAlloc;
        }
    }

public:

    static MemoryMgr& Instance() {
        // 静态单例
        static MemoryMgr memMgr;

        return memMgr;
    }

    void* allocMemory(size_t nSize) {
        // 线程锁(线程安全)

        // 是否超出分配空间
        if(nSize <= MAX_MEMORY_SIZE) {
            return _allocMap[nSize]->allocMemory(nSize);
        }
        else {

            // 加锁
            _mutex.lock();

            MemoryBlock *pReturn = (MemoryBlock*)malloc(sizeof(MemoryBlock) + nSize);

            pReturn->inPool = false;
            pReturn->allocPool = false;
            pReturn->mRef = 1;
            pReturn->mId = -1;
            pReturn->pPrev = nullptr;
            pReturn->pNext = nullptr;
            pReturn->pAlloc = nullptr;

            // 添加如已分配自由内存链表
            if(_pHeadUsed == nullptr) {
                _pHeadUsed = pReturn;
            }
            else {
                pReturn->pNext = _pHeadUsed;
                _pHeadUsed->pPrev = pReturn;
                _pHeadUsed = pReturn;
            }


            mDebug("Alloc Memory: %llx, id: %d, size:%d\n", pReturn, pReturn->mId, nSize);;

            // 解锁
            _mutex.unlock();
            return ((char*)pReturn + sizeof(MemoryBlock));
        }
    }

    // 释放内存
    void freeMemory(void *pMem) {
        // 线程锁(线程安全)

        // 定位内存头
        MemoryBlock *pBlock = (MemoryBlock*)((char *)pMem - sizeof(MemoryBlock));




        if(pBlock->inPool || pBlock->allocPool) {
            // 处于内存池
            pBlock->pAlloc->freeMemory(pMem);
        }
        else {
            // 加锁
            _mutex.lock();

            // 自由分配空间释放
            if(--pBlock->mRef != 0)
                return;

            // 维护自由空间链表
            if(pBlock->pPrev) {
                pBlock->pPrev->pNext = pBlock->pNext;
            }
            else {
                // 断言自由空间链表头为此块
                assert(_pHeadUsed == pBlock);
                _pHeadUsed = pBlock->pNext;
            }
            if(pBlock->pNext) {
                pBlock->pNext->pPrev = pBlock->pPrev;
            }

            mDebug("Free Memory: %llx, id: %d\n", pBlock, pBlock->mId);

            free(pBlock);

            // 解锁
            _mutex.unlock();
        }

    }

    // 添加引用
    void addRef(void *pMem) {
        MemoryBlock *pBlock = (MemoryBlock*)((char*)pMem - sizeof(MemoryBlock));
        ++ pBlock->mRef;
    }
};

#endif //MEMORY_POOL_HPP

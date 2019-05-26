// *****************************************************************************
//
//  Copyright (c) Konstantin Geist. All rights reserved.
//
//  The use and distribution terms for this software are contained in the file
//  named License.txt, which can be found in the root of this distribution.
//  By using this software in any fashion, you are agreeing to be bound by the
//  terms of this license.
//
//  You must not remove this notice, or any other, from this software.
//
// *****************************************************************************

#include "PoolAllocator.h"
#include "RuntimeHelpers.h"
#include <assert.h>

// TODO allocation requests bigger than N should be allocated from common heap
// TODO if a pointer points to interior, it will crash when trying to scan as a root

namespace skizo { namespace script {
using namespace skizo::collections;

#define TARGET_ARENA_SIZE (1024*128)
#define MIN_OBJECT_COUNT_PER_ARENA 64
#define GRANULARITY 16 // 16 bytes for SSE minimum TODO x86-specific

// An arena is a contiguous memory block where fixed-size allocations are made.
// All allocations are prepended a SElementHeader.
struct SArenaHeader
{
    // All elements in an arena are fixed-size.
    size_t ElementSize;

    // Element count.
    size_t ElementCount;

    // Points to the first element right after this header.
    char* Start;
    char* End;
};

// An "element" is the allocated object + metadata (its header).
// So every allocated object has the overhead of sizeof(SElementHeader) + alignment.
struct SElementHeader
{
    // Points to the next element in the free list (if it's inside one).
    SElementHeader* Next;

    // The original pool the object was allocated from. It serves two purposes:
    // To quickly find the original free list to put the element back to (when it's freed).
    // Helps finding allocated objects during heap traversal. If this value isn't null, the object
    // is allocated.
    class CPool* Pool;
};

// A pool is a resizable set of fixed-size arenas and a free list to quickly find free elements.
class CPool: public skizo::core::CObject
{
public:
    CPool(size_t elementSize, SPoolAllocator* allocator);

    SElementHeader* Allocate();
    void AddToFreeList(SElementHeader* element);

private:
    size_t m_elementSize;        // object size + header + alignment
    SPoolAllocator* m_allocator; // required for allocating new arenas
    SElementHeader* m_freeList;
};

    // ****************************
    //          Static
    // ****************************

constexpr bool isLargeObject(size_t sz)
{
    return (sz > TARGET_ARENA_SIZE) || (TARGET_ARENA_SIZE / sz < MIN_OBJECT_COUNT_PER_ARENA);
}

constexpr size_t AlignUp(size_t sz)
{
    /*size_t leftOver = sz % GRANULARITY;
    if(leftOver > 0) {
        sz += GRANULARITY - leftOver;
    }
    return sz;*/
    return sz % GRANULARITY
            ? sz + (GRANULARITY - sz % GRANULARITY)
            : sz;
}

constexpr size_t GetElementSize(size_t objectSize)
{
    return AlignUp(AlignUp(sizeof(SElementHeader)) + objectSize);
}

constexpr char* GetObjectStart(SElementHeader* header)
{
    return reinterpret_cast<char*>(header) + AlignUp(sizeof(SElementHeader));
}

constexpr SElementHeader* GetElementHeader(void* objectStart)
{
    return reinterpret_cast<SElementHeader*>(reinterpret_cast<char*>(objectStart) - AlignUp(sizeof(SElementHeader)));
}

    // ****************************
    //           Pool
    // ****************************

CPool::CPool(size_t elementSize, SPoolAllocator* allocator)
    : m_elementSize(elementSize),
      m_allocator(allocator),
      m_freeList(nullptr)
{
}

SElementHeader* CPool::Allocate()
{
    // No objects in the free list. Create a new arena and allocate new free list elements inside it.
    if(!m_freeList) {
        const SArenaHeader* arena = m_allocator->allocateArena(m_elementSize);

        char* elementStart = arena->Start;
        for(size_t i = 0; i < arena->ElementCount; i++) {
            this->AddToFreeList(reinterpret_cast<SElementHeader*>(elementStart));
            elementStart += m_elementSize;
        }
    }
    assert(m_freeList); // just in case

    // Fetch an element from the free list.
    SElementHeader* element = m_freeList;
    m_freeList = element->Next;

    // Also marks the element as allocated.
    element->Pool = this;

    return element;
}

void CPool::AddToFreeList(SElementHeader* element)
{
    element->Next = m_freeList;
    m_freeList = element;
}

    // ****************************
    //        PoolAllocator
    // ****************************

SPoolAllocator::SPoolAllocator()
    : m_pools(new CHashMap<size_t, CPool*>()),
      m_arenas(new CArrayList<void*>()),
      m_objectsToFree(new CArrayList<void*>()),
      m_objectCount(0),
      m_isEnumerating(false)
{
}

SPoolAllocator::~SPoolAllocator()
{
    for(int i = 0; i < m_arenas->Count(); i++) {
        void* arena = m_arenas->Array()[i];
        free(arena);
    }

    SPointerSetEnumerator largeObjectEnum (m_largeObjectSet);
    void* largeObject;
    while(largeObjectEnum.MoveNext(&largeObject)) {
        free(largeObject);
    }
}

SArenaHeader* SPoolAllocator::allocateArena(size_t elementSize)
{
    const size_t elementCount = TARGET_ARENA_SIZE / elementSize;
    const size_t alignedHeaderSize = AlignUp(sizeof(SArenaHeader));

    const size_t fullArenaSize = alignedHeaderSize + elementCount * elementSize;
    SArenaHeader* arena = (SArenaHeader*)malloc(fullArenaSize);
    if(!arena) {
        _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
    }
    memset(arena, 0, fullArenaSize);

    arena->ElementSize = elementSize;
    arena->ElementCount = elementCount;
    arena->Start = reinterpret_cast<char*>(arena) + alignedHeaderSize;
    arena->End = arena->Start + elementCount * elementSize;

    m_arenas->Add(arena);
    return arena;
}

void* SPoolAllocator::Allocate(size_t objectSize)
{
    const size_t elementSize = GetElementSize(objectSize);

    if(isLargeObject(elementSize)) {
        void* largeObject = malloc(objectSize);
        if(!largeObject) {
            _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
        }

        m_largeObjectSet.Set(largeObject);
        m_objectCount++;

        return largeObject;
    } else {
        CPool* pool;
        if(!m_pools->TryGet(elementSize, &pool)) {
            pool = new CPool(elementSize, this);
            m_pools->Set(elementSize, pool);
        }
        pool->Unref();

        SElementHeader* element = pool->Allocate();
        m_objectCount++;

        return GetObjectStart(element);
    }
}

void SPoolAllocator::Free(void* ptr)
{
    if(m_isEnumerating) {
        m_objectsToFree->Add(ptr);
        return;
    }

    if(m_largeObjectSet.Contains(ptr)) {
        free(ptr);
        m_largeObjectSet.Remove(ptr);
    } else {
        SElementHeader* element = GetElementHeader(ptr);
        CPool* pool = element->Pool;
        pool->AddToFreeList(element);
        element->Pool = nullptr; // also marks as deallocated
    }

    m_objectCount--;
}

bool SPoolAllocator::IsValidPointer(void* objectStart) const
{
    SElementHeader* elementHeader = GetElementHeader(objectStart);
    char* elementPtr = reinterpret_cast<char*>(elementHeader);

    void** arenas = m_arenas->Array();
    const int count = m_arenas->Count();

    for(int i = 0; i < count; i++) {
        const SArenaHeader* arena = reinterpret_cast<SArenaHeader*>(arenas[i]);

        // The element should be inside the arena.
        if(elementPtr >= arena->Start && elementPtr < arena->End) {

            // The element should be properly aligned.
            if(((uintptr_t)elementPtr - (uintptr_t)arena->Start) % arena->ElementSize != 0) {
                return false;
            }

            // The element should be allocated.
            return elementHeader->Pool != nullptr;
        }
    }

    // Nothing found => try in the large object set.
    return m_largeObjectSet.Contains(objectStart);
}

int SPoolAllocator::GetObjectCount() const
{
    return m_objectCount;
}

void SPoolAllocator::freePendingObjects()
{
    for(int i = 0; i < m_objectsToFree->Count(); i++) {
        void* obj = m_objectsToFree->Array()[i];
        this->Free(obj);
    }

    m_objectsToFree->Clear();
}

// TODO can use bitmaps for arenas to quickly find unused elements
void SPoolAllocator::EnumerateObjects(void(*enumProc)(void* obj, void* ctx), void* ctx)
{
    m_isEnumerating = true;

    try {
        int arenaCount = m_arenas->Count();
        void** arenas = m_arenas->Array();
        for(int i = 0; i < arenaCount; i++) {
            SArenaHeader* arena = (SArenaHeader*)arenas[i];

            const size_t elementSize = arena->ElementSize;
            char* cur = arena->Start;
            const char* end = arena->End;

            while(cur < end) {
                SElementHeader* element = (SElementHeader*)cur;
                if(element->Pool) {
                    void* objStart = GetObjectStart(element);
                    enumProc(objStart, ctx);
                }

                cur += elementSize;
            }
        }

        SPointerSetEnumerator largeObjectEnum (m_largeObjectSet);
        void* largeObject;
        while(largeObjectEnum.MoveNext(&largeObject)) {
            enumProc(largeObject, ctx);
        }

        m_isEnumerating = false;
        freePendingObjects();
    } catch (...) {
        m_isEnumerating = false;
        freePendingObjects();
        throw;
    }
}

} }

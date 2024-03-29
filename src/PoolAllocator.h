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

#ifndef POOLALLOCATOR_H_INCLUDED
#define POOLALLOCATOR_H_INCLUDED

#include "ArrayList.h"
#include "HashMap.h"
#include "PointerSet.h"

namespace skizo { namespace script {

struct SArenaHeader;
struct SElementHeader;
class CPool;

/**
 * Used for allocating Skizo objects in the GC heap.
 * The main property of this allocator is that it is possible to quickly find out if a given
 * pointer belongs to the allocator (for the conservative stack scan). As a GC heap is local to its domain,
 * we don't need any fancy multithreading-aware primitives.
 *
 * How it works. Each object size is assigned its own pool. A pool consists of arenas -- raw blocks of memory.
 * A free list is allocated inside such arenas. When an arena fills up, a new one is immediatelly created (but never released).
 * Deallocating an object is merely putting it back to the free list.
 * Finding out if a pointer belongs to the allocator is a matter of locating the arena it belongs to, and figuring out
 * if the pointer is aligned with the beginning of a pooled object and whether the object is actually in use (isn't found
 * in the free list).
 */
struct SPoolAllocator
{
public:
    SPoolAllocator();
    ~SPoolAllocator();

    /**
     * Allocates a memory block with the given size.
     * To be freed with ::Free(...)
     * After allocation, IsValidPointer on the pointer returns true.
     */
    void* Allocate(size_t sz);

    /**
     * Deallocates the given memory block. Should be allocated with ::Allocate(...)
     */
    void Free(void* ptr);

    /**
     * Tells if the given memory block was allocated with this allocator.
     * Useful for conservative scan in the GC.
     */
    bool IsValidPointer(void* ptr) const;

    /**
     * Returns the total number of allocated objects. Useful for debugging.
     */
    int GetObjectCount() const;

    /**
     * Iterates over all allocated objects. Useful for the GC.
     */
    void EnumerateObjects(void(*enumProc)(void* obj, void* ctx), void* ctx);

private:
    friend class CPool;

    SArenaHeader* allocateArena(size_t elementSize);
    void freePendingObjects();

    skizo::core::Auto<skizo::collections::CHashMap<size_t, CPool*>> m_pools;
    skizo::core::Auto<skizo::collections::CArrayList<void*>> m_arenas;

    // It's unsafe to delete objects while enumerating the heap, so such deletions
    // are postponed.
    skizo::core::Auto<skizo::collections::CArrayList<void*>> m_objectsToFree;

    // Large objects are allocated outside of the general heap and stored entirely here.
    SPointerSet m_largeObjectSet;

    int m_objectCount;
    bool m_isEnumerating;
};

} }

#endif // POOLALLOCATOR_H_INCLUDED

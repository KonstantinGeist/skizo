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

#ifndef BUMPPOINTERALLOCATOR_H_INCLUDED
#define BUMPPOINTERALLOCATOR_H_INCLUDED

#include <cstddef>

namespace skizo { namespace script {

/**
 * Allocation type for diagnostics.
 */
enum ESkizoAllocationType
{
    E_SKIZOALLOCATIONTYPE_EXPRESSION,
    E_SKIZOALLOCATIONTYPE_CLASS,
    E_SKIZOALLOCATIONTYPE_MEMBER,
    E_SKIZOALLOCATIONTYPE_TOKEN,

    E_SKIZOALLOCATIONTYPE_COUNT_DONT_USE
};

/**
 * Allocates using the fast bump pointer allocator of the current domain.
 */
#define SO_FAST_ALLOC(sz, allocType) (CDomain::ForCurrentThread()->MemoryManager().BumpPointerAllocator().Allocate(sz, allocType))

/**
 * Fast allocator: allocates data by simply moving the current pointer. All data is freed at once when
 * the allocator is destroyed (on domain teardown).
 * Used by expressions/tokens etc. internally.
 */
struct SBumpPointerAllocator
{
public:
    SBumpPointerAllocator();
    ~SBumpPointerAllocator();

    /**
     * Allocates a new chunk of memory with the given size.
     * The allocation type is used for debugging only.
     * The memory is freed automatically on allocator destruction.
     */
    void* Allocate(int sz, ESkizoAllocationType allocType);

    /**
     * Enables/disables profiling for the allocator. Disabled by default.
     */
    void EnableProfiling(bool value);

    /**
     * Returns the amount of memory allocated for a particular allocation type.
     */
    size_t GetMemoryByAllocationType(ESkizoAllocationType allocType) const;

private:
    struct SBumpPtrAllocPage* m_firstPage;
    struct SBumpPtrAllocPage* m_lastPage;

    bool m_profilingEnabled;
    size_t m_memoryByAllocationType[E_SKIZOALLOCATIONTYPE_COUNT_DONT_USE];

    void addPage();
};

} }

#endif // BUMPPOINTERALLOCATOR_H_INCLUDED

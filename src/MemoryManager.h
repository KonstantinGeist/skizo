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

#ifndef MEMORYMANAGER_H_INCLUDED
#define MEMORYMANAGER_H_INCLUDED

#include "ArrayList.h"
#include "BumpPointerAllocator.h"
#include "HashMap.h"
#include "LinkedList.h"
#include "Mutex.h"
#include "PoolAllocator.h"

namespace skizo { namespace script {

class CClass;

/**
 * For use by SMemoryManager::AddGCRoot(..) and SMemoryManager::RemoveGCRoot(..)
 */
class CGCRootHolder: public skizo::core::CObject
{
public:
    void* pSkizoObject;

    CGCRootHolder(void* p): pSkizoObject(p) { }
};

/**
 * This struct is to be embedded into Domain.
 */
struct SMemoryManager
{
    /**
     * A dictionary of exported objects.
     * Managed by CDomain::ExportObject(..) and others
     * @warning Always access through ExportedObjsMutex
     */
    skizo::core::Auto<skizo::collections::CHashMap<const skizo::core::CString*, void*> > ExportedObjs;
    skizo::core::Auto<skizo::core::CMutex> ExportedObjsMutex;

public:
    SMemoryManager();

    /**
     * Forces a garbage collection.
     * Ignored if called inside a destructor.
     *
     * If domainTeardown is set to true, the garbage collector sweeps out all objects, even if those that are reachable.
     * Used during domain teardown.
     *
     * @warning Internal code should not schedule garbage collection before code emission, as vtables of string
     * literals aren't patched yet.
     */
    void CollectGarbage(bool domainTeardown = false);

    /**
     * Validates the pointer points to a valid Skizo object.
     * For debugging and other purposes.
     * @note Fails for string literals, as they're stored in a separate heap.
     */
    bool IsValidObject(void* obj) const;

    /**
     * Notifies the GC that the object is stored somewhere in native code, don't dispose of it.
     */
    void AddGCRoot(void* soObj);

    /**
     * Notifies the GC that the object is no more used in native code, the GC is free to dispose of it.
     */
    void RemoveGCRoot(void* soObj);

    /**
     * Informs the runtime of a large allocation of native memory that should be taken into account when
	 * scheduling garbage collection.
     */
    void AddMemoryPressure(int i);
    void RemoveMemoryPressure(int i);

    /**
     * Set from SDomainCreation::StackBase, used to scan the stack for pointers.
     */
    void* StackBase() const { return m_stackBase; }
    void SetStackBase(void* value) { m_stackBase = value; }

    /**
     * The initial threshold is SKIZO_MIN_GC_THRESHOLD bytes. If AllocdMemory is higher than the threshold,
     * GC occurs. If AllocdMemory is still higher than the threshold even after a GC, increase the threshold
     * 2 times and deal with it. If AllocdMemory is twice less than the threshold after GC, decrease the threshold
     * 2 times (but it should not be less than SKIZO_MIN_GC_THRESHOLD bytes).
     */
    void SetMinGCThreshold(so_long value) { m_minGCThreshold = value; }

    /**
     * String literals are stored in a separate section of the memory manager.
     * See "icalls/string.cpp" for more information on how string literals are managed.
     */
    const skizo::collections::CArrayList<void*>* StringLiterals() const { return m_stringLiterals; }

    /**
     * See ::StringLiterals()
     */
    void AddStringLiteral(void* literal);

    /**
     * Implements _soX_gc_alloc
     */
    void* Allocate(int sz, void** vtable);

    /**
     * Implements _soX_gc_roots
     */
    void AddGCRoots(void** rootRefs, int count);

    /**
     * Implements _soX_static_vt
     */
    void InitializeStaticValueTypeField(void* obj, CClass* objClass);

    SBumpPointerAllocator& BumpPointerAllocator() { return m_bumpPointerAllocator; }
    const SBumpPointerAllocator& BumpPointerAllocator() const { return m_bumpPointerAllocator; }

    void EnableGCStats(bool value) { m_gcStatsEnabled = value; }

    /**
     * Set in CDomain::CreateDomain(..); used by GC to quickly check if an object is a map (they have special GC maps).
     */
    CClass* MapClass() const { return m_mapClass; }
    void SetMapClass(CClass* value) { m_mapClass = value; }

private:
    void scanStack();
    void gcMark(void* obj_ptr);

    static void sweep(void* obj, void* ctx);

    skizo::core::Auto<skizo::collections::CLinkedList<void*> > m_roots;

    // Used by AddGCRoot(..)/RemoveGCRoot(..) for custom, dynamically added/removed roots.
    skizo::core::Auto<skizo::collections::CArrayList<CGCRootHolder*> > m_gcRootHolders;

    // When new objects are created, these two pointers are updated to reflect the smallest
    // heap pointer and the biggest heap pointer. isValidPtr(..) defined in MemoryManager.cpp uses this
    // information to quickly dismiss pointers outside of the GC heap.
    void* m_heapStart;
    void* m_heapEnd;

    void* m_stackBase;
    so_long m_allocdMemory;
    so_long m_minGCThreshold;
    so_long m_customMemoryPressure;

    // During the sweeping phase, the GC saves objects with dtors to this list to iterate over them later
	// and call their respective dtors.
    skizo::core::Auto<skizo::collections::CArrayList<void*> > m_destructables;

    skizo::core::Auto<skizo::collections::CArrayList<void*> > m_stringLiterals;

    // Avoids infinite recursion in cases when a destructor of an object attempts to call GC::collect() again.
    bool m_disableGC;

    CClass* m_mapClass;

    long int m_lastGCTime; // for profiling
    bool m_gcStatsEnabled; // for profiling

    SPoolAllocator m_poolAllocator;
    SBumpPointerAllocator m_bumpPointerAllocator;

    bool m_dtorsEnabled;
};

} }

#endif // MEMORYMANAGER_H_INCLUDED

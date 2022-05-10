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

#include "MemoryManager.h"
#include "Contract.h"
#include "Domain.h"
#include "icall.h"
#include "RuntimeHelpers.h"
#include "Stopwatch.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

#define IS_LASTBIT_SET(value) ((uintptr_t)(value) & 0x1)
#define SET_LASTBIT(value, bit) value = (void**)(((uintptr_t)(value) & 0xFFFFFFFE) | bit)

SMemoryManager::SMemoryManager():
    ExportedObjs(new CHashMap<const skizo::core::CString*, void*>()),
    ExportedObjsMutex(new CMutex()),
    m_roots(new CLinkedList<void*>()),
    m_gcRootHolders(new CArrayList<CGCRootHolder*>()),
    m_heapStart(nullptr), m_heapEnd(nullptr), m_stackBase(nullptr),
    m_allocdMemory(0), m_minGCThreshold(SKIZO_MIN_GC_THRESHOLD), m_customMemoryPressure(0),
    m_destructables(new CArrayList<void*>()),
    m_stringLiterals(new CArrayList<void*>()),
    m_disableGC(false),
    m_mapClass(nullptr),
    m_lastGCTime(0),
    m_gcStatsEnabled(false),
    m_dtorsEnabled(true)
{
}

extern "C" {

    // *************************************************
    //       Allocation & garbage collection.
    // *************************************************

// WARNING don't introduce RAII
void* SMemoryManager::Allocate(int sz, void** vtable)
{
    SKIZO_REQ_POS(sz);

    // *************************************************************
    //   Checks the size of the heap -- should we collect garbage?
    // *************************************************************

    so_long usedMemory = m_allocdMemory + m_customMemoryPressure;

    // See the header in GC.h
    if(usedMemory > m_minGCThreshold) {

        if(m_gcStatsEnabled) {
            printf("[GC reason] alloc'd memory: %ld; memory pressure: %ld; threshold: %ld\n",
                    (long int)m_allocdMemory,
                    (long int)m_customMemoryPressure,
                    (long int)m_minGCThreshold);
        }

        CollectGarbage(false); // IMPORTANT 'judgement day' flag set to false

        // Recalculate used memory.
        usedMemory = m_allocdMemory + m_customMemoryPressure;

        if(usedMemory > (int)(m_minGCThreshold * 0.75f)) {
            m_minGCThreshold = usedMemory + usedMemory / 2;
            if(m_gcStatsEnabled) {
                printf("GC threshold set to: %ld\n", (long int)m_minGCThreshold);
            }
        } else if(usedMemory < (m_minGCThreshold / 2)) {
            m_minGCThreshold /= 2;
            if(m_minGCThreshold < SKIZO_MIN_GC_THRESHOLD) {
                m_minGCThreshold = SKIZO_MIN_GC_THRESHOLD;
            }
            if(m_gcStatsEnabled) {
                printf("GC threshold set to: %ld\n", (long int)m_minGCThreshold);
            }
        }
    }

    // **************************
    //   Allocates the object.
    // **************************

    const CClass* pClass = ((CClass*)vtable[0]);
    // An abort in the static constructor?
    if(!pClass->IsInitialized()) {
        _soX_abort0(SKIZO_ERRORCODE_TYPE_INITIALIZATION_ERROR);
    }

    // Allocates "sz" with a header.
    void* obj = m_poolAllocator.Allocate(sz);
    if(!obj) {
        _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
    }
    memset(obj, 0, sz);

    if(pClass->SpecialClass() == E_SPECIALCLASS_ARRAY) {
        m_allocdMemory += sz;
    } else {
        m_allocdMemory += pClass->GCInfo().ContentSize;
    }

	// Updates the bounds of the heap for faster pointer validation (see ::IsValidObject(..))
    if(m_heapStart > obj) {
        m_heapStart = obj;
    }
    if(m_heapEnd < ((char*)obj + sz)) {
        m_heapEnd = (char*)obj + sz;
    }

    ((SObjectHeader*)(obj))->vtable = vtable;
    return obj;
}

void* SKIZO_API _soX_gc_alloc(SMemoryManager* mm, int sz, void** vtable)
{
    return mm->Allocate(sz, vtable);
}

// WARNING don't introduce RAII
void* _soX_gc_alloc_env(SMemoryManager* mm, void* _objClass)
{
    CClass* objClass = (CClass*)_objClass;
    SKIZO_REQ_EQUALS(objClass->SpecialClass(), E_SPECIALCLASS_CLOSUREENV);

    // Creates the vtable on demand. It will be deleted in the class' destructor (there's a special clause there
    // for closure envs).
    // NOTE that closure env's vtables are pretty simple, they have no methods.
    if(!objClass->VirtualTable()) {
        void** vtable = new void*[1];
        vtable[0] = _objClass;
        objClass->SetVirtualTable(vtable);
    }

    return _soX_gc_alloc(mm, objClass->GCInfo().ContentSize, objClass->VirtualTable());
}

void SMemoryManager::AddGCRoots(void** rootRefs, int count)
{
    for(int i = 0; i < count; i++) {
        m_roots->Add(rootRefs[i]);
    }    
}

void SMemoryManager::InitializeStaticValueTypeField(void* obj, CClass* objClass)
{
    const SGCInfo& gcInfo = objClass->GCInfo();

    // Zero-initializes the static valuetype field.
    memset(obj, 0, gcInfo.ContentSize);

    // Registers GC roots.
    for(int i = 0; i < gcInfo.GCMapSize; i++) {
        const int offset = gcInfo.GCMap[i];
        m_roots->Add((char*)obj + offset);
    }
}

void SKIZO_API _soX_gc_roots(SMemoryManager* mm, void** rootRefs, int count)
{
    mm->AddGCRoots(rootRefs, count);
}

void SKIZO_API _soX_static_vt(SMemoryManager* mm, void* obj, void* objClass)
{
    mm->InitializeStaticValueTypeField(obj, (CClass*)objClass);
}

}

// WARNING don't introduce RAII
// TODO use work queues to avoid crashing on deep hierarchies due to stack overflows
void SMemoryManager::gcMark(void* obj_ptr)
{
    if(!obj_ptr) {
        return;
    }

    SObjectHeader* obj = static_cast<SObjectHeader*>(obj_ptr);

    // If vtable field's least significant bit is already set to 1, do nothing -- to avoid recursing forever.
    if(!IS_LASTBIT_SET(obj->vtable)) {
        // Extracts the class from the vtable (the class is stored at the zeroth index).
        CClass* pClass = static_cast<CClass*>(obj->vtable[0]);

        // Marks the object live by setting the least significant bit of its "vtable" pointer to 1.
		// NOTE Corrupts the vtable for general use, will be reverted after GC (see below).
        SET_LASTBIT(obj->vtable, 1);

        const int* gcMap;
        int gcMapSize;

        if(pClass->SpecialClass() == E_SPECIALCLASS_ARRAY) {
            // A separate code path for arrays, they have unique GC maps per array.
            const CClass* const pWrappedClass = pClass->ResolvedWrappedClass();
            const SGCInfo& wrappedGCInfo = pWrappedClass->GCInfo();
            
            gcMap = wrappedGCInfo.GCMap;
            const SArrayHeader* array = (SArrayHeader*)obj_ptr;
            size_t offset = offsetof(SArrayHeader, firstItem);

            if(gcMap) {
                for(int i = 0; i < array->length; i++) {

                    if(pWrappedClass->IsValueType()) {
                        for(int j = 0; j < wrappedGCInfo.GCMapSize; j++) {
                            void* child = reinterpret_cast<void**>((char*)obj_ptr + offset + gcMap[j])[0];
                            gcMark(child);
                        }
                    } else {
                        void* child = reinterpret_cast<void**>((char*)obj_ptr + offset)[0];
                        gcMark(child);
                    }

                    offset += wrappedGCInfo.SizeForUse;
                }
            } else if(!pWrappedClass->IsValueType()) {

                // We still want to mark by-ref objects even if they don't have gc maps.
                for(int i = 0; i < array->length; i++) {
                    void* child = reinterpret_cast<void**>((char*)obj_ptr + offset)[0];
                    gcMark(child);

                    offset += pWrappedClass->GCInfo().SizeForUse;
                }

            }

        } else if(pClass == m_mapClass) {

            // Special case for maps.
            const SkizoMapObject* mapObj = ((SMapHeader*)obj_ptr)->mapObj;
            SHashMapEnumerator<SkizoMapObjectKey, void*> mapEnum (mapObj->BackingMap);
            SkizoMapObjectKey mapObjKey;
            void* childObj;
            while(mapEnum.MoveNext(&mapObjKey, &childObj)) {
                gcMark(childObj);
                gcMark(mapObjKey.Key);
            }

        } else {

            // General case.

            const SGCInfo& gcInfo = pClass->GCInfo();
            gcMap = gcInfo.GCMap;
            gcMapSize = gcInfo.GCMapSize;

            // Visits heap object fields recursively according to the GC map.
            for(int i = 0; i < gcMapSize; i++) {
                const int offset = gcMap[i];

                void* child = reinterpret_cast<void**>((char*)obj_ptr + offset)[0];
                gcMark(child);
            }

        }
    }
}

bool SMemoryManager::IsValidObject(void* ptr) const
{
    // Zeros and non-aligned pointers are discarded immediately.
    if(ptr && ((uintptr_t)(ptr) % sizeof(void*) == 0)) {

        // Another shortcut. The heap bounds are updated in _soX_gc_alloc(..)
        if(ptr < m_heapStart && ptr >= m_heapEnd) {
            return false;
        }

        return m_poolAllocator.IsValidPointer(ptr);

    } else {
        return false;
    }
}

// Scans the native stack for heap references and marks them as roots.
// WARNING Doesn't work for architectures without a growing stack.
void SMemoryManager::scanStack()
{
    void** start = (void**)m_stackBase;
    void** end = (void**)&start;
 
    // The descending order for X86/64-based CPUs.
    SKIZO_REQ(end < start, EC_PLATFORM_DEPENDENT);
    for(void** i = end; i < start; i++) {
        void* v = *i;

        if(this->IsValidObject(v)) {
            gcMark(v);
        }
    }
}

void SMemoryManager::sweep(void* rawObj, void* ctx)
{
    SMemoryManager* mngr = (SMemoryManager*)ctx;
    SObjectHeader* obj = (SObjectHeader*)rawObj;

    if(IS_LASTBIT_SET(obj->vtable)) {
        // IMPORTANT Resets the mark for marked (reachable) objects. Not doing so would corrupt the vtables for general use.
        SET_LASTBIT(obj->vtable, 0);
    } else {
        // Frees objects that were left unmarked.

        // GC must know how much memory is allocated/deallocated.
        const CClass* pClass = static_cast<CClass*>(obj->vtable[0]);

        if(pClass->SpecialClass() == E_SPECIALCLASS_ARRAY) {
            const size_t itemSize = pClass->ResolvedWrappedClass()->GCInfo().SizeForUse;
            mngr->m_allocdMemory -= (offsetof(SArrayHeader, firstItem) + ((SArrayHeader*)obj)->length * itemSize);
        } else {
            mngr->m_allocdMemory -= pClass->GCInfo().ContentSize;
        }

        // NOTE Closures have built-in dtors that get rid of C=>Skizo thunks.
        // NOTE the object isn't added to the destructor list if dtorsEnabled=false (the GC is rerun)
        // TODO won't this leak C=>Skizo thunks?
        if((mngr->m_dtorsEnabled && pClass->InstanceDtor()) || pClass->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
            mngr->m_destructables->Add(obj);
        } else {
            mngr->m_poolAllocator.Free(obj);
        }
    }
}

// WARNING don't introduce RAII
void SMemoryManager::CollectGarbage(bool domainTeardown)
{
    // See SMemoryManager::m_disableGC for details.
    if(m_disableGC) {
        return;
    }

    if(m_gcStatsEnabled) {
        printf("\nMemory before GC: %d | Object count before GC: %d\n", (int)m_allocdMemory, m_poolAllocator.GetObjectCount());
    }
    SStopwatch stopwatch;
    stopwatch.Start();

    // ***************
    //   Mark phase.
    // ***************

    // Mark phase is ignored during the "domainTeardown" collection, i.e. on domain teardown.
    if(!domainTeardown) {
        for(SLinkedListNode<void*>* node = m_roots->FirstNode(); node; node = node->Next) {
            // WARNING! m_roots are not pointers to actual variables! m_roots are references to _the locations_ that hold
            // the variables! It's important.
            // So here we dereference the value.
            void* obj = reinterpret_cast<void**>(node->Value)[0];
            gcMark(obj);
        }

        scanStack();
    }

    // ****************
    //   Sweep phase.
    // ****************

    // Dtors may be creating new objects on domain teardown: in that case, the memory manager attempts to recollect
    // garbage once again. However, this algorithm can potentially break if a destructor creates objects with
    // destructors every time the GC is run. To solve the issue, dtorsEnabled is set to false before garbage
    // collection is reattempted.
    m_dtorsEnabled = true;

doGC:
    m_poolAllocator.EnumerateObjects(sweep, this);

    // Resets the mark bits of string literals back to the "accessible" state, otherwise their vtables would be corrupted.
    // NOTE String literals are destroyed only on domain teardown.
    for(int i = 0; i < m_stringLiterals->Count(); i++) {
        SObjectHeader* obj = (SObjectHeader*)m_stringLiterals->Array()[i];
        SET_LASTBIT(obj->vtable, 0);
    }

    // *********************
    //   Destructor phase.
    // *********************

    typedef void (SKIZO_API * FDtor)(SObjectHeader* self);
    m_disableGC = true; // Do not run GC inside destructors if they happen to allocate (and therefore trigger GC).
    for(int i = 0; i < m_destructables->Count(); i++) {
        SObjectHeader* obj = (SObjectHeader*)m_destructables->Array()[i];
        const CClass* pClass = static_cast<CClass*>(obj->vtable[0]);

        FDtor dtor = (FDtor)pClass->DtorImpl();

        try {
            dtor(obj);
        } catch (...) {
            // Aborts/possible leaking Skizo exceptions are ignored.
        }

        m_poolAllocator.Free(obj);
    }
    m_destructables->Clear();
    m_disableGC = false;

    // On domain teardown, we finally get rid of string literals.
    // See "icalls/string.cpp" for more information on how string literals are managed.
    if(domainTeardown) {
        for(int i = 0; i < m_stringLiterals->Count(); i++) {
            void* strLiteral = (SStringHeader*)m_stringLiterals->Array()[i];
            _so_string_dtor(strLiteral);
            // IMPORTANT see Domain::InternStringLiteral(..) where it uses malloc(..)
            free(strLiteral);
        }
        m_stringLiterals->Clear();
    }

    // ********************************************************************
    //  FIX
    // On domain teardown, destructors may have created new objects.
    // The GC tries to get rid of them once again, however, this time dtors
    // are disabled so that such garbage was never allocated again.
    // ********************************************************************

    if(domainTeardown && m_poolAllocator.GetObjectCount() && m_dtorsEnabled) {
        m_dtorsEnabled = false;
        goto doGC;
    }

    // ***********************************

    m_lastGCTime = (long int)stopwatch.End();
    if(m_gcStatsEnabled) {
        printf("Memory after GC: %d, time: %ld | Object count after GC: %d\n",
            (int)m_allocdMemory,
                 m_lastGCTime,
                 m_poolAllocator.GetObjectCount());
    }
}

    // ************************************************
    //           Miscellaneous helper methods.
    // ************************************************

void SMemoryManager::AddGCRoot(void* obj)
{
    // TODO allow adding it as a root twice?

    if(!IsValidObject(obj)) {
        CDomain::Abort("Attempt to root an invalid object.");
    }

    Auto<CGCRootHolder> rootHolder (new CGCRootHolder(obj));
    m_gcRootHolders->Add(rootHolder);
    m_roots->Add(&rootHolder->pSkizoObject);
}

void SMemoryManager::RemoveGCRoot(void* obj)
{
    if(!IsValidObject(obj)) {
        CDomain::Abort("Attempt to unroot an invalid object.");
    }

    int foundIndex = -1;
    for(int i = 0; i < m_gcRootHolders->Count(); i++) {
        const CGCRootHolder* rootHolder = m_gcRootHolders->Array()[i];

        if(rootHolder->pSkizoObject == obj) {
            foundIndex = i;
            break;
        }
    }

    if(foundIndex == -1) {
        CDomain::Abort("Specified GC root not found.");
    }

    CGCRootHolder* rootHolder = m_gcRootHolders->Item(foundIndex);
    const bool b = m_roots->Remove(&rootHolder->pSkizoObject);
    (void)b;
    SKIZO_REQ(b, EC_ILLEGAL_ARGUMENT);
    m_gcRootHolders->RemoveAt(foundIndex);
}

void SMemoryManager::AddMemoryPressure(int i)
{
    if(i < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    const int oldPressure = m_customMemoryPressure;
    m_customMemoryPressure += i;

    // Overflow?
    if(m_customMemoryPressure < oldPressure) {
        m_customMemoryPressure = oldPressure; // Restore.
    }
}

void SMemoryManager::RemoveMemoryPressure(int i)
{
    if(i < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    m_customMemoryPressure -= i;
    if(m_customMemoryPressure < 0) { // disbalanced due to careless user code
        m_customMemoryPressure = 0;
    }
}

void SMemoryManager::AddStringLiteral(void* literal)
{
    m_stringLiterals->Add(literal);
}

} }

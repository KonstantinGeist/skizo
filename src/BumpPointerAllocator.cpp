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

#include "BumpPointerAllocator.h"
#include "Contract.h"
#include <stdlib.h>

#define SKIZO_BUMPPTRALLOCATOR_ENABLED // disable just to test performance
#define SKIZO_EXPRMEMPAGE_SIZE (1024 * 1024)

namespace skizo { namespace script {

struct SBumpPtrAllocPage
{
    SBumpPtrAllocPage *Next, *Prev;
    int Size;
    char FirstByte;
};

SBumpPointerAllocator::SBumpPointerAllocator()
    : m_firstPage(nullptr),
      m_lastPage(nullptr),
      m_profilingEnabled(false)
{
    for(int i = 0; i < E_SKIZOALLOCATIONTYPE_COUNT_DONT_USE; i++) {
        m_memoryByAllocationType[i] = 0;
    }

#ifdef SKIZO_BUMPPTRALLOCATOR_ENABLED
    addPage(); // adds the first page
#endif
}

SBumpPointerAllocator::~SBumpPointerAllocator()
{
    SBumpPtrAllocPage* page = m_firstPage;
    if(page) {
        SBumpPtrAllocPage* tmp = page;
        page = page->Next;

        free(tmp);
    }
}

#ifdef SKIZO_BUMPPTRALLOCATOR_ENABLED
    void SBumpPointerAllocator::addPage()
    {
        SBumpPtrAllocPage* page = (SBumpPtrAllocPage*)malloc(SKIZO_EXPRMEMPAGE_SIZE);
        memset(page, 0, sizeof(SBumpPtrAllocPage));

        if(!m_firstPage) {
            m_firstPage = m_lastPage = page;
        } else {
            m_lastPage->Next = page;
            page->Prev = m_lastPage;
            m_lastPage = page;
        }
    }

    void* SBumpPointerAllocator::Allocate(int sizeRequest, ESkizoAllocationType allocType)
    {
        const int sz = sizeRequest + (sizeof(void*) - sizeRequest % sizeof(void*));
        SKIZO_REQ_EQUALS(sz % sizeof(void*), 0);
        SKIZO_REQ(sz >= sizeRequest, skizo::core::EC_ILLEGAL_ARGUMENT);

        // ****************************************
        // The runtime's memory profiling.
        if(m_profilingEnabled) {
            m_memoryByAllocationType[allocType] += sz;
        }
        // ****************************************

        if((m_lastPage->Size + sz) >= (SKIZO_EXPRMEMPAGE_SIZE - (int)sizeof(SBumpPtrAllocPage))) {
            addPage();
        }

        void* r = &m_lastPage->FirstByte + m_lastPage->Size;
        m_lastPage->Size += sz;
        return r;
    }
#else /* not SKIZO_BUMPPTRALLOCATOR_ENABLED */
    void* SBumpPointerAllocator::Alloc(int sz, ESkizoAllocationType allocType)
    {
        return malloc(sz);
    }
#endif

void SBumpPointerAllocator::EnableProfiling(bool value)
{
    m_profilingEnabled = value;
}

size_t SBumpPointerAllocator::GetMemoryByAllocationType(ESkizoAllocationType allocType) const
{
    SKIZO_REQ_RANGE_D(allocType, (ESkizoAllocationType)0, E_SKIZOALLOCATIONTYPE_COUNT_DONT_USE);
    return m_memoryByAllocationType[allocType];
}

} }

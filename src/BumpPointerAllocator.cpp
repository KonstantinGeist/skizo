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

#define SKIZO_ALLOCATOR_PAGE (1024 * 1024)

namespace skizo { namespace script {
using namespace core;

namespace
{

class CDefaultBumpPointerPageAllocator : public CBumpPointerPageAllocator
{
public:
    void* AllocatePage(size_t sz) override
    {
        return malloc(sz);
    }

    void DeallocatePage(void* page) override
    {
        free(page);
    }
};

}

struct SBumpPointerAllocatorPage
{
    SBumpPointerAllocatorPage *Next, *Prev;
    int Size;
    char FirstByte;
};

void SBumpPointerAllocator::initBase(CBumpPointerPageAllocator* pageAllocator, int alignment)
{
    m_firstPage = nullptr;
    m_lastPage = nullptr;
    m_profilingEnabled = false;

    m_pageAllocator.SetVal(pageAllocator);
    m_alignment = alignment;

    for(int i = 0; i < E_SKIZOALLOCATIONTYPE_COUNT_DONT_USE; i++) {
        m_memoryByAllocationType[i] = 0;
    }

    addPage(); // adds the first page
}

SBumpPointerAllocator::SBumpPointerAllocator(CBumpPointerPageAllocator* pageAllocator, int alignment)
{
    initBase(pageAllocator, alignment);
}

SBumpPointerAllocator::SBumpPointerAllocator()
{
    Auto<CDefaultBumpPointerPageAllocator> allocator (new CDefaultBumpPointerPageAllocator());
    initBase(allocator, (int)sizeof(void*));
}

SBumpPointerAllocator::~SBumpPointerAllocator()
{
    SBumpPointerAllocatorPage* page = m_firstPage;
    while(page) {
        SBumpPointerAllocatorPage* tmp = page;
        page = page->Next;
        m_pageAllocator->DeallocatePage(tmp);
    }
}

void SBumpPointerAllocator::addPage()
{
    SBumpPointerAllocatorPage* page = (SBumpPointerAllocatorPage*)m_pageAllocator->AllocatePage(SKIZO_ALLOCATOR_PAGE);
    memset(page, 0, sizeof(SBumpPointerAllocatorPage));

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
    const int sz = sizeRequest + (m_alignment - sizeRequest % m_alignment);
    SKIZO_REQ_EQUALS(sz % m_alignment, 0);
    SKIZO_REQ(sz >= sizeRequest, skizo::core::EC_ILLEGAL_ARGUMENT);

    if(m_profilingEnabled) {
        m_memoryByAllocationType[allocType] += sz;
    }

    if((m_lastPage->Size + sz) >= (SKIZO_ALLOCATOR_PAGE - (int)sizeof(SBumpPointerAllocatorPage))) {
        addPage();
    }

    void* r = &m_lastPage->FirstByte + m_lastPage->Size;
    m_lastPage->Size += sz;
    return r;
}

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

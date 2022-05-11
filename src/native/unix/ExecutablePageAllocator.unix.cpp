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

#include "../../ExecutablePageAllocator.h"
#include "../../HashMap.h"

#include <sys/mman.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

struct ExecutablePageAllocatorPrivate
{
    Auto<CHashMap<void*, int> > m_addressToSizeMap;

    ExecutablePageAllocatorPrivate()
        : m_addressToSizeMap(new CHashMap<void*, int>())
    {
    }

    int sizeForAddress(void* address) const
    {
        int size;
        if(m_addressToSizeMap->TryGet(address, &size)) {
            return size;
        } else {
            return 0;
        }
    }
};

CExecutablePageAllocator::CExecutablePageAllocator()
    : p (new ExecutablePageAllocatorPrivate())
{
}

CExecutablePageAllocator::~CExecutablePageAllocator()
{
    delete p;
}

void* CExecutablePageAllocator::AllocatePage(size_t sz)
{
    void* r = mmap(NULL,
       sz,
       PROT_READ | PROT_WRITE | PROT_EXEC,
       MAP_ANONYMOUS | MAP_PRIVATE,
       0,
       0);
    if(r == MAP_FAILED) {
        SKIZO_THROW(EC_OUT_OF_RESOURCES);
    }

    p->m_addressToSizeMap->Set(r, (int)sz);

    return r;
}

void CExecutablePageAllocator::DeallocatePage(void* page)
{
    SKIZO_REQ_PTR(page);

    const int sz = p->sizeForAddress(page);
    SKIZO_REQ_POS(sz);

    munmap(page, (size_t)sz);
    p->m_addressToSizeMap->Remove(page);
}

bool CExecutablePageAllocator::HasPointer(void* ptr) const
{
    SHashMapEnumerator<void*, int> mapEnum (p->m_addressToSizeMap);
    void* address;
    int size;

    while(mapEnum.MoveNext(&address, &size)) {
        if(ptr >= (char*)address && ptr < (char*)address+size) {
            return true;
        }
    }

    return false;
}

} }

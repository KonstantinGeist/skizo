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
#include "../../Contract.h"
#include "../../Exception.h"

namespace skizo { namespace script {
using namespace skizo::core;

CExecutablePageAllocator::CExecutablePageAllocator()
{
    // For some MinGW versions.
    #ifndef HEAP_CREATE_ENABLE_EXECUTE
        #define HEAP_CREATE_ENABLE_EXECUTE 0x00040000
    #endif

    p = (ExecutablePageAllocatorPrivate*)HeapCreate(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
    SKIZO_REQ_PTR(p);
}

CExecutablePageAllocator::~CExecutablePageAllocator()
{
    // See MSDN:
    //   Processes can call HeapDestroy without first calling the HeapFree function
    //   to free memory allocated from the heap.
    HeapDestroy((HANDLE)p);
}

void* CExecutablePageAllocator::AllocatePage(size_t sz)
{
    void* v = HeapAlloc((HANDLE)p, HEAP_ZERO_MEMORY, sz);
    if(!v) {
        SKIZO_THROW(EC_OUT_OF_RESOURCES);
    }
    return v;
}

void CExecutablePageAllocator::DeallocatePage(void* page)
{
    SKIZO_REQ_PTR(page);

    HeapFree((HANDLE)p, 0, page);
}

} }

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

#include "Object.h"
#include "AtomicObject.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "init.h"
#include "String.h"

#include <assert.h>
#include <typeinfo>

namespace skizo { namespace core {

    // *************************
    //   Basic leak detector.
    // *************************

#ifdef SKIZO_BASIC_LEAK_DETECTOR
// Basic leak detector.
// Basically just counts the number of skizo objects created/deleted. Doesn't
// specify which objects were created/deleted, doesn't do double-free detection
// etc.
//
// It is a basic step -- if you find that object count is different/huge after
// the program ends, it means you have a leak -- and then you can use advanced
// memory profiling tools such as Valgrind or DrMemory.
//
// Note that some static objects may still be alive, so a small fixed number of
// live objects doesn't mean you have a leak.
struct BasicLeakDetector
{
    so_atomic_int ObjectCount;
    bool Suppress;

    BasicLeakDetector()
        : ObjectCount(0), Suppress(false)
    {
    }

    void printLeakInfo()
    {
        if(this->ObjectCount && !this->Suppress) {
            printf("WARNING: basic leak detector found %d objects unreleased."
                   " Note that some static objects may be still alive, so a small"
                   " fixed number of live objects doesn't mean you have a leak. Also, certain systems"
                   " don't free objects when an unhandled exception is thrown.\n", (int)ObjectCount);
        }
    }

    ~BasicLeakDetector()
    {
        printLeakInfo();
    }
};
// FIX init_priority tells GCC that it should be called the first in the construtor chain
// and the last in the destructor chain.
static BasicLeakDetector basicLeakDetector __attribute__ ((init_priority (101)));

void PrintLeakInfo()
{
    basicLeakDetector.printLeakInfo();
}

void SuppressBasicLeakDetector(bool b)
{
    basicLeakDetector.Suppress = b;
}
#endif

    // *************************
    //        CObject
    // *************************

CObject::CObject()
{
    assert(IsSkizoInitialized());

    m_refCount = 1;

#ifdef SKIZO_BASIC_LEAK_DETECTOR
    CoreUtils::AtomicIncrement(&basicLeakDetector.ObjectCount);
#endif
}

CObject::~CObject()
{
#ifdef SKIZO_BASIC_LEAK_DETECTOR
    CoreUtils::AtomicDecrement(&basicLeakDetector.ObjectCount);
#endif
}

void CObject::Ref() const
{
#ifdef DSKIZO_DEBUG_MODE
    SKIZO_REQ_NOT_EQUALS(m_refCount, 0);
#endif

#ifdef SKIZO_SINGLE_THREADED
    m_refCount++;
#else
    // TODO check for being < INT_MAX (NOTE: _not_ "<= INT_MAX"
    CoreUtils::AtomicIncrement(&m_refCount);
#endif
}

bool CObject::Unref() const
{
#ifdef DSKIZO_DEBUG_MODE
    SKIZO_REQ_NOT_EQUALS(m_refCount, 0);
#endif

    // TODO check for INT_MIN probably
#ifdef SKIZO_SINGLE_THREADED
    if(--m_refCount == 0) {
        delete this;
        return true;
    } else {
        return false;
    }
#else
    if(CoreUtils::AtomicDecrement(&m_refCount) == 0) {
        delete this;
        return true;
    } else {
        return false;
    }
#endif
}

int CObject::GetHashCode() const
{
    return (int)(((reinterpret_cast<uintptr_t>(this)) >> 1) * 1000000007);
}

const CString* CObject::ToString() const
{
    return GetDebugStringInfo(this);
}

bool CObject::Equals(const CObject* obj) const
{
    // Just a reference check.
    return this == obj;
}

    // *************************
    //      CAtomicObject
    // *************************

    // (here to have access to the basic leak detector's fields)

CAtomicObject::CAtomicObject()
{
    m_refCount = 1;

#ifdef SKIZO_BASIC_LEAK_DETECTOR
    CoreUtils::AtomicIncrement(&basicLeakDetector.ObjectCount);
#endif
}

CAtomicObject::~CAtomicObject()
{
    // In a few places, mutexes are created and deleted manually,
    // without using reference counting...
    // assert(m_refCount == 0);

#ifdef SKIZO_BASIC_LEAK_DETECTOR
    CoreUtils::AtomicDecrement(&basicLeakDetector.ObjectCount);
#endif
}

    // *************************
    //    GetDebugStringInfo
    // *************************

const CString* GetDebugStringInfo(const CObject* obj)
{
    return CString::Format("<object of type '%s' at %p refCount = %d>",
                           const_cast<char*>(typeid(*obj).name()),
                           static_cast<const void*>(obj),
                           (int)obj->ReferenceCount());
}

} }

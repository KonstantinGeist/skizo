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

#include "AtomicObject.h"
#include "Contract.h"

namespace skizo { namespace core {

// Constructor/destructor are defined in Object.cpp to access SKIZO_BASIC_LEAK_DETECTOR.

void CAtomicObject::Ref() const
{
    CoreUtils::AtomicIncrement(&m_refCount);
}

// TODO check for INT_MIN probably
bool CAtomicObject::Unref() const
{
    SKIZO_REQ_NOT_EQUALS(m_refCount, 0);

    if(CoreUtils::AtomicDecrement(&m_refCount) == 0) {
        delete this;
        return true;
    } else {
        return false;
    }
}

bool CAtomicObject::Equals(const CAtomicObject* obj) const
{
    return this == obj;
}

int CAtomicObject::GetHashCode() const
{
    return (int)(((reinterpret_cast<uintptr_t>(this)) >> 1) * 1000000007);
}

int CAtomicObject::ReferenceCount() const
{
    return (int)m_refCount;
}

} }

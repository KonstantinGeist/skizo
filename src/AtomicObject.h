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

#ifndef ATOMICOBJECT_H_INCLUDED
#define ATOMICOBJECT_H_INCLUDED

#include "basedefs.h"

namespace skizo { namespace core {

/**
 * Threading-related objects inherit from this class if the project is compiled
 * in the single-threaded mode. This class guarantees that reference counting is
 * atomic no matter the mode.
 */
class CAtomicObject
{
public:
    CAtomicObject();
    virtual ~CAtomicObject();

    /**
     * See CObject::Ref(), except also guaranteed to be always atomic.
     */
    void Ref() const;

    /**
     * See CObject::Unref(), except also guaranteed to be always atomic.
     */
    bool Unref() const;

    /**
     * See CObject::Equals(..)
     */
    virtual bool Equals(const CAtomicObject* obj) const;

    /**
     * See CObject::GetHashCode(..)
     */
    virtual int GetHashCode() const;

    /**
     * Retrieves the current reference count of the object. For debugging purposes
     * only.
     */
    int ReferenceCount() const;
    
private:
    mutable so_atomic_int m_refCount;
};

    // **********
    //   Object
    // **********

inline bool SKIZO_EQUALS(const CAtomicObject* obj1, const CAtomicObject* obj2)
{
    if(obj1 == obj2) {
        return true;
    }
    if(!obj1 || !obj2) {
        return false;
    }
    return obj1->Equals(obj2);
}

inline int SKIZO_HASHCODE(const CAtomicObject* obj)
{
    return obj->GetHashCode();
}

inline void SKIZO_REF(const CAtomicObject* obj)
{
    if(obj) {
        obj->Ref();
    }
}

inline void SKIZO_UNREF(const CAtomicObject* obj)
{
    if(obj) {
        obj->Unref();
    }
}

inline bool SKIZO_IS_NULL(const CAtomicObject* obj)
{
    return !obj;
}

} }

#endif // ATOMICOBJECT_H_INCLUDED

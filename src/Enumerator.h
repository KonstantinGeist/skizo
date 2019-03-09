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

#ifndef ENUMERATOR_H_INCLUDED
#define ENUMERATOR_H_INCLUDED

namespace skizo { namespace collections {

/**
 * Supports a simple iteration over a collection.
 */
template <class T>
struct SEnumerator
{
    /**
     * Sets the enumerator to its initial position, which is before the first
     * element in the collection.
     */
    virtual void Reset() = 0;

    /**
     * Advances the enumerator to the next element of the collection.
     *
     * @param vout the current element in the collection. Note: reference count
     * not increased (if any).
     */
    virtual bool MoveNext(T* vout) = 0;
};

} }

#endif // ENUMERATOR_H_INCLUDED

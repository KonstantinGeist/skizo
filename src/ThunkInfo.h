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

#ifndef THUNKINFO_H_INCLUDED
#define THUNKINFO_H_INCLUDED

namespace skizo { namespace script {

/**
 * Cached info for calls via reflection + ECalls, and others.
 */
class SThunkInfo
{
public:
    /**
     * If this method call returns a primitive, it should be boxed. This pointer points to a constructor
     * of the boxed class which wraps the returned primitive (if any).
     */
    void* pBoxedCreate;

    /**
     * Reflection thunk itself (created by SThunkManager::GetReflectionThunk(..))
     */
    void* pReflectionThunk;

    SThunkInfo()
        : pBoxedCreate(nullptr),
          pReflectionThunk(nullptr)
    {
    }
};
    
} }

#endif // THUNKINFO_H_INCLUDED

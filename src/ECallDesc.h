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

#ifndef ECALLDSC_H_INCLUDED
#define ECALLDSC_H_INCLUDED

#include "StringSlice.h"

namespace skizo { namespace script {

/**
 * Calling conventions of ECalls (external calls).
 */
enum ECallConv
{
    /**
     * Default calling convention.
     */
    E_CALLCONV_CDECL,

    /**
     * Controlled by the "[callConv=stdcall]" attribute.
     */
    E_CALLCONV_STDCALL
};

/**
 * External call description. Embedded into CMethod.
 */
struct SECallDesc
{
    /**
     * If a method is an ECall, this value is not empty (extracted from attribute [module=name]).
     */
    SStringSlice ModuleName;

    /**
     * The name of the ecall. Usually, identical to the method's name.
     */
    SStringSlice EntryPoint;

    /**
     * If a method is an ECall, this value can be non-empty (extracted from attribute [callConv=name]).
     */
    ECallConv CallConv;

    /**
     * The pointer to the actual implementation in an external module; resolved by STransformer in the
     * transform phase.
     */
    void* ImplPtr;

    SECallDesc()
        : CallConv(E_CALLCONV_CDECL),
          ImplPtr(nullptr)
    {
    }

    /**
     * Not all methods are ECalls; in that case, IsValid() returns false (ModuleName and/or EntryPoint not set).
     */
    bool IsValid() const;

    /**
     * Resolves the current ECallDesc; ImplPtr should be non-zero after this call.
     */
    void Resolve(class CMethod* method);
};
    
} }

#endif // ECALLDSC_H_INCLUDED

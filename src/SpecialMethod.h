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

#ifndef SPECIALMETHOD_H_INCLUDED
#define SPECIALMETHOD_H_INCLUDED

namespace skizo { namespace script {

enum ESpecialMethod
{
    E_SPECIALMETHOD_NONE,

    /**
     * If a method is native, then it's implemented somewhere outside of Skizo.
     * There are two types of native methods:
     * a) ICalls -- implemented inside the runtime itself, registered internally.
     * b) ECalls -- implemented externally in a separate native module, loaded dynamically.
     * No need to emit it using expressions or other conventional means.
     * @note Headers are still emitted if this method is not an operator of a primitive type (int, float, etc.)
     */
    E_SPECIALMETHOD_NATIVE,

    /**
     * ECalls defined outside of the base modules in untrusted domains get compiled to special thunks that
     * simply abort (same code can be conditionally shared by trusted and untrusted domains, so we abort in
     * runtime rather than compile time).
     */
    E_SPECIALMETHOD_DISALLOWED_ECALL,

    E_SPECIALMETHOD_FIRE,
    E_SPECIALMETHOD_ADDHANDLER,
    E_SPECIALMETHOD_FOREIGNSYNC,
    E_SPECIALMETHOD_FOREIGNASYNC,
    E_SPECIALMETHOD_ENUM_FROM_INT,

    // *************************
    //   ThunkManager-related.
    // *************************

    /**
     * Closure ctors are generated in ThunkManager instead of the base C compiler to remove some pressure from it.
     */
    E_SPECIALMETHOD_CLOSURE_CTOR,

    E_SPECIALMETHOD_BOXED_METHOD,
    E_SPECIALMETHOD_BOXED_CTOR
};
    
} }

#endif // SPECIALMETHOD_H_INCLUDED

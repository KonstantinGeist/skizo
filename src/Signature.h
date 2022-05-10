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

#ifndef SIGNATURE_H_INCLUDED
#define SIGNATURE_H_INCLUDED

#include "ArrayList.h"
#include "Local.h"
#include "TypeRef.h"

namespace skizo { namespace script {

/**
 * Describes a method signature (including constructors).
 */
class CSignature: public skizo::core::CObject
{
public:
    STypeRef ReturnType;
    skizo::core::Auto<skizo::collections::CArrayList<CParam*> > Params;

    /**
     * Is the method we're describing static?
     */
    bool IsStatic;

    CSignature()
        : Params(new skizo::collections::CArrayList<CParam*>()),
          IsStatic(false)
    {
    }

    bool Equals(const CSignature* other) const;
    bool HasParamByName(const SStringSlice& paramName) const;
};

} }

#endif // SIGNATURE_H_INCLUDED

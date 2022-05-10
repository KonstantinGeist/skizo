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

#ifndef ARRAYINITIALIZATIONTYPE_H_INCLUDED
#define ARRAYINITIALIZATIONTYPE_H_INCLUDED

#include "Object.h"
#include "TypeRef.h"

namespace skizo { namespace script {

/**
 * Array initializations are implemented using "array initialization helpers" which are specialized helper
 * functions for every array initialization type.
 * An array initialization type consists of:
 * a) arity (how many elements there are)
 * b) the type of elements.
 */
class CArrayInitializationType: public skizo::core::CObject
{
public:
    /**
     * An initialization [1] has Arity=1, Initialization [1 2] has Arity=2 etc.
     */
    int Arity;

    /**
     * ArrayType([T])=T
     */
    STypeRef ArrayType;

    CArrayInitializationType(int arity, const STypeRef& arrayType);

    // Support for CHashMap and other collections.
    virtual int GetHashCode() const override;
    virtual bool Equals(const skizo::core::CObject* obj) const override;
};

} }

#endif // ARRAYINITIALIZATIONTYPE_H_INCLUDED

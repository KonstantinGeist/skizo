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

#include "ArrayInitializationType.h"

namespace skizo { namespace script {

CArrayInitializationType::CArrayInitializationType(int arity, const STypeRef& arrayType)
    : Arity(arity),
      ArrayType(arrayType)
{
}

int CArrayInitializationType::GetHashCode() const
{
    return this->Arity * SKIZO_HASHCODE(this->ArrayType);
}

bool CArrayInitializationType::Equals(const CObject* obj) const
{
    const CArrayInitializationType* other = static_cast<const CArrayInitializationType*>(obj);

    if(this->Arity != other->Arity) {
        return false;
    }

    return this->ArrayType.Equals(other->ArrayType);
}

} }

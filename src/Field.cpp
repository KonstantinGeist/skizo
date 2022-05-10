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

#include "Field.h"

namespace skizo { namespace script {

CField::CField()
  : DeclaringClass(nullptr),
    Access(E_ACCESSMODIFIER_PUBLIC),
    IsStatic(false),
    Offset(-1)
{
}

EMemberKind CField::MemberKind() const
{
    return E_RUNTIMEOBJECTKIND_FIELD;
}

} }

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

#include "Local.h"

namespace skizo { namespace script {

        // ******************
        //   Ctors & dtors.
        // ******************

CLocal::CLocal()
    : DeclaringMethod(nullptr),
      IsCaptured(false)
{
}

CLocal::CLocal(const SStringSlice& name, const STypeRef& type, CMethod* declaringMethod)
    : Name(name),
      Type(type),
      DeclaringMethod(declaringMethod),
      IsCaptured(false)
{
}

CLocal* CLocal::Clone() const
{
    CLocal* r = new CLocal(this->Name, this->Type, this->DeclaringMethod);
    r->IsCaptured = this->IsCaptured;
    return r;
}

} }

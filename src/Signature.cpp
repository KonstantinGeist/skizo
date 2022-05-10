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

#include "Signature.h"

namespace skizo { namespace script {

// TODO compare for IsStatic?
bool CSignature::Equals(const CSignature* other) const
{
    if(!this->ReturnType.Equals(other->ReturnType)) {
        return false;
    }

    if(this->Params->Count() != other->Params->Count()) {
        return false;
    }

    for(int i = 0; i < this->Params->Count(); i++) {
        const CParam* thisParam = this->Params->Array()[i];
        const CParam* otherParam = other->Params->Array()[i];

        if(!thisParam->Type.Equals(otherParam->Type)) {
            return false;
        }
    }

    return true;
}

bool CSignature::HasParamByName(const SStringSlice& paramName) const
{
    for(int i = 0; i < Params->Count(); i++) {
        const CParam* param = Params->Array()[i];
        if(param->Name.Equals(paramName)) {
            return true;
        }
    }

    return false;
}

} }

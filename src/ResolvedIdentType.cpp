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

#include "ResolvedIdentType.h"
#include "Class.h"
#include "Const.h"
#include "Field.h"
#include "Local.h"
#include "Method.h"

namespace skizo { namespace script {

bool SResolvedIdentType::IsVoid() const 
{
    return this->EType == E_RESOLVEDIDENTTYPE_VOID;
}

STypeRef SResolvedIdentType::Type() const
{
    switch(this->EType) {
        case E_RESOLVEDIDENTTYPE_PARAM:
            return AsParam_->Type;
        case E_RESOLVEDIDENTTYPE_FIELD:
            return AsField_->Type;
        case E_RESOLVEDIDENTTYPE_LOCAL:
            return AsLocal_->Type;
        case E_RESOLVEDIDENTTYPE_CLASS:
            return AsClass_->ToTypeRef();
        case E_RESOLVEDIDENTTYPE_METHOD:
            return STypeRef(); // void
        case E_RESOLVEDIDENTTYPE_CONST:
            return AsConst_->Type;
        default:
            return STypeRef(); // void
    }
}

} }

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

#include "Const.h"
#include "Class.h"
#include "Contract.h"
#include "Method.h"

namespace skizo { namespace script {

CConst::CConst()
    : Access(E_ACCESSMODIFIER_PUBLIC),
      DeclaringClass(nullptr),
      DeclaringExtClass(nullptr)
{
}

EMemberKind CConst::MemberKind() const
{
    return E_RUNTIMEOBJECTKIND_CONST;
}

bool CConst::IsAccessibleFromMethod(const CMethod* otherMethod) const
{
    bool r = false;

    switch(this->Access) {
        case E_ACCESSMODIFIER_PRIVATE:
            // Special codepath for extension consts, they are allowed to access private consts only if those
            // private consts are defined inside the same extension.
            if(otherMethod->DeclaringExtClass()) {
                r = (this->DeclaringExtClass == otherMethod->DeclaringExtClass());
            } else {
                r = (this->DeclaringClass == otherMethod->DeclaringClass());
            }
            break;

        case E_ACCESSMODIFIER_PROTECTED:
            r = otherMethod->DeclaringClass()->Is(this->DeclaringClass);
            break;

        case E_ACCESSMODIFIER_PUBLIC:
            r = true;
            break;

        case E_ACCESSMODIFIER_INTERNAL:
            r = (this->DeclaringClass->Source().Module == otherMethod->DeclaringClass()->Source().Module);
            break;

        default: SKIZO_REQ_NEVER; break; // Should not happen.
    }

    // Exception for closures: they're allowed to access private/protected consts of enclosing classes.
    if(!r && !otherMethod->DeclaringExtClass()) {
        r = otherMethod->IsEnclosedByClass(this->DeclaringClass);
    }

    return r;
}

} }

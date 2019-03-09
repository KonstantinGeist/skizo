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

#ifndef RESOLVEDIDENTTYPE_H_INCLUDED
#define RESOLVEDIDENTTYPE_H_INCLUDED

#include "TypeRef.h"

namespace skizo { namespace script {

class CLocal;
typedef CLocal CParam;

enum EResolvedIdentType
{
    E_RESOLVEDIDENTTYPE_VOID,
    E_RESOLVEDIDENTTYPE_PARAM,
    E_RESOLVEDIDENTTYPE_FIELD,
    E_RESOLVEDIDENTTYPE_LOCAL,
    E_RESOLVEDIDENTTYPE_CLASS,
    E_RESOLVEDIDENTTYPE_METHOD,
    E_RESOLVEDIDENTTYPE_CONST
};

/**
 * Used by methods like SkizoMethodPrivate::resolveIdent(..) to resolve CIdentExpression's:
 * if this identifier refers to a param, local, class, method or a const?
 */
struct SResolvedIdentType
{
    union {
        CParam* AsParam_;
        class CField* AsField_;
        CLocal* AsLocal_;
        class CClass* AsClass_;
        class CMethod* AsMethod_;
        class CConst* AsConst_;
    };

    EResolvedIdentType EType;

    SResolvedIdentType()
        : AsParam_(nullptr),
          EType(E_RESOLVEDIDENTTYPE_VOID)
    {
    }

    /**
     * A void resolved ident means "nothing found".
     */
    bool IsVoid() const;

    STypeRef Type() const;
};

} }

#endif // RESOLVEDIDENTTYPE_H_INCLUDED

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

#include "../Domain.h"
#include "../ScriptUtils.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_bool_toString(_so_bool b)
{
    CDomain* domain = CDomain::ForCurrentThread();
    Auto<const CString> source (CoreUtils::BoolToString(b? true: false));
    return domain->CreateString(source, true); /* NOTE "true" */
}

int SKIZO_API _so_bool_hashCode(_so_bool b)
{
    return (int)b;
}

_so_bool SKIZO_API _so_bool_equals(_so_bool b, void* otherObj)
{
    return BoxedEquals(&b, sizeof(_so_bool), otherObj, E_PRIMTYPE_BOOL);
}

typedef void (SKIZO_API * _FAction)(void*);
_so_bool SKIZO_API _so_bool_then(_so_bool b, void* actionObj)
{
    if(b) {
        _FAction actionFunc = (_FAction)so_invokemethod_of(actionObj);
        actionFunc(actionObj);
    }

    return b;
}

_so_bool SKIZO_API _so_bool_else(_so_bool b, void* actionObj)
{
    if(!b) {
        _FAction actionFunc = (_FAction)so_invokemethod_of(actionObj);
        actionFunc(actionObj);
    }

    return b;
}

typedef _so_bool (SKIZO_API * _FPredicate)(void*);
void SKIZO_API _so_bool_while(void* predObj, void* actionObj)
{
    _FPredicate predFunc = (_FPredicate)so_invokemethod_of(predObj);
    _FAction actionFunc = (_FAction)so_invokemethod_of(actionObj);

    while(predFunc(predObj)) {
        actionFunc(actionObj);
    }
}

}

} }

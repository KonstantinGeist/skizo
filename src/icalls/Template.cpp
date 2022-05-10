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

#include "../Template.h"
#include "../Domain.h"
#include "../NativeHeaders.h"
#include "../RuntimeHelpers.h"

namespace skizo { namespace script {
using namespace skizo::core;

extern "C" {

void* SKIZO_API _so_Template_createImpl(void* sourceObj, void* typeObj)
{
    SKIZO_NULL_CHECK(sourceObj);
    SKIZO_NULL_CHECK(typeObj);

    CDomain* domain = CDomain::ForCurrentThread();
    const CString* source = ((SStringHeader*)sourceObj)->pStr;
    CClass* klass = nullptr;

    // The method supports `type` as both a class instance and a string instance
    // (to avoid importing the reflection module).
    CClass* metaClass = so_class_of(typeObj);
    if(metaClass == domain->StringClass()) {

        const CString* klassName = ((SStringHeader*)typeObj)->pStr;
        klass = domain->ClassByNiceName(klassName);
        if(!klass) {
            CDomain::Abort("Unknown type name.");
        }

    } else if(metaClass->FlatName().EqualsAscii("Type")) {
        klass = ((STypeHeader*)typeObj)->typeHandle;
    } else {
        CDomain::Abort("Unsupported type description.");
    }

    return CTemplate::CreateForClass(source, klass);
}

void SKIZO_API _so_Template_destroyImpl(void* pSelf)
{
    if(pSelf) {
        ((CTemplate*)pSelf)->Unref();
    }
}

void* SKIZO_API _so_Template_renderImpl(void* pSelf, void* str)
{
   SKIZO_NULL_CHECK(str);

   Auto<const CString> retValue (((CTemplate*)pSelf)->Render(str));
   return CDomain::ForCurrentThread()->CreateString(retValue, false);
}

}

} }

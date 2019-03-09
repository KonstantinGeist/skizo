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

#include "ScriptUtils.h"
#include "StringBuilder.h"
#include "Class.h"
#include "Console.h"
#include "Const.h"
#include "Contract.h"
#include "Domain.h"
#include "Expression.h"
#include "ModuleDesc.h"
#include "NativeHeaders.h"
#include "RuntimeHelpers.h"
#include "Tokenizer.h"

namespace skizo { namespace script { namespace ScriptUtils {
using namespace skizo::core;
using namespace skizo::collections;

void Fail_(const char* msg, const CString* filePath, int lineNumber)
{
    if(filePath) {
        Auto<const CString> str1 (CString::Format("%s (\"%o\":%d)", msg, (CObject*)filePath, lineNumber));
        char* str2 = str1->ToUtf8();
        CDomain::Abort(str2, true);
    } else {
        CDomain::Abort(msg);
    }
}

void Fail_(const char* msg, const SMetadataSource* source)
{
    if(source) {
        Fail_(msg, source->Module? source->Module->FilePath: nullptr, source->LineNumber);
    } else {
        Fail_(msg, nullptr, 0);
    }
}

void FailT(const char* msg, const CToken* faultyToken)
{
    Fail_(msg, faultyToken? faultyToken->FilePath: nullptr, faultyToken? faultyToken->LineNumber: 0);
}

void FailE(const char* msg, const CExpression* faultyExpr)
{
    Fail_(msg, faultyExpr? &faultyExpr->Source: nullptr);
}

void FailC(const char* msg, const CClass* faultyPClass)
{
    Fail_(msg, faultyPClass? &faultyPClass->Source(): nullptr);
}

void FailF(const char* msg, const CField* faultyField)
{
    Fail_(msg, faultyField? &faultyField->Source: nullptr);
}

void FailM(const char* msg, const CMethod* faultyMethod)
{
    Fail_(msg, faultyMethod? &faultyMethod->Source(): nullptr);
}

void FailL(const char* msg, const CLocal* faultyLocal)
{
    Fail_(msg, faultyLocal? &faultyLocal->Source: nullptr);
}

void FailCnst(const char* msg, const CConst* faultyKonst)
{
    Fail_(msg, faultyKonst? &faultyKonst->Source: nullptr);
}

// ****************************************************

void Warn_(const char* msg, const SMetadataSource* source)
{
    const CString* filePath = source->Module? source->Module->FilePath: nullptr;
    const int lineNumber = source->LineNumber;

    Auto<const CString> msgStr;
    if(filePath) {
        msgStr.SetPtr(CString::Format("%s (\"%o\":%d)", msg, (CObject*)filePath, lineNumber));
    } else {
        msgStr.SetPtr(CString::FromUtf8(msg));
    }

    Console::WriteLine(msgStr);
}

void WarnE(const char* msg, const CExpression* faultyExpr)
{
    Warn_(msg, faultyExpr? &faultyExpr->Source: nullptr);
}

// ****************************************************

const CString* EscapeString(const CString* input)
{
    if(!input) {
        return input;
    }

    // First looks if there are slashes at all (fast path).
    if(input->FindChar(SKIZO_CHAR('\\')) == -1) {
        input->Ref();
        return input;
    } else {
        Auto<CStringBuilder> sb (new CStringBuilder());
        const int length = input->Length();
        for(int i = 0; i < length; i++) {
            const so_char16 c = input->Chars()[i];
            if((c == SKIZO_CHAR('\\')) && (i + 1 < length)) {
                const so_char16 d = input->Chars()[i + 1];

                switch(d) {
                    case SKIZO_CHAR('n'): sb->Append(SKIZO_CHAR('\n')); i++; break;
                    case SKIZO_CHAR('t'): sb->Append(SKIZO_CHAR('\t')); i++; break;
                    case SKIZO_CHAR('\\'): sb->Append(SKIZO_CHAR('\\')); i++; break;
                    case SKIZO_CHAR('r'): sb->Append(SKIZO_CHAR('\r')); i++; break;
                    default:
                        sb->Append(c);
                        break;
                }
            } else {
                sb->Append(c);
            }
        }
        return sb->ToString();
    }
}

const CString* UnescapeString(const CString* input)
{
    if(!input) {
        return input;
    }

    Auto<CStringBuilder> sb (new CStringBuilder());
    for(int i = 0; i < input->Length(); i++) {
        const so_char16 c = input->Chars()[i];

        switch(c) {
            case SKIZO_CHAR('\n'):
                sb->Append("\\n"); break;
            case SKIZO_CHAR('\t'):
                sb->Append("\\t"); break;
            case SKIZO_CHAR('\r'):
                sb->Append("\\r"); break;
            default:
                sb->Append(c); break;
        }
    }

    return sb->ToString();
}

SStringSlice NParamName(CDomain* pDomain, int index)
{
    // TODO simplify
    Auto<const CString> a (CString::Format("_soX_param_%d", index));
    return pDomain->NewSlice(a);
}
    // *********************
    //   Type conversions.
    // *********************

CArrayList<const CString*>* ArrayHeaderToStringArray(const SArrayHeader* soArray, bool allowNulls)
{
    if(soArray) {
        // some verification just in case
        SKIZO_REQ_EQUALS(so_class_of(soArray)->SpecialClass(), E_SPECIALCLASS_ARRAY);

        const SStringHeader** soStrs = (const SStringHeader**)&soArray->firstItem; // TODO use macros or something
        Auto<CArrayList<const CString*> > r (new CArrayList<const CString*>());
        for(int i = 0; i < soArray->length; i++) {
            const SStringHeader* soStr = soStrs[i];

            if(!allowNulls) {
                SKIZO_NULL_CHECK(soStr);
            }

            if(soStr) {
                r->Add(so_string_of(soStr));
            } else {
                r->Add(nullptr);
            }
        }
        r->Ref();
        return r;
    } else {
        return nullptr;
    }
}

} } }

namespace skizo { namespace script {

// TODO speed up
SStringSlice PrimitiveOperatorToNeutralName(const SStringSlice& primOp, CDomain* domain)
{
    SStringSlice r;

    if(primOp.EqualsAscii("+")) {
        r = domain->NewSlice("op_add");
    } else if(primOp.EqualsAscii("-")) {
        r = domain->NewSlice("op_subtract");
    } else if(primOp.EqualsAscii("*")) {
        r = domain->NewSlice("op_multiply");
    } else if(primOp.EqualsAscii("/")) {
        r = domain->NewSlice("op_divide");
    } else if(primOp.EqualsAscii("%")) {
        r = domain->NewSlice("op_modulo");
    } else if(primOp.EqualsAscii("==")) {
        r = domain->NewSlice("op_equals");
    } else if(primOp.EqualsAscii(">")) {
        r = domain->NewSlice("op_greaterThan");
    } else if(primOp.EqualsAscii("<")) {
        r = domain->NewSlice("op_lessThan");
    } else if(primOp.EqualsAscii("|")) {
        r = domain->NewSlice("op_or");
    } else if(primOp.EqualsAscii("&")) {
        r = domain->NewSlice("op_and");
    }

    return r;
}

// TODO speed up
SStringSlice NeutralNameToPrimitiveOperator(const SStringSlice& nn, CDomain* domain)
{
    SStringSlice r;

    if(nn.EqualsAscii("op_add")) {
        r = domain->NewSlice("+");
    } else if(nn.EqualsAscii("op_subtract")) {
        r = domain->NewSlice("-");
    } else if(nn.EqualsAscii("op_multiply")) {
        r = domain->NewSlice("*");
    } else if(nn.EqualsAscii("op_divide")) {
        r = domain->NewSlice("/");
    } else if(nn.EqualsAscii("op_modulo")) {
        r = domain->NewSlice("%");
    } else if(nn.EqualsAscii("op_equals")) {
        r = domain->NewSlice("==");
    } else if(nn.EqualsAscii("op_greaterThan")) {
        r = domain->NewSlice(">");
    } else if(nn.EqualsAscii("op_lessThan")) {
        r = domain->NewSlice("<");
    } else if(nn.EqualsAscii("op_or")) {
        r = domain->NewSlice("|");
    } else if(nn.EqualsAscii("op_and")) {
        r = domain->NewSlice("&");
    }

    return r;
}

bool BoxedEquals(void* ptrToValue, size_t valueSize, void* otherObj, EPrimType targetType)
{
    const CClass* pClass = so_class_of(otherObj);
    if(pClass->SpecialClass() == E_SPECIALCLASS_BOXED
    && pClass->ResolvedWrappedClass()->PrimitiveType() == targetType)
    {
        // Boxed value layout: first the vtable, then the actual value.
        const char* boxedValue = (char*)otherObj + sizeof(void*);
        return memcmp(ptrToValue, boxedValue, valueSize) == 0;
    } else {
        return false;
    }
}

} }

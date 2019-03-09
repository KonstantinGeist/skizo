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

#include "Expression.h"
#include "Domain.h"
#include "Method.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

void* CExpression::operator new(size_t size)
{
    return SO_FAST_ALLOC(size, E_SKIZOALLOCATIONTYPE_EXPRESSION);
}

void CExpression::operator delete (void *p)
{
    // NOTHING
}

// *******************
//   CBodyExpression
// *******************

CBodyExpression::CBodyExpression()
    : Exprs(new CArrayList<CExpression*>()),
      Method(nullptr),
      ParentBody(nullptr),
      GeneratedClosureClass(nullptr),
      OwnsMethod(false),
      ReturnAlreadyDefined(false)
{
}

CBodyExpression::~CBodyExpression()
{
    if(OwnsMethod) {
        Method->Unref();
    }
}

void CBodyExpression::SetMethod(CMethod* method, bool ownsMethod)
{
    SKIZO_REQ(!this->Method, EC_INVALID_STATE);

    this->Method = method;
    this->OwnsMethod = ownsMethod;
    if(ownsMethod) {
        Method->Ref();
    }
}

bool CBodyExpression::IsCastableToAction() const
{
    SKIZO_REQ_PTR(Method);

    const CSignature& sig = Method->Signature();
    return sig.ReturnType.PrimType == E_PRIMTYPE_VOID
        && sig.Params->Count() == 0;
}

bool CBodyExpression::IsCastableToRangeLooper() const
{
    SKIZO_REQ_PTR(Method);

    const CSignature& sig = Method->Signature();
    return (sig.ReturnType.PrimType == E_PRIMTYPE_VOID)
        && (sig.Params->Count() == 1)
        && (sig.Params->Array()[0]->Type.PrimType == E_PRIMTYPE_INT);
}

EExpressionKind CBodyExpression::Kind() const
{
    return E_EXPRESSIONKIND_BODY;
}

// *******************
//   CCallExpression
// *******************

EExpressionKind CCallExpression::Kind() const
{
    return E_EXPRESSIONKIND_CALL;
}

// ******************************
//   CIntegerConstantExpression
// ******************************

EExpressionKind CIntegerConstantExpression::Kind() const
{
    return E_EXPRESSIONKIND_INTCONSTANT;
}

CIntegerConstantExpression::CIntegerConstantExpression(int value)
    : Value(value)
{
}

// ********************
//   CIdentExpression
// ********************

EExpressionKind CIdentExpression::Kind() const
{
    return E_EXPRESSIONKIND_IDENT;
}

CIdentExpression::CIdentExpression(const SStringSlice& name, bool isAuto)
    : Name(name),
      IsAuto(isAuto)
{
}

CIdentExpression::CIdentExpression(const SStringSlice& name, const STypeRef& type)
    : Name(name),
      TypeAsInCode(type),
      IsAuto(false)
{
}

// *********************
//   TReturnExpression
// *********************

EExpressionKind CReturnExpression::Kind() const
{
    return E_EXPRESSIONKIND_RETURN;
}

// *******************
//   CThisExpression
// *******************

EExpressionKind CThisExpression::Kind() const
{
    return E_EXPRESSIONKIND_THIS;
}

// ********************
//   CCCodeExpression
// ********************

CCCodeExpression::CCCodeExpression(const SStringSlice& code)
    : Code(code)
{
}

EExpressionKind CCCodeExpression::Kind() const
{
    return E_EXPRESSIONKIND_CCODE;
}

// ****************************
//   CFloatConstantExpression
// ****************************

CFloatConstantExpression::CFloatConstantExpression(float value)
    : Value(value)
{
}

EExpressionKind CFloatConstantExpression::Kind() const
{
    return E_EXPRESSIONKIND_FLOATCONSTANT;
}

// ***************************
//   CNullConstantExpression
// ***************************

EExpressionKind CNullConstantExpression::Kind() const
{
    return E_EXPRESSIONKIND_NULLCONSTANT;
}

// *******************
//   CCastExpression
// *******************

EExpressionKind CCastExpression::Kind() const
{
    return E_EXPRESSIONKIND_CAST;
}

// *********************
//   CSizeofExpression
// *********************

EExpressionKind CSizeofExpression::Kind() const
{
    return E_EXPRESSIONKIND_SIZEOF;
}

// ****************************
//   CStringLiteralExpression
// ****************************

CStringLiteralExpression::CStringLiteralExpression(const skizo::core::CString* stringValue)
    : StringValue(stringValue),
      SkizoObject(nullptr)
{
    stringValue->Ref();
}

CStringLiteralExpression::CStringLiteralExpression(const SStringSlice& stringSlice)
    : StringValue(stringSlice.ToString()), SkizoObject(nullptr)
{
}

EExpressionKind CStringLiteralExpression::Kind() const
{
    return E_EXPRESSIONKIND_STRINGLITERAL;
}

// ***************************
//   CBoolConstantExpression
// ***************************

EExpressionKind CBoolConstantExpression::Kind() const
{
    return E_EXPRESSIONKIND_BOOLCONSTANT;
}

// ****************************
//   CArrayCreationExpression
// ****************************

EExpressionKind CArrayCreationExpression::Kind() const
{
    return E_EXPRESSIONKIND_ARRAYCREATION;
}

// ************************
//   CArrayInitExpression
// ************************

CArrayInitExpression::CArrayInitExpression()
    : Exprs(new CArrayList<CExpression*>()),
      HelperId(0)
{
}

EExpressionKind CArrayInitExpression::Kind() const
{
    return E_EXPRESSIONKIND_ARRAYINIT;
}

// **************************
//   CCharLiteralExpression
// **************************

CCharLiteralExpression::CCharLiteralExpression(so_char16 charValue)
    : CharValue(charValue)
{
}

EExpressionKind CCharLiteralExpression::Kind() const
{
    return E_EXPRESSIONKIND_CHARLITERAL;
}

// *********************************
//   CIdentityComparisonExpression
// *********************************

EExpressionKind CIdentityComparisonExpression::Kind() const
{
    return E_EXPRESSIONKIND_IDENTITYCOMPARISON;
}

// *************************
//   CAssignmentExpression
// *************************

EExpressionKind CAssignmentExpression::Kind() const
{
    return E_EXPRESSIONKIND_ASSIGNMENT;
}

// ********************
//   CAbortExpression
// ********************

EExpressionKind CAbortExpression::Kind() const
{
    return E_EXPRESSIONKIND_ABORT;
}

// *********************
//   CAssertExpression
// *********************

EExpressionKind CAssertExpression::Kind() const
{
    return E_EXPRESSIONKIND_ASSERT;
}

// ******************
//   CRefExpression
// ******************

EExpressionKind CRefExpression::Kind() const
{
    return E_EXPRESSIONKIND_REF;
}

// *******************************
//   CInlinedConditionExpression
// *******************************

EExpressionKind CInlinedConditionExpression::Kind() const
{
    return E_EXPRESSIONKIND_INLINED_CONDITION;
}

// *****************
//   CIsExpression
// *****************

EExpressionKind CIsExpression::Kind() const
{
    return E_EXPRESSIONKIND_IS;
}

// ********************
//   CBreakExpression
// ********************

EExpressionKind CBreakExpression::Kind() const
{
    return E_EXPRESSIONKIND_BREAK;
}

} }

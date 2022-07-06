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

#ifndef SEXPRESSION_H_INCLUDED
#define SEXPRESSION_H_INCLUDED

#include "ArrayList.h"
#include "MetadataSource.h"
#include "ResolvedIdentType.h"
#include "StringSlice.h"
#include "TypeRef.h"

namespace skizo { namespace script {

enum EExpressionKind
{
    E_EXPRESSIONKIND_BODY,
    E_EXPRESSIONKIND_CALL,
    E_EXPRESSIONKIND_INTCONSTANT,
    E_EXPRESSIONKIND_FLOATCONSTANT,
    E_EXPRESSIONKIND_NULLCONSTANT,
    E_EXPRESSIONKIND_THIS,
    E_EXPRESSIONKIND_IDENT,
    E_EXPRESSIONKIND_RETURN,
    E_EXPRESSIONKIND_CCODE,
    E_EXPRESSIONKIND_CAST,
    E_EXPRESSIONKIND_STRINGLITERAL,
    E_EXPRESSIONKIND_CHARLITERAL,
    E_EXPRESSIONKIND_BOOLCONSTANT,
    //   "f: [int] = (array 50);" /* array of length 50 is created */
    E_EXPRESSIONKIND_ARRAYCREATION,
    //   "f: [int] = [1 2 3 4];" /* array of length 4 is created */
    E_EXPRESSIONKIND_ARRAYINIT,
    E_EXPRESSIONKIND_IDENTITYCOMPARISON,
    E_EXPRESSIONKIND_ASSIGNMENT,
    E_EXPRESSIONKIND_ABORT,
    E_EXPRESSIONKIND_INLINED_CONDITION,
    E_EXPRESSIONKIND_IS,
    E_EXPRESSIONKIND_ASSERT,
    E_EXPRESSIONKIND_REF,
    E_EXPRESSIONKIND_BREAK,
    E_EXPRESSIONKIND_SIZEOF
};

class CExpression: public skizo::core::CObject
{
public:
    void* operator new(size_t size);
    void operator delete (void *p);

    STypeRef InferredType;
    SMetadataSource Source;

    virtual EExpressionKind Kind() const = 0;
};

/**
 * Top-level expression belonging to a method which lists a sequence of expressions.
 */
class CBodyExpression: public CExpression
{
public:
    skizo::core::Auto<skizo::collections::CArrayList<CExpression*> > Exprs;

    /**
     * @note Never use directly! Use SetMethod(..) instead.
     */
    class CMethod* Method;

    /**
     * Used by the transformer to link anonymous methods to their parents.
     */
    CBodyExpression* ParentBody;

    /**
     * A link to the generated class assigned by the transformer; to be used by the emitter.
     * The field makes sense only if this body represents an anonymous method (closure).
     */
    class CClass* GeneratedClosureClass;

    /**
     * @note Never use directly! Use SetMethod(..) instead.
     */
    bool OwnsMethod;

    bool ReturnAlreadyDefined;

    CBodyExpression();
    ~CBodyExpression();

    virtual EExpressionKind Kind() const override;

    void SetMethod(CMethod* method, bool ownsMethod);

    // ************************
    //   Convenience methods.
    // ************************

    /**
     * Used by the inlining logic to verify the conditional we want to inline has a correct method class.
     */
    bool IsCastableToAction() const;

    /**
     * Used by the inlining logic to verify the loop we want to inline has a correct method class.
     */
    bool IsCastableToRangeLooper() const;
};

class CIdentExpression: public CExpression
{
public:
    SStringSlice Name;
    
    /**
     * @note Can be null to mean there was nothing typed.
     */
    STypeRef TypeAsInCode;

    /**
     * Is it a field, a param, a local, a class reference, a const?
     */
    SResolvedIdentType ResolvedIdent;
    
    /**
     * Type not stated, expected to be inferred, corresponds to the "auto" syntax.
     */
    bool IsAuto;

    CIdentExpression(const SStringSlice& name, bool isAuto = false);
    CIdentExpression(const SStringSlice& name, const STypeRef& type); // NOTE The type is given as a string slice.

    virtual EExpressionKind Kind() const override;
};

class CNullConstantExpression: public CExpression
{
public:
    virtual EExpressionKind Kind() const override;
};

class CBoolConstantExpression: public CExpression
{
public:
    bool Value;
    CBoolConstantExpression(bool value): Value(value) { }

    virtual EExpressionKind Kind() const override;
};

typedef unsigned char ECallExpressionType;
#define E_CALLEXPRESSION_UNRESOLVED 0
#define E_CALLEXPRESSION_METHODCALL 1
#define E_CALLEXPRESSION_CONSTACCESS 2

class CCallExpression: public CExpression
{
public:
    /**
     * First expression is "self" or the target class for static method calls.
     * Second expression is method.
     * The rest of expressions are arguments (depending on the signature).
     */
    skizo::core::Auto<skizo::collections::CArrayList<CExpression*> > Exprs;

    ECallExpressionType CallType; // short

    /**
     * There's a marker in this call expr as the first item; the call expr is to be converted into a different
     * specialized expr some time later (ReturnExpression, CastExpression etc. -- see parser logic for more details)
     */
    bool IsMarked; // short

    union {
        CConst* uTargetConst;   // If CallType==E_CALLEXPRESSION_CONSTACCESS
        CMethod* uTargetMethod; // If CallType==E_CALLEXPRESSION_METHODCALL
    };

    CCallExpression()
        : Exprs(new skizo::collections::CArrayList<CExpression*>()),
          CallType(E_CALLEXPRESSION_UNRESOLVED),
          IsMarked(false)
        {
        }

    virtual EExpressionKind Kind() const override;
};

class CIntegerConstantExpression: public CExpression
{
public:
    int Value;
    CIntegerConstantExpression(int value);

    virtual EExpressionKind Kind() const override;
};

class CFloatConstantExpression: public CExpression
{
public:
    float Value;
    CFloatConstantExpression(float value);

    virtual EExpressionKind Kind() const override;
};

class CReturnExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;

    virtual EExpressionKind Kind() const override;
};

class CThisExpression: public CExpression
{
public:
    CMethod* DeclMethod;
    CThisExpression()
        : DeclMethod(nullptr)
    {
    }

    virtual EExpressionKind Kind() const override;
};

class CCCodeExpression: public CExpression
{
public:
    SStringSlice Code;
    CCCodeExpression(const SStringSlice& code);

    virtual EExpressionKind Kind() const override;
};

class CCastExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;
    SCastInfo CastInfo;

    /**
     * The parser first creates cast expressions empty as part of usual call expressions;
     * after parsing a call expression it looks if the first element is an empty cast expression.
     * If it is -- converts the current call expression into a full-fledged cast expression.
     */
    bool IsEmpty;

    CCastExpression(const STypeRef& targetType)
        : IsEmpty(true)
    {
        InferredType = targetType;
    }

    virtual EExpressionKind Kind() const override;
};

class CSizeofExpression: public CExpression
{
public:
    STypeRef TargetType;

    CSizeofExpression(const STypeRef& targetType)
    {
        TargetType = targetType;
    }

    virtual EExpressionKind Kind() const override;
};

class CStringLiteralExpression: public CExpression
{
public:
    skizo::core::Auto<const skizo::core::CString> StringValue;
    void* SkizoObject; // As allocated by Skizo's GC.

    CStringLiteralExpression(const skizo::core::CString* stringValue);
    CStringLiteralExpression(const SStringSlice& stringSlice);

    virtual EExpressionKind Kind() const override;
};

class CCharLiteralExpression: public CExpression
{
public:
    so_char16 CharValue;
    CCharLiteralExpression(so_char16 charValue);

    virtual EExpressionKind Kind() const override;
};

class CArrayCreationExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;

    virtual EExpressionKind Kind() const override;
};

class CArrayInitExpression: public CExpression
{
public:
    skizo::core::Auto<skizo::collections::CArrayList<CExpression*> > Exprs;
    int HelperId; // set in STransformer

    CArrayInitExpression();
    virtual EExpressionKind Kind() const override;
};

/**
 *  Rerepresents the "===" syntax.
 * @note Unlike "==" which is simply backed up by a "equals" method; "===" can check identities
 * of any supplied types, even if they don't implement the "==" operator.
 */
class CIdentityComparisonExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr1;
    skizo::core::Auto<CExpression> Expr2;

    virtual EExpressionKind Kind() const override;
};

class CAssignmentExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr1;
    skizo::core::Auto<CExpression> Expr2;

    virtual EExpressionKind Kind() const override;
};

class CIsExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;
    STypeRef TypeAsInCode;

    CIsExpression(const STypeRef& typeAsInCode)
        : TypeAsInCode(typeAsInCode)
    {
    }

    virtual EExpressionKind Kind() const override;
};

class CAbortExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;

    virtual EExpressionKind Kind() const override;
};

class CAssertExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;

    virtual EExpressionKind Kind() const override;
};

/**
 * Takes a local variable, a param, a valuetype "this", or a field, and returns a pointer to it
 * in the form of an "intptr" value. Unsafe context only.
 */
class CRefExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> Expr;

    virtual EExpressionKind Kind() const override;
};

class CBreakExpression: public CExpression
{
    virtual EExpressionKind Kind() const override;
};

// ***************************************************************************
//   'Inlined' expressions.
// (Never produced by the parser, only by the transformer during inlining.)
// ***************************************************************************

class CInlinedConditionExpression: public CExpression
{
public:
    skizo::core::Auto<CExpression> IfCondition;
    skizo::core::Auto<CExpression> ElseCondition;
    skizo::core::Auto<CBodyExpression> Body;

    virtual EExpressionKind Kind() const override;
};

} }

#endif // SEXPRESSION_H_INCLUDED

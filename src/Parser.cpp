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

#include "Parser.h"
#include "Const.h"
#include "Contract.h"
#include "Domain.h"
#include "Field.h"
#include "Method.h"
#include "ModuleDesc.h"
#include "ScriptUtils.h"
#include "Stack.h"
#include "Tokenizer.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

struct STokenReader
{
private:
    const CArrayList<CToken*>* m_tokens;
    int m_pointer;
    CDomain* m_domain;

public:
    STokenReader(const CArrayList<CToken*>* tokens, CDomain* domain)
        : m_tokens(tokens),
          m_pointer(0),
          m_domain(domain)
    {
    }

    // WARNING To be used after NextToken(); i.e. "previousness" is relative to "NextToken()"
    const CToken* PeekPrevToken() const
    {
        if(m_pointer - 2 >= 0) {
            return m_tokens->Item(m_pointer - 2);
        } else {
            return nullptr;
        }
    }

    const CToken* NextToken()
    {
        if(m_pointer >= m_tokens->Count()) {
            return nullptr;
        } else {
            CToken* token = m_tokens->Item(m_pointer);
            m_pointer++;
            return token;
        }
    }

    const CToken* PeekToken() const
    {
        if(m_pointer >= m_tokens->Count()) {
            return nullptr;
        } else {
            return m_tokens->Item(m_pointer);
        }
    }

    const CToken* Expect(ETokenKind kind, char* value = nullptr)
    {
        const CToken* token = NextToken();
        if(!token || token->Kind != kind) {
            ScriptUtils::FailT(m_domain->FormatMessage("Expected '%S'.", Tokenizer::NameForTokenKind(kind)),
                               token);
        }
        if(value && !token->StringSlice.EqualsAscii(value)) {
            ScriptUtils::FailT(m_domain->FormatMessage("'%S' expected ('%s' found).", value, &token->StringSlice), token);
        }
        return token;
    }
};

struct SParser
{
    STokenReader reader;
    CDomain* domain;
    CModuleDesc* curModule;
    CStack<CCallExpression*> callStack;
    CStack<CBodyExpression*> bodyStack;

    EAccessModifier curAccessModifier; // remembered for all following members until the modifier is changed
    bool isStatic;                     // remembered for the next member only
    bool isUnsafe;                     // -- '' --
    bool isAbstract;                   // -- '' --
    bool isNative;                     // -- '' --
    Auto<CArrayList<CAttribute*> > attrs;

    SParser(const CArrayList<CToken*>* tokens, CModuleDesc* module, CDomain* domain);

    // isNameless: for anonymous methods which allow syntax like "method { }"
    void parseFunctionSig(CMethod* method, bool isNameless = false);
    void parseFieldPropertyOrEvent(ETokenKind tokenKind, CClass* klass, const CToken* errorToken);
    void parseConst_checkOrInferType(CConst* konst, EPrimType primType);
    void parseConst(CClass* klass, const CToken* errorToken);
    void parseMethodBody(CMethod* method);
    void parseMethod(CClass* klass, EMethodKind methodKind, const CToken* errorToken);
    void parseEnum(const CToken* errorToken);
    void parseMethodClassOrEventClass(const CToken* controlToken);
    void parseAlias(const CToken* errorToken);
    void parseClassLevel(ETokenKind tokenKind, const CToken* errorToken);
    void parseTopLevel();
    void parseImport(const CToken* errorToken);
    void parseAttribute(const CToken* errorToken);
    void parseForce(const CToken* token);
    STypeRef parseTypeRef(bool isBaseClass = false, bool forcedTypeRef = false);
};

SParser::SParser(const CArrayList<CToken*>* tokens, CModuleDesc* module, CDomain* domain)
    : reader(tokens, domain),
      domain(domain),
      curModule(module),
      curAccessModifier(E_ACCESSMODIFIER_PUBLIC),
      isStatic(false),
      isUnsafe(false),
      isAbstract(false),
      isNative(false),
      attrs(new CArrayList<CAttribute*>())
{
}

// NOTE Doesn't allow typerefs such as [int?] or [int*]
STypeRef SParser::parseTypeRef(bool isBaseClass, bool forcedTypeRef)
{
    const CToken* token = nullptr;
    STypeRef typeRef;

    int arrayLevel = 0;

again:
    token = reader.PeekToken();
    if(!token) {
        ScriptUtils::Fail_("Typeref expected (end of stream).", nullptr, 0);
    }

    if(token->Kind == E_TOKENKIND_LBRACKET) {
        if(isBaseClass) {
            ScriptUtils::FailT("Arrays aren't allowed as base classes.", token);
        }
        arrayLevel++;
        reader.NextToken();
        goto again;
    } else if(token->Kind == E_TOKENKIND_IDENTIFIER) {
        typeRef = STypeRef(token->StringSlice);
        reader.NextToken();

        // "[[int]]" is valid, but "[[int]" isn't.
        for(int i = 0; i < arrayLevel; i++) {
            reader.Expect(E_TOKENKIND_RBRACKET);
        }
    } else {
        ScriptUtils::FailT(domain->FormatMessage("Type name expected ('%s' found).", &token->StringSlice), token);
    }

    // *********************************************************************************************
    // Suffixes.
    // NOTE Doesn't allow typerefs such as int?* or int*? Supporting them would open a can of worms.
    // *********************************************************************************************

    token = reader.PeekToken();
    if(token && token->Kind == E_TOKENKIND_FAILABLE_SUFFIX) {
        if(isBaseClass) {
            ScriptUtils::FailT("Failables aren't allowed as base classes.", token);
        }

        reader.NextToken(); // Skips the failable suffix.
        typeRef.Kind = E_TYPEREFKIND_FAILABLE;
    } else if (token && token->Kind == E_TOKENKIND_ASTERISK) {
        if(isBaseClass) {
            ScriptUtils::FailT("Foreign wrappers aren't allowed as base classes.", token);
        }

        reader.NextToken(); // Skips the foreign suffix.
        typeRef.Kind = E_TYPEREFKIND_FOREIGN;
    }

    typeRef.ArrayLevel = arrayLevel;

    // FIX Composite types should be resolved first, before transforming any method bodies.
    // The previous implementation was doing it lazily, which introduced a problem:
    // "We enqueue StringBuilder* for transforming in such a lazy way that it will be processed after the current
    // method body (where the first mention of StringBuilder* is found) is parsed, and the current method body wants
    // StringBuilder* to implement 'StringRepresentable', which it should do, since StringBuilder* is a subclass of
    // StringBuilder, however, since StringBuilder* was never inferred, its makeSureMethodsFinalized() was never
    // called and at that point, StringBuilder* doesn't contain inherited methods of StringBuilder."
    // The solution is, during parsing, any mention of a composite typeref will generate a ForcedTypeRef, forcing
	// types to be enqueued for transforming (in Transformer.cpp) before they are accessed by random method bodies.
    if(forcedTypeRef || typeRef.IsComposite()) {
        Auto<CForcedTypeRef> forcedTypeRef (new CForcedTypeRef(typeRef, token->FilePath, token->LineNumber));
        domain->AddForcedTypeRef(forcedTypeRef);
        // TODO use a hashmap to deduplicate
    }

    return typeRef;
}

// "primType" is a hack. We map Variant's types to Skizo types in a hacky way (see CConst::Value in headers
// for comments). Now, it's even more hacky because although we map Skizo strings to Variant::Blob, this method
// treats strings as E_PRIMTYPE_OBJECT!
void SParser::parseConst_checkOrInferType(CConst* konst, EPrimType primType)
{
    if(konst->Type.IsVoid()) { // "auto" => infer the type from the value
        if(primType == E_PRIMTYPE_OBJECT) { // special case for strings, the only reference type that's allowed to be const
            konst->Type.SetObject(domain->NewSlice("string"));
        } else {
            konst->Type.SetPrimType(primType);
        }
    } else { // The type is explicitly stated, we only need to verify the value's type is correct.
        static const char* errorMsg = "The type of the const and the type of the value do not match.";

        if(konst->Type.PrimType == E_PRIMTYPE_OBJECT) {
            if(konst->Type.ClassName.EqualsAscii("string")) {
                if(primType != E_PRIMTYPE_OBJECT) {
                    ScriptUtils::FailCnst(errorMsg, konst);
                }
            } else {
                ScriptUtils::FailCnst("The only reference type that can be a const value is 'string'.", konst);
            }
        } else if(konst->Type.PrimType != primType) {
            ScriptUtils::FailCnst(errorMsg, konst);
        }
    }
}

void SParser::parseConst(CClass* klass, const CToken* errorToken)
{
    if(this->isUnsafe || this->isStatic || this->isAbstract || this->isNative) {
        ScriptUtils::FailT("Consts can't be marked unsafe, static, abstract or native.", errorToken);
    }
    if(attrs->Count()) {
        ScriptUtils::FailT("Consts can't have attributes (as of version 0.1)", errorToken);
    }

    const CToken* token = nullptr;
    Auto<CConst> konst (new CConst());
    konst->DeclaringClass = klass;

    // Const name.
    token = reader.Expect(E_TOKENKIND_IDENTIFIER);
    konst->Name = token->StringSlice;

    konst->Source.Module = curModule;
    konst->Source.LineNumber = token->LineNumber;

    // :
    reader.Expect(E_TOKENKIND_COLON);

    // Const type.
    // NOTE The parser doesn't judge anything here. It relies on the transformer to resolve the types.
    token = reader.PeekToken();
    if(!token || !(token->Kind == E_TOKENKIND_AUTO || token->Kind == E_TOKENKIND_IDENTIFIER)) {
        ScriptUtils::FailT("Consts must be explicitly typed or autotyped.", token? token: errorToken);
    }
    // The type is left as void if "auto" is implied.
    if(token->Kind != E_TOKENKIND_AUTO)
        konst->Type = parseTypeRef();
    else {
        // Skips "auto"
        reader.NextToken();
    }

    konst->Access = this->curAccessModifier;

    // =
    reader.Expect(E_TOKENKIND_ASSIGNMENT);

    // Const value.
    token = reader.NextToken();
    if(!token) {
        ScriptUtils::FailT("Consts must be set a value.", errorToken);
    }

    // ********************************************
    //   Parses the value according to the type.
    // ********************************************

    switch(token->Kind) {
        case E_TOKENKIND_INT_LITERAL:
            konst->Value.SetInt(token->StringSlice.ParseInt(token));
            parseConst_checkOrInferType(konst, E_PRIMTYPE_INT);
            break;

        case E_TOKENKIND_FLOAT_LITERAL:
            konst->Value.SetFloat(token->StringSlice.ParseFloat(token));
            parseConst_checkOrInferType(konst, E_PRIMTYPE_FLOAT);
            break;

        case E_TOKENKIND_TRUE:
            konst->Value.SetBool(true);
            parseConst_checkOrInferType(konst, E_PRIMTYPE_BOOL);
            break;

        case E_TOKENKIND_FALSE:
            konst->Value.SetBool(false);
            parseConst_checkOrInferType(konst, E_PRIMTYPE_BOOL);
            break;

        case E_TOKENKIND_CHAR_LITERAL:
            konst->Value.SetInt((int)token->StringSlice.String->Chars()[token->StringSlice.Start]);
            parseConst_checkOrInferType(konst, E_PRIMTYPE_CHAR);
            break;

        case E_TOKENKIND_STRING_LITERAL:
            {
                Auto<const CString> str (token->StringSlice.ToString());
                konst->Value.SetBlob(domain->InternStringLiteral(str));
                parseConst_checkOrInferType(konst, E_PRIMTYPE_OBJECT);
            }
            break;

        default:
            ScriptUtils::FailT("Unexpected token or unsupported value for const.", errorToken);
    }

    SKIZO_REQ(!konst->Type.IsVoid(), EC_ILLEGAL_ARGUMENT);

    // ********************************************

    klass->RegisterConst(konst);

    // ;
    reader.Expect(E_TOKENKIND_SEMICOLON);
}

void SParser::parseFieldPropertyOrEvent(ETokenKind tokenKind, CClass* klass, const CToken* errorToken)
{
    if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
        ScriptUtils::FailT("Interfaces can't have fields.", errorToken);
    }

    if(isNative) {
        ScriptUtils::FailT("Only methods can be marked native.", errorToken);
    }

    if(klass->IsStatic() && !this->isStatic) {
        ScriptUtils::FailT("Instance fields not allowed in static classes.", errorToken);
    }

    if(this->isUnsafe) {
        ScriptUtils::FailT("Only methods can be marked as unsafe.", errorToken);
    }
    if(this->isAbstract) {
        ScriptUtils::FailT("Only classes and methods can be marked as abstract.", errorToken);
    }

    const CToken* token = nullptr;
    Auto<CField> pField (new CField());

    pField->DeclaringClass = klass;

    // NOTE Fields if declared in user code are currently always private, but they can be public if
    // compiler-generated.
    pField->Access = E_ACCESSMODIFIER_PRIVATE;

    pField->IsStatic = this->isStatic;
    this->isStatic = false;

    // *****************************************
    //   Adds attributes.
    // *****************************************
    if(attrs->Count()) {
        pField->Attributes.SetPtr(new CArrayList<CAttribute*>());
        pField->Attributes->AddRange(this->attrs);
        attrs->Clear();
    }
    // *****************************************

    // Field name.
    token = reader.Expect(E_TOKENKIND_IDENTIFIER);
    pField->Name = token->StringSlice;

    pField->Source.Module = curModule;
    pField->Source.LineNumber = token->LineNumber;

    // ':'
    reader.Expect(E_TOKENKIND_COLON);

    // Type name.
    pField->Type = parseTypeRef();

    // ';'
    reader.Expect(E_TOKENKIND_SEMICOLON);

    // **********************************************************************************************************
    //      EVENTS and PROPERTIES
    // **********************************************************************************************************
    if(tokenKind == E_TOKENKIND_PROPERTY || tokenKind == E_TOKENKIND_EVENT) {
        // Renames the field to "m_%name%", while passing "%name%" to CClass::AddAccessMethodsForField(..)
        // so that it could generate nice-looking access methods like "%name%" and "set%name%"
        SStringSlice baseName (pField->Name);
        Auto<const CString> newFieldName (baseName.ToString());
        newFieldName.SetPtr(CString::Format(pField->IsStatic? "g_%o": "m_%o", (const CObject*)newFieldName.Ptr()));
        pField->Name = domain->NewSlice(newFieldName);
        klass->AddAccessMethodsForField(pField, baseName, this->curAccessModifier, tokenKind == E_TOKENKIND_EVENT);
    }
    if(tokenKind == E_TOKENKIND_EVENT) {
        // Events should inject their creation code into every constructor, and at this point, we don't have such
        // information. So we delay it until the infer phase (the type of the field must be an event class and we can't
        // know it until we're done parsing everything).
        klass->AddEventField(pField);
    }
    // **********************************************************************************************************

    if(pField->IsStatic) {
        klass->AddStaticField(pField);
    } else {
        klass->AddInstanceField(pField);
    }

    // NOTE Doesn't check if it's conflicting with class names, since this function is used by the parser as it goes
    // which may not have yet parsed all the classes. Checks so in the transformer.
    klass->VerifyUniqueMemberName(pField->Name);
    klass->AddToNameSet(pField->Name, pField);
}

// When a return/cast/array expression is parsed, it's parsed as a simple top level call expression.
// When a "return"/"cast/array" token is found, it's created as an empty return/cast expression, part
// of the call expr.
// After the call expr is parsed, it checks if the first element is a ret/cast/array expression.
// If that's the case, reconstructs the callExpression into a correct return/cast/array expression.
// + arrayCreation
// + arrayInit
// + identityComparison (checks the second element)
static CExpression* tryConvertCallExpr(const CCallExpression* callExpr)
{
    const CArrayList<CExpression*>* exprs = callExpr->Exprs.Ptr();

    if(exprs->Count() > 0) {
        CExpression* firstExpr = exprs->Array()[0];

        switch(firstExpr->Kind()) {
            case E_EXPRESSIONKIND_RETURN:
            {
                CReturnExpression* retExpr = static_cast<CReturnExpression*>(firstExpr);
                if(exprs->Count() != 2) {
                    ScriptUtils::FailE("Return expression requires 1 argument.", retExpr);
                }
                retExpr->Expr.SetVal(exprs->Item(1));
                retExpr->Ref();
                return retExpr;
            }
            break;

            case E_EXPRESSIONKIND_CAST:
            {
                CCastExpression* castExpr = static_cast<CCastExpression*>(firstExpr);
                if(!castExpr->IsEmpty) {
                    return nullptr;
                }
                if(exprs->Count() != 2) {
                    ScriptUtils::FailE("Cast expression requires 2 arguments.", castExpr);
                }

                castExpr->IsEmpty = false; // TODO ?
                castExpr->Expr.SetVal(exprs->Item(1));
                castExpr->Ref();
                return castExpr;
            }
            break;

            case E_EXPRESSIONKIND_SIZEOF:
            {
                CSizeofExpression* sizeofExpr = static_cast<CSizeofExpression*>(firstExpr);
                if(exprs->Count() != 1) {
                    ScriptUtils::FailE("Sizeof expression requires 1 argument.", sizeofExpr);
                }

                sizeofExpr->Ref();
                return sizeofExpr;
            }
            break;

            case E_EXPRESSIONKIND_ABORT:
            {
                CAbortExpression* abortExpr = static_cast<CAbortExpression*>(firstExpr);
                if(exprs->Count() != 2) {
                    ScriptUtils::FailE("Abort expression requires 1 argument.", abortExpr);
                }

                abortExpr->Expr.SetVal(exprs->Item(1));
                abortExpr->Ref();
                return abortExpr;
            }
            break;

            case E_EXPRESSIONKIND_ASSERT:
            {
                CAssertExpression* assertExpr = static_cast<CAssertExpression*>(firstExpr);
                if(exprs->Count() != 2) {
                    ScriptUtils::FailE("Assert expression requires 1 argument.", assertExpr);
                }

                assertExpr->Expr.SetVal(exprs->Item(1));
                assertExpr->Ref();
                return assertExpr;
            }
            break;

            case E_EXPRESSIONKIND_REF:
            {
                CRefExpression* refExpr = static_cast<CRefExpression*>(firstExpr);
                if(exprs->Count() != 2) {
                    ScriptUtils::FailE("Ref expression requires 1 argument.", refExpr);
                }

                refExpr->Expr.SetVal(exprs->Item(1));
                refExpr->Ref();
                return refExpr;
            }
            break;

            case E_EXPRESSIONKIND_ARRAYCREATION:
            {
                CArrayCreationExpression* arrayCreationExpr = static_cast<CArrayCreationExpression*>(firstExpr);
                if(exprs->Count() != 2) {
                    ScriptUtils::FailE("Array expression requires 1 argument.", arrayCreationExpr);
                }
                arrayCreationExpr->Expr.SetVal(exprs->Item(1));
                arrayCreationExpr->Ref();
                return arrayCreationExpr;
            }
            break;

            case E_EXPRESSIONKIND_ARRAYINIT:
                break;

            case E_EXPRESSIONKIND_BREAK:
            {
                CBreakExpression* breakExpr = static_cast<CBreakExpression*>(firstExpr);
                breakExpr->Ref();
                return breakExpr;
            }

            default: break;
        }
    }

    if(exprs->Count() == 2) {

        // *************************
        //   Special case for 'is'
        // *************************

        CExpression* secondExpr = exprs->Array()[1];
        if(secondExpr->Kind() == E_EXPRESSIONKIND_IS) {

            CIsExpression* isExpr = static_cast<CIsExpression*>(secondExpr);
            isExpr->Expr.SetVal(exprs->Item(0));
            isExpr->Ref();
            return isExpr;

        }

    } else if(exprs->Count() == 3) {
        CExpression* secondExpr = exprs->Array()[1];

        if(secondExpr->Kind() == E_EXPRESSIONKIND_IDENTITYCOMPARISON) {

            // ************************************************
            //   Special case for "===" (identity comparison)
            // ************************************************

            if(exprs->Count() != 3) {
                ScriptUtils::FailE("Identity comparison requires 3 elements.", callExpr);
            }

            CIdentityComparisonExpression* identCompExpr = static_cast<CIdentityComparisonExpression*>(secondExpr);
            identCompExpr->Expr1.SetVal(exprs->Item(0));
            identCompExpr->Expr2.SetVal(exprs->Item(2));
            identCompExpr->Ref();
            return identCompExpr;

        } else if(secondExpr->Kind() == E_EXPRESSIONKIND_ASSIGNMENT) {

            // *************************************
            //   Special case for '=' (assignment)
            // *************************************

            if(exprs->Count() != 3) {
                ScriptUtils::FailE("Assignment expression requires 3 elements.", callExpr);
            }
            if(exprs->Item(0)->Kind() != E_EXPRESSIONKIND_IDENT) {
                ScriptUtils::FailE("Left value of an assignment should be a local, this object's field or param.", callExpr);
            }

            CAssignmentExpression* assExpr = static_cast<CAssignmentExpression*>(secondExpr);
            assExpr->Expr1.SetVal(exprs->Item(0));
            assExpr->Expr2.SetVal(exprs->Item(2));
            assExpr->Ref();
            return assExpr;
        }
        // *****************************************************************
    }

    return nullptr;
}

static void verifyCallComplete(const CCallExpression* callExpr)
{
    // It's agrammatical to have 0 or 1 element in a call expression.
    if(callExpr->Exprs->Count() < 2) {
        ScriptUtils::FailE("A call expression requires at least 2 elements (object and its method).", callExpr);
    }

    switch(callExpr->Exprs->Item(1)->Kind()) {
        case E_EXPRESSIONKIND_IDENT:
        case E_EXPRESSIONKIND_STRINGLITERAL:
            // Everything OK.
            break;

        default:
            ScriptUtils::FailE("Second argument in a call expression must be a method name.", callExpr);
            return;
    }
}

static bool hasValidLastExpr(const CBodyExpression* bodyExpr)
{
    if(bodyExpr->Exprs->Count() == 0) {
        return false;
    } else {
        const CExpression* lastExpr = bodyExpr->Exprs->Item(bodyExpr->Exprs->Count() - 1);
        const EExpressionKind kind = lastExpr->Kind();
        // EXCEPTION: Unsafe methods. They allow no "return" at the end of a method if they end with a CCodeExpression.
        return kind == E_EXPRESSIONKIND_RETURN || kind == E_EXPRESSIONKIND_CCODE;
    }
}

// Parses the method body and creates a tree of expressions which is stored inside the method's metadata. This tree of
// expressions can be later modified by custom transformers.
// NOTE The parser simply parses a semantics-agnostic tree of expressions. It does not know if a particular identifier
// is a field access or a class access etc. All of that is to be done by the transformer.
//
// Things like return/cast/arrayInit/arrayCreate are first parsed as simple callexprs with the first empty element of
// the respective type (CArrayInitExpression etc.) added as a marker. When a callExpr is closed (token ')' is found),
// the parser looks if there are markers present and converts the callExpr to the target expr if that's the case. For that,
// it employs the function tryConvertCallExpr.
void SParser::parseMethodBody(CMethod* method)
{
    if(method->IsAbstract() || method->SpecialMethod() == E_SPECIALMETHOD_NATIVE) {
        reader.Expect(E_TOKENKIND_SEMICOLON);
        return;
    }

    const CToken* token = nullptr;
    const CToken* token2 = nullptr;

    // '{'
    reader.Expect(E_TOKENKIND_LBRACE);

    // To be sure.
    callStack.Clear();
    bodyStack.Clear();

    // Every method's root expression is a body expression.
    Auto<CBodyExpression> curBodyExpr (new CBodyExpression());
    curBodyExpr->SetMethod(method, false);

    Auto<CExpression> curExpr;
    Auto<CCallExpression> curCallExpr (new CCallExpression());
    SStringSlice stringSlice;

    while(true) {
        {
            token = reader.NextToken();
            if(!token) {
                ScriptUtils::FailT("Unexpected end of stream; method body expected.", 0);
            }
        }
        // ***********************************

        switch(token->Kind) {
            // **********
            // IDENTIFIER
            // **********
            // An identifier can be a variable, a method access or a field access.
            case E_TOKENKIND_IDENTIFIER:
            {
                stringSlice = token->StringSlice; // Remembers the name.
                // Let's peek if this variable is typed at this point:
                token2 = reader.PeekToken();
                if(token2 && token2->Kind == E_TOKENKIND_COLON) {

                    // ***********
                    // It's typed!
                    // ***********

                    // It's the second element in a call expression which must be a method call which can't be typed.
                    if(curCallExpr->Exprs->Count() == 1) {
                        ScriptUtils::FailE("Trying to type a method name.", curCallExpr);
                    }

                    reader.NextToken(); // Skips the colon.

                    // Checks if it's "auto".
                    token = reader.PeekToken();
                    if(token && (token->Kind == E_TOKENKIND_AUTO || token->Kind == E_TOKENKIND_ASSIGNMENT)) {
                        // The code expects the transformer to infer the type.
                        if(token->Kind == E_TOKENKIND_AUTO) {
                            reader.NextToken(); // Skips "auto".
                        }
                        curExpr.SetPtr((CExpression*)new CIdentExpression(stringSlice, true /* NOTE: isAuto = true. */));
                    } else {
                        const STypeRef identTypeRef (parseTypeRef());
                        curExpr.SetPtr((CExpression*)new CIdentExpression(stringSlice, identTypeRef));
                    }

                    // ********************************************************
                    // Validates if it's a lvalue.
                    // ********************************************************
                    // Typed vars are only allowed to be lvalues of assignments.
                    bool isLValueLocal = (curCallExpr->Exprs->Count() == 0); // there should be nothing before it
                    if(isLValueLocal) {
                        token = reader.PeekToken();
                        if(!token || token->Kind != E_TOKENKIND_ASSIGNMENT) { // there should be "=" after it
                            isLValueLocal = false;
                        }
                    }
                    if(!isLValueLocal) {
                        ScriptUtils::FailT("Typed variables can only be lvalues of assignment.", token);
                    }
                    // ********************************************************
                } else {
                    // ***************
                    // It's not typed.
                    // ***************
                    curExpr.SetPtr((CExpression*)new CIdentExpression(stringSlice));
                }

                curCallExpr->Exprs->Add(curExpr);
            }
            // *****************
            //   INT CONSTANT
            // *****************
            break; case E_TOKENKIND_INT_LITERAL:
            {
                curExpr.SetPtr(new CIntegerConstantExpression(token->StringSlice.ParseInt(token)));
                curCallExpr->Exprs->Add(curExpr);
            }
            // ******************
            //   FLOAT CONSTANT
            // ******************
            break; case E_TOKENKIND_FLOAT_LITERAL:
            {
                curExpr.SetPtr(new CFloatConstantExpression(token->StringSlice.ParseFloat(token)));
                curCallExpr->Exprs->Add(curExpr);
            }
            // *********************
            //    String literal.
            // *********************
            break; case E_TOKENKIND_STRING_LITERAL:
            {
                Auto<const CString> stringValue (token->StringSlice.ToString());
                stringValue.SetPtr(ScriptUtils::EscapeString(stringValue));

                curExpr.SetPtr(new CStringLiteralExpression(stringValue));
                curCallExpr->Exprs->Add(curExpr);
            }
            // *******************
            //    Char constant.
            // *******************
            break; case E_TOKENKIND_CHAR_LITERAL:
            {
                Auto<const CString> stringValue (token->StringSlice.ToString());
                stringValue.SetPtr(ScriptUtils::EscapeString(stringValue));
                if(stringValue->Length() != 1) {
                    ScriptUtils::FailT(domain->FormatMessage("Char literal too large ('%s' found).", &token->StringSlice), token);
                }
                curExpr.SetPtr(new CCharLiteralExpression(stringValue->Chars()[0]));
                curCallExpr->Exprs->Add(curExpr);
            }
            // *****************
            //   NULL CONSTANT
            // *****************
            break; case E_TOKENKIND_NULL:
            {
                curExpr.SetPtr(new CNullConstantExpression());
                curCallExpr->Exprs->Add(curExpr);
            }
            // ************************
            //   TRUE/FALSE CONSTANTS
            // ************************
            break; case E_TOKENKIND_TRUE: case E_TOKENKIND_FALSE:
            {
                curExpr.SetPtr(new CBoolConstantExpression(token->Kind == E_TOKENKIND_TRUE));
                curCallExpr->Exprs->Add(curExpr);
            }
            // **********
            //    THIS
            // **********
            break; case E_TOKENKIND_THIS:
            {
                curExpr.SetPtr(new CThisExpression());
                curCallExpr->Exprs->Add(curExpr);
            }
            // **********
            //    BREAK
            // **********
            break; case E_TOKENKIND_BREAK:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'break' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CBreakExpression());
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
            }
            // ************
            //   RETURN
            // ************
            break; case E_TOKENKIND_RETURN:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'return' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CReturnExpression());
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                curCallExpr->Exprs->Add(curExpr);
            }
            // *********
            //   CAST
            // *********
            break; case E_TOKENKIND_CAST:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'cast' used in an inappropriate context.", token);
                }

                STypeRef typeRef (parseTypeRef());
                curExpr.SetPtr(new CCastExpression(typeRef)); // marker
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // **********
            //   SIZEOF
            // **********
            break; case E_TOKENKIND_SIZEOF:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'sizeof' used in an inappropriate context.", token);
                }

                STypeRef typeRef (parseTypeRef());
                curExpr.SetPtr(new CSizeofExpression(typeRef)); // marker
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // *********
            //   ABORT
            // *********
            break; case E_TOKENKIND_ABORT:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'abort' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CAbortExpression()); // marker
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // **********
            //   ASSERT
            // **********
            break; case E_TOKENKIND_ASSERT:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'assert' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CAssertExpression()); // marker
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // **********
            //   REF
            // **********
            break; case E_TOKENKIND_REF:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'ref' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CRefExpression()); // marker
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // **********************************************
            //   NEWARRAY (syntax in Skizo: "array")
            // **********************************************
            break; case E_TOKENKIND_NEWARRAY:
            {
                // curCallExpr->IsMarked is to be consistent
                if(curCallExpr->Exprs->Count() != 0 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'array' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CArrayCreationExpression()); // marker
                //
                curExpr->Source.Module = curModule;
                curExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // ****************
            //   LEFT PARENTH
            // ****************
            break; case E_TOKENKIND_LPARENTH:
            {
                // Left parenthesis creates a new current call expression and pushes the prev call expr to the stack.
                callStack.Push(curCallExpr);
                curCallExpr.SetPtr(new CCallExpression());

                curCallExpr->Source.Module = curModule;
                curCallExpr->Source.LineNumber = token->LineNumber;
            }
            // *****************************************************************************************************
            //   LEFT BRACKET (array initialization)
            // NOTE array initializations in the form of [1 2 3] are internally processed as special-form call exprs
			// to simplify parsing. They are marked with a ArrayInitExpr subelement as the first element of the
			// call expr. E_TOKENKIND_RPARENTH and E_TOKENKIND_RBRACKET then check if the first element
			// is ArrayInitExpr to tell between the two forms of callExpr (i.e. parenthesis-based and bracket-based).
            // The E_TOKENKIND_RBRACKET codepath also converts such a temporary callexpr into a full-fledged
			// ArrayInitExpr after it's completed parsing it.
            // ******************************************************************************************************
            break; case E_TOKENKIND_LBRACKET:
            {
                // Left bracket creates a new array init expression and pushes the prev call expr to the stack.
                callStack.Push(curCallExpr);
                curCallExpr.SetPtr(new CCallExpression());
                // Creates an empty ArrayInitExpression as the first item which is to be used
                // as a marker that it's a special callExpr (just like "return" and "cast" codepaths).
                // In tryConvertCallExpr, the callExpr will be replaced with this expression entirely.
                Auto<CArrayInitExpression> initExpr (new CArrayInitExpression()); // marker
                curCallExpr->Exprs->Add(initExpr);
                //
                curCallExpr->Source.Module = initExpr->Source.Module = curModule;
                curCallExpr->Source.LineNumber = initExpr->Source.LineNumber = token->LineNumber;
                //
                curCallExpr->IsMarked = true;
            }
            // *****************
            //   RIGHT PARENTH
            // *****************
            break; case E_TOKENKIND_RPARENTH:
            {
                // The right parenthesis restores the previously pushed call expression and adds the
                // current call expression as a child to that prev call expression.
                if(callStack.IsEmpty()) {
                    ScriptUtils::FailT("Parenthesis mismatch.", token);
                }
                Auto<CCallExpression> prevCallExpr (callStack.Pop());
                // A mismatch like: "[1 2 3)"
                if(curCallExpr->Exprs->Item(0)->Kind() == E_EXPRESSIONKIND_ARRAYINIT) {
                    ScriptUtils::FailT("Expected ']', got ')'.", token);
                }

                Auto<CExpression> convertedExpr (tryConvertCallExpr(curCallExpr));
                if(convertedExpr) {
                    prevCallExpr->Exprs->Add(convertedExpr);
                } else {
                    verifyCallComplete(curCallExpr);
                    prevCallExpr->Exprs->Add(curCallExpr);
                }

                curCallExpr.SetVal(prevCallExpr);
            }
            // *****************
            //   RIGHT BRACKET
            // *****************
            break; case E_TOKENKIND_RBRACKET:
            {
                // The right bracket restores the previously pushed call expression and adds the current call expression
                // as a child to that prev call expression.
                if(callStack.IsEmpty()) {
                    ScriptUtils::FailT("Bracket mismatch.", token);
                }
                Auto<CCallExpression> prevCallExpr (callStack.Pop());
                // A mismatch like: "(1 2 3]"
                if(curCallExpr->Exprs->Item(0)->Kind() != E_EXPRESSIONKIND_ARRAYINIT) {
                    ScriptUtils::FailT("Expected ')', got ']'.", token);
                }

                // *****************************************************************************************
                // A special case for []. Array initialization depends on the type of the first item. If the
				// parser sees [1], it understands that, since the first element is an integer, then the
				// whole expression must be [int].
                // The above-mentioned approach doesn't work with []. The type of such an array is not
				// inferrable at this point. Leaving it like that would require us to hard-code a new infer
				// path for []. What we do instead is to automatically convert [] to (array 0) in advance,
				// which is semantically identical to [].
				// ArrayCreationExpr is already implemented to infer its type from the expected surroundings
				// (assignment/argument passing/return).
                // *****************************************************************************************
                // "-1" ignores the marker (first element of type).
                if((curCallExpr->Exprs->Count() - 1) == 0) {
                    Auto<CArrayCreationExpression> arrayCreationExpr (new CArrayCreationExpression());
                    //
                    arrayCreationExpr->Source.Module = curModule;
                    arrayCreationExpr->Source.LineNumber = token->LineNumber;
                    //
                    arrayCreationExpr->Expr.SetPtr(new CIntegerConstantExpression(0));
                    prevCallExpr->Exprs->Add(arrayCreationExpr);
                }
                // ***************************************************************
                else {
                    CArrayInitExpression* arrayInitExpr = static_cast<CArrayInitExpression*>(curCallExpr->Exprs->Item(0));
                    SKIZO_REQ_POS(curCallExpr->Exprs->Count());
                    for(int i = 1; i < curCallExpr->Exprs->Count(); i++) {
                        arrayInitExpr->Exprs->Add(curCallExpr->Exprs->Item(i));
                    }
                    prevCallExpr->Exprs->Add(arrayInitExpr);
                }

                curCallExpr.SetVal(prevCallExpr);
            }
            // ********
            //   ';'
            // ********
            break; case E_TOKENKIND_SEMICOLON:
            {
                // The semicolon gets the current expression and adds it to the list of current bodies' expressions.

                // Reconstructs the call expression into a return/cast expression if the first element is "return" or "cast".
                // + anything else tryConvertCallExpr supports.
                Auto<CExpression> convertedExpr (tryConvertCallExpr(curCallExpr));
                if(convertedExpr) {
                    bool isReturnExpr = convertedExpr->Kind() == E_EXPRESSIONKIND_RETURN;
                    // Returns are allowed to be only the last statement in a body.
                    if(isReturnExpr && curBodyExpr->ReturnAlreadyDefined) {
                        ScriptUtils::FailT("Multiple return expressions are not allowed.", token);
                    }

                    curBodyExpr->Exprs->Add(convertedExpr);
                    if(isReturnExpr) {
                        curBodyExpr->ReturnAlreadyDefined = true;
                    }
                } else {
                    verifyCallComplete(curCallExpr);
                    curBodyExpr->Exprs->Add(curCallExpr);
                }

                // And creates a new current expression.
                curCallExpr.SetPtr(new CCallExpression());
                //
                curCallExpr->Source.Module = curModule;
                curCallExpr->Source.LineNumber = token->LineNumber;
                //
            }
            // ************
            //   METHOD
            // ************
            // Creates a closure expression.
            break; case E_TOKENKIND_METHOD:
            {
                Auto<CMethod> anonMethod (new CMethod(nullptr));
                anonMethod->Flags() |= E_METHODFLAGS_IS_ANONYMOUS;
                anonMethod->SetMethodKind(E_METHODKIND_NORMAL);
                anonMethod->SetParentMethod(curBodyExpr->Method);

                // Saves the previous current body expr.
                bodyStack.Push(curBodyExpr);
                curBodyExpr.SetPtr(new CBodyExpression()); // The new current body expr.
                //
                curBodyExpr->Source.Module = curModule;
                curBodyExpr->Source.LineNumber = token->LineNumber;
                //
                curBodyExpr->SetMethod(anonMethod, true);

                // Saves the previous call expr.
                callStack.Push(curCallExpr);
                curCallExpr.SetPtr(new CCallExpression()); // The new current call expr of the new current body.
                //
                curCallExpr->Source.Module = curModule;
                curCallExpr->Source.LineNumber = token->LineNumber;
                //

                parseFunctionSig(anonMethod, true);

                // '{'
                reader.Expect(E_TOKENKIND_LBRACE);
            }
            // *********************
            break; case E_TOKENKIND_RBRACE:
            {
                // The previous token can only be ";" or "@"
                const CToken* prevToken = reader.PeekPrevToken();
                const ETokenKind prevKind = prevToken->Kind;
                if(prevKind != E_TOKENKIND_SEMICOLON
                && prevKind != E_TOKENKIND_CCODE
                && prevKind != E_TOKENKIND_LBRACE) {
                    ScriptUtils::FailT("'{', ';' or '@' expected before '}'", token);
                }

                // *************************************************************
                // Checks if a method returns a value if it's expected to do so.
                // If the current method returns something, then the last expression must be a return expression.
                if(!curBodyExpr->Method->Signature().ReturnType.IsVoid()) {
                    if(!hasValidLastExpr(curBodyExpr)) {
                        ScriptUtils::FailT("In a method which returns a value, the last expression must be a return expression or inline C code expression in unsafe context.", token);
                    }
                }
                // *************************************************************

                if(bodyStack.IsEmpty()) {
                    // If there is no previous body, then we're currently in the top level of the function.
                    // As we've met '}', the function is over and we can quit.
                    method->SetExpression(curBodyExpr);
                    return;
                } else {
                    // Restores the previous body.
                    Auto<CBodyExpression> prevBody (bodyStack.Pop());

                    // If there exists a previous body, then the current body is nested inside the previous body; and we
                    // have just completed describing the nested body's content (got '}'). Now add the current body to the
                    // restored previous current call expr as its casual item and set the previous body as the current body
                    // (restores).
                    curCallExpr.SetPtr(callStack.Pop());
                    curCallExpr->Exprs->Add(curBodyExpr);
                    curBodyExpr.SetVal(prevBody);
                }
            }
            // *********************
            //    Inline C code.
            // *********************
            break; case E_TOKENKIND_CCODE:
            {
                if(!method->IsUnsafe() || (!domain->IsTrusted() && !curModule->IsBaseModule)) {
                    ScriptUtils::FailT("Only unsafe contexts in trusted domains or in base modules allow inline C code.", token);
                }

                curExpr.SetPtr(new CCCodeExpression(token->StringSlice));

                // curCall expr is created a new
                if(curCallExpr->Exprs->Count() != 0) {
                    ScriptUtils::FailT("Can't create a C code fragment inside a call expression.", token);
                }

                curBodyExpr->Exprs->Add(curExpr);
            }
            // *************************
            //    Identity comparison.
            // *************************
            break; case E_TOKENKIND_IDENTITYCOMPARISON:
            {
                // "===" must be at the second place and the current curCallExpr shouldn't be marked
                if(curCallExpr->Exprs->Count() != 1 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'===' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CIdentityComparisonExpression()); // marker; to be converted to the target expr later
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // *******************
            //    Is expression.
            // *******************
            break; case E_TOKENKIND_IS:
            {
                // "is" must be at the second place and the current curCallExpr shouldn't be marked
                if(curCallExpr->Exprs->Count() != 1 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'is' used in an inappropriate context.", token);
                }

                STypeRef typeRef (parseTypeRef());
                curExpr.SetPtr(new CIsExpression(typeRef)); // marker; to be converted to the target expr later
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // ********************
            //    Set expression.
            // ********************
            break; case E_TOKENKIND_ASSIGNMENT:
            {
                // "=" must be at the second place and the current curCallExpr shouldn't be marked
                if(curCallExpr->Exprs->Count() != 1 || curCallExpr->IsMarked) {
                    ScriptUtils::FailT("'=' used in an inappropriate context.", token);
                }

                curExpr.SetPtr(new CAssignmentExpression()); // marker; to be converted to the target expr later
                curCallExpr->Exprs->Add(curExpr);
                curCallExpr->IsMarked = true;
            }
            // *********************
            //    Everything else.
            // *********************
            break; default:
            {
                if(Tokenizer::IsOperator(token->Kind)) {
                    curExpr.SetPtr((CExpression*)new CIdentExpression(token->StringSlice));
                    curCallExpr->Exprs->Add(curExpr);
                } else {
                    ScriptUtils::FailT(domain->FormatMessage("Unexpected token '%s' (id=%d) in method body.", &token->StringSlice, (int)token->Kind),
                                       token);
                }
            }
            break;
        }

        if(curExpr) {
            curExpr->Source.Module = curModule;
            curExpr->Source.LineNumber = token->LineNumber;
        }
    }

    // ***********************************
}

// Parses from the function name to the ')'.
void SParser::parseFunctionSig(CMethod* method, bool isNameless)
{
    STypeRef& returnType = method->Signature().ReturnType;
    const EMethodKind methodKind = method->MethodKind();
    const bool noBody = method->IsAbstract() || method->SpecialMethod() == E_SPECIALMETHOD_NATIVE;

    const CToken* token = reader.PeekToken();

    if(isNameless && token && (token->Kind == E_TOKENKIND_LBRACE)) {
        // Syntax like "method{}" or "^{}.
        return;
    } else {
        // '('
        reader.Expect(E_TOKENKIND_LPARENTH);

        token = reader.PeekToken();
        if(isNameless && token && (token->Kind == E_TOKENKIND_RPARENTH)) {
           // Syntax like "method (){}" or "^(){}"
           reader.NextToken(); // skips
           goto case_returnType;
        } else if(!isNameless) {
            // Method name.
            token = reader.Expect(E_TOKENKIND_IDENTIFIER);
            method->SetName(token->StringSlice);

            SMetadataSource& source = method->Source();
            source.Module = curModule;
            source.LineNumber = token->LineNumber;
        }

    again:
        token = reader.NextToken();
        if(!token) {
            ScriptUtils::FailT("Unexpected end of stream; a parameter or ')' expected.", token);
        }

        if(token->Kind != E_TOKENKIND_RPARENTH) {

            if(token->Kind != E_TOKENKIND_IDENTIFIER) {
                ScriptUtils::FailT(domain->FormatMessage("Unexpected param name ('%S' found).", Tokenizer::NameForTokenKind(token->Kind)), token);
            }

            // NOTE! Param's name is checked for ambiguity with class members in the transformer, because at this point,
            // not all of the fields have been parsed yet.
            if(method->Signature().HasParamByName(token->StringSlice)) {
                ScriptUtils::FailT(domain->FormatMessage("Duplicate param name '%s'.", &token->StringSlice), token);
            }

            Auto<CParam> param (new CParam());
            param->DeclaringMethod = method;

            // Param name.
            param->Name = token->StringSlice;

            // ':'
            reader.Expect(E_TOKENKIND_COLON);

            // Param type.
            param->Type = parseTypeRef();

            method->Signature().Params->Add(param);
            goto again;
        }

case_returnType:
        // Requires a return type if it's a normal method. A normal method is allowed to
        // have no return type explicitly stated -- that means the method returns nothing.
        if(methodKind == E_METHODKIND_NORMAL) {
            token = reader.PeekToken();
            if(noBody) {
                if(!token || (token->Kind != E_TOKENKIND_COLON && token->Kind != E_TOKENKIND_SEMICOLON)) {
                    ScriptUtils::FailT("Expected ':' or ';'.", token);
                }
            } else {
                if(!token || (token->Kind != E_TOKENKIND_COLON && token->Kind != E_TOKENKIND_LBRACE)) {
                    ScriptUtils::FailT( "Expected ':' or '{'.", token);
                }
            }

            if(token->Kind == E_TOKENKIND_COLON) {
                reader.NextToken(); // Skips the colon.
                // The return type is explicitly stated.
                returnType = parseTypeRef();
            } else { // token->Kind == E_TOKENKIND_LBRACE
                // The return type is implicitly set to void.
                returnType.SetPrimType(E_PRIMTYPE_VOID);
            }
        }
    }
}

void SParser::parseMethod(CClass* klass, EMethodKind methodKind, const CToken* errorToken)
{
    if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE && this->curAccessModifier != E_ACCESSMODIFIER_PUBLIC) {
        ScriptUtils::FailT("Interfaces allow only public methods.", errorToken);
    }

    if(methodKind == E_METHODKIND_DTOR) {
        if(klass->IsValueType()) {
            ScriptUtils::FailT("Structs can't have destructors.", errorToken);
        }
        if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
            ScriptUtils::FailT("Interfaces can't have destructors.", errorToken);
        }
        if(isStatic && klass->StaticDtor()) {
            ScriptUtils::FailT("Static destructor already defined.", errorToken);
        }
        if(!isStatic && klass->InstanceDtor()) {
            ScriptUtils::FailT("Instance destructor already defined.", errorToken);
        }
    } else if(methodKind == E_METHODKIND_CTOR) {
        if(isStatic && klass->StaticCtor()) {
            ScriptUtils::FailT("Static constructor already defined.", errorToken);
        }
        if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
            ScriptUtils::FailT("Interfaces can't have constructors.", errorToken);
        }
    }

    Auto<CMethod> method (new CMethod(klass));

    method->SetAccess(this->curAccessModifier);
    method->Signature().IsStatic = this->isStatic;
    this->isStatic = false;
    if(this->isUnsafe) {
        method->Flags() |= E_METHODFLAGS_IS_UNSAFE;
    }
    this->isUnsafe = false;
    if(isAbstract) {
        method->Flags() |= E_METHODFLAGS_IS_ABSTRACT;
    }
    this->isAbstract = false;

    if(this->isNative) {
        method->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
    }
    this->isNative = false;

    method->SetMethodKind(methodKind);

    if(method->SpecialMethod() == E_SPECIALMETHOD_NATIVE && method->IsAbstract()) {
        ScriptUtils::FailT("A method can't be both native and abstract.", errorToken);
    }
    if(method->SpecialMethod() == E_SPECIALMETHOD_NATIVE && !method->Signature().IsStatic) {
        ScriptUtils::FailT("Native methods must be static.", errorToken);
    }
    if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
        // Whether you mark a method abstract or not -- does not matter. They're all abstract always.
        method->Flags() |= E_METHODFLAGS_IS_ABSTRACT;
        if(method->Signature().IsStatic) {
            ScriptUtils::FailT("Interface methods can't be static.", errorToken);
        }
    }
    if(method->IsAbstract() && !klass->IsAbstract()) {
        ScriptUtils::FailT("Abstract methods can be defined only in abstract classes.", errorToken);
    }
    if(method->IsAbstract() && method->Signature().IsStatic) {
        ScriptUtils::FailT("Static methods can't be abstract.", errorToken);
    }
    if(method->IsAbstract() && (methodKind != E_METHODKIND_NORMAL)) {
        ScriptUtils::FailT("Ctors and dtors can't be marked as abstract.", errorToken);
    }
    if(klass->IsStatic() && !method->Signature().IsStatic) {
        ScriptUtils::FailT("Only static methods allowed in a static class.", errorToken);
    }

    // Instance dtors and static ctors/dtors don't have names/arguments.
    if(methodKind != E_METHODKIND_DTOR && !((methodKind == E_METHODKIND_CTOR) && method->Signature().IsStatic)) {
        parseFunctionSig(method);
    }

    // *****************************************
    //   Adds attributes.
    // *****************************************
    if(attrs->Count()) {
        method->AddAttributes(this->attrs);
        attrs->Clear();
    }
    // *****************************************

    parseMethodBody(method);

    // The end.
    if(methodKind == E_METHODKIND_NORMAL) {
        if(method->Signature().IsStatic) {
            klass->AddStaticMethod(method);
        } else {
            method->SetVTableIndex(klass->InstanceMethods()->Count());
            klass->AddInstanceMethod(method);
        }
    } else if(methodKind == E_METHODKIND_CTOR) {
        if(method->Signature().IsStatic) {
            klass->SetStaticCtor(method);
        } else {
            klass->AddInstanceCtor(method);
            method->Signature().ReturnType = klass->ToTypeRef();
        }
    } else if(methodKind == E_METHODKIND_DTOR) {
        if(method->Signature().IsStatic) {
            klass->SetStaticDtor(method);
        } else {
            klass->SetInstanceDtor(method);
        }
    } else {
        SKIZO_REQ_NEVER
    }

    const bool isStaticCtorOrDtor = (methodKind == E_METHODKIND_CTOR || methodKind == E_METHODKIND_DTOR) && method->Signature().IsStatic;
    if(!isStaticCtorOrDtor) {
        // NOTE Doesn't check if it's conflicting with class names, since this function is used by the parser as it goes
        // which may not have yet parsed all the classes. Checks so in the transformer.
        klass->VerifyUniqueMemberName(method->Name());
        klass->AddToNameSet(method->Name(), method);
    }
}

// Enums are a syntactic sugar.
// WARNING IMPORTANT The layout "vtable => intValue => stringValue" should not be changed, as SEnumHeader relies on it
// as does the embedding API.
void SParser::parseEnum(const CToken* errorToken)
{
    if(isStatic || isAbstract) {
        ScriptUtils::FailT("Enum class can't be marked static or abstract.", errorToken);
    }

    int range = 0;
    const CToken* token = nullptr;
    const SStringSlice stringClassName (domain->NewSlice("string"));

    token = reader.Expect(E_TOKENKIND_IDENTIFIER); // 'enum %NAME%'
    Auto<CClass> enumClass (new CClass(domain));
    enumClass->SetFlatName(token->StringSlice);

    SMetadataSource& source = enumClass->Source();
    source.Module = curModule;
    source.LineNumber = token->LineNumber;

    reader.Expect(E_TOKENKIND_LBRACE); // '{'

    // *********************
    //   enum::intValue #0
    // *********************

    {
        Auto<CField> intValueField (new CField());
        intValueField->DeclaringClass = enumClass;
        intValueField->Name = domain->NewSlice("m_intValue");
        intValueField->Type.SetPrimType(E_PRIMTYPE_INT);
        enumClass->RegisterInstanceField(intValueField);
    }

    // ************************
    //   enum::stringValue #1
    // ************************

    Auto<CField> stringValueField (new CField());
    stringValueField->DeclaringClass = enumClass;
    stringValueField->Name = domain->NewSlice("m_stringValue");
    stringValueField->Type.SetObject(stringClassName);
    enumClass->RegisterInstanceField(stringValueField);

    // ***************************************
    //   enum::create(intValue, stringValue)
    // ***************************************

    {
        Auto<CMethod> ctor (new CMethod(enumClass));
        ctor->SetMethodKind(E_METHODKIND_CTOR);
        ctor->SetName(domain->NewSlice("create"));
        ctor->SetAccess(E_ACCESSMODIFIER_PRIVATE);
        ctor->Flags() |= E_METHODFLAGS_IS_UNSAFE;
        ctor->Signature().ReturnType = enumClass->ToTypeRef();

        {
            Auto<CParam> param1 (new CParam());
            param1->Name = domain->NewSlice("intValue");
            param1->Type.SetPrimType(E_PRIMTYPE_INT);
            ctor->Signature().Params->Add(param1);

            Auto<CParam> param2 (new CParam());
            param2->Name = domain->NewSlice("stringValue");
            param2->Type = stringValueField->Type;
            ctor->Signature().Params->Add(param2);
        }
        ctor->SetCBody("self->m_intValue = l_intValue;\n"
                       "self->m_stringValue = l_stringValue;\n");
        enumClass->RegisterInstanceCtor(ctor);
    }

    // ***************
    //   enum::toInt
    // ***************

    {
        Auto<CMethod> nMethod (new CMethod(enumClass));
        nMethod->SetName(domain->NewSlice("toInt"));
        nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
        nMethod->SetCBody("return self->m_intValue;\n");
        enumClass->RegisterInstanceMethod(nMethod);
    }

    // *******************
    //   enum::toString
    // *******************

    {
        Auto<CMethod> nMethod (new CMethod(enumClass));
        nMethod->SetName(domain->NewSlice("toString"));
        nMethod->Signature().ReturnType.SetObject(stringClassName);
        nMethod->SetCBody("return self->m_stringValue;\n");
        enumClass->RegisterInstanceMethod(nMethod);
    }

    // The static ctor of the enum sets all static fields to their respecting enum values.
    // We populate the body expr of the static ctor as we parse.
    Auto<CBodyExpression> staticCtorBodyExpr (new CBodyExpression());

    // TODO performance, simpler code
    Auto<const CString> enumNameString (enumClass->FlatName().ToString());

    while(true) {
        token = reader.NextToken();
        if(!token ||
        (token->Kind != E_TOKENKIND_IDENTIFIER && token->Kind != E_TOKENKIND_RBRACE))
        {
            ScriptUtils::FailT("Expected name or '}'.", token? token: errorToken);
        }

        if(token->Kind == E_TOKENKIND_RBRACE) {
            break;
        } else { /* E_TOKENKIND_IDENTIFIER */
            enumClass->VerifyUniqueMemberName(token->StringSlice);

            // **********************************************************
            //   The static field that backs up this enumeration value.
            // **********************************************************

            Auto<CField> nStaticField (new CField());
            nStaticField->DeclaringClass = enumClass;

            // TODO optimize, simplify
            Auto<const CString> fieldName (CString::Format("0value_%d", range));
            nStaticField->Name = domain->NewSlice(fieldName);

            nStaticField->IsStatic = true;
            nStaticField->Type = enumClass->ToTypeRef();
            enumClass->RegisterStaticField(nStaticField);

            // *************************************
            //   Getter to access each enum value.
            // *************************************

            Auto<CMethod> nMethod (new CMethod(enumClass));
            nMethod->SetName(token->StringSlice);
            nMethod->Signature().IsStatic = true;
            nMethod->Signature().ReturnType.SetObject(enumClass->FlatName());
            enumClass->RegisterStaticMethod(nMethod);

            // *********************************************
            //   Creates the body of the generated method.
            {
                Auto<CBodyExpression> bodyExpr (new CBodyExpression());
                Auto<CReturnExpression> retExpr (new CReturnExpression());
                Auto<CIdentExpression> identExpr (new CIdentExpression(nStaticField->Name));
                nMethod->SetExpression(bodyExpr);
                bodyExpr->Exprs->Add(retExpr);
                retExpr->Expr.SetVal(identExpr);
            }
            // *********************************************

            reader.Expect(E_TOKENKIND_SEMICOLON); // ';'

            // ***************************************************
            //   Adds enum value creation to the static ctor.
            //   WARNING This code relies on the emitter (TODO?)
            // ***************************************************
            {
                // TODO performance, simplify
                Auto<const CString> daNameString (token->StringSlice.ToString());
                void* soNameString = domain->InternStringLiteral(daNameString);
                Auto<const CString> cCodeStr (CString::Format("_so_%o_0value_%d = _so_%o_create(%d, (struct _so_string*)%p);\n",
                                                        (const CObject*)enumNameString,
                                                        range,
                                                        (const CObject*)enumNameString,
                                                        range,
                                                        soNameString));
                SStringSlice cCodeSlice (domain->NewSlice(cCodeStr));
                Auto<CCCodeExpression> cCodeExpr (new CCCodeExpression(cCodeSlice));
                staticCtorBodyExpr->Exprs->Add(cCodeExpr);
            }
            // *******************
        }

        range++;
    }

    if(range == 0) {
        ScriptUtils::FailT("Enums with zero items not allowed.", errorToken);
    }

    // ****************************
    //   Creates the static ctor.
    // ****************************

    Auto<CMethod> staticCtor (new CMethod(enumClass));
    staticCtor->SetMethodKind(E_METHODKIND_CTOR);
    staticCtor->Signature().IsStatic = true;
    staticCtor->SetExpression(staticCtorBodyExpr);
    enumClass->SetStaticCtor(staticCtor);

    // *************************
    //   enum::fromInt(intValue)
    // *************************

    {
        Auto<CMethod> nMethod (new CMethod(enumClass));
        nMethod->SetSpecialMethod(E_SPECIALMETHOD_ENUM_FROM_INT);
        nMethod->SetName(domain->NewSlice("fromInt"));
        nMethod->Signature().IsStatic = true;
        nMethod->Signature().ReturnType = enumClass->ToTypeRef();
        {
            Auto<CParam> param1 (new CParam());
            param1->Name = domain->NewSlice("intValue");
            param1->Type.SetPrimType(E_PRIMTYPE_INT);
            nMethod->Signature().Params->Add(param1);
        }
        enumClass->RegisterStaticMethod(nMethod);
    }

    // *************************

    domain->RegisterClass(enumClass);
}

// TODO use an API for creating classes
// NOTE Unified code for method classes and event classes as they are similar.
void SParser::parseMethodClassOrEventClass(const CToken* controlToken)
{
    // Expects "method class(" or "event class(".
    reader.Expect(E_TOKENKIND_CLASS);

    Auto<CClass> methodClass (CClass::CreateIncompleteMethodClass(domain));
    CMethod* pInvokeMethod = methodClass->InvokeMethod();
    SStringSlice methodNameBeforeOverriden (pInvokeMethod->Name());
    parseFunctionSig(methodClass->InvokeMethod());

    if(pInvokeMethod->Signature().ReturnType.PrimType != E_PRIMTYPE_VOID && controlToken->Kind == E_TOKENKIND_EVENT) {
        ScriptUtils::FailT("Event classes can't return values.", controlToken);
    }

    SStringSlice originalName (pInvokeMethod->Name());
    if(controlToken->Kind == E_TOKENKIND_EVENT) {
        Auto<const CString> handlerName (CString::Format("0EventHandler_%d", domain->NewUniqueId()));
        methodClass->SetFlatName(domain->NewSlice(handlerName));
        methodClass->Flags() |= E_CLASSFLAGS_IS_COMPGENERATED;
    } else {
        methodClass->SetFlatName(originalName);
    }

    // FIX parseFunctionSig(..) overriden "invoke"
    pInvokeMethod->SetName(methodNameBeforeOverriden);

    methodClass->Source() = pInvokeMethod->Source();
    domain->RegisterClass(methodClass);

    if(attrs->Count()) {
        // The class and the target method both share the same attributes.
        methodClass->AddAttributes(attrs);
        pInvokeMethod->AddAttributes(this->attrs);
    }

    reader.Expect(E_TOKENKIND_SEMICOLON); // Skips ';'

    // *******************************************************************
    // Now generates an event class if this is an event class description.
    // *******************************************************************

    if(controlToken->Kind == E_TOKENKIND_EVENT) {
        Auto<CClass> eventClass (new CClass(domain));
        eventClass->SetSpecialClass(E_SPECIALCLASS_EVENTCLASS); // !
        eventClass->SetWrappedClass(methodClass->ToTypeRef());
        eventClass->SetFlatName(originalName);

        SMetadataSource& source = eventClass->Source();
        source.Module = curModule;
        source.LineNumber = controlToken->LineNumber;

        //  The event shares attributes with the event handler and event handler's invoke.
        eventClass->AddAttributes(attrs);

            // NOTE We don't use structDef because GC would not know anything about the memory layout
            // to perform meaningful marking.
            // I don't want to overcomplicate the GC with a new special case. Arrays are enough.
            // Plus reflection would break. So we basically have to describe the fields manually.
            // Access to them can be emitted directly as CCode, though.
            // ******************************************
            //   The array which contains the handlers.
            // ******************************************
            Auto<CField> handlersField (new CField());
            handlersField->DeclaringClass = eventClass;
            handlersField->Access = E_ACCESSMODIFIER_PRIVATE;
            handlersField->Name = domain->NewSlice("m_array");
            handlersField->Type.SetObject(methodClass->FlatName());
            handlersField->Type.ArrayLevel++;
            handlersField->Source.Module = curModule;
            handlersField->Source.LineNumber = controlToken->LineNumber;
            eventClass->RegisterInstanceField(handlersField);
            // *************************************************
            //   ::create()
            //   Another dummy.
            // *************************************************
            {
                Auto<CMethod> ctor (new CMethod(eventClass));
                ctor->SetMethodKind(E_METHODKIND_CTOR);
                ctor->SetName(domain->NewSlice("create"));
                ctor->Signature().ReturnType = eventClass->ToTypeRef();
                eventClass->RegisterInstanceCtor(ctor);
            }
            // *************************************************
            //   ::fire(..) method which invokes the handlers.
            //   Added here only for reflection. The body is
            //   emitted by the emitter directly.
            // *************************************************
            {
                Auto<CMethod> nMethod (methodClass->InvokeMethod()->Clone());
                nMethod->SetDeclaringClass(eventClass);
                nMethod->Flags() &= ~E_METHODFLAGS_IS_ABSTRACT;
                nMethod->SetName(domain->NewSlice("fire"));
                nMethod->SetSpecialMethod(E_SPECIALMETHOD_FIRE);
                eventClass->RegisterInstanceMethod(nMethod);
            }
            // *************************************************
            //   ::addHandler(..)
            // *************************************************
            {
                Auto<CMethod> nMethod (new CMethod(eventClass));
                nMethod->SetName(domain->NewSlice("addHandler"));
                nMethod->SetSpecialMethod(E_SPECIALMETHOD_ADDHANDLER);
                {
                    Auto<CParam> param1 (new CParam());
                    param1->Type.SetObject(methodClass->FlatName());
                    param1->Name = domain->NewSlice("e");
                    nMethod->Signature().Params->Add(param1);
                }
                eventClass->RegisterInstanceMethod(nMethod);
            }
            // **************************************************************
        domain->RegisterClass(eventClass);
    }

    attrs->Clear();
}

void SParser::parseAlias(const CToken* errorToken)
{
    const CToken* token = nullptr;

    if(attrs->Count() > 0) {
        ScriptUtils::FailT("Aliases can't have attributes.", errorToken);
    }
    if(isStatic || isAbstract) {
        ScriptUtils::FailT("Aliases can't be marked as static or abstract.", errorToken);
    }

    Auto<CClass> aliasClass (new CClass(domain));
    aliasClass->SetSpecialClass(E_SPECIALCLASS_ALIAS);

    // The name of the alias.
    token = reader.Expect(E_TOKENKIND_IDENTIFIER);

    //
    SMetadataSource& source = aliasClass->Source();
    source.Module = curModule;
    source.LineNumber = token->LineNumber;
    //

    aliasClass->SetFlatName(token->StringSlice);

    // '='
    reader.Expect(E_TOKENKIND_ASSIGNMENT);

    // The basetype.
    aliasClass->SetWrappedClass(parseTypeRef());

    // ';'
    reader.Expect(E_TOKENKIND_SEMICOLON);

    domain->RegisterClass(aliasClass);
    domain->AddAlias(aliasClass);
}

static const char* parseMsg = "Unexpected token; 'field', 'method', 'property', 'event', 'ctor' or 'dtor' expected.";
void SParser::parseClassLevel(ETokenKind tokenKind, const CToken* errorToken)
{
    const bool isExtension = (tokenKind == E_TOKENKIND_EXTEND);

    const CToken* token = reader.Expect(E_TOKENKIND_IDENTIFIER);
    Auto<CClass> klass (new CClass(domain));

    if(tokenKind == E_TOKENKIND_STRUCT) {
        klass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
    }
    klass->SetFlatName(token->StringSlice);

    // Class/struct name/basic information.
    if(!isExtension) {
        if(isStatic) {
            klass->Flags() |= E_CLASSFLAGS_IS_STATIC;
        }
        if(isAbstract) {
            klass->Flags() |= E_CLASSFLAGS_IS_ABSTRACT;
        }
    }
    //
    SMetadataSource& source = klass->Source();
    source.Module = curModule;
    source.LineNumber = token->LineNumber;
    //
    this->isStatic = false;
    this->isAbstract = false;
    this->curAccessModifier = E_ACCESSMODIFIER_PUBLIC;

    // *****************************************
    //   Adds attributes.
    // *****************************************
    if(attrs->Count()) {
        klass->AddAttributes(attrs);
        attrs->Clear();
    }
    // *****************************************

    if(tokenKind == E_TOKENKIND_INTERFACE) {
        klass->SetSpecialClass(E_SPECIALCLASS_INTERFACE);
        // Whether an interface is marked abstract or not does not matter: they're always abstract.
        klass->Flags() |= E_CLASSFLAGS_IS_ABSTRACT;
    }

    if(this->isUnsafe) {
        ScriptUtils::FailT("Only methods can be marked unsafe.", token);
    }
    if(klass->IsAbstract() && klass->IsStatic()) {
        ScriptUtils::FailT("Static classes can't be abstract.", token);
    }
    if(klass->IsValueType()) {
        if(klass->IsAbstract() || klass->IsStatic()) {
            ScriptUtils::FailT("Structs can't be abstract or static.", token);
        }
    }
    if(klass->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
        if(klass->IsStatic()) {
            ScriptUtils::FailT("Interfaces can't be static.", token);
        }
    }

    token = reader.PeekToken();

    // ':' for inheriting classes.
    if(token && (token->Kind == E_TOKENKIND_COLON)) {
        if(isExtension) {
            ScriptUtils::FailT("'Extend' definitions can't inherit new classes.", token);
        }
        if(klass->IsValueType()) {
            ScriptUtils::FailT("Structs can't inherit from other classes.", token);
        }

        reader.NextToken(); // Skips ':'.

        // The name of the parent class.
        klass->SetBaseClass(parseTypeRef(true));
        reader.Expect(E_TOKENKIND_LBRACE); // Skips '{'.
    } else if(token && (token->Kind == E_TOKENKIND_LBRACE)) {
        reader.NextToken(); // Skips '{'.
    } else {
        ScriptUtils::FailT("Expected ':' or '{'", token);
    }

    // Looks for clues such as 'field', 'method', 'ctor' until '}' is found.
again:
    token = reader.NextToken();
    if(!token) {
        ScriptUtils::FailT(parseMsg, errorToken);
    }

    if(token->Kind != E_TOKENKIND_RBRACE) {
        switch(token->Kind) {
            case E_TOKENKIND_FIELD:
            case E_TOKENKIND_PROPERTY:
            case E_TOKENKIND_EVENT:
                if(isExtension) {
                    ScriptUtils::FailT("'extend' definitions aren't allowed to add new fields.", token);
                }
                parseFieldPropertyOrEvent(token->Kind, klass, token); break;
            case E_TOKENKIND_METHOD:
                parseMethod(klass, E_METHODKIND_NORMAL, token); break;
            case E_TOKENKIND_CONST:
                parseConst(klass, token); break;
            case E_TOKENKIND_CTOR:
                if(isExtension) {
                    ScriptUtils::FailT("'extend' definitions aren't allowed to add new constructors.", token);
                }
                parseMethod(klass, E_METHODKIND_CTOR, token); break;
            case E_TOKENKIND_DTOR:
                if(isExtension) {
                    ScriptUtils::FailT("'extend' definitions aren't allowed to add new destructors.", token);
                }
                parseMethod(klass, E_METHODKIND_DTOR, token); break;
            case E_TOKENKIND_PRIVATE:
                curAccessModifier = E_ACCESSMODIFIER_PRIVATE; break;
            case E_TOKENKIND_PROTECTED:
                curAccessModifier = E_ACCESSMODIFIER_PROTECTED; break;
            case E_TOKENKIND_PUBLIC:
                curAccessModifier = E_ACCESSMODIFIER_PUBLIC; break;
            case E_TOKENKIND_INTERNAL:
                curAccessModifier = E_ACCESSMODIFIER_INTERNAL; break;
            case E_TOKENKIND_STATIC:
                isStatic = true;
                break;
            case E_TOKENKIND_UNSAFE:
                this->isUnsafe = true;
                break;
            case E_TOKENKIND_ABSTRACT:
                this->isAbstract = true;
                break;
            case E_TOKENKIND_NATIVE:
                this->isNative = true;
                break;
            case E_TOKENKIND_LBRACKET:
                parseAttribute(token);
                break;
            default:
                ScriptUtils::FailT(parseMsg, token);
				break;
        }

        goto again;
    }

    if(this->isStatic) {
        ScriptUtils::FailT("'static' modifier is appliable only to fields, methods, ctors and dtors.", token);
    }
    if(this->isUnsafe) {
        ScriptUtils::FailT("'unsafe' modifier is appliable only to methods.", token);
    }
    if(this->isAbstract) {
        ScriptUtils::FailT("'abstract' modifier is appliable only to classes and methods.", token);
    }
    if(this->attrs->Count()) {
        ScriptUtils::FailT("Attributes are appliable only to classes, fields and methods.", token);
    }

    // After we have parsed the class, let's see if it has the "ptrWrapper" attribute applied.
    if(klass->IsPtrWrapper()) {
        klass->AddPtrWrapperMembers();
    }

    // Is the class a binary blob?
    {
        int forcedNativeSize;
        if(klass->TryGetIntAttribute("nativeSize", 0, &forcedNativeSize, true)) {
            if(isExtension) {
                ScriptUtils::FailT("An 'extend' definition doesn't support the 'nativeSize' attribute.", token);
            }
            if(!klass->IsValueType()) {
                ScriptUtils::FailT("The 'nativeSize' attribute is applicable only to structs.", token);
            }

            if(klass->InstanceFields()->Count() > 0) {
                ScriptUtils::FailT("A binary blob must declare zero fields.", token);
            }
            if(forcedNativeSize < 1) {
                ScriptUtils::FailT("Binary blob size must be greater than zero.", token);
            }

            klass->SetSpecialClass(E_SPECIALCLASS_BINARYBLOB);

            SGCInfo& gcInfo = klass->GCInfo();
            gcInfo.SizeForUse = gcInfo.ContentSize = forcedNativeSize;
        }
    }

    if(isExtension) {
        // Extensions are postponed.
        domain->AddExtension(klass);
    } else {
        // After we have parsed the class, let's see if it has any instance constructors. Create a default constructor
        // if there isn't any. The default constructor merely memsets all fields to zero.
        // TODO custom transformers
        if(!klass->IsStatic() && !klass->IsAbstract() && (klass->InstanceCtors()->Count() == 0)) {
            Auto<CMethod> defaultCtor (new CMethod(klass));
            defaultCtor->SetName(domain->NewSlice("createDefault"));
            defaultCtor->SetMethodKind(E_METHODKIND_CTOR);
            defaultCtor->Signature().ReturnType.SetObject(klass->FlatName());
            defaultCtor->Flags() |= E_METHODFLAGS_COMPILER_GENERATED;
            klass->AddInstanceCtor(defaultCtor);
            klass->AddToNameSet(defaultCtor->Name(), defaultCtor);
        }

        domain->RegisterClass(klass);
    }
}

void SParser::parseImport(const CToken* errorToken)
{
    const CToken* token = reader.NextToken();
    if(!token || token->Kind != E_TOKENKIND_IDENTIFIER) {
        ScriptUtils::FailT(domain->FormatMessage("'import' requires an identifier ('%S' found).",
                                                 token? Tokenizer::NameForTokenKind(token->Kind): "end of stream"),
                           token? token: errorToken);
    }

    // Imports.
    {
        Auto<const CString> newSource (token->StringSlice.ToString());

        // ****************************************************
        Auto<const CString> loweredSource (newSource->ToLowerCase());
        if(!newSource->Equals(loweredSource)) {
            ScriptUtils::FailT(domain->FormatMessage("Module names allow only lowercase symbols ('%s' given).",
                                                     &token->StringSlice),
                               errorToken);
        }
        // ****************************************************

        if(!domain->ContainsSource(newSource)) {
            domain->EnqueueSource(newSource);
        }
    }

    reader.Expect(E_TOKENKIND_SEMICOLON);
}

void SParser::parseAttribute(const CToken* errorToken)
{
    const CToken* token = nullptr;
    Auto<CAttribute> attr;

    token = reader.NextToken();
    // NOTE We allow values that look like keywords be attribute names, too.
    if(!token || (token->Kind != E_TOKENKIND_IDENTIFIER && !Tokenizer::IsKeyword(token->StringSlice))) {
        ScriptUtils::FailT(domain->FormatMessage("Expected an attribute name ('%S' found).",
                                                 token? Tokenizer::NameForTokenKind(token->Kind): "end of stream"),
                           token? token: errorToken);
    }

    // ************************************************************
    //   Checks if an attribute with such name was already added.
    // ************************************************************
    for(int i = 0; i < attrs->Count(); i++) {
        if(attrs->Array()[i]->Name.Equals(token->StringSlice)) {
            ScriptUtils::FailT(domain->FormatMessage("Duplicate attribute '%s'.", &token->StringSlice), errorToken);
        }
    }
    // ************************************************************

    attr.SetPtr(new CAttribute());
    attr->Name = token->StringSlice;

    token = reader.NextToken();
    if(token && token->Kind == E_TOKENKIND_ASSIGNMENT) {
        token = reader.NextToken();
        if(!token
        || (token->Kind != E_TOKENKIND_IDENTIFIER
            && token->Kind != E_TOKENKIND_STRING_LITERAL
            && token->Kind != E_TOKENKIND_INT_LITERAL
            && token->Kind != E_TOKENKIND_FLOAT_LITERAL
            && token->Kind != E_TOKENKIND_TRUE
            && token->Kind != E_TOKENKIND_FALSE
            && token->Kind != E_TOKENKIND_NULL
            && token->Kind != E_TOKENKIND_CHAR_LITERAL))
        {
            ScriptUtils::FailT("Expected an attribute value.", token? token: errorToken);
        }
        attr->Value = token->StringSlice;
        reader.Expect(E_TOKENKIND_RBRACKET);
    } else if(token && token->Kind == E_TOKENKIND_RBRACKET) {
        // Nothing.
    } else {
        ScriptUtils::FailT("Expected an attribute value or ']'.", token? token: errorToken);
    }

    attrs->Add(attr);
}

void SParser::parseForce(const CToken* token)
{
    // parseTypeRef(..) takes care of adding ForcedTypeRef's.
    // Don't do here anything else.
    STypeRef typeRef (parseTypeRef(false, true)); // isBaseClass=false, forcedTypeRef=true

    // ';'
    reader.Expect(E_TOKENKIND_SEMICOLON);
}

// Top level. Keywords that are searched for: 'class' or 'struct'.
void SParser::parseTopLevel()
{
    const CToken* token = nullptr;

    while((token = reader.NextToken())) {
        if(token->Kind == E_TOKENKIND_CLASS
        || token->Kind == E_TOKENKIND_STRUCT
        || token->Kind == E_TOKENKIND_INTERFACE
        || token->Kind == E_TOKENKIND_EXTEND) {
            parseClassLevel(token->Kind, token);
        } else if(token->Kind == E_TOKENKIND_ENUM) {
            parseEnum(token);
        } else if(token->Kind == E_TOKENKIND_METHOD || token->Kind == E_TOKENKIND_EVENT) {
            parseMethodClassOrEventClass(token);
        } else if(token->Kind == E_TOKENKIND_ALIAS) {
            parseAlias(token);
        } else if(token->Kind == E_TOKENKIND_STATIC) {
            isStatic = true;
        } else if(token->Kind == E_TOKENKIND_ABSTRACT) {
            isAbstract = true;
        } else if(token->Kind == E_TOKENKIND_IMPORT) {
            parseImport(token);
        } else if(token->Kind == E_TOKENKIND_LBRACKET) {
            parseAttribute(token);
        } else if(token->Kind == E_TOKENKIND_FORCE) {
            parseForce(token);
        } else {
            ScriptUtils::FailT("'class', 'struct', 'extend', 'static' (class modifier), 'method' (as part of 'method class'), 'import', 'force' or 'alias' expected.", token);
        }
    }
}

void SkizoParse(CDomain* domain, const CString* filePath, const CString* code, bool isBaseModule)
{
    Auto<CArrayList<CToken*> > tokens (Tokenizer::Tokenize(domain, filePath, code));

    Auto<CModuleDesc> module (new CModuleDesc(filePath, isBaseModule));
    domain->AddModule(module);

    SParser parser (tokens, module, domain);
    parser.parseTopLevel();
}

} }

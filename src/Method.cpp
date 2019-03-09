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

#include "Method.h"
#include "Contract.h"
#include "Domain.h"
#include "ScriptUtils.h"
#include "StringBuilder.h"
#include "Tokenizer.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

    // ******************
    //   Ctors & dtors.
    // ******************

CMethod::CMethod(CClass* declaringClass)
        : m_declaringClass(declaringClass),
          m_declaringExtClass(nullptr),
          m_methodKind(E_METHODKIND_NORMAL),
          m_access(E_ACCESSMODIFIER_PUBLIC),
          m_specialMethod(E_SPECIALMETHOD_NONE),
          m_vtableIndex(-1),
          m_baseMethod(nullptr), m_parentMethod(nullptr),
          m_closureEnvClass(nullptr),
          m_numberOfCalls(0), m_totalTimeInMs(0),
          m_targetField(nullptr)
{
    m_flags = E_METHODFLAGS_NONE;
}

void* CMember::operator new(size_t size)
{
    return SO_FAST_ALLOC(size, E_SKIZOALLOCATIONTYPE_MEMBER);
}

void CMember::operator delete (void *p)
{
    // NOTHING
}

EMemberKind CMethod::MemberKind() const
{
    return E_RUNTIMEOBJECTKIND_METHOD;
}

CLocal* CMethod::LocalByIndex(int index) const
{
    if(m_locals) {
        SHashMapEnumerator<SStringSlice, CLocal*> localsEnum (m_locals);
        CLocal* local;
        int match = 0;
        while(localsEnum.MoveNext(nullptr, &local)) {
            if(index == match) {
                return local;
            }
            match++;
        }
    }
    SKIZO_REQ_NEVER
    return nullptr;
}

CParam* CMethod::ParamByName(const SStringSlice& ident) const
{
    for(int i = 0; i < m_sig.Params->Count(); i++) {
        CParam* param = m_sig.Params->Array()[i];

        if(param->Name.Equals(ident)) {
            return param;
        }
    }

    return nullptr;
}

CLocal* CMethod::LocalByName(const SStringSlice& ident) const
{
    CLocal* local = nullptr;
    if(m_locals && m_locals->TryGet(ident, &local)) {
        local->Unref();
    }
    return local;
}

bool CMethod::IsLegalVarName(const SStringSlice& ident) const
{
    // _so_ and _soX_ are prefixes that are reserved for the emitter and runtime helper methods.
    if(ident.StartsWithAscii("_so_") || ident.StartsWithAscii("_soX_")) {
        return false;
    }

    if(Tokenizer::IsKeyword(ident)) {
        return false;
    }

    if(m_declaringClass->DeclaringDomain()->ClassByFlatName(ident)) {
        return false;
    }

    if(this->ParamByName(ident)) {
        return false;
    }

    return true;
}

CLocal* CMethod::NewLocal(const SStringSlice& name, const STypeRef& typeRef)
{
    if(!m_locals) {
        m_locals.SetPtr(new CHashMap<SStringSlice, CLocal*>());
    }

    SKIZO_REQ(!m_locals->Contains(name), EC_ILLEGAL_ARGUMENT);

    const SResolvedIdentType resolvedIdent (this->ResolveIdent(name));
    // NOTE local names don't conflict with method names as those require a target
    if(!resolvedIdent.IsVoid() && resolvedIdent.EType != E_RESOLVEDIDENTTYPE_METHOD) {
        ScriptUtils::FailM(m_declaringClass->DeclaringDomain()->FormatMessage("Local name '%s' conflicts with another name (class, method, field, const or param).", &name), this);
    }

    Auto<CLocal> newLocal (new CLocal(name, typeRef, this));
    m_locals->Set(name, newLocal);

    return newLocal;
}

// ************
//   setCBody
// ************

void CMethod::SetCBody(const char* cBody)
{
    CDomain* domain = CDomain::ForCurrentThread();
    SKIZO_REQ_PTR(domain);

    // WARNING Important to leave it here, as removing it places "pushframe", which is buggy together
    // with arbitrary C code
    m_flags |= E_METHODFLAGS_IS_UNSAFE;

    Auto<CBodyExpression> bodyExpr (new CBodyExpression());
    Auto<CCCodeExpression> cCodeExpr (new CCCodeExpression(domain->NewSlice(cBody)));
    bodyExpr->Exprs->Add(cCodeExpr);
    m_expression.SetVal(bodyExpr);
}

// TODO copypaste for void CMethod::setCBody(const char* cBody); remove
void CMethod::SetCBody(const CString* cBody)
{
    CDomain* domain = CDomain::ForCurrentThread();
    SKIZO_REQ_PTR(domain);

    m_flags |= E_METHODFLAGS_IS_UNSAFE;

    Auto<CBodyExpression> bodyExpr (new CBodyExpression());
    Auto<CCCodeExpression> cCodeExpr (new CCCodeExpression(domain->NewSlice(cBody)));
    bodyExpr->Exprs->Add(cCodeExpr);
    m_expression.SetVal(bodyExpr);
}

static STypeRef typeRefFromCode(char c)
{
    STypeRef ref;
    switch(c) {
        case 'v': ref.PrimType = E_PRIMTYPE_VOID; break;
        case 'i': ref.PrimType = E_PRIMTYPE_INT; break;
        case 'f': ref.PrimType = E_PRIMTYPE_FLOAT; break;
        case 'b': ref.PrimType = E_PRIMTYPE_BOOL; break;
        case 'c': ref.PrimType = E_PRIMTYPE_CHAR; break;
        case 'p': ref.PrimType = E_PRIMTYPE_INTPTR; break;
        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
        break;
    }
    return ref;
}

void CMethod::SetMethodSig(const char* sig)
{
    int len = strlen(sig);
    SKIZO_REQ_POS(len);
    m_sig.ReturnType = typeRefFromCode(sig[0]);
    for(int i = 1; i < len; i++) {
        Auto<CParam> param (new CParam());
        param->Type = typeRefFromCode(sig[i]);
        m_sig.Params->Add(param);
    }
}

CMethod* CMethod::Clone() const
{
    CMethod* clone = new CMethod(m_declaringClass);
    clone->m_declaringClass = m_declaringClass;

    clone->m_flags = m_flags;
    // Cache-related flags should be set to 0 to force a recache.
    clone->m_flags &= ~E_METHODFLAGS_WAS_EVER_CALLED;
    clone->m_flags &= ~E_METHODFLAGS_ECALL_ATTRIBUTES_RESOLVED;
    clone->m_flags &= ~E_METHODFLAGS_FORCE_NO_HEADER; // ?
    clone->m_flags &= ~E_METHODFLAGS_HAS_BREAK_EXPRS;
    clone->m_flags &= ~E_METHODFLAGS_IS_INFERRED;

    clone->m_name = m_name;
    for(int i = 0; i < m_sig.Params->Count(); i++) {
        Auto<CParam> paramCopy (m_sig.Params->Array()[i]->Clone());
        paramCopy->DeclaringMethod = clone;
        clone->m_sig.Params->Add(paramCopy);
    }
    clone->m_sig.ReturnType = m_sig.ReturnType;
    clone->m_methodKind = m_methodKind;
    clone->m_access = m_access;
    clone->m_sig.IsStatic = m_sig.IsStatic;
    clone->m_specialMethod = m_specialMethod;
    clone->m_vtableIndex = m_vtableIndex;
    clone->m_baseMethod = m_baseMethod;
    clone->m_parentMethod = m_parentMethod;
    clone->m_expression.SetVal(m_expression);
    if(m_locals) {
        clone->m_locals.SetPtr(new CHashMap<SStringSlice, CLocal*>());

        SHashMapEnumerator<SStringSlice, CLocal*> localEnum (m_locals);
        SStringSlice localName;
        CLocal* local;
        while(localEnum.MoveNext(&localName, &local)) {
            Auto<CLocal> localCopy (local->Clone());
            localCopy->DeclaringMethod = clone;
            clone->m_locals->Set(localName, localCopy);
        }
    }
    clone->m_closureEnvClass = m_closureEnvClass;

    return clone;
}

// TODO allocates too much memory?
char* CMethod::GetCName() const
{
    const SStringSlice className (m_declaringClass->FlatName());

    SStringSlice methodName;
    if(m_methodKind == E_METHODKIND_DTOR) {
        methodName = m_declaringClass->DeclaringDomain()->NewSlice("dtor");
    } else {
        methodName = m_name;
    }

    Auto<CStringBuilder> sb (new CStringBuilder());

    if(m_ecallDesc.CallConv == E_CALLCONV_STDCALL) {
        sb->Append("_"); // NOTE This is how TCC does it.
    }

    sb->Append("_so_");
    sb->Append(className.String, className.Start, className.End - className.Start);
    sb->Append("_");
    sb->Append(methodName.String, methodName.Start, methodName.End - methodName.Start);

    if(m_ecallDesc.CallConv == E_CALLCONV_STDCALL) {
        // NOTE TCC respects this kind of mangling, unlike Windows.
        sb->Append("@");

        // WARNING Assumes that all arguments are always word-size.
        // TODO x86-only?
        int realParamCount = m_sig.Params->Count();
        if(!m_sig.IsStatic) {
            realParamCount++; // accounts for "this" pointer
        }
        sb->Append(realParamCount * (int)sizeof(void*));
    }

    Auto<const CString> daStr (sb->ToString());
    return daStr->ToUtf8();
}

// Method body context: resolves fields & methods of the declaring class, class names, locals and params.
SResolvedIdentType CMethod::ResolveIdent(const SStringSlice& ident, bool includeParams) const
{
    SResolvedIdentType r;

    if(m_locals && m_locals->TryGet(ident, &r.AsLocal_)) {
        r.AsLocal_->Unref();
        r.EType = E_RESOLVEDIDENTTYPE_LOCAL;
        return r;
    }

    if(includeParams) {
        CParam* param = this->ParamByName(ident);
        if(param) {
            r.EType = E_RESOLVEDIDENTTYPE_PARAM;
            r.AsParam_ = param;
            return r;
        }
    }

    r = m_declaringClass->ResolveIdent(ident);

    // If nothing is found, tries to find this ident in the parent method.
    // NOTE: don't confuse parent methods with base methods!
    if(r.IsVoid() && m_parentMethod) {
        return m_parentMethod->ResolveIdent(ident, includeParams);
    }

    return r;
}

bool CMethod::IsUnsafe() const
{
    if(m_parentMethod) {
        return m_parentMethod->IsUnsafe();
    } else {
        return m_flags & E_METHODFLAGS_IS_UNSAFE;
    }
}

bool CMethod::IsStaticContext() const
{
    if(m_parentMethod) {
        return m_parentMethod->IsStaticContext();
    } else {
        return m_sig.IsStatic;
    }
}

CMethod* CMethod::UltimateBaseMethod() const
{
    if(m_baseMethod) {
        return m_baseMethod->UltimateBaseMethod();
    } else {
        return const_cast<CMethod*>(this);
    }
}

void CMethod::InitSimpleGetter()
{
    if(m_specialMethod == E_SPECIALMETHOD_NONE
    && !this->IsTrulyVirtual()
    && !m_sig.IsStatic && m_sig.Params->Count() == 0
    && m_sig.ReturnType.PrimType != E_PRIMTYPE_VOID
    && m_expression && m_expression->Exprs->Count() == 1
    && !this->IsUnsafe()
    && m_declaringClass->IsInitialized()) // may throw
    {
        const CExpression* theOnlyExpr = m_expression->Exprs->Item(0);
        if(theOnlyExpr->Kind() == E_EXPRESSIONKIND_RETURN) {
            const CReturnExpression* retExpr = static_cast<const CReturnExpression*>(theOnlyExpr);
            if(retExpr->Expr->Kind() == E_EXPRESSIONKIND_IDENT) {
                const CIdentExpression* problyFieldExpr = static_cast<const CIdentExpression*>(retExpr->Expr.Ptr());
                m_targetField = m_declaringClass->MyField(problyFieldExpr->Name, false);
            }
        }
    }

    if(m_targetField) {
        if(!m_declaringClass->DeclaringDomain()->ExplicitNullCheck() || m_declaringClass->IsValueType()) {
            m_flags |= E_METHODFLAGS_IS_INLINABLE;
        }
    }
}

bool CMethod::IsMethodClassInvoke() const
{
    return (m_declaringClass->SpecialClass() == E_SPECIALCLASS_METHODCLASS
         && m_declaringClass->InvokeMethod() == this);
}

bool CMethod::IsEnclosedByClass(const CClass* pTargetClass) const
{
    if(m_declaringClass == pTargetClass
    // FIX closures must access not only the methods of their direct enclosing classes, but also methods defined
    // in base classes of the enclosing classes, so that a closure could access a protected method of the parent,
    // for example.
    || m_declaringClass->IsSubclassOf(pTargetClass)) {
        return true;
    } else {
        if(m_parentMethod) {
            return m_parentMethod->IsEnclosedByClass(pTargetClass);
        } else {
            return false;
        }
    }
}

bool CMethod::IsAccessibleFromMethod(const CMethod* otherMethod) const
{
    bool r = false;

    switch(m_access) {
        case E_ACCESSMODIFIER_PRIVATE:
            if(this == otherMethod) {
                r = true;
            } else {
                // Special codepath for extension methods, they are allowed to call private methods only if those
                // private methods are defined inside the same extension.
                if(otherMethod->m_declaringExtClass) {
                    r = (m_declaringExtClass == otherMethod->m_declaringExtClass);
                } else {
                    r = (m_declaringClass == otherMethod->m_declaringClass);
                }
            }
            break;

        case E_ACCESSMODIFIER_PROTECTED:
            r = (this == otherMethod) || (otherMethod->m_declaringClass->Is(m_declaringClass));
            break;

        case E_ACCESSMODIFIER_PUBLIC:
            r = true;
            break;

        case E_ACCESSMODIFIER_INTERNAL:
            // We can access an internal method if it's defined in the same module (file).
            r = (m_declaringClass->Source().Module == otherMethod->m_declaringClass->Source().Module);
            break;

        default: SKIZO_REQ_NEVER; break; // Should not happen.
    }

    // Exception for closures: they're allowed to access private methods of enclosing classes.
    if(!r && !otherMethod->m_declaringExtClass) {
        r = otherMethod->IsEnclosedByClass(m_declaringClass);
    }

    return r;
}

bool CMethod::IsValidEntryPoint() const
{
    if(m_sig.ReturnType.PrimType != E_PRIMTYPE_VOID) {
        return false;
    }
    if(m_sig.Params->Count() != 0) {
        return false;
    }
    if(m_ecallDesc.CallConv != E_CALLCONV_CDECL) {
        return false;
    }

    return true;
}

bool CMethod::ShouldEmitReglocalsCode() const
{
    // NOTE Unsafe contexts can potentially wreck our soft debugging technique, so it's disabled.
    if(this->HasBreakExprs() && !this->IsUnsafe()) {
        return !m_sig.IsStatic || (((m_locals? m_locals->Size(): 0) + m_sig.Params->Count()) > 0);
    } else {
        return false;
    }
}

bool CMethod::ShouldEmitVCH() const
{
    return m_declaringClass->SpecialClass() != E_SPECIALCLASS_INTERFACE // just to be sure
        && !m_declaringClass->IsValueType() // just to be sure
        && ((this->IsTrulyVirtual() && m_declaringClass->IsClassHierarchyRoot())
        || (this->WasEverCalled() && this->IsAbstract()));
}

void CMethod::ResolveECallAttributes()
{
    if(this->ECallAttributesResolved()) {
        return;
    }
    m_flags |= E_METHODFLAGS_ECALL_ATTRIBUTES_RESOLVED;

    if(m_attrs) {
        for(int i = 0; i < m_attrs->Count(); i++) {
            const CAttribute* attr = m_attrs->Array()[i];

            if(attr->Name.EqualsAscii("module")) {
                if(m_specialMethod != E_SPECIALMETHOD_NATIVE) {
                    ScriptUtils::FailM("Only native methods can be marked with the 'module' attribute.", this);
                }

                m_ecallDesc.ModuleName = attr->Value;
                m_ecallDesc.EntryPoint = m_name;
            } else if(attr->Name.EqualsAscii("callConv")) {
                // NOTE Method classes support different calling conventions for interop with native code.
                if(m_specialMethod != E_SPECIALMETHOD_NATIVE && !this->IsMethodClassInvoke()) {
                    ScriptUtils::FailM("Only native methods and method classes can be marked with the 'callConv' attribute.", this);
                }

                if(attr->Value.EqualsAscii("cdecl")) {
                    m_ecallDesc.CallConv = E_CALLCONV_CDECL;
                } else if(attr->Value.EqualsAscii("stdcall")) {
                    m_ecallDesc.CallConv = E_CALLCONV_STDCALL;
                } else {
                    ScriptUtils::FailM(CDomain::ForCurrentThread()->FormatMessage("Unknown calling convention '%s'.", &attr->Value),
                                       this);
                }
            }
        }
    }
}

void CMethod::AddAttributes(const CArrayList<CAttribute*>* attributes)
{
    if(!m_attrs) {
        m_attrs.SetPtr(new CArrayList<CAttribute*>());
    }
    m_attrs->AddRange(attributes);
}

} }

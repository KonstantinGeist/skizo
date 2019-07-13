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

#include "Transformer.h"
#include "ArrayList.h"
#include "Class.h"
#include "Const.h"
#include "Contract.h"
#include "Domain.h"
#include "ModuleDesc.h"
#include "Queue.h"
#include "ScriptUtils.h"

// TODO NOTE
// Code duplication between return/call/assignment/parameter passing when it comes to:
//
// * null => targetClass
// * subclass => parentclass (cast)
// * anonymous method => method class
//
// Try unify code?

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

static const char* returnNotAllowed = "Return expressions not allowed in this context.";

struct STransformer
{
    CDomain* domain;
    CMethod* curMethod;
    int curOrderIndex;

    Auto<CQueue<CClass*> > classesToProcess;
    Auto<CQueue<CClass*> > classesToProcess2;

    STransformer(CDomain* d)
        : domain(d),
          curMethod(nullptr),
          curOrderIndex(0),
          classesToProcess(new CQueue<CClass*>()),
          classesToProcess2(new CQueue<CClass*>())
    {
    }

    // Upon resolving, if the type was never inferred, enqueues it to the queue of types to process.
    bool ResolveTypeRef(STypeRef& typeRef);

    // Merges extensions with their target patchees.
    void mergeExtensions();

    void inferHierarchies();
    void inferForcedTypeRefs();

    void inferConsts(CClass* pClass);
    void inferFields(CClass* pClass);
    void inferEventFields(CClass* pClass);
    void inferInstanceCtors(CClass* pClass);
    void inferMethod(CMethod* method);

    // callExprPos simply tells if it's ok to have class names here (if it's an ident)
    // -1 and 0 means "it's ok", anything else produces an error.
    // Integers are used here because usually in call expressions only the first element is
    // allowed to be a class name, so we just pass the number of the element to see if it's ok.
    void inferValueExpr(CExpression* valueExpr, int callExprPos = -1, bool isAssignmentLValue = false);
    void inferIdentExpr(CIdentExpression* identExpr, int callExprPos = -1, bool isAssignmentLValue = false);

    void inferClosureExpr(CBodyExpression* callExpr, STypeRef& closureType);

    // If canInlineBranches is true, tries to inline the call expression. If the inlining is performed,
    // returns an add-ref'd value that should replace the original call expression.
    // If canInlineBranches is false, returns nothing.
    //
    // canInlineBranches is usually true if the expression is a top-level body statement, and false otherwise.
    CExpression* inferCallExpr(CCallExpression* callExpr, bool canInlineBranches = false);

    void inferRetExpr(CReturnExpression* retExpr);
    void inferCastExpr(CCastExpression* castExpr);
    void inferSizeofExpr(CSizeofExpression* sizeofExpr);
    void inferIntConstExpr(CIntegerConstantExpression* retExpr);
    void inferFloatConstExpr(CFloatConstantExpression* floatConstExpr);
    void inferBoolConstExpr(CBoolConstantExpression* valueExpr);
    void inferStringLitExpr(CStringLiteralExpression* stringLitExpr);
    void inferCharLitExpr(CCharLiteralExpression* charLitExpr);
    void inferThisExpr(CThisExpression* thisExpr);
    void inferArrayCreationExpr(CArrayCreationExpression* arrayCreationExpr, STypeRef& inferedTypeRef);
    void inferArrayInitExpr(CArrayInitExpression* arrayInitExpr, bool inferValues = true);
    void inferIdentCompExpr(CIdentityComparisonExpression* identCompExpr);
    void inferIsExpr(CIsExpression* isExpr);
    void inferAssignmentExpr(CAssignmentExpression* assExpr);
    void inferAbortExpr(CAbortExpression* abortExpr);
    void inferAssertExpr(CAssertExpression* assertExpr);
    void inferRefExpr(CRefExpression* assertExpr);
    void inferBreakExpr(CBreakExpression* breakExpr);
    void inferBodyStatements(CBodyExpression* bodyExpr, CMethod* pMethod, bool isInlinedBranching);

    void makeSureEnvClassReady(CMethod* meth);
    void addClosureEnvField(CLocal* capturedLocal);
    void addClosureEnvSelfField(CMethod* parentMethod);
    void addClosureEnvUpper(CMethod* parentMethod);

    // Inserts implicit casts: box/unbox where needed (based on the information provided by STypeRef::GetCastInfo).
    // The emitter then emits runtime helpers based on such "injected" cast expressions.
    CExpression* insertImplicitConversionIfAny(CExpression* expr,
                                               const SCastInfo& castInfo,
                                               STypeRef& targetType);

    // Inserts a constructor call that creates a target failable from null (if possible).
    // NOTE Doesn't automatically infer the inserted values.
    CExpression* insertFailableCtorFromNullValueNoInfer(STypeRef& targetType);

    // Used by inferEventFields(..), creates an expression that generates an event object.
    CAssignmentExpression* createEventCreationExpr(CField* eventField);
};

bool STransformer::ResolveTypeRef(STypeRef& typeRef)
{
    if(!domain->ResolveTypeRef(typeRef)) {
        return false;
    }

    if(!typeRef.ResolvedClass->IsInferred()) {
        classesToProcess->Enqueue(typeRef.ResolvedClass);
    }

    return true;
}

void STransformer::inferHierarchies()
{
    // **********************************************************************
    // After everything has been parsed, relink subclasses to parent classes.
    // **********************************************************************

    const CArrayList<CClass*>* klasses = domain->Classes();
    for(int i = 0; i < klasses->Count(); i++) {
        CClass* pClass = klasses->Array()[i];

        if(!pClass->IsClassHierarchyRoot()) {
            if(!domain->ResolveTypeRef(pClass->BaseClass())) {
                ScriptUtils::FailC(domain->FormatMessage("Unknown type '%T' declared as base of '%C'.",
                                                         &pClass->BaseClass(),
                                                         pClass),
                                   pClass);
            }
            CClass* pResolvedBase = pClass->ResolvedBaseClass();

            // The runtime relies on the assumption that "invoke" method is always at index 0.
            // Trying to inherit from a method class manually can break this assumption.
            if(pResolvedBase->SpecialClass() == E_SPECIALCLASS_METHODCLASS
            && !pResolvedBase->IsCompilerGenerated())
            {
                ScriptUtils::FailC("User code can't inherit from method classes directly.", pClass);
            }
            if(pResolvedBase->SpecialClass() == E_SPECIALCLASS_EVENTCLASS) {
                ScriptUtils::FailC("User code can't inherit from event classes.", pClass);
            }
            if(pResolvedBase->IsByValue()) {
                ScriptUtils::FailC("Can't inherit from primitives and structs.", pClass);
            }
            if(pResolvedBase->IsStatic()) {
                ScriptUtils::FailC("Can't inherit from a static class.", pClass);
            }
            if((pClass->SpecialClass() == E_SPECIALCLASS_INTERFACE)
            && pResolvedBase->SpecialClass() != E_SPECIALCLASS_INTERFACE)
            {
                ScriptUtils::FailC("Interfaces can inherit only from other interfaces.", pClass);
            }
            // Some built-in native types like "string" have predefined native structure layout instead of
            // relying on the emitter.
            if(!pResolvedBase->StructDef().IsEmpty()) {
                ScriptUtils::FailC("Can't inherit from a type with a native structure layout defined.", pClass);
            }
        }
    }

    for(int i = 0; i < klasses->Count(); i++) {
        CClass* pClass = klasses->Array()[i];

        if(pClass->ResolvedBaseClass()) {
            pClass->CheckCyclicDependencyInHierarchy(pClass->ResolvedBaseClass());
        }
        pClass->MakeSureMethodsFinalized();
    }

    // ************************************************************************************
    //   FIX
    // If a class has no destructor but has base destructors, an empty destructor must
    // still be created so that base destructors were called from there.
    // NOTE Placed after ::CheckCyclicDependencyInHierarchy above, otherwise ::HasBaseDtors
    // could stackoverflow.
    // ************************************************************************************

    for(int i = 0; i < klasses->Count(); i++) {
        CClass* pClass = klasses->Array()[i];

        if(!pClass->InstanceDtor() && pClass->HasBaseDtors()) {
            Auto<CMethod> emptyDtor (new CMethod(pClass));
            emptyDtor->SetMethodKind(E_METHODKIND_DTOR);
            pClass->SetInstanceDtor(emptyDtor);
        }
    }
}

void STransformer::inferForcedTypeRefs()
{
    // Resolves forced typerefs.
    CArrayList<CForcedTypeRef*>* forcedTypeRefs = domain->ForcedTypeRefs();
    
    for(int i = 0; i < forcedTypeRefs->Count(); i++) {
        CForcedTypeRef* forcedTypeRef = forcedTypeRefs->Array()[i];
        STypeRef& typeRef = forcedTypeRef->TypeRef;

        if(!ResolveTypeRef(typeRef)) {
            ScriptUtils::Fail_(domain->FormatMessage("Couldn't resolve a forced '%T'.", &forcedTypeRef->TypeRef),
                               forcedTypeRef->FilePath, forcedTypeRef->LineNumber);
        }

        // For valuetypes, the simple `force` syntax also generates the boxed version.
        // The previous idea of using syntax `force boxed int` was discarded as it introduces a new concept to the syntax.
        if(typeRef.IsBoxable()) {
            CClass* boxedClass = domain->BoxedClass(typeRef);
            if(!boxedClass->IsInferred()) {
                classesToProcess->Enqueue(boxedClass);
            }
        }
    }
    forcedTypeRefs->Clear();
}

// NOTE Extensions are allowed to define only new methods and consts.
void STransformer::mergeExtensions()
{
    const CArrayList<CClass*>* extensions = domain->Extensions();

    for(int i = 0; i < extensions->Count(); i++) {
        CClass* pExtension = extensions->Array()[i];

        SKIZO_REQ_EQUALS(pExtension->InstanceFields()->Count(), 0);
        SKIZO_REQ_EQUALS(pExtension->StaticFields()->Count(), 0);

        // "Extend" takes an existing previously defined class and extends it.
        CClass* classToPatch = domain->ClassByFlatName(pExtension->FlatName());
        if(!classToPatch) {
            ScriptUtils::FailC(domain->FormatMessage("Attempting to extend an unknown type '%C'.", pExtension),
                               pExtension);
        }
        if(classToPatch->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
            ScriptUtils::FailC("Interfaces aren't extendable.", pExtension);
        }
        if(classToPatch->IsAbstract()) { // includes method classes which are abstract by definition
            ScriptUtils::FailC("Abstract classes aren't extendable.", pExtension);
        }
        if(classToPatch->SpecialClass() == E_SPECIALCLASS_EVENTCLASS) {
            ScriptUtils::FailC("Event classes aren't extendable.", pExtension);
        }
        SKIZO_REQ(!classToPatch->IsMethodListFinalized(), EC_INVALID_STATE);

        // Merges static methods.
        {
            const CArrayList<CMethod*>* extensionStaticMethods = pExtension->StaticMethods();
            for(int j = 0; j < extensionStaticMethods->Count(); j++) {
                CMethod* m = extensionStaticMethods->Array()[j];

                // IMPORTANT rewires the declaring classes.
                m->SetDeclaringExtClass(m->DeclaringClass());
                m->SetDeclaringClass(classToPatch);

                if(!classToPatch->TryRegisterStaticMethod(m)) {
                    ScriptUtils::FailM(domain->FormatMessage("Can't extend class '%C' with a static method '%s': name already in use.", classToPatch, &m->Name()), m);
                }
            }
        }

        // Merges instance methods.
        {
            const CArrayList<CMethod*>* extensionInstanceMethods = pExtension->InstanceMethods();
            for(int j = 0; j < extensionInstanceMethods->Count(); j++) {
                CMethod* m = extensionInstanceMethods->Array()[j];

                if(classToPatch->IsStatic()) {
                    ScriptUtils::FailC("Static classes can't be extended with instance methods.", pExtension);
                }

                // IMPORTANT rewires the declaring classes.
                m->SetDeclaringExtClass(m->DeclaringClass());
                m->SetDeclaringClass(classToPatch);

                if(!classToPatch->TryRegisterInstanceMethod(m)) {
                    ScriptUtils::FailM(domain->FormatMessage("Can't extend class '%C' with an instance method '%s': name already in use.", classToPatch, &m->Name()), m);
                }
            }
        }

        // Merges consts.
        {
            const CArrayList<CConst*>* extensionConsts = pExtension->Constants();
            if(extensionConsts) {
                for(int j = 0; j < extensionConsts->Count(); j++) {
                    CConst* konst = extensionConsts->Array()[j];

                    // IMPORTANT rewires the declaring classes.
                    konst->DeclaringExtClass = konst->DeclaringClass;
                    konst->DeclaringClass = classToPatch;

                    classToPatch->RegisterConst(konst);
                }
            }
        }
    }

    // IMPORTANT don't clear Domain::m_extensions as it owns extension classes used by
    // CMethod::m_declaringExtClass and CConst::DeclaringExtClass to differentiate
    // scopes.
}

void SkizoTransform(CDomain* domain)
{
    STransformer transformer (domain);

    // Important to do it before everything else.
    transformer.mergeExtensions();

    // Aliases are transformed first, so that the rest of the classes could rewrite aliases to basetypes
    // as if aliases never existed.
    const CArrayList<CClass*>* aliases = domain->Aliases();
    for(int i = 0; i < aliases->Count(); i++) {
        CClass* aliasClass = aliases->Array()[i];

        if(!transformer.ResolveTypeRef(aliasClass->WrappedClass())) {
            ScriptUtils::FailC(domain->FormatMessage("Unknown type '%C' declared for alias.", aliasClass),
                               aliasClass);
        }

        aliasClass->Flags() |= E_CLASSFLAGS_IS_INFERRED;
    }

    transformer.inferHierarchies();
    transformer.inferForcedTypeRefs();

    // The the rest of the classes.
    const CArrayList<CClass*>* klasses = domain->Classes();
    for(int i = 0; i < klasses->Count(); i++) {
        CClass* klass = klasses->Array()[i];

        // Aliases are already inferred.
        if(klass->SpecialClass() != E_SPECIALCLASS_ALIAS) {
            transformer.classesToProcess->Enqueue(klass);
        }
    }

    // ************************************************
    // Infers types and verifies method parameters etc.
    // ************************************************

    while(!transformer.classesToProcess->IsEmpty()) {
        Auto<CClass> klass (transformer.classesToProcess->Dequeue());
        CClass* pClass = klass;

        if(pClass->IsInferred()) {
            continue;
        }

        pClass->MakeSureMethodsFinalized();
        SKIZO_REQ_NOT_EQUALS(pClass->SpecialClass(), E_SPECIALCLASS_ALIAS);
        transformer.classesToProcess2->Enqueue(klass);

        transformer.inferConsts(pClass);
        transformer.inferFields(pClass);
        // IMPORTANT Fields should be inferred before staticCtor and methods because inferEventFields(..)
        // modifies methods and can also create a new ctor.
        // IMPORTANT Should follow AFTER ::inferFields(..) because depends on resolved types of the fields.
        transformer.inferEventFields(pClass);
        transformer.inferInstanceCtors(pClass);
        transformer.inferMethod(pClass->StaticCtor());

        {
            const CArrayList<CMethod*>* instanceMethods = pClass->InstanceMethods();
            for(int j = 0; j < instanceMethods->Count(); j++) {
                transformer.inferMethod(instanceMethods->Array()[j]);
            }
        }

        {
            const CArrayList<CMethod*>* staticMethods = pClass->StaticMethods();
            for(int j = 0; j < staticMethods->Count(); j++) {
                transformer.inferMethod(staticMethods->Array()[j]);
            }
        }

        transformer.inferMethod(pClass->InstanceDtor());
        transformer.inferMethod(pClass->StaticDtor());

        pClass->BorrowAttributes();

        pClass->Flags() |= E_CLASSFLAGS_IS_INFERRED;
    }

    while(!transformer.classesToProcess2->IsEmpty()) {
        Auto<CClass> klass (transformer.classesToProcess2->Dequeue());
        klass->CalcGCMap();
    }
}

void STransformer::inferConsts(CClass* pClass)
{
    const CArrayList<CConst*>* consts = pClass->Constants();
    if(consts) {
        for(int i = 0; i < consts->Count(); i++) {
            CConst* konst = consts->Array()[i];

            if(!ResolveTypeRef(konst->Type)) {
                ScriptUtils::FailCnst(domain->FormatMessage("Const of unknown type '%T'.", &konst->Type),
                                      konst);
            }
        }
    }
}

void STransformer::inferFields(CClass* pClass)
{
    if(pClass->InstanceFields()->Count() == 0
    && !pClass->IsStatic() // static classes are ok
    && pClass->PrimitiveType() == E_PRIMTYPE_OBJECT // primitives like int/float/char are ok
    && pClass->IsValueType() // reference types are ok
    && pClass->SpecialClass() != E_SPECIALCLASS_BINARYBLOB // binary blobs are ok
    && pClass->StructDef().IsEmpty()) { // primitives with struct defs are ok
        ScriptUtils::FailC(domain->FormatMessage("Non-static valuetypes with zero fields not allowed."), pClass);
    }

    const CArrayList<CField*>* instanceFields = pClass->InstanceFields();
    for(int i = 0; i < instanceFields->Count(); i++) {
        CField* pField = instanceFields->Array()[i];
        if(domain->ClassByFlatName(pField->Name)) {
            ScriptUtils::FailF(domain->FormatMessage("Instance field name '%C::%s' conflicts with a type name.", pClass, &pField->Name),
                               pField);
        }
        if(!this->ResolveTypeRef(pField->Type)) {
            ScriptUtils::FailF(domain->FormatMessage("Instance field '%C::%s' of unknown type '%T'.", pClass, &pField->Name, &pField->Type),
                               pField);
        }
        if(pField->Type.PrimType == E_PRIMTYPE_VOID) {
            ScriptUtils::FailF("Field declared void.", pField);
        }
    }

    const CArrayList<CField*>* staticFields = pClass->StaticFields();
    for(int i = 0; i < staticFields->Count(); i++) {
        CField* pField = staticFields->Array()[i];
        if(domain->ClassByFlatName(pField->Name)) {
            ScriptUtils::FailF(domain->FormatMessage("Static field name '%C::%s' conflicts with a type name.", pClass, &pField->Name),
                               pField);
        }
        if(!this->ResolveTypeRef(pField->Type)) {
            ScriptUtils::FailF(domain->FormatMessage("Static field '%C::%s' of unknown type '%T'.", pClass, &pField->Name, &pField->Type),
                               pField);
        }
        if(pField->Type.PrimType == E_PRIMTYPE_VOID) {
            ScriptUtils::FailF(domain->FormatMessage("Field '%C::%s' declared void.", pClass, &pField->Name), pField);
        }
    }
}

CAssignmentExpression* STransformer::createEventCreationExpr(CField* eventField)
{
    CAssignmentExpression* assignExpr = new CAssignmentExpression();
    assignExpr->Expr1.SetPtr(new CIdentExpression(eventField->Name));
    Auto<CCallExpression> callExpr (new CCallExpression());
    {
        Auto<CIdentExpression> expr (new CIdentExpression(eventField->Type.ClassName));
        callExpr->Exprs->Add(expr);
        expr.SetPtr(new CIdentExpression(domain->NewSlice("create")));
        callExpr->Exprs->Add(expr);
    }
    assignExpr->Expr2.SetVal(callExpr);
    return assignExpr;
}

void STransformer::inferEventFields(CClass* pClass)
{
    const CArrayList<CField*>* events = pClass->EventFields();
    if(events) {
        // Verifies first.
        for(int i = 0; i < events->Count(); i++) {
            const CField* eventField = events->Array()[i];
            // The type of the field should be resolved already, because inferEventFields(..) follows after inferFields(..)
            SKIZO_REQ_PTR(eventField->Type.ResolvedClass);
            if(eventField->Type.ResolvedClass->SpecialClass() != E_SPECIALCLASS_EVENTCLASS) {
                ScriptUtils::FailF("Events support only event classes.", eventField);
            }
        }

        // Generates static events.
        for(int i = 0; i < events->Count(); i++) {
            CField* eventField = events->Array()[i];

            if(eventField->IsStatic) {
                // ********************************************************
                // Generates a static ctor if there's none.
                if(!pClass->StaticCtor()) {
                    Auto<CMethod> staticCtor (new CMethod(pClass));
                    staticCtor->SetMethodKind(E_METHODKIND_CTOR);
                    staticCtor->Signature().IsStatic = true;
                    pClass->SetStaticCtor(staticCtor);
                }
                // ********************************************************

                Auto<CAssignmentExpression> assignExpr (createEventCreationExpr(eventField));
                if(!pClass->StaticCtor()->Expression()) {
                    Auto<CBodyExpression> expr (new CBodyExpression());
                    pClass->StaticCtor()->SetExpression(expr);
                }
                pClass->StaticCtor()->Expression()->Exprs->Insert(0, assignExpr);
            }
        }

        // Generates instance events.
        for(int i = 0; i < events->Count(); i++) {
            CField* eventField = events->Array()[i];

            if(!eventField->IsStatic) {
                const CArrayList<CMethod*>* instanceCtors = pClass->InstanceCtors();
                for(int j = 0; j < instanceCtors->Count(); j++) {
                    Auto<CAssignmentExpression> assignExpr (createEventCreationExpr(eventField));
                    CMethod* pMethod = instanceCtors->Array()[j];
                    if(!pMethod->Expression()) {
                        Auto<CBodyExpression> expr (new CBodyExpression());
                        pMethod->SetExpression(expr);
                    }
                    pMethod->Expression()->Exprs->Insert(0, assignExpr);
                }
            }
        }

        // We don't need the list anymore.
        pClass->ClearEventFields();
    }
}

void STransformer::inferInstanceCtors(CClass* pClass)
{
    const bool isStructClass = pClass->PrimitiveType() == E_PRIMTYPE_OBJECT && pClass->IsValueType();

    const CArrayList<CMethod*>* instanceCtors = pClass->InstanceCtors();
    for(int j = 0; j < instanceCtors->Count(); j++) {
        CMethod* instanceCtor = instanceCtors->Array()[j];
        if(isStructClass && !instanceCtor->IsCompilerGenerated() && instanceCtor->Signature().Params->Count() == 0) {
            ScriptUtils::FailC("Structs aren't allowed to have explicit parameterless instance constructors.", pClass);
        }

        inferMethod(instanceCtor);
    }
}

void STransformer::inferBreakExpr(CBreakExpression* breakExpr)
{
    if(!domain->SoftDebuggingEnabled()) {
        ScriptUtils::WarnE("'Break' statement ignored (/softdebug:true required).", breakExpr);
    } else if(curMethod->IsUnsafe()) {
        ScriptUtils::WarnE("'Break' statement ignored (unsafe method).", breakExpr);
    } else {
        // 'Break' statement is only a marker which tells where to place a breakpoint.
        curMethod->Flags() |= E_METHODFLAGS_HAS_BREAK_EXPRS;
    }
}

void STransformer::inferBodyStatements(CBodyExpression* bodyExpr, CMethod* pMethod, bool isInlinedBranching)
{
    for(int i = 0; i < bodyExpr->Exprs->Count(); i++) {
        CExpression* subExpr = bodyExpr->Exprs->Array()[i];

        switch(subExpr->Kind()) {
            case E_EXPRESSIONKIND_CALL:
            {
                Auto<CExpression> inlinedExpr (inferCallExpr(static_cast<CCallExpression*>(subExpr), true));
                if(inlinedExpr) {
                    bodyExpr->Exprs->Set(i, inlinedExpr);
                }
            }
            break;
            case E_EXPRESSIONKIND_RETURN:
                // ********************************************************************************
                // Constructors internally return a value, but it's forbidden to explicitly return
                // something from constructors.
                // ********************************************************************************
                if(pMethod && pMethod->MethodKind() == E_METHODKIND_CTOR) {
                    ScriptUtils::FailE("Return expressions not allowed in constructors.", subExpr);
                    return;
                }
                if(isInlinedBranching) {
                    ScriptUtils::FailE(returnNotAllowed, subExpr);
                    return;
                }
                // ********************************************************************************

                inferRetExpr(static_cast<CReturnExpression*>(subExpr));
                break;
            case E_EXPRESSIONKIND_CCODE:
                // Nothing to infer.
                break;
            case E_EXPRESSIONKIND_ASSIGNMENT:
                inferAssignmentExpr(static_cast<CAssignmentExpression*>(subExpr)); break;
                break;
            case E_EXPRESSIONKIND_ABORT:
                inferAbortExpr(static_cast<CAbortExpression*>(subExpr)); break;
                break;
            case E_EXPRESSIONKIND_ASSERT:
                inferAssertExpr(static_cast<CAssertExpression*>(subExpr)); break;
            case E_EXPRESSIONKIND_REF:
                ScriptUtils::FailE("Ref expression not allowed in this context.", subExpr); break;
            case E_EXPRESSIONKIND_BREAK:
                inferBreakExpr(static_cast<CBreakExpression*>(subExpr)); break;
                break;
            default:
                ScriptUtils::FailE("Only method calls, assignments, 'return', 'abort', 'assert', 'break' or inline C code allowed in this context.", subExpr);
                break;
        }
    }
}

void STransformer::inferMethod(CMethod* method)
{
    if(!method) {
        return;
    }

    CMethod* pMethod = method;
    CClass* pDeclClass = pMethod->DeclaringClass();

    if(pMethod->IsInferred()) {
        return;
    }
    pMethod->Flags() |= E_METHODFLAGS_IS_INFERRED;

    // *********************************************************
    // Registers the native method to be checked later for impl.
    // *********************************************************
    if(pMethod->SpecialMethod() == E_SPECIALMETHOD_NATIVE) {
        if(pDeclClass->PrimitiveType() == E_PRIMTYPE_OBJECT) { // TODO?
            domain->MarkMethodAsICall(pMethod);
        }
    }
    // *************************************************

    if(pMethod->SpecialMethod() == E_SPECIALMETHOD_NATIVE && pMethod->Expression()) {
        ScriptUtils::FailM(domain->FormatMessage("Native method '%C::%s' with a body declared.", pDeclClass, &pMethod->Name()),
                           pMethod);
    }

    if(domain->ClassByFlatName(pMethod->Name())) {
        ScriptUtils::FailM(domain->FormatMessage("Method name '%C::%s' conflicts with a type name.", pDeclClass, &pMethod->Name()),
                           pMethod);
    }

    // ********************
    //   Resolves params.
    // ********************

    if(pMethod->MethodKind() == E_METHODKIND_DTOR) {
        SKIZO_REQ_EQUALS(pMethod->Signature().Params->Count(), 0);
        SKIZO_REQ_EQUALS(pMethod->Signature().ReturnType.PrimType, E_PRIMTYPE_VOID);
    }

    for(int i = 0; i < pMethod->Signature().Params->Count(); i++) {
        CParam* param = pMethod->Signature().Params->Array()[i];

        // ****************************************************************************
        // Checks if a param has a name that makes it ambiguous.
        // NOTE includeParams is set to to "false" because they were already checked.
        SResolvedIdentType resolvedIdent (pMethod->ResolveIdent(param->Name, false));
        // NOTE: params never conflict with method names as those require a target
        if(!resolvedIdent.IsVoid() && resolvedIdent.EType != E_RESOLVEDIDENTTYPE_METHOD) {
            ScriptUtils::FailL(domain->FormatMessage("Parameter name '%s' of method '%C::%s' conflicts with a type or member name.",
                                                     &param->Name, pDeclClass, &pMethod->Name()),
                               param);
        }
        // ****************************************************************************

        if(param->Type.PrimType == E_PRIMTYPE_VOID) {
            ScriptUtils::FailL(domain->FormatMessage("Parameter '%s' of method '%C::%s' declared void.",
                                                     &param->Name, pDeclClass, &pMethod->Name()),
                               param);
        }

        if(!this->ResolveTypeRef(param->Type)) {
            ScriptUtils::FailL(domain->FormatMessage("Parameter '%s' of method '%C::%s' is of unknown type '%T'.",
                                                     &param->Name, pDeclClass, &pMethod->Name(), &param->Type),
                               param);
        }
    }

    // *****************************
    //   Resolves the return type.
    // *****************************

    if(!this->ResolveTypeRef(pMethod->Signature().ReturnType)) {
        ScriptUtils::FailM(domain->FormatMessage("Return value of method '%C::%s' is of unknown type '%T'.",
                                                 pDeclClass, &pMethod->Name(), &pMethod->Signature().ReturnType),
                           pMethod);
    }

    if(pMethod->MethodKind() == E_METHODKIND_CTOR && !pMethod->Signature().IsStatic) {
        SKIZO_REQ_EQUALS(pMethod->Signature().ReturnType.ResolvedClass, pMethod->DeclaringClass());
    }

    // ******************************
    //   Resolves locals.
    // ******************************

    // Locals are resolved when created while inferring expressions.

    // ***********************
    //   Infers expressions.
    // ***********************

    CExpression* rootExpr = pMethod->Expression();
    if(rootExpr) {
        SKIZO_REQ_EQUALS(rootExpr->Kind(), E_EXPRESSIONKIND_BODY);
        CBodyExpression* bodyExpr = static_cast<CBodyExpression*>(rootExpr);

        curMethod = method;

        inferBodyStatements(bodyExpr, pMethod, false);
    }

    // *********************************************
    //   Resolves icall/ecall-related attributes.
    // *********************************************

    pMethod->ResolveECallAttributes();

    // ********************
    //   Resolves ECalls.
    // ********************

    if(pMethod->ECallDesc().IsValid()) {
        // ECall

        if(!pMethod->Signature().IsStatic) {
            ScriptUtils::FailM("ECalls must be static.", pMethod);
        }

        if(!domain->IsTrusted() && pMethod->Source().Module && !pMethod->Source().Module->IsBaseModule) {
            // Will be specially handled in the emitter.
            pMethod->SetSpecialMethod(E_SPECIALMETHOD_DISALLOWED_ECALL);
        } else {
            // Resolve.
            pMethod->ECallDesc().Resolve(pMethod);
            domain->AddECall((void*)pMethod);
        }

        // *********************************************************************************************
        //   FIX
        //
        // Disallows heap-allocated objects as arguments to ecalls. Users must use Marshal::dataOffset
        // and pass intptr's instead.
        // This removes potential problems whereby a user forgets to add "dataOffset" so that native code,
        // having no idea about vtables and such, overwrites such crucial data with random values.
        // *********************************************************************************************

        for(int i = 0; i < pMethod->Signature().Params->Count(); i++) {
            const CParam* param = pMethod->Signature().Params->Array()[i];
            const CClass* paramClass = param->Type.ResolvedClass;
            SKIZO_REQ_PTR(paramClass);

            if(paramClass->SpecialClass() != E_SPECIALCLASS_NONE
            || !paramClass->IsValueType()
            || (paramClass->IsValueType() && paramClass->PrimitiveType() == E_PRIMTYPE_OBJECT))
            {
                ScriptUtils::FailM("Only non-composite valuetypes allowed as ECalls arguments. "
                                   "To pass heap-allocated objects, use Marshal::dataOffset(..); "
                                   "to pass composite valuetypes, use (ref X).", pMethod);
            }
        }

        // ********************************************************************************
        // Disallows returning structs in ECalls since semantics aren't sufficiently clear
        // among compilers.
        // It's OK for ICalls because people are more sure what they're doing in that case.
        // ********************************************************************************

        const CClass* retClass = pMethod->Signature().ReturnType.ResolvedClass;
        SKIZO_REQ_PTR(retClass);
        if(retClass->SpecialClass() != E_SPECIALCLASS_NONE
        || !retClass->IsValueType()
        || (retClass->IsValueType() && retClass->PrimitiveType() == E_PRIMTYPE_OBJECT))
        {
            ScriptUtils::FailM("Only non-composite valuetypes allowed as ECall return values. "
                               "Certain systems return structures with a hidden first pointer to a buffer.", pMethod);
        }

    } else if(pMethod->SpecialMethod() == E_SPECIALMETHOD_NATIVE) {
        // ICall

        // Paranoia level: 80.
        // ICalls are completely disallowed outside of the base module directory.
        // Explanation: the runtime links icalls to classes/methods in a very straightforward manner.
        // It doesn't care where an icall stems from, it simply looks if there's an existing class under such
        // name registered in the metadata and happily links it to the native C code.
        // Imagine there's a class 'A' which has an icall named 'm' defined in the base module directory.
        // If a domain never imports this class, the domain is free to declare their own class under the same name.
        // The runtime will be duped into thinking it's the standard class and will link in the C function without
        // suspecting anything. This is potentially exploitable through an altered signature definition to
        // leave the stack imbalanced. We ban it in untrusted domains to remove a gaping security hole; for trusted
        // domains this removes potential problems due to simple name collisions.

        // TODO pMethod->Source().Module can be null for classes defined inside the execution engine itself, such as "string" or "int".
        // Make sure user code can not mimick this!
        if(pMethod->Source().Module && !pMethod->Source().Module->IsBaseModule) {
            ScriptUtils::FailM("ICalls can be defined only in base modules (placed in the base module directory).", pMethod);
        }
    }

    pMethod->InitSimpleGetter();
}

void STransformer::inferValueExpr(CExpression* valueExpr, int callExprPos, bool isAssignmentLValue)
{
    switch(valueExpr->Kind()) {
        case E_EXPRESSIONKIND_BODY:
             // Don't infer yet. It's to be inferred in assignments and elsewhere.
             break;
        case E_EXPRESSIONKIND_CALL:
            inferCallExpr(static_cast<CCallExpression*>(valueExpr)); break;
        case E_EXPRESSIONKIND_IDENT:
            inferIdentExpr(static_cast<CIdentExpression*>(valueExpr), callExprPos, isAssignmentLValue); break;
        case E_EXPRESSIONKIND_INTCONSTANT:
            inferIntConstExpr(static_cast<CIntegerConstantExpression*>(valueExpr)); break;
        case E_EXPRESSIONKIND_FLOATCONSTANT:
            inferFloatConstExpr(static_cast<CFloatConstantExpression*>(valueExpr)); break;
        case E_EXPRESSIONKIND_STRINGLITERAL:
            inferStringLitExpr(static_cast<CStringLiteralExpression*>(valueExpr)); break;
        case E_EXPRESSIONKIND_CHARLITERAL:
            inferCharLitExpr(static_cast<CCharLiteralExpression*>(valueExpr)); break;
        case E_EXPRESSIONKIND_NULLCONSTANT:
            // Nothing to infer.
            break;
        case E_EXPRESSIONKIND_BOOLCONSTANT:
            inferBoolConstExpr(static_cast<CBoolConstantExpression*>(valueExpr)); break;
            break;
        case E_EXPRESSIONKIND_THIS:
            inferThisExpr(static_cast<CThisExpression*>(valueExpr)); break;
        case E_EXPRESSIONKIND_RETURN:
            ScriptUtils::FailE(returnNotAllowed, valueExpr); break;
        case E_EXPRESSIONKIND_CAST:
            inferCastExpr(static_cast<CCastExpression*>(valueExpr)); break;
            break;
        case E_EXPRESSIONKIND_SIZEOF:
            inferSizeofExpr(static_cast<CSizeofExpression*>(valueExpr)); break;
            break;
        case E_EXPRESSIONKIND_ARRAYCREATION:
            // Just like closures, array creation expressions are inferred depending on the target type.
            break;
        case E_EXPRESSIONKIND_ARRAYINIT:
            // The type of array initialization depends on the type of the first item.
            inferArrayInitExpr(static_cast<CArrayInitExpression*>(valueExpr)); break;
            break;
        case E_EXPRESSIONKIND_IDENTITYCOMPARISON:
            inferIdentCompExpr(static_cast<CIdentityComparisonExpression*>(valueExpr)); break;
            break;
        case E_EXPRESSIONKIND_IS:
            inferIsExpr(static_cast<CIsExpression*>(valueExpr));
            break;
        case E_EXPRESSIONKIND_ASSIGNMENT:
            ScriptUtils::FailE("Assignment not allowed in this context.", valueExpr); break;
        case E_EXPRESSIONKIND_ABORT:
            ScriptUtils::FailE("Abort expression not allowed in this context.", valueExpr); break;
        case E_EXPRESSIONKIND_ASSERT:
            ScriptUtils::FailE("Assert expression not allowed in this context.", valueExpr); break;
        case E_EXPRESSIONKIND_REF:
            inferRefExpr(static_cast<CRefExpression*>(valueExpr)); break;
        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
        break;
    }
}

void STransformer::makeSureEnvClassReady(CMethod* meth)
{
    CMethod* pMeth = meth;

    if(!pMeth->ClosureEnvClass()) {
        Auto<CClass> closureEnvClass (new CClass(domain));
        closureEnvClass->SetSpecialClass(E_SPECIALCLASS_CLOSUREENV);
        // vtables will be generated lazily inside _soX_gc_alloc_env
        closureEnvClass->Flags() &= ~E_CLASSFLAGS_EMIT_VTABLE;
        closureEnvClass->Flags() |= E_CLASSFLAGS_FREE_VTABLE;
        pMeth->SetClosureEnvClass(closureEnvClass);

        const int uniqueId = domain->NewUniqueId();
        Auto<const CString> generatedName (CString::Format("0ClosureEnv_%d", uniqueId));
        closureEnvClass->SetFlatName(domain->NewSlice(generatedName));
        closureEnvClass->Flags() |= E_CLASSFLAGS_IS_COMPGENERATED;

        domain->RegisterClass(closureEnvClass);
        classesToProcess->Enqueue(closureEnvClass);
    }
}

void STransformer::addClosureEnvUpper(CMethod* meth)
{
    makeSureEnvClassReady(meth);
    CMethod* pMethod = meth;

    // TODO use a pregenerated slice, or something
    SStringSlice upperSlice (domain->NewSlice("_soX_upper"));
    if(!pMethod->ClosureEnvClass()->MyField(upperSlice, false)) {
        Auto<CField> envField (new CField());
        CField* pEnvField = envField;
        pEnvField->DeclaringClass = pMethod->ClosureEnvClass();
        pEnvField->Name = upperSlice;
        pEnvField->Type.SetObject(domain->NewSlice("any"));
        pMethod->ClosureEnvClass()->RegisterInstanceField(envField);
    }
}

void STransformer::addClosureEnvField(CLocal* capturedLocal)
{
    makeSureEnvClassReady(capturedLocal->DeclaringMethod);
    CMethod* pDeclMethod = capturedLocal->DeclaringMethod;

    if(!pDeclMethod->ClosureEnvClass()->MyField(capturedLocal->Name, false)) {
        Auto<CField> envField (new CField());
        CField* pEnvField = envField;
        pEnvField->DeclaringClass = pDeclMethod->ClosureEnvClass();
        pEnvField->Name = capturedLocal->Name;
        pEnvField->Type = capturedLocal->Type;
        pDeclMethod->ClosureEnvClass()->RegisterInstanceField(envField);
    }
}

void STransformer::addClosureEnvSelfField(CMethod* parentMethod)
{
    makeSureEnvClassReady(parentMethod);
    CMethod* pParentMethod = parentMethod;

    // TODO use a pregenerated slice, or something
    SStringSlice selfString (domain->NewSlice("_soX_self"));
    if(!pParentMethod->ClosureEnvClass()->MyField(selfString, false)) {
        Auto<CField> envField (new CField());
        CField* pEnvField = envField;
        pEnvField->DeclaringClass = pParentMethod->ClosureEnvClass();
        pEnvField->Name = selfString;
        pEnvField->Type = parentMethod->DeclaringClass()->ToTypeRef();
        pParentMethod->ClosureEnvClass()->RegisterInstanceField(envField);
    }
}

void STransformer::inferIdentExpr(CIdentExpression* identExpr, int callExprPos, bool isAssignmentLValue)
{
    if(identExpr->TypeAsInCode.IsVoid() && !identExpr->IsAuto) {
        // The local variable is not typed in this expression.

        // Let's find out if it was actually typed before.
        const SResolvedIdentType resolvedIdent (curMethod->ResolveIdent(identExpr->Name));
        if(resolvedIdent.EType == E_RESOLVEDIDENTTYPE_METHOD) {
            // TODO ?
            ScriptUtils::FailE("It's not allowed to refer to methods as data. Use closures instead.", identExpr);
        }
        else if((resolvedIdent.EType == E_RESOLVEDIDENTTYPE_CLASS) && callExprPos > 0) {
            ScriptUtils::FailE("It's not allowed to refer to classes as data.", identExpr);
        }

        if(resolvedIdent.EType == E_RESOLVEDIDENTTYPE_LOCAL) {
            // ***************************************************************************
            //   Captured local detected.
            //   Creates an environment class for the current method, if none was created.
            // ***************************************************************************

            if(resolvedIdent.AsLocal_->DeclaringMethod != curMethod) {
                // This makes the emitter know what locals are to be placed in closure environments.

                CMethod* parentMethod = curMethod;
            again0:
                parentMethod = parentMethod->ParentMethod();
                SKIZO_REQ_PTR(parentMethod);

                // NOTE All closures between declClosure and useClosure are forced to have closure environments.
                // This allows for data chaining even if some closures in the middle have no
                // captured data whatsoever (which generally doesn't initiate closure env construction).
                addClosureEnvUpper(parentMethod);

                if(parentMethod != resolvedIdent.AsLocal_->DeclaringMethod) {
                    goto again0;
                }

                resolvedIdent.AsLocal_->IsCaptured = true;
                addClosureEnvField(resolvedIdent.AsLocal_);
            }

            // ****************************
        } else if(resolvedIdent.EType == E_RESOLVEDIDENTTYPE_PARAM) {
            // ***************************************************************************
            //   Captured param detected.
            //   Creates an environment class for the current method, if none was created.
            // ***************************************************************************

            if(resolvedIdent.AsParam_->DeclaringMethod != curMethod) {
                // This makes the emitter know what locals are to be placed in closure environments.

                CMethod* parentMethod = curMethod;
            again2:
                parentMethod = parentMethod->ParentMethod();
                SKIZO_REQ_PTR(parentMethod);

                // NOTE All closures between declClosure and useClosure are forced to have closure environments.
                // This allows for data chaining even if some closures in the middle have no
                // captured data whatsoever (which generally doesn't initiate closure env construction).
                addClosureEnvUpper(parentMethod);

                if(parentMethod != resolvedIdent.AsParam_->DeclaringMethod) {
                    goto again2;
                }

                // This makes the emitter know what param are to be copied to closure environments.
                // NOTE The params are _copied_ to the environment, unlike locals which are defined in closure
                // environments from the beginning.
                resolvedIdent.AsParam_->IsCaptured = true;
                addClosureEnvField(resolvedIdent.AsParam_);
            }

            // ****************************
        } else if(resolvedIdent.EType == E_RESOLVEDIDENTTYPE_FIELD) {
            CField* pField = resolvedIdent.AsField_;

            if(!pField->IsStatic && curMethod->Signature().IsStatic) {
                ScriptUtils::FailE("Static methods can't access instance fields.", identExpr);
            }

            // ***********************************************************************************************************
            if(curMethod->MethodKind() == E_METHODKIND_DTOR
            && !(curMethod->Flags() & E_METHODFLAGS_IS_UNSAFE)
            && !isAssignmentLValue
            && pField->Type.ResolvedClass == curMethod->DeclaringClass())
            {
                ScriptUtils::FailE(domain->FormatMessage("Field '%C::%s' may contain 'this' which can escape the destructor and become a zombie after a garbage collection, which is inherently unsafe. "
                                                         "Mark the destructor 'unsafe' to allow such behavior at your own risk.",
                                                         pField->Type.ResolvedClass,
                                                         &identExpr->Name),
                                   identExpr);
            }
            // ***********************************************************************************************************

            // The closure refers to a field of one of enclosing methods' declaring classes.
            if(!pField->IsStatic && (pField->DeclaringClass != curMethod->DeclaringClass())) {
                // Now, we need to find the parent method that corresponds to the found declaring class in the closure chain.
                CMethod* parentMethod = curMethod;
            again1:
                parentMethod = parentMethod->ParentMethod();
                SKIZO_REQ_PTR(parentMethod);

                // NOTE All closures between declClosure and useClosure are forced to have closure environments.
                // This allows for data chaining even if some closures in the middle have no
                // captured data whatsoever (which generally doesn't initiate closure env construction).
                addClosureEnvUpper(parentMethod);

                // Don't forget to put "self" to the closure environment when the parent method begins.
                // NOTE All closures between declClosure and useClosure are forced to have closure environments
                // and "self" captured. This allows for data chaining even if some closures in the middle have no
                // captured data whatsoever (which generally doesn't initiate closure env construction).
                parentMethod->Flags() |= E_METHODFLAGS_IS_SELF_CAPTURED;

                // TODO don't capture "self" for intermediate methods?
                addClosureEnvSelfField(parentMethod);

                if(parentMethod->DeclaringClass() != pField->DeclaringClass) {
                    goto again1;
                }
            }
        }

        if(resolvedIdent.IsVoid()) {
            ScriptUtils::FailE(domain->FormatMessage("Attempt to use untyped variable, unknown class or unknown static method '%s'.",
                                                     &identExpr->Name),
                               identExpr);
        } else {
            // ******************************************************************************************************
            // An extension method may be calling a private const of the patched class => disallowed.
            if(resolvedIdent.EType == E_RESOLVEDIDENTTYPE_CONST) {
                if(!resolvedIdent.AsConst_->IsAccessibleFromMethod(curMethod)) {
                    ScriptUtils::FailCnst(domain->FormatMessage("Can't access a non-public const '%C::%s' from class '%C'.",
                                                                 resolvedIdent.AsConst_->DeclaringClass,
                                                                 &resolvedIdent.AsConst_->Name,
                                                                 curMethod->DeclaringClass()),
                                          resolvedIdent.AsConst_);
                }
            }
            // ******************************************************************************************************

            identExpr->ResolvedIdent = resolvedIdent;
            identExpr->InferredType = resolvedIdent.Type();
            if(!this->ResolveTypeRef(identExpr->InferredType)) {
                ScriptUtils::FailE(domain->FormatMessage("Identifier '%s' of unknown type '%T'.",
                                                         &identExpr->Name, &identExpr->InferredType),
                                   identExpr);
            }
        }
    } else {
        // The ident is typed in this case.

        // Check if it was already typed.
        CLocal* prevTypedLocal (curMethod->LocalByName(identExpr->Name));
        if(prevTypedLocal) {
            this->ResolveTypeRef(identExpr->TypeAsInCode);
            ScriptUtils::FailE(domain->FormatMessage("Trying to retype variable '%s' from '%T' to '%T'.",
                                                     &identExpr->Name, &prevTypedLocal->Type, &identExpr->TypeAsInCode),
                               identExpr);
        }

        if(identExpr->IsAuto) {
            // Expects inferAssignmentExpr to deal with it correctly.
            if(!this->ResolveTypeRef(identExpr->TypeAsInCode)) {
                ScriptUtils::FailE(domain->FormatMessage("Identifier '%s' of unknown type '%T'.",
                                                         &identExpr->Name, &identExpr->TypeAsInCode),
                                   identExpr);
            }
            identExpr->InferredType = identExpr->TypeAsInCode;
        } else {
            // The ident.
            if(!this->ResolveTypeRef(identExpr->TypeAsInCode)) {
                ScriptUtils::FailE(domain->FormatMessage("Identifier '%s' of unknown type '%T'.",
                                                         &identExpr->Name, &identExpr->TypeAsInCode),
                                   identExpr);
            }

            curMethod->NewLocal(identExpr->Name, identExpr->TypeAsInCode);
            identExpr->InferredType = identExpr->TypeAsInCode;
        }
    }
}

void STransformer::inferIntConstExpr(CIntegerConstantExpression* intConstExpr)
{
    intConstExpr->InferredType.SetPrimType(E_PRIMTYPE_INT);
    bool b = this->ResolveTypeRef(intConstExpr->InferredType);
    (void)b;
    SKIZO_REQ(b, EC_INVALID_STATE);
}

void STransformer::inferFloatConstExpr(CFloatConstantExpression* floatConstExpr)
{
    floatConstExpr->InferredType.SetPrimType(E_PRIMTYPE_FLOAT);
    bool b = this->ResolveTypeRef(floatConstExpr->InferredType);
    (void)b;
    SKIZO_REQ(b, EC_INVALID_STATE);
}

void STransformer::inferBoolConstExpr(CBoolConstantExpression* boolConstExpr)
{
    boolConstExpr->InferredType.SetPrimType(E_PRIMTYPE_BOOL);
    bool b = this->ResolveTypeRef(boolConstExpr->InferredType);
    (void)b;
    SKIZO_REQ(b, EC_INVALID_STATE);
}

void STransformer::inferStringLitExpr(CStringLiteralExpression* stringLitExpr)
{
    stringLitExpr->InferredType = domain->StringClass()->ToTypeRef();

    // IMPORTANT!
    stringLitExpr->SkizoObject = domain->InternStringLiteral(stringLitExpr->StringValue);
}

void STransformer::inferCharLitExpr(CCharLiteralExpression* charLitExpr)
{
    charLitExpr->InferredType = domain->CharClass()->ToTypeRef();
}

// A closure/anonymous method is basically an ad-hoc implementation of the parent method class
// which overrides the virtual "invoke" function.
// IMPORTANT Do not change the layout!
void STransformer::inferClosureExpr(CBodyExpression* closureExpr, STypeRef& closureType)
{
    closureExpr->InferredType = closureType;
    bool b = this->ResolveTypeRef(closureExpr->InferredType);
    (void)b;
    SKIZO_REQ(b, EC_INVALID_STATE);
    SKIZO_REQ_EQUALS(closureExpr->InferredType.ResolvedClass->SpecialClass(), E_SPECIALCLASS_METHODCLASS);

    // ********************
    //   Creates a class.
    // ********************

    Auto<CClass> klass (new CClass(domain));
    CClass* pClass = klass;
    EClassFlags& classFlags = pClass->Flags();
    classFlags |= E_CLASSFLAGS_IS_COMPGENERATED;
    // VTables for closures are generated outside of the baseline C compiler in closure creation thunks.
    classFlags &= ~E_CLASSFLAGS_EMIT_VTABLE;
    classFlags |= E_CLASSFLAGS_FREE_VTABLE;
    pClass->SetSpecialClass(E_SPECIALCLASS_METHODCLASS);

    // ***************************************************************************************
    // Assigns the file path and the line number, borrowing it from the closureExpr.
    // First, it's useful for printing nice errors, but, most IMPORTANTly, closures can access
    // internal members defined in the same module.
    // ***************************************************************************************
    pClass->Source() = closureExpr->Source;

    // *********************
    //   Creates the name.
    // *********************

    Auto<const CString> generatedName (CString::Format("0Closure_%d", domain->NewUniqueId()));
    pClass->SetFlatName(domain->NewSlice(generatedName));

    // ***********************
    //   Assigns its parent.
    // ***********************

    b = this->ResolveTypeRef(closureType);
    SKIZO_REQ(b, EC_INVALID_STATE);
    pClass->SetBaseClass(closureType);

    // *************************************************************************
    //   Creates the most important field of a closure: its closure environment.
    //   NOTE: the environment can always be null if the closure doesn't
    //   reference anything outside of its scope.
    // *************************************************************************

    {
        // The type of "_soX_env" is not known at this stage, so we type it as "any".
        // "Any" is used instead of intptr because "any" is garbage-collectible.
        SStringSlice anyClassName (domain->NewSlice("any"));

        Auto<CField> envField (new CField());
        CField* pEnvField = envField;
        pEnvField->DeclaringClass = klass;
        pEnvField->Name = domain->NewSlice("_soX_env");
        pEnvField->Type.SetObject(anyClassName);
        pClass->RegisterInstanceField(envField);
    }

    // *****************************************************
    //   MethodClass::m_codeOffset
    // used to remember the result of Marshal::codeOffset
    // *****************************************************
    {
        Auto<CField> codeOffsetField (new CField());
        codeOffsetField->DeclaringClass = klass;
        codeOffsetField->Name = domain->NewSlice("m_codeOffset");
        codeOffsetField->Type.SetPrimType(E_PRIMTYPE_INTPTR);
        klass->RegisterInstanceField(codeOffsetField);
    }

    // *****************************
    //   Creates the constructor.
    // *****************************

    Auto<CMethod> ctor (new CMethod(klass));
    CMethod* pCtor = ctor;
    pCtor->SetMethodKind(E_METHODKIND_CTOR);
    pCtor->SetName(domain->NewSlice("create"));
    pCtor->Signature().ReturnType = pClass->ToTypeRef();
    {
        Auto<CParam> param (new CParam());
        param->Name = domain->NewSlice("_env");
        // The param type is void* to remove some warnings about type punning we employ.
        param->Type.SetPrimType(E_PRIMTYPE_INTPTR);
        pCtor->Signature().Params->Add(param);
    }
    // The constructor will be emitted by the ThunkManager.
    pCtor->SetSpecialMethod(E_SPECIALMETHOD_CLOSURE_CTOR);
    domain->ThunkManager().AddMethod(pCtor);

    pClass->RegisterInstanceCtor(ctor);

    // *******************************************************************
    //   Creates the new and only method by cloning the anonymous method
    //   defined in the body (to be on the safe side).
    // *******************************************************************

    SKIZO_REQ_PTR(closureExpr->Method);
    SKIZO_REQ_PTR(closureExpr->Method->ParentMethod());

    Auto<CMethod> invokeMethod (closureExpr->Method->Clone());
    klass->SetInvokeMethod(invokeMethod);
    invokeMethod->SetDeclaringClass(klass);         // anonymous methods are classless
    // ***
    invokeMethod->Source() = closureExpr->Source;
    // ***
    invokeMethod->SetParentMethod(curMethod);
    invokeMethod->SetName(domain->NewSlice("invoke"));
    invokeMethod->SetExpression(closureExpr); // closureExpr->Method doesn't store the closureExpr
    invokeMethod->Flags() &= ~E_METHODFLAGS_IS_ANONYMOUS;
    pClass->RegisterInstanceMethod(invokeMethod);

    // ***********************************
    //   Adds it to the list of classes.
    // ***********************************

    domain->RegisterClass(klass);

    // IMPORTANT enqueues the newly created class to the processing queue so that everything was inferred
    // there too.
    classesToProcess->Enqueue(klass);

    closureExpr->GeneratedClosureClass = klass;
}

// Inserts explicit casts+some other voodoo from implicit cast information, as the emitter only works with explicit casts.
// NOTE it's the responsibility of this function to infer generated exprs
CExpression* STransformer::insertImplicitConversionIfAny(CExpression* inputExpr,
                                                         const SCastInfo& castInfo,
                                                         STypeRef& targetType)
{
    switch(castInfo.CastType) {
        case E_CASTTYPE_BOX:
        {
            Auto<CCastExpression> castExpr (new CCastExpression(targetType));
            castExpr->IsEmpty = false;
            SKIZO_REQ_PTR(targetType.ResolvedClass);
            castExpr->CastInfo = castInfo;
            castExpr->Expr.SetVal(inputExpr);
            castExpr->Ref();

            // ********************************************
            // Forces the domain to generate a new wrapper.
            // (or get a previously generated one).
            // ********************************************
            CClass* boxedClass = domain->BoxedClass(inputExpr->InferredType);

            if(!boxedClass->IsInferred()) {
                classesToProcess->Enqueue(boxedClass);
            }

            return castExpr;
        }
        break;

        case E_CASTTYPE_VALUE_TO_FAILABLE:
        case E_CASTTYPE_ERROR_TO_FAILABLE:
        {
            // "Value => result struct" implicit conversion.
            // Constructs a call in the form "%RESULT_STRUCT_NAME% createFromValue $inputExpr$".
            //
            // OR
            //
            // "Error => result struct" implicit conversion.
            // Constructs a call in the form "%RESULT_STRUCT_NAME% createFromError $inputExpr$".

            Auto<CCallExpression> callExpr (new CCallExpression());
            Auto<CIdentExpression> classNameExpr (new CIdentExpression(targetType.ResolvedClass->FlatName()));
            Auto<CIdentExpression> ctorNameExpr(new CIdentExpression(
                                                    domain->NewSlice(castInfo.CastType == E_CASTTYPE_ERROR_TO_FAILABLE?
                                                                                        "createFromError":
                                                                                        "createFromValue")));
            callExpr->Exprs->Add(classNameExpr);
            callExpr->Exprs->Add(ctorNameExpr);
            callExpr->Exprs->Add(inputExpr);

            inferCallExpr(callExpr);

            callExpr->Ref();
            return callExpr;
        }
        break;

        default:
            // No implicit cast needs to be injected => just return the input expression.
            inputExpr->Ref();
            return inputExpr;
    }
}

CExpression* STransformer::insertFailableCtorFromNullValueNoInfer(STypeRef& targetType)
{
    SKIZO_REQ_PTR(targetType.ResolvedClass);

    Auto<CCallExpression> callExpr (new CCallExpression());
    Auto<CIdentExpression> classNameExpr (new CIdentExpression(targetType.ResolvedClass->FlatName()));
    Auto<CIdentExpression> ctorNameExpr (new CIdentExpression(domain->NewSlice("createFromValue")));
    Auto<CNullConstantExpression> nullExpr (new CNullConstantExpression());
    callExpr->Exprs->Add(classNameExpr);
    callExpr->Exprs->Add(ctorNameExpr);
    callExpr->Exprs->Add(nullExpr);

    callExpr->Ref();
    return callExpr;
}

void STransformer::inferIdentCompExpr(CIdentityComparisonExpression* identCompExpr)
{
    inferValueExpr(identCompExpr->Expr1, 1);
    inferValueExpr(identCompExpr->Expr2, 1);

    // Support for null.
    if(identCompExpr->Expr1->Kind() == E_EXPRESSIONKIND_NULLCONSTANT) {
        identCompExpr->Expr1->InferredType = identCompExpr->Expr2->InferredType;
    } else if(identCompExpr->Expr2->Kind() == E_EXPRESSIONKIND_NULLCONSTANT) {
        identCompExpr->Expr2->InferredType = identCompExpr->Expr1->InferredType;
    }

    if(identCompExpr->Expr1->InferredType.IsVoid() || identCompExpr->Expr2->InferredType.IsVoid()) {
        ScriptUtils::FailE("Arguments of the identity comparison aren't inferable.", identCompExpr);
    }

    if(!identCompExpr->Expr1->InferredType.Equals(identCompExpr->Expr2->InferredType)) {
        ScriptUtils::FailE(domain->FormatMessage("Arguments of the identity comparison aren't of the same type ('%T' vs. '%T').",
                                                 &identCompExpr->Expr1->InferredType,
                                                 &identCompExpr->Expr2->InferredType),
                           identCompExpr);
    }

    CClass* klass = identCompExpr->Expr1->InferredType.ResolvedClass;

    // Primitive objects and reference types are compared using C's == while
    // valuetypes have to be compared using special helper code.
    if(klass->IsValueType() && klass->PrimitiveType() == E_PRIMTYPE_OBJECT) {
        domain->IdentityComparisonHelpers()->Set(klass->FlatName(), klass);
    }

    identCompExpr->InferredType.SetPrimType(E_PRIMTYPE_BOOL);
    ResolveTypeRef(identCompExpr->InferredType);
}

void STransformer::inferIsExpr(CIsExpression* isExpr)
{
    if(!ResolveTypeRef(isExpr->TypeAsInCode)) {
        ScriptUtils::FailE(domain->FormatMessage("'is' expression compares with unknown type '%T'.",
                                                 &isExpr->TypeAsInCode),
                           isExpr);
    }

    inferValueExpr(isExpr->Expr);
    ResolveTypeRef(isExpr->Expr->InferredType);

    isExpr->InferredType.SetPrimType(E_PRIMTYPE_BOOL);
    ResolveTypeRef(isExpr->InferredType);
}

void STransformer::inferAbortExpr(CAbortExpression* abortExpr)
{
    inferValueExpr(abortExpr->Expr, 1);
    ResolveTypeRef(abortExpr->Expr->InferredType);

    if(abortExpr->Expr->InferredType.ResolvedClass != domain->StringClass()) {
        ScriptUtils::FailE("Abort expression expects a string argument.", abortExpr);
    }
}

void STransformer::inferAssertExpr(CAssertExpression* assertExpr)
{
    inferValueExpr(assertExpr->Expr, 1);
    ResolveTypeRef(assertExpr->Expr->InferredType);

    if(assertExpr->Expr->InferredType.ResolvedClass != domain->BoolClass()) {
        ScriptUtils::FailE("Assert expression expects a bool argument.", assertExpr);
    }
}

void STransformer::inferRefExpr(CRefExpression* refExpr)
{
    static const char* refExprError = "A ref expression can only take an address of a local variable, a param, a field or a valuetype 'this'.";

    if(!curMethod->IsUnsafe()) {
        ScriptUtils::FailE("Ref expressions allowed only in unsafe contexts.", refExpr);
    }

    inferValueExpr(refExpr->Expr);

    if(refExpr->Expr->Kind() == E_EXPRESSIONKIND_IDENT
    || (refExpr->Expr->Kind() == E_EXPRESSIONKIND_THIS
        && refExpr->Expr->InferredType.ResolvedClass->IsValueType()))
    {
        if(refExpr->Expr->Kind() == E_EXPRESSIONKIND_IDENT) {
            SResolvedIdentType resolvedIdent (curMethod->ResolveIdent(static_cast<CIdentExpression*>(refExpr->Expr.Ptr())->Name, true));

            switch(resolvedIdent.EType) {
                case E_RESOLVEDIDENTTYPE_FIELD:
                case E_RESOLVEDIDENTTYPE_LOCAL:
                case E_RESOLVEDIDENTTYPE_PARAM:
                    // OK
                    break;

                default:
                    ScriptUtils::FailE(refExprError, refExpr);
            }
        }

        refExpr->InferredType.SetPrimType(E_PRIMTYPE_INTPTR);
        bool b = ResolveTypeRef(refExpr->InferredType);
        (void)b;
        SKIZO_REQ(b, EC_INVALID_STATE);
    } else {
        ScriptUtils::FailE(refExprError, refExpr);
    }
}

void STransformer::inferAssignmentExpr(CAssignmentExpression* assExpr)
{
    // An assignment expr is verified to have at least 3 elements during the parsing phase because it's agrammatical
    // to have more than/less than 3 elements in an assExpr.

    inferValueExpr(assExpr->Expr1, -1, true);
    inferValueExpr(assExpr->Expr2, 1);

    //SKIZO_REQ_PTR(assExpr->Expr2->InferredType.ResolvedClass);

    {
        CIdentExpression* lValueExpr = static_cast<CIdentExpression*>(assExpr->Expr1.Ptr());

        if(!lValueExpr->IsAuto) {
            SResolvedIdentType resolvedLValue (curMethod->ResolveIdent(lValueExpr->Name));

            if(resolvedLValue.EType == E_RESOLVEDIDENTTYPE_CONST) {
                ScriptUtils::FailE("Const values are immutable.", assExpr->Expr1);
            }

            // ******************************************************************************
            if((resolvedLValue.EType == E_RESOLVEDIDENTTYPE_FIELD)
            && curMethod->MethodKind() != E_METHODKIND_CTOR
            && curMethod->DeclaringClass()->IsValueType()) {
                ScriptUtils::FailE("Valuetypes are immutable, fields can only be changed in constructors.", assExpr->Expr1);
            }
            // ******************************************************************************
            lValueExpr->ResolvedIdent = resolvedLValue;

            if(resolvedLValue.IsVoid()) {
                ScriptUtils::FailE("Left value of the assignment is an unknown local, field or param.", assExpr->Expr1);
            }
            if(resolvedLValue.EType == E_RESOLVEDIDENTTYPE_CLASS) {
                ScriptUtils::FailE("Left value of the assignment can't be a class reference.", assExpr->Expr1);
            }
        }
        if(lValueExpr->IsAuto) {
            if(assExpr->Expr2->InferredType.IsVoid()) {
                ScriptUtils::FailE("Can't infer the rvalue of the assignment (auto).", assExpr->Expr2);
            }

            // Auto locals are registered here.
            lValueExpr->TypeAsInCode = lValueExpr->InferredType = assExpr->Expr2->InferredType;

            lValueExpr->ResolvedIdent.EType = E_RESOLVEDIDENTTYPE_LOCAL;
            lValueExpr->ResolvedIdent.AsLocal_ = curMethod->NewLocal(lValueExpr->Name, lValueExpr->TypeAsInCode);
        }

        // What if types between the callTargetExpr (leftValue) and rValue don't match?
        const SCastInfo castInfo (assExpr->Expr1->InferredType.GetCastInfo(assExpr->Expr2->InferredType));
        // NOTE It's always false if one of the exprs is auto-inferrable from the context. We skip to the next "else" in that case.
        if(castInfo.IsCastable) {
            if(castInfo.DoesRequireExplicitCast()) {
                ScriptUtils::FailE(domain->FormatMessage("Implicit downcast from '%T' to '%T' in assignment.",
                                                         &assExpr->Expr2->InferredType,
                                                         &assExpr->Expr1->InferredType),
                                   assExpr);
            }

            assExpr->Expr2.SetPtr(insertImplicitConversionIfAny(assExpr->Expr2, castInfo, assExpr->Expr1->InferredType));
        } else {
            if((assExpr->Expr2->Kind() == E_EXPRESSIONKIND_NULLCONSTANT) && assExpr->Expr1->InferredType.IsNullAssignable()) {
                // It's actually OK if leftValue is a heap object and the right value is null constant,
                // even if their actual InferredType's don't match.

                // ************************************************************************
                // A null is assigned to a failable struct, like this:
                //    i: int? = null;
                // This syntax requires an implicit call to Failable::createFromValue(..)
                //
                // NOTE this code is not part of GetCastInfo codepath because "null" has no type to cast from
                if(assExpr->Expr1->InferredType.IsFailableStruct()) {
                    // Same as above, but with failables (except we use 0Result_%d_createFromValue(0) there)
                    assExpr->Expr2.SetPtr(insertFailableCtorFromNullValueNoInfer(assExpr->Expr1->InferredType));
                    inferCallExpr(static_cast<CCallExpression*>(assExpr->Expr2.Ptr()));
                }
                // ************************************************************************
           } else if((assExpr->Expr2->Kind() == E_EXPRESSIONKIND_ARRAYCREATION) && assExpr->Expr1->InferredType.IsArrayClass()) {

                // FIX for "a: [int]? = (array 10);"
                // See a similar failable correction code just above: injects Failable::createFromValue(..)
                // This piece of code is problematic because "array creation" is auto-inferrable. Other stuff like "a: [int]? = ..." doesn't require it.
                if(assExpr->Expr1->InferredType.IsFailableStruct()) {
                    // NOTE insertImplicitConversionIfAny automatically infers stuff, so no inferArrayCreationExpr(..)
                    assExpr->Expr2.SetPtr(insertImplicitConversionIfAny(assExpr->Expr2,
                                                                        SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                                        assExpr->Expr1->InferredType));
                } else {
                    // If the leftValue is an array class and the right value is an array creation expression...
                    inferArrayCreationExpr(static_cast<CArrayCreationExpression*>(assExpr->Expr2.Ptr()), assExpr->Expr1->InferredType);
                }

           } else if((assExpr->Expr2->Kind() == E_EXPRESSIONKIND_BODY) && assExpr->Expr1->InferredType.IsMethodClass()) {
                // If the leftValue is a method class and the right value is an anonymous method, compare their signatures.
                CBodyExpression* closureExpr = static_cast<CBodyExpression*>(assExpr->Expr2.Ptr());
                SKIZO_REQ_PTR(closureExpr->Method);

                // FIX for "a: Action? = ^{}"
                // Same fix as for array creation above.
                if(assExpr->Expr1->InferredType.IsFailableStruct()) {
                    // NOTE insertImplicitConversionIfAny automatically infers stuff, so no inferArrayCreationExpr(..)
                    assExpr->Expr2.SetPtr(insertImplicitConversionIfAny(assExpr->Expr2,
                                                                        SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                                        assExpr->Expr1->InferredType));
                }
                else {
                    if(!assExpr->Expr1->InferredType.ResolvedClass->IsMethodClassCompatibleSig(closureExpr->Method)) {
                        ScriptUtils::FailE(domain->FormatMessage("Type mismatch in assignment: closure signature not compatible with method class '%T'.",
                                                                 &assExpr->Expr1->InferredType),
                                           assExpr);
                    }

                    inferClosureExpr(closureExpr, assExpr->Expr1->InferredType);
                }
            } else {
                ScriptUtils::FailE(domain->FormatMessage("Type mismatch in assignment: expected '%T', found '%T'.",
                                                         &assExpr->Expr1->InferredType,
                                                         &assExpr->Expr2->InferredType),
                                   assExpr);
            }
        }
    }
}

CExpression* STransformer::inferCallExpr(CCallExpression* callExpr, bool canInlineBranches)
{
    // A callExpr is verified to have at least 2 elements during the parsing phase because it's agrammatical
    // to have 1 element in a callExpr.

    // ****************************
    // Infers argument types first.
    // ****************************

    for(int i = 0; i < callExpr->Exprs->Count(); i++) {
        CExpression* subExpr = callExpr->Exprs->Array()[i];

        if(i == 1) {
            SKIZO_REQ((subExpr->Kind() == E_EXPRESSIONKIND_IDENT) || (subExpr->Kind() == E_EXPRESSIONKIND_STRINGLITERAL), EC_INVALID_STATE);

            // *************************************************
            //   Inlining intrinsics for conditionals & loops.
            // *************************************************

            // NOTE Has the first element already inferred to see that the type we're looking for is indeed "bool" or "Range"
            if(canInlineBranches
            && domain->InlineBranching()
            && (callExpr->Exprs->Count() == 3))
            {
                 // *********************
                 //   InlinedCondition
                 // *********************

                 if((callExpr->Exprs->Item(0)->InferredType.ResolvedClass == domain->BoolClass())
                 && (subExpr->Kind() == E_EXPRESSIONKIND_IDENT)
                 && (static_cast<CIdentExpression*>(subExpr))->Name.EqualsAscii("then")
                 && (callExpr->Exprs->Item(2)->Kind() == E_EXPRESSIONKIND_BODY)
                 && static_cast<CBodyExpression*>(callExpr->Exprs->Item(2))->IsCastableToAction())
                {
                    CInlinedConditionExpression* inlinedCondExpr = new CInlinedConditionExpression();
                    inlinedCondExpr->IfCondition.SetVal(callExpr->Exprs->Item(0));
                    inlinedCondExpr->Body.SetVal(static_cast<CBodyExpression*>(callExpr->Exprs->Item(2)));
                    // IfCondition already inferred; the body wasn't yet.
                    inferBodyStatements(inlinedCondExpr->Body, nullptr, true);
                    return inlinedCondExpr;
                }
            }

            // ************************

        } else {
            inferValueExpr(subExpr, i);
        }
    }

    // ***********************************************************************************
    // After argument types have been inferred, we can now verify them against the method.
    // ***********************************************************************************

    CExpression* callTargetExpr = callExpr->Exprs->Item(0);
    if(!callTargetExpr->InferredType.ResolvedClass) {
        if(callTargetExpr->Kind() == E_EXPRESSIONKIND_BODY) {
            ScriptUtils::FailE("Cannot correctly infer the method class of the anonymous method.", callTargetExpr);
        } else {
            ScriptUtils::FailE("Can't infer the type of the target object.", callTargetExpr);
            //SKIZO_REQ_PTR(callTargetExpr->InferredType.ResolvedClass);
        }
    }

    CClass* pCallTargetClass = callTargetExpr->InferredType.ResolvedClass;

    // It was verified earlier that the second argument is a method name (or a const name).
    CIdentExpression* targetMethodExpr = static_cast<CIdentExpression*>(callExpr->Exprs->Item(1));
    CMethod* targetMethod;

    // **************************************************
    if((callTargetExpr->Kind() == E_EXPRESSIONKIND_IDENT)
    && (static_cast<CIdentExpression*>(callTargetExpr)->ResolvedIdent.EType == E_RESOLVEDIDENTTYPE_CLASS)) {

        // **************
        // Static method.
        // **************

        // *****************************************************************************************************
        //   Is it a const? (consts have a grammar similar to static methods, hence we have it slapped in here
        CConst* targetConst = pCallTargetClass->MyConst(targetMethodExpr->Name);
        if(targetConst) {
            if(!targetConst->IsAccessibleFromMethod(curMethod)) {
                ScriptUtils::FailE(domain->FormatMessage("Can't access a non-public const '%C::%s' from class '%C'.",
                                                            targetConst->DeclaringClass,
                                                            &targetConst->Name,
                                                            curMethod->DeclaringClass()),
                                      targetMethodExpr);
            }

            // FIX
            // I'm not sure exactly why it is so... Otherwise, the const isn't correctly inferred.
            if(!domain->ResolveTypeRef(targetConst->Type)) {
                ScriptUtils::FailCnst(domain->FormatMessage("Const of unknown type '%T'.", &targetConst->Type),
                                      targetConst);
            }

            SKIZO_REQ_PTR(targetConst->Type.ResolvedClass);
            callExpr->InferredType = targetConst->Type;

            // *************************
            callExpr->CallType = E_CALLEXPRESSION_CONSTACCESS;
            callExpr->uTargetConst = targetConst;
            // *************************

            return nullptr;
        } else {
            // *****************************************************************************************************

            // Checks if a static call.
            targetMethod = pCallTargetClass->StaticMethodOrCtor(targetMethodExpr->Name);
            if(!targetMethod) {
                ScriptUtils::FailE(domain->FormatMessage("Specified static method, constructor or const '%s' not found.",
                                                         &targetMethodExpr->Name),
                                   targetMethodExpr);
            }
            targetMethod->Flags() |= E_METHODFLAGS_WAS_EVER_CALLED;

            if(pCallTargetClass->IsAbstract() && (targetMethod->MethodKind() == E_METHODKIND_CTOR)) {
                ScriptUtils::FailE(domain->FormatMessage("Can't instantiate abstract class '%C'.",
                                                         pCallTargetClass),
                                   callExpr);
            }

            // **************************************************************************************************************
            // "Marshal" is a special class which is allowed only in unsafe contexts, because it deals with untyped pointers.
            CClass* pClassTarget = static_cast<CIdentExpression*>(callTargetExpr)->ResolvedIdent.AsClass_;

            if(pClassTarget->FlatName().EqualsAscii("Marshal")) {
                bool isMarshalCallAllowed = curMethod->IsUnsafe()                                                      // it is safe to call "Marshal" if the method is marked unsafe
                                         && (domain->IsTrusted()                                                       // and the domain is trusted
                                         || (curMethod->Source().Module && curMethod->Source().Module->IsBaseModule)); // or the current module is a base module

                if(!isMarshalCallAllowed) {
                    ScriptUtils::FailE("Special class 'Marshal' allowed only in unsafe contexts in trusted domains or in base modules.", callExpr);
                }
            }
            // **************************************************************************************************************

            // *************************
            callExpr->CallType = E_CALLEXPRESSION_METHODCALL;
            callExpr->uTargetMethod = targetMethod;
            // *************************

        }

    } else {

        // ****************
        // Instance method.
        // ****************

        targetMethod = pCallTargetClass->MyMethod(targetMethodExpr->Name,
                                                  false, // look for instance methods only
                                                  E_METHODKIND_NORMAL);
        if(!targetMethod) {
            ScriptUtils::FailE(domain->FormatMessage("Specified instance method '%C::%s' not found.",
                                                     pCallTargetClass, &targetMethodExpr->Name),
                               targetMethodExpr);
        }

        // NOTE aids in VCH generation for abstract methods that are never overriden
        targetMethod->Flags() |= E_METHODFLAGS_WAS_EVER_CALLED;

        // "MyMethod" may resolve method names into different names. For example, "+" operator is resolved to "op_add".
        // Later, the emitter is to accept "op_add" only. This is why we fix up the ident expression here.
        targetMethodExpr->Name = targetMethod->Name();

        callExpr->CallType = E_CALLEXPRESSION_METHODCALL;
        callExpr->uTargetMethod = targetMethod;
    }

    // ************************
    // Checks access modifiers.
    // ************************

    if(!targetMethod->IsAccessibleFromMethod(curMethod)) {
        ScriptUtils::FailE(domain->FormatMessage("Can't access non-public method '%C::%s' from class '%C'.",
                                                 targetMethod->DeclaringClass(),
                                                 &targetMethod->Name(),
                                                 curMethod->DeclaringClass()),
                           targetMethodExpr);
    }

    // *************************************
    // ECalls are banned from safe contexts.
    // *************************************

    targetMethod->ResolveECallAttributes(); // may have not been resolved yet
    if(targetMethod->ECallDesc().IsValid() && !curMethod->IsUnsafe()) {
        ScriptUtils::FailE("ECalls allowed only in unsafe contexts.", targetMethodExpr);
    }

    // ************************

    CArrayList<CParam*>* params = targetMethod->Signature().Params;

    // ************************
    //   Validates arguments.
    // ************************
    {
        const int passedArgCount = (callExpr->Exprs->Count() - 2); // minus object and the name of the method
        if(passedArgCount != params->Count()) {
            ScriptUtils::FailE(domain->FormatMessage("Argument count mismatch in call to '%C::%s': expected '%d', found '%d'.",
                                                     targetMethod->DeclaringClass(),
                                                     &targetMethod->Name(),
                                                     params->Count(),
                                                     passedArgCount),
                               targetMethodExpr);
        }
    }

    for(int i = 0; i < params->Count(); i++) {
        CExpression* argExpr = callExpr->Exprs->Item(i + 2);
        CParam* param = params->Array()[i];

        if(!this->ResolveTypeRef(param->Type)) {
            ScriptUtils::FailL(domain->FormatMessage("Parameter '%s' of unknown type '%T'.", &param->Name, &param->Type),
                               param);
        }

        const SCastInfo castInfo (param->Type.GetCastInfo(argExpr->InferredType));

        if(castInfo.IsCastable) {

            if(castInfo.DoesRequireExplicitCast()) {
                ScriptUtils::FailE(domain->FormatMessage("Implicit downcast from '%T' to '%T' in argument passing (requires an explicit cast).",
                                                         &argExpr->InferredType,
                                                         &param->Type),
                                   targetMethodExpr);
            }
            argExpr = insertImplicitConversionIfAny(argExpr, castInfo, param->Type);
            callExpr->Exprs->Set(i + 2, argExpr);
            argExpr->Unref();

        } else {

            // It's ok to pass a null constant instead of a parameter typed as "heap class".
            if(param->Type.IsNullAssignable() && (argExpr->Kind() == E_EXPRESSIONKIND_NULLCONSTANT)) {
                // Do nothing.

                // See a similar code above in this same method (in the assignment section) for more info.
                if(param->Type.IsFailableStruct()) {
                    argExpr = insertFailableCtorFromNullValueNoInfer(param->Type);
                    callExpr->Exprs->Set(i + 2, argExpr);
                    argExpr->Unref();
                    inferCallExpr(static_cast<CCallExpression*>(argExpr));
                }

            } else if((argExpr->Kind() == E_EXPRESSIONKIND_ARRAYCREATION) && param->Type.IsArrayClass()) {

                // FIX a: [int]?  <=> (array 10);
                // See the corresponding section of inferAssignmentExpr(..) for more info.
                if(param->Type.IsFailableStruct()) {
                    argExpr = insertImplicitConversionIfAny(argExpr,
                                                            SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                            param->Type);
                    callExpr->Exprs->Set(i + 2, argExpr);
                    argExpr->Unref();
                } else {
                    // If the leftValue is an array class and the right value is an array creation expression...
                    inferArrayCreationExpr(static_cast<CArrayCreationExpression*>(argExpr), param->Type);
                }

            } else if((argExpr->Kind() == E_EXPRESSIONKIND_BODY) && param->Type.IsMethodClass()) {

                // If the leftValue is a method class and the right value is an anonymous method, compare their signatures.
                CBodyExpression* closureExpr = static_cast<CBodyExpression*>(argExpr);
                SKIZO_REQ_PTR(closureExpr->Method);

                // FIX for "a: Action? <=> ^{}"
                // Same fix as for array creation above.
                if(param->Type.IsFailableStruct()) {
                    // NOTE insertImplicitConversionIfAny automatically infers stuff, so no inferArrayCreationExpr(..)
                    argExpr = insertImplicitConversionIfAny(argExpr,
                                                            SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                            param->Type);
                    callExpr->Exprs->Set(i + 2, argExpr);
                    argExpr->Unref();
                }
                else {
                    if(!param->Type.ResolvedClass->IsMethodClassCompatibleSig(closureExpr->Method)) {
                        ScriptUtils::FailE(domain->FormatMessage("Type mismatch for argument '%d' of call to '%C::%s': closure signature not compatible with method class '%T'.",
                                                                 i,
                                                                 targetMethod->DeclaringClass(),
                                                                 &targetMethod->Name(),
                                                                 &param->Type),
                                           callExpr);
                    }

                    inferClosureExpr(closureExpr, param->Type);
                }

            } else {
                ScriptUtils::FailE(domain->FormatMessage("Argument type mismatch for argument '%d' of call to '%C::%s': expected '%T', found '%T'.",
                                                         i,
                                                         targetMethod->DeclaringClass(),
                                                         &targetMethod->Name(),
                                                         &param->Type,
                                                         &argExpr->InferredType),
                                   argExpr);
            }
        }
    }

    callExpr->InferredType = targetMethod->Signature().ReturnType;
    if(!this->ResolveTypeRef(callExpr->InferredType)) {
        ScriptUtils::FailE("Unknown type declared for call expression's return.", callExpr);
    }

    return nullptr;
}

void STransformer::inferCastExpr(CCastExpression* castExpr)
{
    CExpression* inputValueExpr = castExpr->Expr;
    SKIZO_REQ_PTR(inputValueExpr);

    inferValueExpr(inputValueExpr, 1);

    if(!this->ResolveTypeRef(castExpr->InferredType)) {
        ScriptUtils::FailE(domain->FormatMessage("Trying to cast to unknown type '%T'.", &castExpr->InferredType),
                           castExpr);
    }

    if(inputValueExpr->Kind() == E_EXPRESSIONKIND_NULLCONSTANT && castExpr->InferredType.IsNullAssignable()) {

        // Do nothing.
        // NOTE Reports null constants as non-castable so that we skipped C cast insertion at all
        // (see SEmitter::emitCastExpr)

        // ***********************************************************************************
        // See a similar codepath in inferCallExpr (in the assignment section) for more info.
        if(castExpr->InferredType.IsFailableStruct()) {
            inputValueExpr = insertFailableCtorFromNullValueNoInfer(castExpr->InferredType);
            castExpr->Expr.SetVal(inputValueExpr);
            inputValueExpr->Unref();
            inferCallExpr(static_cast<CCallExpression*>(inputValueExpr));
        }
        // ***********************************************************************************

    } else if((inputValueExpr->Kind() == E_EXPRESSIONKIND_BODY) && castExpr->InferredType.IsMethodClass()) {

        // ***************************************
        //   Special case for anonymous methods.
        // ***************************************

        // If the value is an anonymous method and the target type is a method class, compare their signatures.
        CBodyExpression* closureExpr = static_cast<CBodyExpression*>(inputValueExpr);
        SKIZO_REQ_PTR(closureExpr->Method);

        // FIX for "a: Action? <=> ^{}"
        // Same fix as for array creation above.
        if(castExpr->InferredType.IsFailableStruct()) {
            inputValueExpr = insertImplicitConversionIfAny(inputValueExpr,
                                                           SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                           castExpr->InferredType);
            castExpr->Expr.SetVal(inputValueExpr);
            inputValueExpr->Unref();
        } else {
            if(!castExpr->InferredType.ResolvedClass->IsMethodClassCompatibleSig(closureExpr->Method)) {
                ScriptUtils::FailE(domain->FormatMessage("Closure not convertable to method class '%T'.",
                                                         &castExpr->InferredType),
                                   castExpr);
            }

            inferClosureExpr(closureExpr, castExpr->InferredType);
        }
    } else if((inputValueExpr->Kind() == E_EXPRESSIONKIND_ARRAYCREATION) && castExpr->InferredType.IsArrayClass()) {
        // FIX a: (cast [int]?  (array 10));
        // See the corresponding section of inferAssignmentExpr(..) for more info.
        if(castExpr->InferredType.IsFailableStruct()) {
            inputValueExpr = insertImplicitConversionIfAny(inputValueExpr,
                                                           SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                           castExpr->InferredType);
            castExpr->Expr.SetVal(inputValueExpr);
            inputValueExpr->Unref();
        } else {
            // If the leftValue is an array class and the right value is an array creation expression...
            inferArrayCreationExpr(static_cast<CArrayCreationExpression*>(inputValueExpr), castExpr->InferredType);
        }
    } else {
        SCastInfo castInfo (castExpr->InferredType.GetCastInfo(inputValueExpr->InferredType));
        if(!castInfo.IsCastable) {
            castInfo = castExpr->InferredType.GetCastInfo(inputValueExpr->InferredType);

            ScriptUtils::FailE(domain->FormatMessage("Cannot cast a value of type '%T' to '%T'.",
                                                     &inputValueExpr->InferredType,
                                                     &castExpr->InferredType),
                               castExpr);
        }

        // *************************************************************************
        if(castInfo.CastType == E_CASTTYPE_BOX) {
            CClass* boxedClass = domain->BoxedClass(inputValueExpr->InferredType);
            if(!boxedClass->IsInferred()) {
                classesToProcess->Enqueue(boxedClass);
            }
        } else if(castInfo.CastType == E_CASTTYPE_UNBOX) {
            CClass* boxedClass = domain->BoxedClass(castExpr->InferredType);
            if(!boxedClass->IsInferred())
                classesToProcess->Enqueue(boxedClass);
        }
        // *************************************************************************

        castExpr->CastInfo = castInfo;
    }
}

void STransformer::inferSizeofExpr(CSizeofExpression* sizeofExpr)
{
    if(!this->ResolveTypeRef(sizeofExpr->TargetType)) {
        ScriptUtils::FailE(domain->FormatMessage("Sizeof unknown type '%T'.", &sizeofExpr->TargetType),
                           sizeofExpr);
    }

    sizeofExpr->InferredType.SetPrimType(E_PRIMTYPE_INT);
    bool b = this->ResolveTypeRef(sizeofExpr->InferredType);
    (void)b;
    SKIZO_REQ(b, EC_INVALID_STATE);
}

void STransformer::inferRetExpr(CReturnExpression* retExpr)
{
    CExpression* valueExpr = retExpr->Expr;
    SKIZO_REQ_PTR(valueExpr);
    inferValueExpr(valueExpr, 1);

    const SCastInfo castInfo (curMethod->Signature().ReturnType.GetCastInfo(valueExpr->InferredType));
    if(castInfo.IsCastable) {

        if(castInfo.DoesRequireExplicitCast()) {
            ScriptUtils::FailE(domain->FormatMessage("Implicit downcast from '%T' to '%T' in return.",
                                                     &valueExpr->InferredType,
                                                     &curMethod->Signature().ReturnType),
                               retExpr);
        }
        valueExpr = insertImplicitConversionIfAny(valueExpr, castInfo, curMethod->Signature().ReturnType);
        retExpr->Expr.SetVal(valueExpr);
        valueExpr->Unref();

    } else {
        if((valueExpr->Kind() == E_EXPRESSIONKIND_BODY) && curMethod->Signature().ReturnType.IsMethodClass()) {

            // If the value is a method class and the right value is an anonymous method, compare their signatures.
            CBodyExpression* closureExpr = static_cast<CBodyExpression*>(valueExpr);
            SKIZO_REQ_PTR(closureExpr->Method);

            // FIX for "a: Action? <=> ^{}"
            // Same fix as for array creation above.
            if(curMethod->Signature().ReturnType.IsFailableStruct()) {
                valueExpr = insertImplicitConversionIfAny(valueExpr,
                                                          SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                          curMethod->Signature().ReturnType);
                retExpr->Expr.SetVal(valueExpr);
                valueExpr->Unref();
            } else {
                if(!curMethod->Signature().ReturnType.ResolvedClass->IsMethodClassCompatibleSig(closureExpr->Method)) {
                    ScriptUtils::FailE(domain->FormatMessage("Type mismatch in return: closure signature not compatible with method class '%T'.",
                                                             &curMethod->Signature().ReturnType),
                                       retExpr);
                }
                inferClosureExpr(closureExpr, curMethod->Signature().ReturnType);
            }

        } else if(valueExpr->Kind() == E_EXPRESSIONKIND_NULLCONSTANT && curMethod->Signature().ReturnType.IsNullAssignable()) {
            // OK.

            // **********************************************************************
            // NOTE See a similar codepath in inferCallExpr for more info
            if(curMethod->Signature().ReturnType.IsFailableStruct()) {
                valueExpr = insertFailableCtorFromNullValueNoInfer(curMethod->Signature().ReturnType);
                retExpr->Expr.SetVal(valueExpr);
                valueExpr->Unref();
                inferCallExpr(static_cast<CCallExpression*>(valueExpr));
            }
            // **********************************************************************
        } else if((valueExpr->Kind() == E_EXPRESSIONKIND_ARRAYCREATION) && curMethod->Signature().ReturnType.IsArrayClass()) {

            // FIX a: [int]? <=> (array 10);
            // See the corresponding section of inferAssignmentExpr(..) for more info.
            if(curMethod->Signature().ReturnType.IsFailableStruct()) {
                valueExpr = insertImplicitConversionIfAny(valueExpr,
                                                               SCastInfo(E_CASTTYPE_VALUE_TO_FAILABLE),
                                                               curMethod->Signature().ReturnType);
                retExpr->Expr.SetVal(valueExpr);
                valueExpr->Unref();
            } else {
                // If the leftValue is an array class and the right value is an array creation expression...
                inferArrayCreationExpr(static_cast<CArrayCreationExpression*>(valueExpr), curMethod->Signature().ReturnType);
            }

        } else {
            ScriptUtils::FailE(domain->FormatMessage("Returned value is of wrong type: expected '%T', found '%T'.",
                                                     &curMethod->Signature().ReturnType,
                                                     &valueExpr->InferredType),
                               retExpr);
        }
    }

    retExpr->InferredType = valueExpr->InferredType;
}

void STransformer::inferThisExpr(CThisExpression* thisExpr)
{
    CMethod* pCurMethod = curMethod;

    if(pCurMethod->IsStaticContext()) {
        ScriptUtils::FailE("'this' not allowed in static methods.", thisExpr);
    }

    if(pCurMethod->MethodKind() == E_METHODKIND_DTOR && !(pCurMethod->Flags() & E_METHODFLAGS_IS_UNSAFE)) {
        ScriptUtils::FailE("'this' can escape the destructor and become a zombie after a garbage collection, which is inherently unsafe. "
                           "Mark the destructor 'unsafe' to allow such behavior at your own risk.",
                           thisExpr);
    }

    // "this" refers to the declaring class of the topmost parent method of a closure.

    if(pCurMethod->ParentMethod()) {
        CMethod* intermediate = curMethod, *topMostParent = nullptr;

        while(intermediate) {
            topMostParent = intermediate;
            intermediate = intermediate->ParentMethod();
        }

        topMostParent->Flags() |= E_METHODFLAGS_IS_SELF_CAPTURED;
        addClosureEnvSelfField(topMostParent);

        thisExpr->InferredType = topMostParent->DeclaringClass()->ToTypeRef();
        thisExpr->DeclMethod = topMostParent;
    } else {
        thisExpr->InferredType = pCurMethod->DeclaringClass()->ToTypeRef();
    }
}

void STransformer::inferArrayCreationExpr(CArrayCreationExpression* arrayCreationExpr, STypeRef& inferredTypeRef)
{
    if(!inferredTypeRef.ResolvedClass) {
        ScriptUtils::FailE("Can't correctly infer the type of the array expression (insufficient type information).", arrayCreationExpr);
    }

    SKIZO_REQ(!inferredTypeRef.IsFailableStruct(), EC_ILLEGAL_ARGUMENT);

    arrayCreationExpr->InferredType = inferredTypeRef;
    inferValueExpr(arrayCreationExpr->Expr, 1);
}

void STransformer::inferArrayInitExpr(CArrayInitExpression* arrayInitExpr, bool inferValues)
{
    // The whole type of the array initialization expr depends on the type of the first item.
    CExpression* firstExpr = arrayInitExpr->Exprs->Item(0); // verified in the parser to have at least one item
    if(inferValues) {
        inferValueExpr(firstExpr, 1);
    }
    if(!firstExpr->InferredType.ResolvedClass) {
        ScriptUtils::FailE("Can't correctly infer the type of the array expression (insufficient type information).", arrayInitExpr);
    }

    for(int i = 1; i < arrayInitExpr->Exprs->Count(); i++) {
        CExpression* arrayElement = arrayInitExpr->Exprs->Item(i);
        if(inferValues) {
            inferValueExpr(arrayElement, 1);
        }

        if(!arrayElement->InferredType.Equals(firstExpr->InferredType)) {
            ScriptUtils::FailE(domain->FormatMessage("Elements in array initialization must be of same type: expected '%T', found '%T'.",
                                                     &firstExpr->InferredType,
                                                     &arrayElement->InferredType),
                               arrayInitExpr);
        }
    }

    // If the first element is T, then the result of this expression must be [T].
    // Converts T to [T] by increasing ArrayLevel by 1, and then forcing type resolution
    // so that Domain generated an array class for us.
    arrayInitExpr->InferredType = firstExpr->InferredType;
    arrayInitExpr->InferredType.ArrayLevel++;
    arrayInitExpr->InferredType.ResolvedClass = nullptr; // force re-resolution
    bool b = this->ResolveTypeRef(arrayInitExpr->InferredType);
    (void)b;
    SKIZO_REQ(b, EC_INVALID_STATE); // TODO?

    // Adds a helper function to the registry. A helper function helps an array init expression
    // create and populate an array. Helper functions are purely a construct of the code generation
    // backend, they aren't CMethod's. This code only registers a need for such a function,
    // the rest is to be done by the emitter.
    Auto<CArrayInitializationType> initType (new CArrayInitializationType(arrayInitExpr->Exprs->Count(),
                                                              arrayInitExpr->InferredType));
    int helperId;
    if(!domain->ArrayInitHelperRegistry()->TryGet(initType, &helperId)) {
        helperId = domain->NewUniqueId();
        domain->ArrayInitHelperRegistry()->Set(initType, helperId);
    }
    arrayInitExpr->HelperId = helperId;
}

} }

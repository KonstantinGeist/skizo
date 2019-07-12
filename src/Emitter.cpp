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

#include "Emitter.h"
#include "Const.h"
#include "Domain.h"
#include "Field.h"
#include "Local.h"
#include "Method.h"
#include "ScriptUtils.h"
#include "StringBuilder.h"
#include "StringSlice.h"
#include "TextBuilder.h"

namespace skizo { namespace script {
using namespace core;
using namespace collections;

struct SEmitter
{
    CDomain* domain;

    STextBuilder& mainCB;
    STextBuilder varSegCB;
    STextBuilder methodBodyCB;
    STextBuilder captureCB;
    STextBuilder rewriteCB;
    Auto<CArrayList<CField*>> staticHeapFields; // used when emitting static ctors
    Auto<CArrayList<CField*>> staticValueTypeFields; // valuetypes have special handling

    SEmitter(CDomain* _domain, STextBuilder& cb)
        : domain(_domain),
          mainCB(cb),
          staticHeapFields(new CArrayList<CField*>()),
          staticValueTypeFields(new CArrayList<CField*>())
    {
    }

    void appendCapturePath(STextBuilder& cb,
                           const CClass* declClass,
                           const CMethod* useMethod,
                           SStringSlice name,
                           bool isSelf = false);

    void emitStructHeader(const CClass* klass, bool isFull);
    void emitFunctionHeader(const CMethod* method, EMethodKind methodKind, bool isVirtualCallHelper = false);
    void emitFunctionHeaders(const CClass* klass);
    void emitVCH(const CMethod* method, bool headerOnly); // VCH = virtual call helper
    void emitBodyExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitCallExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr, bool isTop);
    void emitIdentExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitIntConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitFloatConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitStringLitExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitCharLitExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitNullConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitBoolConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitReturnExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitCCodeExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);

    // "expectedType" allows to automatically cast the value to the target type if an implicit cast occurs (upcast).
    void emitValueExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr, const STypeRef* expectedType = nullptr, bool isTopLevel = false);

    void emitThisExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitCastExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitSizeofExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitArrayCreationExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitArrayInitExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitIdentCompExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitIsExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitAssignmentExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitAbortExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitAssertExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitRefExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitBreakExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitInlinedCondExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr);
    void emitBodyStatements(STextBuilder& cb, const CMethod* method, const CArrayList<CExpression*>* exprs);

    // "specificClass" allows to override the type of "self" from what is defined in "method".
    // Situation: a base class implements a method, and a subclass doesn't override it.
    // When the emitter emits a virtual call, it needs to create a function pointer type to cast a void* value in
    // the vtable to, so that C could actually call that function. There's only the base method, whose "declaringClass"
    // value is set to the base class, but we're going to call that function on a subclass type, and C would complain
    // about implicit typecasts subclass=>baseclass.
    void emitFunctionSig(STextBuilder& bc, const CMethod* method, const CClass* specificClass);

    void emitInstanceFieldName(STextBuilder& cb, const CField* field);
    void emitStaticFieldName(STextBuilder& cb, const CField* field);
    void emitExplicitNullCheck(const CMethod* method);
    void emitInstanceMethod(const CMethod* method);
    void emitInstanceCtor(const CClass* klass, const CMethod* method);
    void emitFunctionBody(STextBuilder& cb, const CMethod* method);
    void emitEventFire(STextBuilder& cb, const CMethod* method);
    void emitDisallowedECall(STextBuilder& cb, const CMethod* method);
    void emitEnumFromInt(STextBuilder& cb, const CMethod* method);
    void emitRemoteMethodClientStub(STextBuilder& cb, const CMethod* method);
    void emitRemoteMethodServerStubSync(const CMethod* method, const CClass* specificClass);
    void emitFunctionBodies(const CClass* klass);
    void emitFunctionName(STextBuilder& cb, const CMethod* method, bool isVirtCallHelper = false);
    void emitInstanceFields(const CClass* klass);
    void emitConstValue(STextBuilder& cb, const CConst* konst);
    void emitDtorName(const CMethod* method);
    void emitVTable(const CClass* klass);
    void emitStaticCtorDtor(const CClass* klass);
    void emitArrayInitHelper(const CArrayInitializationType* initType, int helperId);
    void emitIdentCompHelper(const CClass* valueTypeClass);
    void emitUnboxHelper(const CClass* boxedClass);

    void emit();
};

void SEmitter::emitStaticFieldName(STextBuilder& cb, const CField* field)
{
    cb.Emit("_so_%s_%s",
            &field->DeclaringClass->FlatName(),
            &field->Name);
}

void SEmitter::emitInstanceFieldName(STextBuilder& cb, const CField* field)
{
    // All private fields are prefixed with the name of the declaring class,
    // because field names can conflict in a class hierarchy (defined in a base class
    // vs. redefined in a subclass).
    if(field->Access == E_ACCESSMODIFIER_PRIVATE) {
        cb.Emit(" _so_%s_%s",
                    &field->DeclaringClass->FlatName(),
                    &field->Name);
    } else {
        if(field->DeclaringClass->SpecialClass() == E_SPECIALCLASS_CLOSUREENV) {
            cb.Emit(" l_%s", &field->Name);
        } else {
            cb.Emit(" %s", &field->Name);
        }
    }
}

// Recursive function: emit the parent's fields, then emit this class' fields.
void SEmitter::emitInstanceFields(const CClass* klass)
{
    // NOTE Foreign proxies don't emit inherited fields as those fields exist in another domain actually.
    // We do inherit vtables, though.
    if(!klass->IsClassHierarchyRoot() && klass->SpecialClass() != E_SPECIALCLASS_FOREIGN) {
        emitInstanceFields(klass->ResolvedBaseClass());
    }

    const CArrayList<CField*>* instanceFields = klass->InstanceFields();
    for(int i = 0; i < instanceFields->Count(); i++) {
        const CField* field = instanceFields->Array()[i];

        mainCB.Emit("%t", &field->Type);

        emitInstanceFieldName(mainCB, field);

        // Aligns all fields by the word size. This ensures correct interoperation between the runtime
        // and the generated C code.
        mainCB.Emit(" _soX_ALIGNED;\n");
    }

    // A dummy field for binary blobs to align it to the correct size as stated by the [nativeSize] attribute.
    if(klass->SpecialClass() == E_SPECIALCLASS_BINARYBLOB) {
        mainCB.Emit(" char _soX_dummyFields[%d];\n", klass->GCInfo().ContentSize);
    }

    // NOTE TCC crashes on empty structs; however, valuetypes with zero fields are disallowed semantically in Skizo.
}

void SEmitter::emitConstValue(STextBuilder& cb, const CConst* konst)
{
    cb.Emit("_so_%s_%s",
             &konst->DeclaringClass->FlatName(),
             &konst->Name);
}

void SEmitter::emitStructHeader(const CClass* klass, bool isFull)
{
    // NOTE: interfaces don't reference other structs/types, we never access their fields
    // or methods directly (only through "_soX_findmethod"). Which means we never have to define
    // any real type information (layout etc.) about interfaces at all. We can use incomplete C types.
    if(klass->PrimitiveType() != E_PRIMTYPE_OBJECT
    || klass->SpecialClass() == E_SPECIALCLASS_INTERFACE
    || klass->SpecialClass() == E_SPECIALCLASS_METHODCLASS
    || klass->SpecialClass() == E_SPECIALCLASS_BOXED)
    {
        return;
    }

    if(!isFull) {
        mainCB.Emit("struct _so_%s;\n", &klass->FlatName());
        return;
    }

    // ********************
    //   Emits constants.
    // ********************

    const CArrayList<CConst*>* consts = klass->Constants();
    if(consts) {
        for(int i = 0; i < consts->Count(); i++) {
            const CConst* konst = consts->Array()[i];

            mainCB.Emit("#define _so_%s_%s ",
                                &klass->FlatName(),
                                &konst->Name);

            switch(konst->Type.PrimType) {
                case E_PRIMTYPE_INT: mainCB.Emit("%d\n", konst->Value.IntValue()); break;
                case E_PRIMTYPE_FLOAT: mainCB.Emit("%f\n", konst->Value.FloatValue()); break;
                case E_PRIMTYPE_BOOL: mainCB.Emit(konst->Value.BoolValue()? "_soX_TRUE\n": "_soX_FALSE\n"); break;
                case E_PRIMTYPE_CHAR: mainCB.Emit("((_so_char)%d)\n", konst->Value.IntValue()); break;
                case E_PRIMTYPE_OBJECT: // string
                    mainCB.Emit("((struct _so_string*)%p)\n", konst->Value.BlobValue());
                    break;

                default: SKIZO_REQ_NEVER; break;
            }
        }
    }

    // **************************************
    // Is the class layout defined in C code?
    // **************************************

    if(!klass->StructDef().IsEmpty()) {
        mainCB.Emit("struct _so_%s {\n"
                    "%s\n"
                    "};\n",
                    &klass->FlatName(),
                    &klass->StructDef());
        return;
    }

    // *****************************************************
    // Emits the structure (the header and instance fields).
    // *****************************************************

    if(!klass->IsStatic()) {
        mainCB.Emit("struct _so_%s {\n", &klass->FlatName());
        if(!klass->IsValueType()) {
            mainCB.Emit("void** _soX_vtable;\n");
        }

        emitInstanceFields(klass);
        mainCB.Emit("};\n");
    }

    // ********************
    // Emits static fields.
    // ********************

    {
        const CArrayList<CField*>* staticFields = klass->StaticFields();
        for(int i = 0; i < staticFields->Count(); i++) {
            const CField* field = staticFields->Array()[i];

            mainCB.Emit("static %t ", &field->Type);
            emitStaticFieldName(mainCB, field);

            if(field->Type.IsStructClass()) {
                // Composite valuetypes are initialized by calling _soX_static_vt which zero-initializes the value and registers GC roots if any.
                mainCB.Emit(";\n");
            } else {
                // Reference types and primitives (int, float, bool, intptr) can be zero-initialized.
                mainCB.Emit(" = 0;\n");
            }
        }
    }
}

void SEmitter::emitFunctionHeaders(const CClass* klass)
{
    // *******************
    // Emits constructors.
    // *******************

    {
        const CArrayList<CMethod*>* instanceCtors = klass->InstanceCtors();
        for(int i = 0; i < instanceCtors->Count(); i++) {
            const CMethod* method = instanceCtors->Array()[i];

            if(!method->ForceNoHeader()) {
                emitFunctionHeader(method, E_METHODKIND_CTOR);
                mainCB.Emit(";\n");
            }
        }
    }

    // ******************************
    // Emits the destructor (if any).
    // ******************************

    if(klass->InstanceDtor()) {
        emitFunctionHeader(klass->InstanceDtor(), E_METHODKIND_DTOR);
        mainCB.Emit(";\n");
    }

    // ***********************
    // Emits instance methods.
    // ***********************

    const CArrayList<CMethod*>* instanceMethods = klass->InstanceMethods();
    for(int i = 0; i < instanceMethods->Count(); i++) {
        const CMethod* method = instanceMethods->Array()[i];

        if(method->DeclaringClass() != klass) {
            continue;
        }

        // Don't generate headers for operators of primitive types (implemented by C).
        if(!method->ForceNoHeader()) {
            if(!method->IsAbstract()) {
                emitFunctionHeader(method, E_METHODKIND_NORMAL);
                mainCB.Emit(";\n");
            }
        }

        if(method->ShouldEmitVCH()) {
            emitVCH(method, true);
        }
    }

    // *********************
    // Emits static methods.
    // *********************

    const CArrayList<CMethod*>* staticMethods = klass->StaticMethods();
    for(int i = 0; i < staticMethods->Count(); i++) {
        const CMethod* method = staticMethods->Array()[i];

        if(!method->ForceNoHeader()) {
            emitFunctionHeader(method, E_METHODKIND_NORMAL);
            mainCB.Emit(";\n");
        }
    }

    // *************************
    // Emits static ctors/dtors.
    // *************************

    if(klass->StaticCtor()) {
        mainCB.Emit("void _so_%s_static_ctor(int stage);\n",
                    &klass->FlatName());
    }
    if(klass->StaticDtor()) {
        mainCB.Emit("void _so_%s_static_dtor();\n",
                    &klass->FlatName());
    }
}

void SEmitter::emitVCH(const CMethod* method, bool headerOnly)
{
    SKIZO_REQ_NOT_EQUALS(method->VTableIndex(), -1);
    emitFunctionHeader(method, E_METHODKIND_NORMAL, true);

    if(headerOnly) {
        mainCB.Emit(";\n");
    } else {
        const CSignature* sig = &method->Signature();

        // Checks for null.
        mainCB.Emit("{\n");
        emitExplicitNullCheck(method);

        // Extracts the method ptr from the vtable.
        mainCB.Emit(sig->ReturnType.IsVoid()? "(": "return (");
        emitFunctionSig(mainCB,
                        method,
                        method->DeclaringClass()); // signature to cast to
        mainCB.Emit("self->_soX_vtable[%d])(self", method->VTableIndex() + 1); // skips the vtable

        // Dumps arguments.
        int paramCount = sig->Params->Count();
        for(int i = 0; i < paramCount; i++) {
            if(i < paramCount) {
                mainCB.Emit(", ");
            }

            CParam* param = sig->Params->Array()[i];
            if(param->Name.IsEmpty()) {
                mainCB.Emit("_soX_arg%d", i);
            } else {
                mainCB.Emit("l_%s", &param->Name);
            }
        }
        mainCB.Emit(");\n"
                    "}\n");
    }
}

// TODO Add only public/protected methods to the vtable.
void SEmitter::emitVTable(const CClass* klass)
{
    if(klass->EmitVTable() && klass->HasVTable()) {
        const CArrayList<CMethod*>* instanceMethods = klass->InstanceMethods();
        const int methodCount = instanceMethods->Count();

        mainCB.Emit("void* _soX_vtbl_%s[%d] = {\n",
                    &klass->FlatName(),
                    methodCount + 1);

        // The first item in a vtable is a hardcoded pointer to the class of the object
        // for faster retrieval (for `is` operator, reflection etc.)
        mainCB.Emit("(void*)%p", (void*)klass);
        if(methodCount) {
            mainCB.Emit(", ");
        }

        for(int i = 0; i < methodCount; i++) {
            const CMethod* method = instanceMethods->Array()[i];

            if(i != method->VTableIndex()) {
                SKIZO_REQ_EQUALS(i, method->VTableIndex());
            }

            mainCB.Emit("(void*)");
            emitFunctionName(mainCB, method);
            if(i < methodCount - 1) {
                mainCB.Emit(", ");
            }
        }

        mainCB.Emit("\n};\n");
    }
}

void SEmitter::emitFunctionName(STextBuilder& cb, const CMethod* method, bool isVirtCallHelper)
{
    if(isVirtCallHelper) {
        cb.Emit("_soX_vch_%s_", &method->DeclaringClass()->FlatName());
    } else {
        cb.Emit("_so_%s_", &method->DeclaringClass()->FlatName());
    }

    if(method->MethodKind() == E_METHODKIND_DTOR) {
        // Destructors are nameless.
        cb.Emit("dtor");
    } else {
        cb.Emit("%s", &method->Name());
    }
}

void SEmitter::emitFunctionHeader(const CMethod* method, EMethodKind methodKind, bool isVirtualCallHelper)
{
    switch(method->SpecialMethod()) {
        case E_SPECIALMETHOD_NATIVE:
        case E_SPECIALMETHOD_CLOSURE_CTOR:
        case E_SPECIALMETHOD_BOXED_METHOD:
            mainCB.Emit("extern ");
            break;
        default:
            break;
    }

    const STypeRef declClass (method->DeclaringClass()->ToTypeRef());

    // Return type.
    if(methodKind == E_METHODKIND_CTOR) {
        mainCB.Emit("%t ", &declClass);
    } else if(methodKind == E_METHODKIND_DTOR) {
        mainCB.Emit("void ");
    } else if(methodKind == E_METHODKIND_NORMAL) {
        mainCB.Emit("%t ", &method->Signature().ReturnType);
    } else {
        SKIZO_REQ_NEVER
        return;
    }

    // Method name.
    emitFunctionName(mainCB, method, isVirtualCallHelper);
    mainCB.Emit("(");

    // Note the special case for ctors of structs.
    if(!method->Signature().IsStatic) {
        if(methodKind == E_METHODKIND_NORMAL
        || methodKind == E_METHODKIND_DTOR)
        {
            mainCB.Emit("%t self", &declClass);
            if(method->Signature().Params->Count() > 0) {
                mainCB.Emit(", ");
            }
        }
    }

    // Params.
    int i;
    for(i = 0; i < method->Signature().Params->Count(); i++) {
        const CParam* param = method->Signature().Params->Array()[i];

        mainCB.Emit("%t ", &param->Type);
        if(param->Name.IsEmpty()) {
            mainCB.Emit("_soX_arg%d", i);
        } else {
            mainCB.Emit("l_%s", &param->Name);
        }

        if(i < method->Signature().Params->Count() - 1) {
            mainCB.Emit(", ");
        }
    }

    mainCB.Emit(")");

    switch(method->ECallDesc().CallConv) {
        case E_CALLCONV_CDECL:
            // Do nothing.
            break;

        case E_CALLCONV_STDCALL:
            mainCB.Emit(" __attribute__ ((stdcall))");
            break;

        default:
            SKIZO_REQ_NEVER
    }
}

void SEmitter::appendCapturePath(STextBuilder& cb,
                                 const CClass* declClass,
                                 const CMethod* useMethod,
                                 SStringSlice name,
                                 bool isSelf)
{
    captureCB.Clear();

    if(useMethod->DeclaringClass() == declClass) {
        SKIZO_REQ(!name.IsEmpty(), EC_ILLEGAL_ARGUMENT);
        captureCB.Emit("_soX_newEnv->l_%s", &name);
    } else {
        int level = 0;
        const CMethod* m = useMethod;
        while(m) {
            if(m->DeclaringClass() == declClass) {
                break;
            }

            if(m->ParentMethod()) {

                if(m->ParentMethod()->DeclaringClass() == declClass) {
                    // The target env we've found.

                    captureCB.Prepend("((struct _so_%s*)(", &m->ParentMethod()->ClosureEnvClass()->FlatName());
                    if(level == 0) {
                        captureCB.Emit("self->_soX_env");
                    }
                    captureCB.Emit("))->");

                    if(name.IsEmpty()) {
                        SKIZO_REQ(isSelf, EC_ILLEGAL_ARGUMENT);

                        captureCB.Emit("l__soX_self");
                    } else {
                        if(isSelf) {
                            if(declClass->IsValueType()) {
                                captureCB.Emit("l__soX_self._so_%s_", &declClass->FlatName());
                            } else {
                                captureCB.Emit("l__soX_self->_so_%s_", &declClass->FlatName());
                            }

                            captureCB.Emit("%s", &name);
                        } else {
                            captureCB.Emit("l_%s", &name);
                        }

                    }

                } else {
                    captureCB.Prepend("((struct _so_%s*)(", &m->ParentMethod()->ClosureEnvClass()->FlatName());
                    if(level == 0) {
                        captureCB.Emit("self->_soX_env");
                    }
                    captureCB.Emit("))->l__soX_upper");
                }
            }

            level++;
            m = m->ParentMethod();
        }
    }

    cb.Append(captureCB);
}

void SEmitter::emitIdentExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_IDENT);
    const CIdentExpression* identExpr = static_cast<const CIdentExpression*>(expr);

    // ********
    // Appends.
    // ********

    switch(identExpr->ResolvedIdent.EType) {

        case E_RESOLVEDIDENTTYPE_FIELD:
            if(identExpr->ResolvedIdent.AsField_->IsStatic) {
                emitStaticFieldName(cb, identExpr->ResolvedIdent.AsField_);
            } else {
                const CClass* fieldDeclClass = identExpr->ResolvedIdent.AsField_->DeclaringClass;

                if(fieldDeclClass != method->DeclaringClass()) {
                    appendCapturePath(cb,
                                      fieldDeclClass, // declMethod
                                      method,         // useMethod
                                      identExpr->Name,
                                      true);
                } else {
                    if(method->DeclaringClass()->IsValueType()) {
                        cb.Emit("self.");
                    } else {
                        cb.Emit("self->");
                    }
                    emitInstanceFieldName(cb, identExpr->ResolvedIdent.AsField_);
                }
            }
            break;

        case E_RESOLVEDIDENTTYPE_LOCAL:
            if(identExpr->ResolvedIdent.AsLocal_->IsCaptured) {
                appendCapturePath(cb,
                                  identExpr->ResolvedIdent.AsLocal_->DeclaringMethod->DeclaringClass(), // declMethod
                                  method,                                                               // useMethod
                                  identExpr->Name);
            } else {
                // NOTE adds "l_" not to conflict with C keywords
                cb.Emit("l_%s ", &identExpr->Name);
            }
            break;

        case E_RESOLVEDIDENTTYPE_PARAM:
            if(identExpr->ResolvedIdent.AsParam_->IsCaptured) {
                    appendCapturePath(cb,
                                      identExpr->ResolvedIdent.AsParam_->DeclaringMethod->DeclaringClass(), // declMethod
                                      method,                                                               // useMethod
                                      identExpr->Name);
            } else {
                cb.Emit("l_%s ", &identExpr->Name);
            }
            break;

        case E_RESOLVEDIDENTTYPE_CONST:
            emitConstValue(cb, identExpr->ResolvedIdent.AsConst_);
            break;

        default:
            identExpr->Name.DebugPrint();
            SKIZO_REQ_NEVER
    }
}

void SEmitter::emitFloatConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_FLOATCONSTANT);
    const CFloatConstantExpression* floatConstExpr = static_cast<const CFloatConstantExpression*>(expr);

    cb.Emit("%f", floatConstExpr->Value);
}

void SEmitter::emitStringLitExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_STRINGLITERAL);
    const CStringLiteralExpression* stringLitExpr = static_cast<const CStringLiteralExpression*>(expr);
    SKIZO_REQ_PTR(stringLitExpr->SkizoObject);

    // Emits a pre-allocated object and its hard-coded reference.
    // The object is preallocated in the transformer phase.
    cb.Emit("((struct _so_string*)%p)", (void*)stringLitExpr->SkizoObject);
}

void SEmitter::emitCharLitExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_CHARLITERAL);
    const CCharLiteralExpression* charLitExpr = static_cast<const CCharLiteralExpression*>(expr);

    cb.Emit("((_so_char)%d)", (int)charLitExpr->CharValue);
}

void SEmitter::emitIntConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_INTCONSTANT);
    const CIntegerConstantExpression* intConstExpr = static_cast<const CIntegerConstantExpression*>(expr);

    cb.Emit("%d", intConstExpr->Value);
}

void SEmitter::emitNullConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_NULLCONSTANT);
    cb.Emit("0");
}

void SEmitter::emitBoolConstExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_BOOLCONSTANT);
    const CBoolConstantExpression* boolConstExpr = static_cast<const CBoolConstantExpression*>(expr);

    cb.Emit(boolConstExpr->Value? "_soX_TRUE": "_soX_FALSE");
}

void SEmitter::emitReturnExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_RETURN);
    const CReturnExpression* returnExpr = static_cast<const CReturnExpression*>(expr);

    if(method->ShouldEmitReglocalsCode()) {
        // With soft-debugging, we register/unregister locals in runtime.
        cb.Emit("_soX_unreglocals();\n");
    }

    const bool isUnsafe = method->IsUnsafe();

    if(domain->ProfilingEnabled() && !isUnsafe) {
        // Profiling emits a separate dedicated epilog, unlike stackTraceEnabled.
        // NOTE _soX_tc in this code is a special variable that was returned by the epilog
        // (_soX_pushframe_prf)

        cb.Emit("%t _soX_r = ", &method->Signature().ReturnType);
        emitValueExpr(cb, method, returnExpr->Expr, &method->Signature().ReturnType, true);
        cb.Emit(";\n"
                "_soX_popframe_prf((void*)%p, _soX_tc);\n"
                "return _soX_r;\n", (void*)domain);
    } else if(domain->StackTraceEnabled() && !isUnsafe) {
        // Stack trace information.
        // NOTE We can't correctly deal with pushframe/popframe if there is unsafe code in this method
        // (epilogs/prologs can be by-passed in inline C disbalancing the stack).

        cb.Emit("%t _soX_r = ", &method->Signature().ReturnType);
        emitValueExpr(cb, method, returnExpr->Expr, &method->Signature().ReturnType, true);
        cb.Emit(";\n"
                "_soX_popframe((void*)%p);\n"
                "return _soX_r;\n", (void*)domain);
    } else {
        cb.Emit("return ");
        emitValueExpr(cb, method, returnExpr->Expr, 0, true);
    }
}

void SEmitter::emitCCodeExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_CCODE);
    const CCCodeExpression* cCodeExpr = static_cast<const CCCodeExpression*>(expr);

    cb.Emit("%s", &cCodeExpr->Code);
}

void SEmitter::emitThisExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_THIS);

    if(method->ParentMethod()) {
        const CThisExpression* thisExpr = static_cast<const CThisExpression*>(expr);
        SKIZO_REQ_PTR(thisExpr->DeclMethod);

        const SStringSlice emptyName; // "self" doesn't have field names attached; empty name.
        appendCapturePath(cb, thisExpr->DeclMethod->DeclaringClass(), method, emptyName, true /* isSelf = true */);
    } else {
        cb.Emit("self");
    }
}

// NOTE These are explicit casts. They can be inserted by the transformer if an implicit cast expression is found.
void SEmitter::emitCastExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_CAST);
    const CCastExpression* castExpr = static_cast<const CCastExpression*>(expr);

    const SCastInfo& castInfo = castExpr->CastInfo;

    cb.Emit("(");

    // NOTE: VALUE_TO_FAILABLE and ERROR_TO_FAILABLE implicit conversions are done by
    // STransformer::insertImplicitConversionIfAny which inserts required method calls so that every
    // cast is reduced to UPCAST, DOWNCAST, BOX or UNBOX.
    if(castInfo.IsCastable) {
        cb.Emit("(%t)", &castExpr->InferredType);

        switch(castInfo.CastType) {
            case E_CASTTYPE_UPCAST:
                // Do nothing.
                break;

            // Downcasts require dynamic check for the target type.
            case E_CASTTYPE_DOWNCAST:
                // Hardcoded class reference.
                cb.Emit("_soX_downcast((void*)%p, ", (void*)castExpr->InferredType.ResolvedClass);
                break;

            case E_CASTTYPE_BOX:
                SKIZO_REQ_PTR(castExpr->InferredType.ResolvedClass);
                {
                    // The transformer must have pregenerated a reference type to hold this value.
                    const CClass* boxedClass = domain->BoxedClass(castExpr->Expr->InferredType,
                                     /* mustBeAlreadyCreated = */ true);
                    SKIZO_REQ_PTR(boxedClass);
                    cb.Emit("_so_%s_create(", &boxedClass->FlatName());
                }
                break;

            case E_CASTTYPE_UNBOX:
                SKIZO_REQ_PTR(castExpr->InferredType.ResolvedClass);
                {
                    // The emitter must have pregenerated the unboxing method.
                    cb.Emit("_soX_unbox_%s(", &castExpr->InferredType.ResolvedClass->FlatName());
                }
                break;

            default:
            {
                SKIZO_THROW(EC_NOT_IMPLEMENTED);
                break;
            }
        }
    }

    emitValueExpr(cb, method, castExpr->Expr);

    if(castInfo.CastType != E_CASTTYPE_UPCAST /*&& castInfo.CastType != E_CASTTYPE_DOWNCAST_ALIAS*/) {
        cb.Emit(")");
    }

    cb.Emit(")");
}

void SEmitter::emitSizeofExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_SIZEOF);
    const CSizeofExpression* sizeofExpr = static_cast<const CSizeofExpression*>(expr);

    SKIZO_REQ_PTR(sizeofExpr->TargetType.ResolvedClass);

    cb.Emit("%d", sizeofExpr->TargetType.ResolvedClass->GCInfo().SizeForUse);
}

void SEmitter::emitArrayCreationExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_ARRAYCREATION);
    const CArrayCreationExpression* arrayCreationExpr = static_cast<const CArrayCreationExpression*>(expr);
    SKIZO_REQ_PTR(arrayCreationExpr->InferredType.ResolvedClass);

    // _soX_newarray refers to the pre-created array class for array creation.
    cb.Emit("(_soX_newarray(");
    emitValueExpr(cb, method, arrayCreationExpr->Expr);
    cb.Emit(", _soX_vtbl_%s))", &arrayCreationExpr->InferredType.ResolvedClass->FlatName());
}

void SEmitter::emitArrayInitExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_ARRAYINIT);
    const CArrayInitExpression* arrayInitExpr = static_cast<const CArrayInitExpression*>(expr);

    cb.Emit("_soX_arrInitHelper_%d(", arrayInitExpr->HelperId);
    for(int i = 0; i < arrayInitExpr->Exprs->Count(); i++) {
        emitValueExpr(cb, method, arrayInitExpr->Exprs->Array()[i]);

        if(i < (arrayInitExpr->Exprs->Count() - 1)) {
            cb.Emit(", ");
        }
    }
    cb.Emit(")");
}

void SEmitter::emitValueExpr(STextBuilder& cb,
                             const CMethod* method,
                             const CExpression* subExpr,
                             const STypeRef* expectedType, bool isTopLevel)
{
    bool typesMatch = true;
    if(expectedType && !expectedType->Equals(subExpr->InferredType)) {
        typesMatch = false;
    }

    // Casts to the expected type (upcasts/simple casts).
    if(!typesMatch) {
        STypeRef expectedTypeCopy (*expectedType);
        cb.Emit("((%t)", &expectedTypeCopy);
    }

    switch(subExpr->Kind()) {
        case E_EXPRESSIONKIND_BODY: emitBodyExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_CALL: emitCallExpr(cb, method, subExpr, isTopLevel); break;
        case E_EXPRESSIONKIND_IDENT: emitIdentExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_INTCONSTANT: emitIntConstExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_FLOATCONSTANT: emitFloatConstExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_STRINGLITERAL: emitStringLitExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_CHARLITERAL: emitCharLitExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_NULLCONSTANT: emitNullConstExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_BOOLCONSTANT: emitBoolConstExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_THIS: emitThisExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_CAST: emitCastExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_SIZEOF: emitSizeofExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_ARRAYCREATION: emitArrayCreationExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_ARRAYINIT: emitArrayInitExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_IDENTITYCOMPARISON: emitIdentCompExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_IS: emitIsExpr(cb, method, subExpr); break;
        case E_EXPRESSIONKIND_REF: emitRefExpr(cb, method, subExpr); break;
        default:
        {
            ScriptUtils::FailE("Expression type not allowed in this context.", subExpr);
        }
        break;
    }

    if(!typesMatch) {
        cb.Emit(")");
    }
}

void SEmitter::emitBodyExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_BODY);
    const CBodyExpression* closureExpr = static_cast<const CBodyExpression*>(expr);

    SKIZO_REQ_PTR(closureExpr->GeneratedClosureClass);
    const CArrayList<CMethod*>* intanceCtors = closureExpr->GeneratedClosureClass->InstanceCtors();
    SKIZO_REQ_EQUALS(intanceCtors->Count(), 1);

    // ********************************************************
    //   Emits a constructor that generates a closure object.
    // ********************************************************

    const CMethod* ctorToCall = intanceCtors->Array()[0];

    // *****************************************************************************
    //   Emits a typecast because the actual closure has a type different from the
    //   demanded method class (closures are automatically generated subclasses of
    //   their specified method classes).
    // *****************************************************************************

    cb.Emit("(%t)", &closureExpr->InferredType);

    // ****************************
    //   Emits the function call.
    // ****************************

    emitFunctionName(cb, ctorToCall);
    cb.Emit("(");

    if(method->ClosureEnvClass()) {
        cb.Emit("_soX_newEnv");
    } else {
        cb.Emit("0");
    }

    cb.Emit(")");
}

// isTop: top functions don't need parentheses around them.
void SEmitter::emitCallExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr, bool isTop)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_CALL);
    const CCallExpression* callExpr = static_cast<const CCallExpression*>(expr);
    const int count = callExpr->Exprs->Count();

    if(!isTop) {
        cb.Emit("(");
    }

    if(callExpr->CallType == E_CALLEXPRESSION_METHODCALL) {

        const CMethod* targetMethod = callExpr->uTargetMethod;

        // *****************************************************************************************
        // Checks if it's a built-in primitive operation, such as addition between integers.
        // In that case, we can emit C arithmetics directly.
        SStringSlice primOpName;
        if(targetMethod->DeclaringClass()->PrimitiveType() != E_PRIMTYPE_OBJECT) {
            primOpName = NeutralNameToPrimitiveOperator(targetMethod->Name(), domain);

            // **************************************************************************
            // NOTE FIX "int::op_divide" isn't mapped to C's math operators so that the Emitter used a special function
            // -- "_so_int_op_divide" -- which preemptively checks the divisor to avoid killing the whole process
            // on divide by zero CPU fault.
            if(targetMethod->DeclaringClass()->PrimitiveType() == E_PRIMTYPE_INT
            && primOpName.EqualsAscii("/"))
            {
                // Sets back to void.
                primOpName.SetEmpty();
            }
            // **************************************************************************
        }
        // *****************************************************************************************

        if(!primOpName.IsEmpty()) {

            // *******************************************
            // Primitive arithmetics.
            // *******************************************

            for(int i = 0; i < count; i++) {
                const CExpression* subExpr = callExpr->Exprs->Array()[i];

                if(i == 1) {
                    cb.Emit("%s ", &primOpName);
                } else {
                    emitValueExpr(cb,
                                  method,
                                  subExpr,
                                  &callExpr->Exprs->Array()[0]->InferredType); // TODO ?
                }
            }
        } else {

            if(targetMethod->Signature().IsStatic || targetMethod->MethodKind() == E_METHODKIND_CTOR) {

                // *******************************************
                //   Static method call or an instance ctor.
                // *******************************************

                emitFunctionName(cb, targetMethod);
                cb.Emit("(");

                // ***********************
                //   Parameters emitted.
                // ***********************

                for(int i = 2; i < count; i++) {
                    const CExpression* subExpr = callExpr->Exprs->Array()[i];

                    emitValueExpr(cb, method, subExpr, &targetMethod->Signature().Params->Item(i - 2)->Type);
                    if((i < count - 1)) {
                        cb.Emit(", ");
                    }
                }

                // ***********************

                cb.Emit(")");

            } else {

                if(targetMethod->DeclaringClass()->SpecialClass() == E_SPECIALCLASS_INTERFACE) {
                    SKIZO_REQ(!targetMethod->Signature().IsStatic, EC_ILLEGAL_ARGUMENT); // Static methods on interfaces are disallowed.

                    // **********************
                    //   An interface call.
                    // **********************

                    CExpression* selfExpr = callExpr->Exprs->Array()[0];

                    cb.Emit("(");
                    emitFunctionSig(cb, targetMethod, selfExpr->InferredType.ResolvedClass);

                    cb.Emit("_soX_findmethod(");
                    emitValueExpr(cb, method, selfExpr);

                    cb.Emit(", (void*)%p))(", (void*)targetMethod);
                    emitValueExpr(cb, method, selfExpr);

                    if((count - 2) > 0) {
                        cb.Emit(", ");
                    }

                } else if(targetMethod->IsTrulyVirtual() || targetMethod->IsAbstract()) {

                    // **************************************************************************
                    //   A virtual method call. Gets a function ptr by its index in the vtable,
                    //   casts to the required function signature and calls it.
                    // **************************************************************************

                    const CExpression* selfExpr = callExpr->Exprs->Array()[0];

                    // ******************************************************************************
                    // If the self-expression is a call expression, (consider "(MyClass create)
                    // doSomething" where the element itself ia a complex expression), we need to
                    // create a temporary variable to hold the result of such a complex expression
                    // because virtual methods have the form "self->vtable[1](self, x, y)" where 'self' is
                    // repeated twice. In the case of call expressions, that would mean two actual calls.
                    // We solve this issue by generating helper methods that store evaluated
                    // "self" expressions on the C stack as a very simple solution.
                    //
                    // Same for checking 'self' of virtual calls for null.
                    // ******************************************************************************

                    const CMethod* ultimateBaseMethod = targetMethod->UltimateBaseMethod();

                    emitFunctionName(cb, ultimateBaseMethod, true);
                    cb.Emit("("); // compensates for ')' further in the code
                    if(ultimateBaseMethod != targetMethod) {
                        // `ultimateBaseMethod` is defined in the base class. Cast "self" to the base class.
                        // We cast "self" to the base class so that multiple subclasses could use one single function.
                        const STypeRef ultimateBaseTypeRef (ultimateBaseMethod->DeclaringClass()->ToTypeRef());
                        cb.Emit("(%t)", &ultimateBaseTypeRef);
                    }
                    emitValueExpr(cb, method, selfExpr);

                    if((count - 2) > 0) {
                        cb.Emit(", ");
                    }

                } else {

                    // *************************************************************************
                    // The method is not truly virtual: it's never overriden or never overrides.
                    // We can by-pass the vtable system and call the method directly.
                    // *************************************************************************

                    const CExpression* selfExpr = callExpr->Exprs->Array()[0];

                    if((selfExpr->Kind() == E_EXPRESSIONKIND_IDENT || !targetMethod->DeclaringClass()->IsValueType())
                       && targetMethod->TargetField() && targetMethod->IsInlinable())
                    {
                        // ************************************************************************************
                        // More optimization (a form of inlining): if the method in question is a "simple
                        // getter" we can emit code that references the field directly.
                        // ************************************************************************************

                        // NOTE Checks above if "self" is a reference type. Doesn't allow "self" to be a valuetype object
                        // to avoid subtle issues with the C backend in cases the returned valuetype object is a temporary
                        // (which can be overriden), so accessing a field of such an object directly might lead to unexpected
                        // results because of by-copy semantics.
                        // The exception for valuetypes is when "selfExpr" is an ident, meaning that its value is known
                        // for sure to be stored somewhere else.

                         {

                            // No Explicit Check: can be inlined

                            if(targetMethod->DeclaringClass()->IsValueType()) {
                                cb.Emit("(");
                                emitValueExpr(cb, method, selfExpr);
                                cb.Emit(".");
                            } else {
                                cb.Emit("((");
                                emitValueExpr(cb, method, selfExpr);
                                cb.Emit(")->");
                            }

                            // NOTE the corresponding ")" as added in the bottom of the function, after the generic parameter
                            // emission phase.
                            cb.Emit("_so_%s_%s",
                                    &targetMethod->TargetField()->DeclaringClass->FlatName(),
                                    &targetMethod->TargetField()->Name);
                        }

                    } else {

                        // ****************************
                        //   Non-virtual method call.
                        // ****************************

                        emitFunctionName(cb, targetMethod);
                        cb.Emit("(");

                        // A subclass tries to call a base method. We must cast "self" to the base class so that the C
                        // backend didn't complain.
                        if(selfExpr->InferredType.ResolvedClass != targetMethod->DeclaringClass()) {
                            const STypeRef tmpTypeRef (targetMethod->DeclaringClass()->ToTypeRef());
                            cb.Emit("(%t)", &tmpTypeRef);
                        }

                        emitValueExpr(cb, method, selfExpr);

                        if((count - 2) > 0) {
                            cb.Emit(", ");
                        }
                    }

                }

                // ***********************
                //   Parameters emitted.
                // ***********************

                for(int i = 2; i < count; i++) {
                    const CExpression* subExpr = callExpr->Exprs->Array()[i];

                    emitValueExpr(cb, method, subExpr, &targetMethod->Signature().Params->Item(i - 2)->Type);

                    if((i < count - 1)) {
                        cb.Emit(", ");
                    }
                }

                // ***********************

                cb.Emit(")");

            }

        }

    } else if (callExpr->CallType == E_CALLEXPRESSION_CONSTACCESS) {
        emitConstValue(cb, callExpr->uTargetConst);
    } else {
        SKIZO_REQ_NEVER // TODO ?
    }

    if(!isTop) {
        cb.Emit(")");
    }
}

void SEmitter::emitIdentCompExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_IDENTITYCOMPARISON);
    const CIdentityComparisonExpression* identCompExpr = static_cast<const CIdentityComparisonExpression*>(expr);

    const CClass* klass = identCompExpr->Expr1->InferredType.ResolvedClass;
    SKIZO_REQ_PTR(klass);

    // Non-primitive value types are compared using intrinsics generated in emitIdentCompHelper(..)
    if(klass->IsValueType() && klass->PrimitiveType() == E_PRIMTYPE_OBJECT) {
        cb.Emit("_soX_idco_%s(", &klass->FlatName());
        emitValueExpr(cb, method, identCompExpr->Expr1);
        cb.Emit(", ");
        emitValueExpr(cb, method, identCompExpr->Expr2);
        cb.Emit(")");
    } else {
        cb.Emit("((");
        emitValueExpr(cb, method, identCompExpr->Expr1);
        cb.Emit(") == (");
        emitValueExpr(cb, method, identCompExpr->Expr2);
        cb.Emit("))");
    }
}

void SEmitter::emitIsExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_IS);
    const CIsExpression* isExpr = static_cast<const CIsExpression*>(expr);
    SKIZO_REQ_PTR(isExpr->TypeAsInCode.ResolvedClass);

    const CClass* actualClass = isExpr->Expr->InferredType.ResolvedClass;
    const CClass* targetClass = isExpr->TypeAsInCode.ResolvedClass;

    if(actualClass->IsValueType()) {
        // Type checks for valuetypes can be done in compile-time.
        cb.Emit(actualClass->Is(targetClass)? "_soX_TRUE": "_soX_FALSE");
    } else {
        // FIX We used to emit literals directly if we could prove in compile time the types were OK.
        // That introduced a problem:
        //
        //    s: string = null;
        //    b: bool = (s is string); /* TRUE */
        //
        // but:
        //
        //    b: bool = (null is string); /* FALSE */
        //
        // Applying `is` to a null should always return false.

        cb.Emit("_soX_is(");
        emitValueExpr(cb, method, isExpr->Expr);
        cb.Emit(", (void*)%p)", (void*)targetClass);
    }
}

void SEmitter::emitAssignmentExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_ASSIGNMENT);
    const CAssignmentExpression* assExpr = static_cast<const CAssignmentExpression*>(expr);

    emitValueExpr(cb, method, assExpr->Expr1);
    cb.Emit("=");

    emitValueExpr(cb, method, assExpr->Expr2, &assExpr->Expr1->InferredType);
}

void SEmitter::emitAbortExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_ABORT);
    const CAbortExpression* abortExpr = static_cast<const CAbortExpression*>(expr);

    cb.Emit("_soX_abort(");
    emitValueExpr(cb, method, abortExpr->Expr);
    cb.Emit(")");
}

void SEmitter::emitAssertExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_ASSERT);
    const CAssertExpression* assertExpr = static_cast<const CAssertExpression*>(expr);

    cb.Emit("if(!");
    emitValueExpr(cb, method, assertExpr->Expr);
    cb.Emit(") _soX_abort0(3);\n");
}

void SEmitter::emitRefExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_REF);
    const CRefExpression* refExpr = static_cast<const CRefExpression*>(expr);

    cb.Emit("(void*)(&"); // The semantics require an intptr.
    emitValueExpr(cb, method, refExpr->Expr);
    cb.Emit(")");
}

void SEmitter::emitBreakExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_BREAK);

    // NOTE For domains without soft debugging enabled, break statements are null statements.
    if(method->HasBreakExprs()) {
        cb.Emit("_soX_break();\n");
    }
}

void SEmitter::emitInlinedCondExpr(STextBuilder& cb, const CMethod* method, const CExpression* expr)
{
    SKIZO_REQ_EQUALS(expr->Kind(), E_EXPRESSIONKIND_INLINED_CONDITION);
    const CInlinedConditionExpression* condExpr = static_cast<const CInlinedConditionExpression*>(expr);

    cb.Emit("if(");
    emitValueExpr(cb, method, condExpr->IfCondition);
    cb.Emit(") {\n");
    emitBodyStatements(cb, method, condExpr->Body->Exprs);
    cb.Emit("}\n");
}

void SEmitter::emitFunctionSig(STextBuilder& cb,
                               const CMethod* method,
                               const CClass* specificClass)
{
    const CSignature* sig = &method->Signature();

    switch(method->ECallDesc().CallConv) {
        case E_CALLCONV_CDECL:
            cb.Emit("(%t(*)(", &sig->ReturnType);
            break;

        case E_CALLCONV_STDCALL:
            cb.Emit("(%t(__attribute__ ((stdcall)) *)(", &sig->ReturnType);
            break;

        default:
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
            break;
    }

    const int count = sig->Params->Count();

    // Needs to also add the "self" thing.
    if(!sig->IsStatic) {
        SKIZO_REQ_PTR(specificClass);
        STypeRef tmpTypeRef (specificClass->ToTypeRef());
        cb.Emit("%t", &tmpTypeRef);
    }
    if(count) {
        cb.Emit(", ");
    }

    for(int i = 0; i < count; i++) {
        cb.Emit("%t", &sig->Params->Array()[i]->Type);

        if(i < count - 1) {
            cb.Emit(", ");
        }
    }
    cb.Emit("))");
}

void SEmitter::emitBodyStatements(STextBuilder& cb, const CMethod* method, const CArrayList<CExpression*>* exprs)
{
    // NOTE: not every expression is allowed in the top level.
    for(int i = 0; i < exprs->Count(); i++) {
        const CExpression* subExpr = exprs->Array()[i];

        switch(subExpr->Kind()) {
            case E_EXPRESSIONKIND_CALL:
                emitCallExpr(cb, method, subExpr, true);
                cb.Emit(";\n");
                break;
            case E_EXPRESSIONKIND_RETURN:
                emitReturnExpr(cb, method, subExpr);
                cb.Emit(";\n");
                break;
            case E_EXPRESSIONKIND_CCODE:
                emitCCodeExpr(cb, method, subExpr);
                break;
            case E_EXPRESSIONKIND_ASSIGNMENT:
                emitAssignmentExpr(cb, method, subExpr);
                cb.Emit(";\n");
                break;
            case E_EXPRESSIONKIND_ABORT:
                emitAbortExpr(cb, method, subExpr);
                cb.Emit(";\n");
                break;
            case E_EXPRESSIONKIND_ASSERT:
                emitAssertExpr(cb, method, subExpr);
                break;
            case E_EXPRESSIONKIND_INLINED_CONDITION:
                emitInlinedCondExpr(cb, method, subExpr);
                break;
            case E_EXPRESSIONKIND_BREAK:
                emitBreakExpr(cb, method, subExpr);
                break;
            default:
                { SKIZO_THROW(EC_NOT_IMPLEMENTED); }
                break;
        }
    }
}

void SEmitter::emitDisallowedECall(STextBuilder& cb, const CMethod* method)
{
    cb.Emit("_soX_abort0(6);\n"); // SKIZO_ERRORCODE_DISALLOWED_CALL == 6
}

void SEmitter::emitEventFire(STextBuilder& cb, const CMethod* method)
{
    const CClass* handlerClass = method->DeclaringClass()->ResolvedWrappedClass();
    SKIZO_REQ_PTR(handlerClass);

    const CMethod* handlerInvoke = handlerClass->InvokeMethod();
    SKIZO_REQ_PTR(handlerInvoke);
    SKIZO_REQ_NOT_EQUALS(handlerInvoke->VTableIndex(), -1);

    const CSignature* sig = &handlerInvoke->Signature();
    SKIZO_REQ_EQUALS(sig->ReturnType.PrimType, E_PRIMTYPE_VOID); // to make sure the transformer did it the right way

    // NOTE Copies the pointer from the event field to the local stackframe. We iterate by accessing this local only,
    // so that handlers were able to modify the handler list during iteration.
    cb.Emit("struct _soX_ArrayHeader* _soX_cpy = (struct _soX_ArrayHeader*)self->_so_%s_m_array;\n"
            "if(!_soX_cpy) return;", // important, as the list may be empty
            &method->DeclaringClass()->FlatName());
    // Fast iteration over the handler list.
    cb.Emit("int _soX_index; for(_soX_index = 0; _soX_index < _soX_cpy->_soX_length; _soX_index++) {\n"
            "struct _soX_0Closure* _soX_it = ((struct _soX_0Closure**)(&_soX_cpy->_soX_firstItem))[_soX_index];\n" // the object. cast to "void**" for easier vtable retrieval
            "void** _soX_vtbl = ((void***)_soX_it)[0];\n");

    // **************************
    //   Emits a virtual call.
    // **************************

    cb.Emit("(");
    emitFunctionSig(cb, handlerInvoke, handlerClass); // the last argument is the signature to cast to
    cb.Emit("_soX_vtbl[%d])(_soX_it", handlerInvoke->VTableIndex() + 1); // +"1" skips the class ptr

    // Parameters.
    const int paramCount = sig->Params->Count();

    for(int i = 0; i < paramCount; i++) {
        cb.Emit(", l_%s", &sig->Params->Array()[i]->Name);
    }

    // **************************

    cb.Emit(");\n"
            "}\n");
}

// TODO pushframe/popframe etc.?
void SEmitter::emitRemoteMethodClientStub(STextBuilder& cb, const CMethod* method)
{
    const CSignature& sig = method->Signature();

    // Blocking ret.
    if(!sig.ReturnType.IsVoid()) {
        cb.Emit("%t _soX_blockingRet;\n", &sig.ReturnType);
    }

    // Emits the arg array.
    const int paramCount = sig.Params->Count();
    if(paramCount) {
        cb.Emit("void* _soX_args[%d] = { ", paramCount);
        for(int i = 0; i < paramCount; i++) {
            const CParam* param = sig.Params->Array()[i];

            SKIZO_REQ_PTR(param->Type.ResolvedClass);

            if(param->Type.ResolvedClass->IsValueType()) {
                cb.Emit("&l_%s", &param->Name);
            } else {
                cb.Emit("l_%s", &param->Name);
            }

            if(i < paramCount - 1) {
                cb.Emit(", ");
            }
        }
        cb.Emit(" };\n");
    }

    // The actual call.
    cb.Emit("_soX_msgsnd_sync(self->_so_%s_m_hdomain, self->_so_%s_m_name, (void*)%p, %S, %S);\n",
                                &method->DeclaringClass()->FlatName(),
                                &method->DeclaringClass()->FlatName(),
                                method,
                                paramCount? "_soX_args": "(void*)0",
                                sig.ReturnType.IsVoid()? "(void*)0": "&_soX_blockingRet");

    // Don't forget to return the value.
    if(!sig.ReturnType.IsVoid()) {
        cb.Emit("return _soX_blockingRet;\n");
    }
}

void SEmitter::emitRemoteMethodServerStubSync(const CMethod* method, const CClass* specificClass)
{
    SKIZO_REQ_EQUALS(method->SpecialMethod(), E_SPECIALMETHOD_FOREIGNSYNC);
    const CClass* pProxyClass = method->DeclaringClass();

    const CClass* wrappedClass = pProxyClass->ResolvedWrappedClass();
    SKIZO_REQ_PTR(wrappedClass);

    const STypeRef declTypeRef (wrappedClass->ToTypeRef());
    mainCB.Emit("void _soX_server_%s_%s(%t self, void* msg, void* retValue) {\n",
                    &wrappedClass->FlatName(),
                    &method->Name(),
                    &declTypeRef);

    // Emits local variables we will unpack the values from the message to.
    const CSignature& sig = method->Signature();
    const int paramCount = sig.Params->Count();
    for(int i = 0; i < paramCount; i++) {
        const CParam* param = sig.Params->Array()[i];

        mainCB.Emit("%t l_%s;\n", &param->Type, &param->Name);
    }

    // Emits an array we'll use in _soX_unpack to decrease the amount of method calls.
    if(paramCount) {
        mainCB.Emit("void* _soX_args[%d] = { ", paramCount);
        for(int i = 0; i < paramCount; i++) {
            const CParam* param = sig.Params->Array()[i];

            mainCB.Emit("&l_%s", &param->Name);
            if(i < paramCount - 1) {
                mainCB.Emit(", ");
            }
        }
        mainCB.Emit(" };\n"
                    "_soX_unpack(_soX_args, msg, (void*)%p);\n", method);
        // NOTE Unpacker isn't used if no arguments are passed.
    }

    // Finds the method impl. NOTE that if nothing was found, simply ignores the method call instead of
    // aborting the whole domain. This is to prevent other domains from maliciously trying to crash this domain.
    // NOTE It's also important to emit _soX_findmethod2(..) *after* _soX_unpack(..) as _soX_unpack unref's
    // marshaled-by-bleed strings.
    mainCB.Emit("void* methodImpl = _soX_findmethod2(self, msg);\n"
                "if(!methodImpl) return;\n");

    // Performs the actual call.
    if(!sig.ReturnType.IsVoid()) {
        mainCB.Emit("%t _soX_r = ", &sig.ReturnType);
    }

    mainCB.Emit("(");
    emitFunctionSig(mainCB, method, wrappedClass);
    mainCB.Emit("methodImpl)(self"); // skips the class ptr

    if(paramCount) {
        mainCB.Emit(", ");
    }

    for(int i = 0; i < paramCount; i++) {
        const CParam* param = sig.Params->Array()[i];

        mainCB.Emit("l_%s", &param->Name);
        if(i < paramCount - 1) {
            mainCB.Emit(", ");
        }
    }

    mainCB.Emit(");\n");

    if(!sig.ReturnType.IsVoid()) {
        mainCB.Emit("*((%t*)retValue) = _soX_r;\n", &sig.ReturnType);
    }

    mainCB.Emit("}\n");
}

void SEmitter::emitEnumFromInt(STextBuilder& cb, const CMethod* method)
{
    const int enumRange = method->DeclaringClass()->StaticFields()->Count();
    cb.Emit("if(l_intValue < 0 || l_intValue >= %d) _soX_abort0(0);\n" // 0 == RANGECHECK
            "switch(l_intValue) {",
            enumRange);
    for(int i = 0; i < enumRange; i++) {
        cb.Emit("case %d: return _so_%s_0value_%d;\n", i, &method->DeclaringClass()->FlatName(), i);
    }
    cb.Emit("}");
}

void SEmitter::emitFunctionBody(STextBuilder& cb, const CMethod* method)
{
    if(method->SpecialMethod() != E_SPECIALMETHOD_NONE) {

        // ***************************
        //   Runtime-generated body.
        // ***************************
        // TODO check the declaring class to be sure?
        // NOTE Explicit null check is already emitted somewhere else.

        switch(method->SpecialMethod()) {
            case E_SPECIALMETHOD_DISALLOWED_ECALL:
                emitDisallowedECall(cb, method);
                break;

            case E_SPECIALMETHOD_FIRE:
                emitEventFire(cb, method);
                break;

            case E_SPECIALMETHOD_ADDHANDLER:
                // `addHandler` simply redirects the call to a generic icall that does the magic.
                cb.Emit("_soX_addhandler(self, l_e);\n");
                break;

            case E_SPECIALMETHOD_FOREIGNSYNC:
                emitRemoteMethodClientStub(cb, method);
                break;

            case E_SPECIALMETHOD_FOREIGNASYNC:
                SKIZO_REQ_NEVER // TODO
                break;

            case E_SPECIALMETHOD_ENUM_FROM_INT:
                emitEnumFromInt(cb, method);
                break;

            default:
                SKIZO_REQ_NEVER // TODO
                break;
        }

    } else {
        const CExpression* rootExpr = method->Expression();
        if(!rootExpr) {
            return;
        }
        SKIZO_REQ_EQUALS(rootExpr->Kind(), E_EXPRESSIONKIND_BODY);
        const CBodyExpression* bodyExpr = static_cast<const CBodyExpression*>(rootExpr);

        // ****************************
        //   The closure environment.
        // ****************************

        if(method->ClosureEnvClass()) {
            // Why managed constructors if we can have this directly in C?

            varSegCB.Emit("struct _so_%s* _soX_newEnv = _soX_gc_alloc_env((void*)%p, (void*)%p);\n",
                          &method->ClosureEnvClass()->FlatName(),
                          &domain->MemoryManager(),
                          method->ClosureEnvClass());

            if(method->DeclaringClass()->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
                SKIZO_REQ_EQUALS(method->DeclaringClass()->InstanceMethods()->Count(), 1);
                varSegCB.Emit("_soX_newEnv->l__soX_upper = self->_soX_env;\n");
            }

            if(method->IsSelfCaptured()) {
                varSegCB.Emit("_soX_newEnv->l__soX_self = self;\n");
            }

            // Values of captured parameters are copied to the env.
            for(int i = 0; i < method->Signature().Params->Count(); i++) {
                const CParam* param = method->Signature().Params->Array()[i];

                if(param->IsCaptured) {
                    varSegCB.Emit("_soX_newEnv->l_%s = l_%s;\n",
                                  &param->Name,
                                  &param->Name);
                }
            }
        }

        // ***************************************************
        //   Defines & initializes local variables to zero.
        // ***************************************************

        if(method->Locals() && method->Locals()->Size()) {
            SHashMapEnumerator<SStringSlice, CLocal*> localsEnum (method->Locals());
            SStringSlice localName;
            CLocal* local;
            while(localsEnum.MoveNext(&localName, &local)) {

                // Inserts variable initialization into the variable segment (varSegCB).
                // NOTE Captured locals are defined in the heap-allocated environment
                // and the memory manager already sets all bytes to zero there.
                if(!local->IsCaptured) {

                    // If the method has break expressions, all local variables are initialized with zeros, because
                    // a break expression may appear before all variables are correctly initialized (user code can
                    // access them in the debugging callback).
                    if(method->HasBreakExprs()) {

                        if(local->Type.IsStructClass()) {
                            varSegCB.Emit("%t l_%s = {0};\n", &local->Type, &localName);
                        } else {
                            varSegCB.Emit("%t l_%s = 0;\n", &local->Type, &localName);
                        }

                    } else {
                        // The grammar doesn't allow variables without a default value assigned, so we don't have
                        // to zero it out here.
                        // NOTE adds "l_" not to conflict with C keywords.
                        varSegCB.Emit("%t l_%s;\n", &local->Type, &localName);
                    }
                }
            }
        }

        // FIX
        // Closures are unsafe if their enclosing method is unsafe as well, even if m_isUnsafe says "false";
        // never use m_isUnsafe directly.
        const bool isUnsafe = method->IsUnsafe();

        // ***************************************************
        //   Stack trace information.
        // ***************************************************

        // NOTE we can't correctly deal with pushframe/popframe if there is unsafe code in this method
        // (inline C code can return control early).
        if(domain->ProfilingEnabled() && !isUnsafe) {
            cb.Emit("int _soX_tc = _soX_pushframe_prf((void*)%p, (void*)%p);\n", (void*)domain, (void*)method);
        } else if(domain->StackTraceEnabled() && !isUnsafe) {
            cb.Emit("_soX_pushframe((void*)%p, (void*)%p);\n", (void*)domain, (void*)method);
        }

        // *******************************************************************************************
        //   Checks if the class succeeded to initialize for classes which have static constructors.
        // *******************************************************************************************

        if(method->Signature().IsStatic && method->DeclaringClass()->StaticCtor()) {
            cb.Emit("_soX_checktype((void*)%p);\n", (void*)method->DeclaringClass());
        }

        // *****************************************************************
        //   Soft debugging.
        // IMPORTANT the order of variables should be synchronized with the CWatchIterator::NextWatch
        if(method->ShouldEmitReglocalsCode()) {
            // With soft-debugging, we register/unregister locals in runtime.

            Auto<CArrayList<CLocal*> > localList (new CArrayList<CLocal*>());
            if(!method->Signature().IsStatic) {
                localList->Add(nullptr); // signifies "this"
            }
            for(int i = 0; i < method->Signature().Params->Count(); i++) {
                localList->Add(method->Signature().Params->Array()[i]);
            }

            CLocal* local;

            if(method->Locals()) {
                SHashMapEnumerator<SStringSlice, CLocal*> localsEnum (method->Locals());
                SStringSlice localName;
                while(localsEnum.MoveNext(&localName, &local)) {
                    localList->Add(local);
                }
            }

            cb.Emit("void* _soX_locals[%d] = {\n", localList->Count());
            for(int i = 0; i < localList->Count(); i++) {
                local = localList->Array()[i];
                if(local) {
                    if(local->IsCaptured) {
                        cb.Emit("0"); // TODO watch capture locals, too
                    } else {
                        // NOTE adds "l_" not to conflict with C keywords
                        cb.Emit("&l_%s", &local->Name);
                    }
                } else {
                    SKIZO_REQ_EQUALS(i, 0);
                    SKIZO_REQ(!method->Signature().IsStatic, EC_ILLEGAL_ARGUMENT);
                    cb.Emit("&self");
                }
                if(i < localList->Count() - 1) {
                    cb.Emit(", ");
                }
            }
            cb.Emit("};\n");

            cb.Emit("_soX_reglocals(_soX_locals, %d);\n", localList->Count());
        }
        // *****************************************************************

        // *********
        //   Body.
        // *********

        emitBodyStatements(cb, method, bodyExpr->Exprs);

        // ***************************************************
        //   Stack trace information.
        // ***************************************************

        // Frame popping is usually done in the ReturnExpr; however if a method returns nothing, there's no
        // return expression, so we do it here.
        // Also ctors don't have an explicit "return".
        if(!isUnsafe
        && (method->Signature().ReturnType.PrimType == E_PRIMTYPE_VOID || method->MethodKind() == E_METHODKIND_CTOR))
        {
            // *****************************************************************
            //   Soft debugging.
            if(method->ShouldEmitReglocalsCode()) {
                // With soft-debugging, we register/unregister locals in runtime.
                cb.Emit("_soX_unreglocals();\n");
            }
            // *****************************************************************

            if(domain->ProfilingEnabled()) {
                cb.Emit("_soX_popframe_prf((void*)%p, _soX_tc);\n", (void*)domain);
            } else if(domain->StackTraceEnabled()) {
                cb.Emit("_soX_popframe((void*)%p);\n", (void*)domain);
            }
        }
    }

    mainCB.Append(varSegCB);
    mainCB.Append(cb);

    varSegCB.Clear();
    cb.Clear();
}

void SEmitter::emitExplicitNullCheck(const CMethod* method)
{
    if(domain->ExplicitNullCheck() && !method->DeclaringClass()->IsValueType()) {
        // 2 == SKIZO_ERRORCODE_NULLDEREFERENCE (see icall.h)
        mainCB.Emit("_soX_TN\n");
    }
}

void SEmitter::emitInstanceMethod(const CMethod* method)
{
    if(!method->IsAbstract()) {
        emitFunctionHeader(method, E_METHODKIND_NORMAL);
        mainCB.Emit(" {\n");
        emitExplicitNullCheck(method);
        emitFunctionBody(methodBodyCB, method);
        mainCB.Emit("}\n");
    }
}

void SEmitter::emitInstanceCtor(const CClass* klass, const CMethod* method)
{
    SKIZO_REQ_EQUALS(method->MethodKind(), E_METHODKIND_CTOR);

    const ESpecialMethod specialMethod = method->SpecialMethod();
    if(specialMethod == E_SPECIALMETHOD_NATIVE
    || specialMethod == E_SPECIALMETHOD_CLOSURE_CTOR
    || specialMethod == E_SPECIALMETHOD_BOXED_CTOR)
    {
        return;
    }

    emitFunctionHeader(method, E_METHODKIND_CTOR);
    mainCB.Emit(" {\n");

    if(klass->IsValueType()) {
        const STypeRef typeRef (klass->ToTypeRef());
        mainCB.Emit("%t self;\n"
                    "_soX_zero(&self, sizeof(%t));\n",
                    &typeRef,
                    &typeRef);
    } else {
        const STypeRef typeRef (klass->ToTypeRef());
        mainCB.Emit("%t self;\n", &typeRef);

        // NOTE No need for memset because _so_gc_alloc does that for us.
        // NOTE Closures share the same structure, so they're a special case to minimize the amount of generated C code.
        if(klass->SpecialClass() == E_SPECIALCLASS_METHODCLASS) {
            mainCB.Emit("self = _soX_gc_alloc((void*)%p, (int)sizeof(struct _soX_0Closure), _soX_vtbl_%s);\n",
                                    &domain->MemoryManager(),
                                    &klass->FlatName());
        } else {
            mainCB.Emit("self = _soX_gc_alloc((void*)%p, (int)sizeof(struct _so_%s), _soX_vtbl_%s);\n",
                                    &domain->MemoryManager(),
                                    &klass->FlatName(),
                                    &klass->FlatName());
        }
    }

    emitFunctionBody(methodBodyCB, method);
    mainCB.Emit("return self;\n}\n");
}

void SEmitter::emitFunctionBodies(const CClass* klass)
{
    // ****************************
    // Emits instance constructors.
    // ****************************

    {
        const CArrayList<CMethod*>* instanceCtors = klass->InstanceCtors();
        for(int i = 0; i < instanceCtors->Count(); i++) {
            const CMethod* method = instanceCtors->Array()[i];

            if(method->SpecialMethod() != E_SPECIALMETHOD_NATIVE
            && method->SpecialMethod() != E_SPECIALMETHOD_CLOSURE_CTOR)
            {
                emitInstanceCtor(klass, method);
            }
        }
    }

    // ***************************************
    // Emits the instance destructor (if any).
    // ***************************************

    if(klass->InstanceDtor() && klass->InstanceDtor()->SpecialMethod() != E_SPECIALMETHOD_NATIVE) {
        emitFunctionHeader(klass->InstanceDtor(), E_METHODKIND_DTOR);
        mainCB.Emit(" {\n");

        //  Automatically links in dtors of base classes.
        if(!klass->IsClassHierarchyRoot()
        && klass->ResolvedBaseClass()->InstanceDtor())
        {
            mainCB.Emit("_so_%s_dtor((%t)self);\n",
                            &klass->ResolvedBaseClass()->FlatName(),
                            &klass->BaseClass());
        }

        emitFunctionBody(methodBodyCB, klass->InstanceDtor());
        // NOTE Destructors are callback-like. The memory of the object itself is not released here.
        mainCB.Emit("}\n");
    }

    // **********************
    // Emits virtual methods.
    // **********************

    {
        const CArrayList<CMethod*>* instanceMethods = klass->InstanceMethods();
        for(int i = 0; i < instanceMethods->Count(); i++) {
            const CMethod* method = instanceMethods->Array()[i];

            if(method->SpecialMethod() != E_SPECIALMETHOD_NATIVE
            && method->SpecialMethod() != E_SPECIALMETHOD_BOXED_METHOD
            && (method->DeclaringClass() == klass)) // makes sure we don't emit methods defined in base classes
            {
                emitInstanceMethod(method);
            }

            if(method->ShouldEmitVCH()) {
                emitVCH(method, false);
            }
        }
    }

    // Emits static methods.
    {
        const CArrayList<CMethod*>* staticMethods = klass->StaticMethods();
        for(int i = 0; i < staticMethods->Count(); i++) {
            const CMethod* method = staticMethods->Array()[i];

            if(method->SpecialMethod() != E_SPECIALMETHOD_NATIVE) {
                emitFunctionHeader(method, E_METHODKIND_NORMAL);
                mainCB.Emit( " {\n");
                emitFunctionBody(methodBodyCB, method);
                mainCB.Emit( "}\n");
            }
        }
    }

    emitStaticCtorDtor(klass);
}

void SEmitter::emitStaticCtorDtor(const CClass* klass)
{
    // Emits constructors/destructors.
    // Emits static ctors/dtors.
    // NOTE Emits the static ctor if there are static fields in the class no matter whether the static ctor was explicitly
    // defined or not -- because we need to register static fields as roots, and we do it in static ctors (stage 0).
    if(klass->StaticCtor() || klass->StaticFields()->Count() > 0) {

        staticHeapFields->Clear();
        staticValueTypeFields->Clear();

        const CArrayList<CField*>* staticFields = klass->StaticFields();
        for(int i = 0; i < staticFields->Count(); i++) {
            CField* staticField = staticFields->Array()[i];

            if(staticField->Type.IsHeapClass()) {
                staticHeapFields->Add(staticField);
            } else if(staticField->Type.IsStructClass()) {
                staticValueTypeFields->Add(staticField);
            }
        }

        mainCB.Emit("void _so_%s_static_ctor(int stage) {\n", &klass->FlatName());
            mainCB.Emit("if(stage == 0) {\n");
                // ******************************************************************
                //   Registers static heap fields' locations as gc roots (stage 0).
                // ******************************************************************

                if(staticHeapFields->Count()) {
                    mainCB.Emit("void* rootRefs[%d] = {\n", staticHeapFields->Count());
                    for(int i = 0; i < staticHeapFields->Count(); i++) {
                        const CField* staticField = staticHeapFields->Array()[i];
                        mainCB.Emit("&_so_%s_%s", &klass->FlatName(), &staticField->Name); // TODO move out such uses to a separate helper method

                        if(i < staticHeapFields->Count() - 1) {
                            mainCB.Emit(", ");
                        }
                    }
                    mainCB.Emit("\n};\n"
                                "_soX_gc_roots((void*)%p, rootRefs, %d);\n",
                                &domain->MemoryManager(),
                                staticHeapFields->Count());
                }

                // ***************************************************************************************
                //   Initializes static valuetype fields and GC-roots references inside them (stage 0).
                // ***************************************************************************************

                for(int i = 0; i < staticValueTypeFields->Count(); i++) {
                    const CField* staticField = staticValueTypeFields->Array()[i];
                    mainCB.Emit("_soX_static_vt((void*)%p, &_so_%s_%s, (void*)%p);\n", &domain->MemoryManager(), &klass->FlatName(), &staticField->Name, (void*)staticField->Type.ResolvedClass);
                }

            mainCB.Emit( "} else {\n");

            // ****************************************
            //   Emits the static ctor (stage 1).
            // ****************************************

            if(klass->StaticCtor()) {
                emitFunctionBody(methodBodyCB, klass->StaticCtor());
            }

            mainCB.Emit( "}\n");
        mainCB.Emit( "}\n");
    }

    if(klass->StaticDtor()) {
        mainCB.Emit("void _so_%s_static_dtor() {\n", &klass->FlatName());
        emitFunctionBody(methodBodyCB, klass->StaticDtor());
        mainCB.Emit( "}\n");
    }
}

void SEmitter::emit()
{
    const CArrayList<CClass*>* klasses = domain->Classes();

    // ********************************
    // Emits basic runtime definitions.
    // ********************************

    mainCB.Emit("#define _so_bool int\n"
                "#define _soX_TRUE 1\n"
                "#define _soX_FALSE 0\n"

                // NOTE WARNING A fix for the probably broken TCC codegen.
                // Returning a short from a function and immediately comparing it to a value
                // doesn't work correctly.
                // Perhaps not a bug, but a GCC<=>TCC interop issue as this happens only for icalls.
                "#define _so_char int\n"

                "#define _soX_GET_FIELD(className, fieldName) (self->_so_ ## className ## _ ## fieldName)\n"
                "#define _soX_GET_FIELD2(className, fieldName) (self._so_ ## className ## _ ## fieldName)\n"
                "#define _so_int_to(from, to) (_so_Range_create(from, to))\n"
                "#define _so_int_upto(from, to) (_so_Range_create(from, (to) + 1))\n"
                "#define _so_int_toFloat(i) ((float)(i))\n"
                "#define _so_float_toInt(f) ((int)(f))\n"
                "#define _soX_TN if(!self) _soX_abort0(2);\n");

    // Some shortcuts to emit less code.
    mainCB.Emit("#define _soX_ALIGNED __attribute__ ((aligned(sizeof(void*))))\n");

    // This is needed for a faster access to "array::length".
    mainCB.Emit("struct _soX_ArrayHeader {\n"
                "void** _soX_vtable;\n"
                "int _soX_length;\n"
                "char _soX_firstItem _soX_ALIGNED;\n"
                "};\n"
                "#define _soX_ARRLENGTH(self) (((struct _soX_ArrayHeader*)self)->_soX_length)\n");

    // All closures share the same structure.
    mainCB.Emit("struct _soX_0Closure {\n"
                "void** _soX_vtable;\n"
                "struct _so_any* _soX_env _soX_ALIGNED;\n"
                "void* m_codeOffset _soX_ALIGNED;\n"
                "};\n");

    // Logical bool operations "not/and/not" are implemented using macros.
    mainCB.Emit("#define _so_bool_and(x, y) ((x) && (y))\n"
                "#define _so_bool_or(x, y) ((x) || (y))\n"
                "#define _so_bool_not(x) (!(x))\n");

    // Runtime helpers.
    mainCB.Emit("extern void* _soX_gc_alloc(void* mm, int sz, void** vtable);\n"
                "extern void* _soX_gc_alloc_env(void* mm, void* objClass);\n"
                "extern void _soX_gc_roots(void* mm, void** rootRefs, int count);\n"
                "extern void _soX_static_vt(void* mm, void* obj, void* objClass);\n"
                "extern void _soX_regvtable(void* klass, void** vtable);\n"
                "extern void _soX_patchstrings();\n"
                "extern void* _soX_downcast(void* klass, void* objptr);\n"
                "extern void _soX_unbox(void* vt, int vtSize, void* vtClass, void* intrfcObj);\n"
                "extern void* _soX_findmethod(void* objptr, void* method);\n"
                "extern void* _soX_findmethod2(void* objptr, void* msg);\n"
                "extern _so_bool _soX_is(void* obj, void* type);\n"
                "extern void _soX_zero(void* a, int sz);\n"
                "extern _so_bool _soX_biteq(void* a, void* b, int sz)\n;"
                "extern void* _soX_newarray(int arrayLength, void** vtable);\n"
                "extern void _soX_abort0(int errCode);\n"
                "extern void _soX_abort_e(void* errObj);\n"
                "extern void _soX_cctor(void* pClass, void* cctor);\n"
                "extern void _soX_checktype(void* pClass);\n"
                "extern void _soX_addhandler(void* event, void* handler);\n"
                "extern void _soX_msgsnd_sync(void* hDomain, void* soObjName, void* method, void** args, void* blockingRet);\n"
                "extern void _soX_unpack(void** args, void* daMsg, void* method);\n"
                "extern int _so_int_op_divide(int a, int b);\n");

    if(domain->StackTraceEnabled()) {
        mainCB.Emit("extern void _soX_pushframe(void* domain, void* method);\n"
                    "extern void _soX_popframe(void* domain);\n");
    }
    if(domain->ProfilingEnabled()) {
        mainCB.Emit("extern int _soX_pushframe_prf(void* domain, void* method);\n"
                    "extern void _soX_popframe_prf(void* domain, int tc);\n");
    }
    if(domain->SoftDebuggingEnabled()) {
        mainCB.Emit("extern void _soX_break();\n"
                    "extern void _soX_reglocals(void** localRefs, int sz);\n"
                    "extern void _soX_unreglocals();\n");
    }

    // **************************************************************************
    // Emits structs.
    //
    // FIX Value-types are emitted first, to avoid bugs in the one-pass TCC
    // when a reference type references a valuetype whose body was never defined
    // at that point -- TCC silently crashes with no error in that case.
    // **************************************************************************

    // First, valuetypes.

    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        if(klass->IsValueType()) {
            emitStructHeader(klass, false);
        }
    }

    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        if(klass->IsValueType()) {
            emitStructHeader(klass, true);
        }
    }

    // Second, reference classes.

    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        if(!klass->IsValueType()) {
            emitStructHeader(klass, false);
        }
    }
    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        if(!klass->IsValueType()) {
            emitStructHeader(klass, true);
        }
    }

    // ***********************
    // Emits function headers.
    // ***********************

    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        emitFunctionHeaders(klass);
    }

    // **************************************
    // Emits vtables that refer to functions.
    // **************************************

    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        emitVTable(klass);
    }

    // *************************
    // Emits array init helpers.
    // *************************

    {
        CArrayInitializationType* initType;
        int helperId;
        SHashMapEnumerator<CArrayInitializationType*, int> mapEnum (domain->ArrayInitHelperRegistry());
        while(mapEnum.MoveNext(&initType, &helperId)) {
            emitArrayInitHelper(initType, helperId);
        }
    }

    // *****************************************************
    // Emits identity comparison code for valuetypes
    // (the C backend doesn't support comparison of structs)
    // *****************************************************

    {
        SStringSlice k;
        CClass* valueTypeClass;
        SHashMapEnumerator<SStringSlice, CClass*> mapEnum (domain->IdentityComparisonHelpers());
        while(mapEnum.MoveNext(&k, &valueTypeClass)) {
            emitIdentCompHelper(valueTypeClass);
        }
    }

    // ********************
    // Emits unbox helpers.
    // ********************

    {
        SStringSlice k;
        CClass* boxedClass;
        SHashMapEnumerator<SStringSlice, CClass*> mapEnum (domain->BoxedClasses());
        while(mapEnum.MoveNext(&k, &boxedClass)) {
            emitUnboxHelper(boxedClass);
        }
    }

    // **********************
    // Emits function bodies.
    // **********************

    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        emitFunctionBodies(klass);
    }

    // *********************************
    // Emits remote method server stubs.
    // *********************************
    {
        CClass* foreignProxyClass;
        SHashMapEnumerator<SStringSlice, CClass*> mapEnum (domain->ForeignProxies());
        while(mapEnum.MoveNext(nullptr, &foreignProxyClass)) {
            const CArrayList<CMethod*>* instanceMethods = foreignProxyClass->InstanceMethods();
            
            for(int i = 0; i < instanceMethods->Count(); i++) {
                const CMethod* m = instanceMethods->Array()[i];

                if(m->SpecialMethod() == E_SPECIALMETHOD_FOREIGNSYNC) {
                    emitRemoteMethodServerStubSync(m, foreignProxyClass);
                } else {
                    // TODO ?
                }
            }
        }
    }

    // **********************************************
    // Emits the program prolog (calls static ctors).
    // **********************************************

    mainCB.Emit("void _soX_prolog() {\n");

    // Registers vtables.
    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];
        if(klass->EmitVTable() && klass->HasVTable()) {
            mainCB.Emit("_soX_regvtable((void*)%p, _soX_vtbl_%s);\n", klass, &klass->FlatName());
        }
    }

    // Patches string literals.
    mainCB.Emit("_soX_patchstrings();\n");

    // Static ctors employ a two-stage system.
    // The first stage: registers static fields as gc roots.
    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];

        if(klass->StaticCtor() || klass->StaticFields()->Count() > 0) {
            mainCB.Emit("_so_%s_static_ctor(0);\n", &klass->FlatName());
        }
    }

    // The second stage: calls user-defined logic.
    // NOTE how user-defined logic is wrapped by _soX_cctor (see).
    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];

        if(klass->StaticCtor()) {
            mainCB.Emit("_soX_cctor((void*)%p, &_so_%s_static_ctor);\n", (void*)klass, &klass->FlatName());
        }
    }

    mainCB.Emit("}\n");

    // **********************************************
    // Emits the program epilog (calls static dtors).
    // **********************************************

    mainCB.Emit("void _soX_epilog() {\n");
    for(int i = 0; i < klasses->Count(); i++) {
        const CClass* klass = klasses->Array()[i];

        if(klass->StaticDtor()) {
            mainCB.Emit("_so_%s_static_dtor(1);\n", &klass->FlatName());
        }
    }

    mainCB.Emit("}\n");
}

void SEmitter::emitArrayInitHelper(const CArrayInitializationType* initType, int helperId)
{
    // ***********
    //   Header.
    // ***********

    mainCB.Emit("static %t _soX_arrInitHelper_%d(", &initType->ArrayType, helperId);
    const STypeRef* subTypeRef = &initType->ArrayType.ResolvedClass->WrappedClass();
    for(int i = 0; i < initType->Arity; i++) {
        mainCB.Emit("%t _arg%d", subTypeRef, i);

        if(i < (initType->Arity - 1)) {
            mainCB.Emit(", ");
        }
    }
    mainCB.Emit(") {\n");

    // *********
    //   Body.
    // *********

    mainCB.Emit("%t self = (%t)_soX_newarray(%d, _soX_vtbl_%s);\n",
                             &initType->ArrayType,
                             &initType->ArrayType,
                             initType->Arity,
                             &initType->ArrayType.ResolvedClass->FlatName());

    for(int i = 0; i < initType->Arity; i++) {
        mainCB.Emit("_so_%s_set(self, %d, _arg%d);\n",
                                &initType->ArrayType.ResolvedClass->FlatName(),
                                i,
                                i);
    }

    mainCB.Emit("return self;\n"
                "}\n");
}

void SEmitter::emitIdentCompHelper(const CClass* klass)
{
    const STypeRef typeRef (klass->ToTypeRef());

    mainCB.Emit("_so_bool _soX_idco_%s(%t a, %t b) {\n"
                "return _soX_biteq(&a, &b, %d);\n"
                "}\n",
                &klass->FlatName(),
                &typeRef,
                &typeRef,
                klass->GCInfo().ContentSize);
}

void SEmitter::emitUnboxHelper(const CClass* boxedClass)
{
    SKIZO_REQ_EQUALS(boxedClass->SpecialClass(), E_SPECIALCLASS_BOXED);
    SKIZO_REQ_PTR(boxedClass->ResolvedWrappedClass());

    const STypeRef& subTypRef = boxedClass->WrappedClass();
    mainCB.Emit("%t _soX_unbox_%s(void* _obj) {\n"
                "%t _soX_r;\n"
                "_soX_unbox(&_soX_r, sizeof(%t), (void*)%p, _obj);\n"
                "return _soX_r;\n"
                "}\n",
                &subTypRef,
                &subTypRef.ResolvedClass->FlatName(),
                &subTypRef, &subTypRef,
                (void*)boxedClass->ResolvedWrappedClass());
}

void SkizoEmit(CDomain* domain, STextBuilder& cb)
{
    SEmitter emitter (domain, cb);
    emitter.emit();
}

} }

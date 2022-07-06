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

#ifndef METHOD_H_INCLUDED
#define METHOD_H_INCLUDED

#include "AccessModifier.h"
#include "Attribute.h"
#include "ECallDesc.h"
#include "Expression.h"
#include "Member.h"
#include "MetadataSource.h"
#include "MethodFlags.h"
#include "HashMap.h"
#include "Signature.h"
#include "SpecialMethod.h"
#include "StringSlice.h"
#include "ThunkInfo.h"

namespace skizo { namespace script {

enum EMethodKind
{
    E_METHODKIND_NORMAL,
    E_METHODKIND_CTOR,
    E_METHODKIND_DTOR
};

/**
 * Describes a method.
 * @warnning Implementation pecularity: instance methods of base classes are prepended to the list of methods in its
 * subclasses; however instance fields of base class are never prepended in subclasses.
 * @todo IMPORTANT don't forget to update the Clone function when adding new fields here!
 */
class CMethod: public CMember
{
public:
    /**
     * The class that declares the method.
     * For anonymous methods, the class is compiler-generated.
     */
    class CClass* DeclaringClass() const { return m_declaringClass; }
    void SetDeclaringClass(CClass* value) { m_declaringClass = value; }

    /**
     * If the method is defined in an extension, separately from the rest of the class, this field is non-null.
     * @note The class object this field refers to is not complete and can't be used as usual. It's not listed in
     * Domain::m_klasses or Domain::m_klassMap. It's just a container holding extended
     * methods. Its unique pointer is used to differentiate extension methods defined in different 'extend' definitions
     * because extension methods are allowed to call only those private methods which were defined in the same
     * surrounding 'extend' block.
     */
    CClass* DeclaringExtClass() const { return m_declaringExtClass; }
    void SetDeclaringExtClass(CClass* value) { m_declaringExtClass = value; }

    /**
     * Remembers where the method was declared for nicer errors.
     */
    const SMetadataSource& Source() const { return m_source; }
    SMetadataSource& Source() { return m_source; }

    bool ForceNoHeader() const { return m_flags & E_METHODFLAGS_FORCE_NO_HEADER; }
    bool IsAbstract() const { return m_flags & E_METHODFLAGS_IS_ABSTRACT; }
    bool IsTrulyVirtual() const { return m_flags & E_METHODFLAGS_IS_TRULY_VIRTUAL; }
    bool IsSelfCaptured() const { return m_flags & E_METHODFLAGS_IS_SELF_CAPTURED; }
    bool HasBreakExprs() const { return m_flags & E_METHODFLAGS_HAS_BREAK_EXPRS; }
    bool WasEverCalled() const { return m_flags & E_METHODFLAGS_WAS_EVER_CALLED; }
    bool ECallAttributesResolved() const { return m_flags & E_METHODFLAGS_ECALL_ATTRIBUTES_RESOLVED; }
    bool IsInferred() const { return m_flags & E_METHODFLAGS_IS_INFERRED; }
    bool IsInlinable() const { return m_flags & E_METHODFLAGS_IS_INLINABLE; }
    bool IsCompilerGenerated() const { return m_flags & E_METHODFLAGS_COMPILER_GENERATED; }

    /**
     * The name of the method.
     * @note Always "::invoke" for anonymous methods.
     */
    const SStringSlice& Name() const { return m_name; }
    void SetName(const SStringSlice& value) { m_name = value; }

    const CSignature& Signature() const { return m_sig; }
    CSignature& Signature() { return m_sig; }

    /**
     * @note Always NORMAL for anonymous methods.
     */
    EMethodKind MethodKind() const { return m_methodKind; }
    void SetMethodKind(EMethodKind value) { m_methodKind = value; }
    
    EAccessModifier Access() const { return m_access; }
    void SetAccess(EAccessModifier value) { m_access = value; }

    /**
     * See ESpecialMethod for more info.
     */
    ESpecialMethod SpecialMethod() const { return m_specialMethod; }
    void SetSpecialMethod(ESpecialMethod value) { m_specialMethod = value; }

    /**
     * Makes sense only if this method is an ECall or if it's compiled using ThunkJIT (= ThunkManager).
     */
    const SECallDesc& ECallDesc() const { return m_ecallDesc; }
    SECallDesc& ECallDesc() { return m_ecallDesc; }

    /**
     * The index at which the method is found in the virtual table.
     * @note Meaningless for non-virtual methods.
     */
    int VTableIndex() const { return m_vtableIndex; }
    void SetVTableIndex(int value) { m_vtableIndex = value; }

    /**
     * The base method in the virtual method chain
     */
    CMethod* BaseMethod() const { return m_baseMethod; }
    void SetBaseMethod(CMethod* value) { m_baseMethod = value; }

    /**
     * The parent method in the closure chain.
     */
    CMethod* ParentMethod() const { return m_parentMethod; }
    void SetParentMethod(CMethod* value) { m_parentMethod = value; }

    /**
     * The syntax tree of this method.
     */
    CBodyExpression* Expression() const { return m_expression; }
    void SetExpression(CBodyExpression* value) { m_expression.SetVal(value); }

    /**
     * A hash map that maps local names to locals.
     * Populated in the transformer.
     */
    skizo::collections::CHashMap<SStringSlice, CLocal*>* Locals() const { return m_locals; }

    /**
     * The attributes of the method.
     */
    skizo::collections::CArrayList<CAttribute*>* Attributes() const { return m_attrs; }

    /**
     * If this method contains closures which reference variables defined in this method, the transformer must
     * generate a "closure environment" and the emitter is to put such variables inside it.
     */
    CClass* ClosureEnvClass() const { return m_closureEnvClass; }
    void SetClosureEnvClass(CClass* value) { m_closureEnvClass = value; }

    /**
     * If this method is a simple getter, this value is set to the target field of the getter (perhaps, also a setter).
     * @note A simple getter may or may not be inlinable, check the E_METHODFLAGS_IS_INLINABLE flag
     */
    class CField* TargetField() const { return m_targetField; }

    SThunkInfo& ThunkInfo() const { return m_thunkInfo; }

    explicit CMethod(class CClass* declaringClass);

    EMethodFlags& Flags() { return m_flags; }
    
    int NumberOfCalls() const { return m_numberOfCalls; }
    int TotalTimeInMs() const { return m_totalTimeInMs; }
    void AddTotalTimeInMs(int delta) { m_totalTimeInMs += delta; }
    void AddNumberOfCalls(int delta) { m_numberOfCalls += delta; }

    // ********************************************************
    // Used by the Emitter in the function body emission phase.
    // ********************************************************

    SResolvedIdentType ResolveIdent(const SStringSlice& ident, bool includeParams = true) const;
    bool ExistsLocalFieldParam(const SStringSlice& ident) const;
    
    /**
     * Important to allow only such names that do not conflict with generated C code.
     */
    bool IsLegalVarName(const SStringSlice& ident) const;
    
    CParam* ParamByName(const SStringSlice& ident) const; // returns 0 if nothing
    CLocal* LocalByName(const SStringSlice& ident) const; // returns void if nothing
    CLocal* NewLocal(const SStringSlice& name, const STypeRef& typeRef);
    void SetMethodSig(const char* sig);

    /**
     * Returns the base method in the root of the hierarchy, or itself if there's no base method.
     */
    CMethod* UltimateBaseMethod() const;

    /**
     * Recursively checks in the closure chain if it's an unsafe method (anonymous methods inherit the unsafe context
     * of their parent methods).
     */
    bool IsUnsafe() const;

    /**
     * Recursively checks in the closure chain if the context of this closure is static (and therefore there's no "this").
     */
    bool IsStaticContext() const;

    void SetCBody(const char* cBody);
    void SetCBody(const skizo::core::CString* cBody);

    /**
     * Skizo grammar doesn't allow to directly refer to fields outside of the instance class, which forces a
     * programmer to wrap them with getter methods/properties. If we can detect that a method is a "simple getter",
     * the emitter can by-pass the usual slow method call semantics and emit a direct reference to the instance field.
     * Many getter methods are "simple", if they:
     * - return a value from an instance field and don't have any arguments
     * - the method is not truly virtual
     * Updates m_targetField if it's a simple getter.
     * @note Used by STransformer.
     * Assumes that the return type of the method was already inferred.
     */
    void InitSimpleGetter();

    /**
     * @note Different from initSimpleGetter() in that it allows any kind of getters, including unsafe stuff.
     * This method is used to report properties. initSimpleGetter's purpose is to provide additional heuristics
     * for code generation.
     * Used to implement _so_Type_propertiesImpl(..)
     * Implemented in Reflection.cpp
     */
    bool IsGetter(bool isStatic) const;

    /**
     * Checks if the method is a matching setter for a given getter. See :isGetter() for more info
     * Used to implement _so_Type_propertiesImpl(..)
     * @warning "getter" should be a verified getter, there's no check
     * Implemented in Reflection.cpp
     */
    bool IsSetterFor(const CMethod* getter) const;

    /**
     * Returns true if this method represents "invoke" method of a method class.
     */
    bool IsMethodClassInvoke() const;

    /**
     * Gets the target C name of this method.
     * Destroy with CString::FreeUtf8, can be used with Utf8Auto.
     */
    char* GetCName() const;

    /**
     * Checks if the target class is enclosing this method (useless for anything other than closures).
     * Note also that base classes of the target class pass as well.
     */
    bool IsEnclosedByClass(const class CClass* pTargetClass) const;

    /**
     * Checks if method "other" can invoke this method (access modifiers are respected)
     */
    bool IsAccessibleFromMethod(const CMethod* other) const;

    /**
     * If this method is a valid entrypoint for new domains?
     */
    bool IsValidEntryPoint() const;

    /**
     * Used by the emitter: there's no need to emit debugging code if a method is static && has no params && has no locals.
     */
    bool ShouldEmitReglocalsCode() const;

    /**
     * Is this method a truly virtual root of an hierarchy? If that's the case, emit a VCH (virtual call helper)
     *
     * @note Used by the emitter after everything is transformed.
     */
    bool ShouldEmitVCH() const;

    void ResolveECallAttributes();

    CLocal* LocalByIndex(int index) const;

    /**
     * Creates a shallow copy: expressions and parameters are shared.
     */
    CMethod* Clone() const;

    virtual EMemberKind MemberKind() const override;

    void AddAttributes(const skizo::collections::CArrayList<CAttribute*>* attributes);

    // ***************
    //   Reflection.
    // ***************

    /**
     * Dynamically invokes this method, accepts an array of any's allocated by Skizo's memory manager.
     * Implemented in Reflection.cpp and is used by the reflection system.
     */
    void* InvokeDynamic(void* thisObj, void* args) const;

    // ***************
    //    Remoting.
    // ***************

    /**
     * Returns the server stub implementation of this method (if any). Used by the remoting system.
     */
    void* GetServerStubImpl() const;

private:
    CClass* m_declaringClass;
    class CClass* m_declaringExtClass;
    SMetadataSource m_source;
    SStringSlice m_name;      
    CSignature m_sig;
    EMethodKind m_methodKind;
    EAccessModifier m_access;
    ESpecialMethod m_specialMethod;
    SECallDesc m_ecallDesc;
    int m_vtableIndex;
    CMethod* m_baseMethod;
    CMethod* m_parentMethod;
    skizo::core::Auto<CBodyExpression> m_expression;
    skizo::core::Auto<skizo::collections::CHashMap<SStringSlice, CLocal*> > m_locals;
    skizo::core::Auto<skizo::collections::CArrayList<CAttribute*> > m_attrs;
    class CClass* m_closureEnvClass;

    // NOTE Unused if profiling wasn't enabled via SDomainCreation::ProfilingEnabled.
    int m_numberOfCalls;
    int m_totalTimeInMs; // Average time is calculated as (m_totalTimeInMs/m_numberOfCalls)

    class CField* m_targetField;
    mutable SThunkInfo m_thunkInfo;
    EMethodFlags m_flags;

    mutable void* m_serverStubImpl;
};

} }

#endif // METHOD_H_INCLUDED

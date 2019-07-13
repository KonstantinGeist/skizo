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

#ifndef CLASS_H_INCLUDED
#define CLASS_H_INCLUDED

#include "Field.h"
#include "Method.h"
#include "NativeHeaders.h"
#include "Nullable.h"
#include "SharedHeaders.h"
#include "TypeRef.h"

/**
 * Extracts the class of a Skizo-allocated object.
 */
#define so_class_of(ptr) ((CClass*)(((SObjectHeader*)(ptr))->vtable[0]))

/**
 * Extracts an impl ptr to the virtual method at index "index".
 */
#define so_virtmeth_of(ptr, index) (((SObjectHeader*)(ptr))->vtable[(index) + 1])

namespace skizo { namespace core {
    class CString;
} }

namespace skizo { namespace script {

typedef int EClassFlags;
#define E_CLASSFLAGS_NONE 0
// If this flag is true, the type is a valuetype, i.e. it has copy semantics.
#define E_CLASSFLAGS_IS_VALUETYPE (1 << 0)
#define E_CLASSFLAGS_IS_STATIC (1 << 1)
#define E_CLASSFLAGS_IS_ABSTRACT (1 << 2)
#define E_CLASSFLAGS_IS_COMPGENERATED (1 << 3)
// Should we emit the vtable? Not used anymore by native classes, but still may be useful in the future.
#define E_CLASSFLAGS_EMIT_VTABLE (1 << 4)
// Signals if m_instanceMethods contains methods from the parent class, or not yet (used in ::MakeSureMethodsFinalized())
#define E_CLASSFLAGS_IS_METHODLIST_FINALIZED (1 << 5)
#define E_CLASSFLAGS_IS_SIZE_CALCULATED (1 << 6)
// Used by ::BorrowAttributes()
#define E_CLASSFLAGS_ATTRIBUTES_BORROWED (1 << 7)
// During the transformation phase, we don't want a class be added to the transformation queue twice.
#define E_CLASSFLAGS_IS_INFERRED (1 << 8)
// True by default; false if an abort is issued in the static constructor of the class.
#define E_CLASSFLAGS_IS_INITIALIZED (1 << 9)
// m_pvtbl isn't governed by TCC, get rid of it
#define E_CLASSFLAGS_FREE_VTABLE (1 << 10)

// *************
//   For maps.
// *************

typedef int (SKIZO_API * FHashCode)(void*);
typedef _so_bool (SKIZO_API * FEquals)(void*, void*);

// *******************
//   Class metadata.
// *******************

class CProperty: public skizo::core::CObject
{
public:
    skizo::core::Auto<CMethod> Getter;
    skizo::core::Auto<CMethod> Setter;
};

/**
 * Some classes are special, and are treated specially.
 */
enum ESpecialClass 
{
    E_SPECIALCLASS_NONE,
    E_SPECIALCLASS_ARRAY,
    E_SPECIALCLASS_FAILABLE,
    E_SPECIALCLASS_FOREIGN,
    E_SPECIALCLASS_BOXED,
    E_SPECIALCLASS_METHODCLASS,
    E_SPECIALCLASS_EVENTCLASS,
    E_SPECIALCLASS_INTERFACE,

    /**
     * For structs that have [nativeSize=N]
     */
    E_SPECIALCLASS_BINARYBLOB,

    E_SPECIALCLASS_ALIAS,

    E_SPECIALCLASS_CLOSUREENV // internal
};

struct SGCInfo
{
    /**
     * A list of offsets to object fields which reference heap-allocated objects, used to speed up GC.
     */
    int* GCMap;

    /**
     * The size of such map.
     */
    int GCMapSize;

    /**
     * Total size:
     *   a) used by the GC to know the size of the object
     *   b) used when calculating GC maps in CClass::CalcGCMap() for valuetypes (they have variable sizes)
     *   c) if the special class is "binary blob", this value stores the forced native size as specified by the [nativeSize]
     *      attribute. Attribute "[nativeSize]" forces a valuetype to be a binary blob of a certain size (used for interop
     *      with native code).
     *      Aborts if nativeSize=0 declared in the code.
     *      @note The value type must declare zero fields; otherwise, if [nativeSize=N] is found, aborts.
     */
    size_t ContentSize;

    /**
     * Used to estimate the size of one element in an array.
     * sizeof(void*) for references classes; equals to ContentSize for valuetypes.
     */
    size_t SizeForUse;

    SGCInfo();
    ~SGCInfo();
};

class CClass: public skizo::core::CObject
{
public:
    void* operator new(size_t size);
    void operator delete (void *p);

    explicit CClass(class CDomain* declaringDomain);
    virtual ~CClass();

    // *****************************
    //        Basic data.
    // *****************************

    CDomain* DeclaringDomain() const { return m_declaringDomain; }

    EPrimType PrimitiveType() const { return m_primType; }
    void SetPrimitiveType(EPrimType value) { m_primType = value; }

    /**
     * Packs several boolean fields to use less memory. See EClassFlags
     */
    EClassFlags& Flags() { return m_flags; }

    /**
     * See ESpecialClass
     */
    ESpecialClass SpecialClass() const { return m_specialClass; }
    void SetSpecialClass(ESpecialClass value) { m_specialClass = value; }

    /**
     * Base class.
     * Virtual methods' correct order and inherited fields are not linked in until everything is parsed first.
     */
    const STypeRef& BaseClass() const { return m_baseClass; }

    CClass* ResolvedBaseClass() const;
    STypeRef& BaseClass()  { return m_baseClass; }
    bool IsClassHierarchyRoot() const;
    void SetBaseClass(const STypeRef& typeRef) { m_baseClass = typeRef; }

    /**
     * If this class is an array class, then the returned value represents the subclass of the array class.
     * Say, if this class is "[int]", then the returned value points to "int".
     *
     * If this is a failable class, this field represents the wrapped class.
     * If this is a boxed class, this field represents the target type the boxed class wraps.
     * If this is an alias, this field points to the basetype.
     * If this is an event class, this field points to the generated handler class.
     */
    const STypeRef& WrappedClass() const { return m_wrappedClass; }

    CClass* ResolvedWrappedClass() const;
    void SetWrappedClass(const STypeRef& value) { m_wrappedClass = value; }
    STypeRef& WrappedClass() { return m_wrappedClass; }

    bool IsValueType() const { return m_flags & E_CLASSFLAGS_IS_VALUETYPE; }
    bool IsRefType() const { return !(m_flags & E_CLASSFLAGS_IS_VALUETYPE) && m_primType == E_PRIMTYPE_OBJECT; }
    bool IsStatic() const { return m_flags & E_CLASSFLAGS_IS_STATIC; }
    bool EmitVTable() const { return m_flags & E_CLASSFLAGS_EMIT_VTABLE; }
    bool IsAbstract() const { return m_flags & E_CLASSFLAGS_IS_ABSTRACT; }
    bool IsCompilerGenerated() const { return m_flags & E_CLASSFLAGS_IS_COMPGENERATED; }
    bool IsMethodListFinalized() const { return m_flags & E_CLASSFLAGS_IS_METHODLIST_FINALIZED; }
    bool AttributesBorrowed() const { return m_flags & E_CLASSFLAGS_ATTRIBUTES_BORROWED; }
    bool IsSizeCalculated() const { return m_flags & E_CLASSFLAGS_IS_SIZE_CALCULATED; }
    bool IsInferred() const { return m_flags & E_CLASSFLAGS_IS_INFERRED; }
    bool IsInitialized() const { return m_flags & E_CLASSFLAGS_IS_INITIALIZED; }
    bool FreeVTable() const { return m_flags & E_CLASSFLAGS_FREE_VTABLE; }

    /**
     * Tells if this class is "Error" or one of its descendants.
     */
    bool IsErrorClass() const;

    bool IsByValue() const;

    /**
     * Converts this class to a resolved typeref.
     */
    STypeRef ToTypeRef() const;

    // *******************************
    //        For reflection.
    // *******************************

    /**
     * For reflection: a cached pointer to a runtime-allocated object on the Skizo side.
     * @note The runtime-allocated object has to be GC-rooted forever when created (GC-rooted by the Skizo
	 * wrapper, re-check that!)
     * See icalls _so_Type_fromTypeHandle and _so_Type_setToTypeHandle
     */
    void* RuntimeObject() const { return m_pRuntimeObj; }
    void SetRuntimeObject(void* value) { m_pRuntimeObj = value; }

    /**
     * The "nice" name of the class. For example, the underlying (flat) name of a boxed integer class can be
     * "0Boxed_1"; this method, on the other hand, returns the actual name as it is found in the Skizo code ("int")
     * Used by reflection.
     * @note Initialized by ::makeSureNiceNameGenerated() (lazily initialized). So this value can be null.
     */
    const skizo::core::CString* NiceName() const;

    /**
     * Defines a dummy virtual method: a method without body.
     * For primitive classes (reflection + verification).
     *
     * @note Don't use operator names like "+" or "%", rather use neutral names like "op_add" and "op_modulo".
     */
    void DefICall(const SStringSlice& name, const char* methodSig, bool forceNoHeader /* = false*/);

    /**
     * Finds getters/setters and combines them into properties. A property is an emergent phenomenon
     * internally: two methods that look like getters/setters are automatically a property if they meet
     * certain criteria.
     * Implemented in Reflection.cpp
     */
    skizo::collections::CArrayList<CProperty*>* GetProperties(bool isStatic) const;

    /**
     * Remembers where the method was declared for nicer errors.
     *
     * @note "internal" access depends on it, too. If an internal method belongs to the declaring
	 * class defined in the same module as the method which calls it, then such method is given
	 * access to the internal method.
	 *
	 * @note Cross-domain method calls depend on it, too (checks class versions)
     */
    const SMetadataSource& Source() const { return m_source; }
    SMetadataSource& Source() { return m_source; }

    // *********************************
    //      Code generation-related.
    // *********************************

    /**
     * The internal, flat name. For example, the flat name of "[int]" can be 0Array_1.
     * Used internally by the machine code.
     */
    const SStringSlice& FlatName() const { return m_flatName; }
    void SetFlatName(const SStringSlice& value) { m_flatName = value; }

    /**
     * This field should usually be zero for anything other than built-in classes like "string" or "Array".
     * If this value is not zero, then the emitter emits this code instead of relying on the list of
     * fields.
     * @note No static fields automatically emitted.
     * @note The fields a struct def defines aren't automatically known to the GC! Don't forget about that.
     */
    const SStringSlice& StructDef() const { return m_structDef; }
    void SetStructDef(const SStringSlice& value) { m_structDef = value; }

    // *************************************
    //        Method call mechanisms.
    // *************************************

    /**
     * Emitted code in the prolog function associates metadata with generated vtables.
     * Virtual tables in some classes are allocated on demand.
     */
    void** VirtualTable() const { return m_pvtbl; }
    void SetVirtualTable(void** vtable) { m_pvtbl = vtable; }

    /**
     * Not all classes have vtables.
     */
    bool HasVTable() const;

    /**
     * A cache to match CMethod's to method ptrs in a fast way.
     * Used by _soX_findmethod(..)
     */
    void* TryGetMethodImplForInterfaceMethod(CMethod* intrfcMethod) const;
    void SetMethodImplForInterfaceMethod(CMethod* intrfcMethod, void* methodImpl);

    /**
     * The invoke method in method classes (faster access).
     */
    CMethod* InvokeMethod() const { return m_invokeMethod; }
    void SetInvokeMethod(CMethod* method) { m_invokeMethod = method; }

    // *************************************
    //             Members.
    // *************************************

    const skizo::collections::CArrayList<CField*>* InstanceFields() const { return m_instanceFields; }
    const skizo::collections::CArrayList<CField*>* StaticFields() const { return m_staticFields; }

    const skizo::collections::CArrayList<CMethod*>* InstanceCtors() const { return m_instanceCtors; }

    CMethod* StaticCtor() const { return m_staticCtor; }
    void SetStaticCtor(CMethod* ctor) { m_staticCtor.SetVal(ctor); }

    bool TryGetInstanceMethodByName(const SStringSlice& name, CMethod** out_method) const;

    const skizo::collections::CArrayList<CMethod*>* InstanceMethods() const { return m_instanceMethods; }
    const skizo::collections::CArrayList<CMethod*>* StaticMethods() const { return m_staticMethods; }

    CMethod* InstanceDtor() const { return m_instanceDtor; }
    void SetInstanceDtor(CMethod* dtor) { m_instanceDtor.SetVal(dtor); }

    CMethod* StaticDtor() const { return m_staticDtor; }
    void SetStaticDtor(CMethod* dtor) { m_staticDtor.SetVal(dtor); }

    const skizo::collections::CArrayList<CConst*>* Constants() const { return m_consts; }
    
    /**
     * A set of all class-level names known so far for faster access/name collision verification.
     */
    skizo::collections::SHashMapEnumerator<SStringSlice, CMember*> GetNameSetEnumerator() const;
    void AddToNameSet(const SStringSlice& name, CMember* member);

    // **************************
    //       GC-related.
    // **************************

    const SGCInfo& GCInfo() const { return m_gcInfo; }
    SGCInfo& GCInfo() { return m_gcInfo; }

	/**
     * Calculates the GC map of this class. See SGCInfo::GCMap
     */
    void CalcGCMap();

    /**
     * Retrieves a pointer to the destructor (if any) implementation in machine code.
     * Used by the GC to call destructors during the finalization phase.
     */
    void* DtorImpl() const;

    // ************************
    //   Member registration.
    // ************************

    void RegisterInstanceMethod(CMethod* method);
    bool TryRegisterInstanceMethod(CMethod* method);
    void RegisterInstanceField(CField* field);
    void RegisterStaticField(CField* field);
    void RegisterInstanceCtor(CMethod* method);
    void RegisterStaticMethod(CMethod* method);
    bool TryRegisterStaticMethod(CMethod* method);
    void RegisterConst(CConst* konst);
    
    // Just adds to the list of methods; doesn't register in the name set unlike RegisterXXX functions. TODO ?
    void AddInstanceCtor(CMethod* method);
    void AddStaticMethod(CMethod* method);
    void AddInstanceMethod(CMethod* method);
    void AddInstanceField(CField* field);
    void AddStaticField(CField* field);

    // **********************
    //   Member resolution.
    // **********************

    CMethod* StaticMethodOrCtor(const SStringSlice& name) const;

    CField* MyField(const SStringSlice& name, bool isStatic) const;
    CMethod* MyMethod(const SStringSlice& name, bool isStatic, EMethodKind methodKind) const;
    CConst* MyConst(const SStringSlice& name) const;

    // **************************
    //       Auxiliaries.
    // **************************

    /**
     * Checks for a cyclic dependency in this class. For example, if A inherits B, and B inherits A, there exists
	 * an infinite loop, a cyclic dependency, which can crash the transformer when it will try to follow the
	 * infinite chain.
     * Base classes should be resolved already.
     */
    void CheckCyclicDependencyInHierarchy(const CClass* pStartBase) const;

    /**
     * Makes sure virtual methods of this class are "finalized" i.e. virtual methods of the parent class are
	 * inserted into the list of virtual methods of this class with some reindexing.
     * Used by the transformer.
     */
    void MakeSureMethodsFinalized();

    /**
     * Recursively borrows attributes from parent classes. Called from the transformer, NEVER call it manually.
     */
    void BorrowAttributes();

    void AddAttributes(const skizo::collections::CArrayList<CAttribute*>* attrs);
    const skizo::collections::CArrayList<CAttribute*>* Attributes() const;

    /**
     * Used in STransformer::inferHierarchies() to deal with base dtors.
     */
    bool HasBaseDtors() const;

    /**
     * Gets Map-related methods, if any (used in the implementation of maps, which are built-in in Skizo).
     */
    void GetMapMethods(void* obj, FHashCode* out_hashCode, FEquals* out_equals) const;

    /**
     * Creates an incomplete method class, its signature should be manipulated afterwards.
     * It also has no source, no name etc.
     * Used by Parser and Domain during asynchronous method resolution.
     * @note Doesn't automatically registers the class inside the domain because it's incomplete.
     */
    static CClass* CreateIncompleteMethodClass(CDomain* domain);

    // ***************************************
    //    Type compatibility & consistency.
    // ***************************************

    /**
     * Checks if the passed method's signature is compatible with this method class (makes sense only if this
     * class is a method class).
     */
    bool IsMethodClassCompatibleSig(const CMethod* pMethod) const;

    // Methods for inspecting relationships between classes.
    SCastInfo GetCastInfo(const CClass* other) const;
    bool IsSubclassOf(const CClass* other) const;
    bool Is(const CClass* other) const;
    bool DoesImplementInterface(const CClass* intrfc) const;
    bool DoesImplementInterfaceNoCache(const CClass* intrfc) const;

    /**
     * Checks if a certain name already exists in the context of this class. Checks methods, field names, etc.
     * (see the implementation)
     */
    SResolvedIdentType ResolveIdent(const SStringSlice& ident);

    /**
     * Used by the parser, as it's parsing a class definition, it's checking if a given member name is unique.
     * @note Doesn't check if it's conflicting with class names, since this function is used by the parser as it goes
     * which may not have yet parsed all the classes. Checks so in the transformer.
     */
    void VerifyUniqueMemberName(const SStringSlice& name) const;
    bool IsMemberDefined(const char* name) const;

    // ****************************************
    //   Attribute-controled code generation
    // ****************************************

    /**
     * Attribute "[ptrWrapper]" generates a ctor and a dtor basing it on icalls called "(intptr)_so_%CLASS%_ctorImpl"
     * and "_so_%CLASS%_dtorImpl(intptr)" which refer to the pre-generated field m_ptr and which must be implemented
     * by the embedding code.
     */
    bool IsPtrWrapper() const;
    void AddPtrWrapperMembers();

    /**
     * Generates access methods for this field (field+getter/setter for reference types and field+getter for valuetypes).
	 * Nothing more.
     * @warning It's an internal method which should be called by the parser when it finds a field marked with the
	 * [property] or [event] attribute.
     * Paramater 'forceGetterOnly' is for events, they don't allow setters.
     */
    void AddAccessMethodsForField(CField* pField,
                                  const SStringSlice& propertyName,
                                  EAccessModifier access,
                                  bool forceGetterOnly = false);

    void AddEventField(CField* pField);

    /**
     * Remembers which fields were marked as events so that we can insert event creation logic into every
     * constructor of the current class during the transform phase (the transformation phase is required so that we were
     * sure the type of the field is an event class and nothing else + all the constructors are parsed and ready).
     * Appended in the parser (during field parsing), analyzed in STransformer::inferEventFields(..) and cleared
     * there as well.
     */
    const skizo::collections::CArrayList<CField*>* EventFields() const;
    void ClearEventFields();

    /**
     * @note doesn't addref out_attr
     * @note out_attr can be null
     */
    bool TryGetIntAttribute(const char* attrName,
                            CAttribute** out_attr,
                            int* out_value,
                            bool failIfTypesDontMatch) const;

    // *********************************************
    //   Serialization/deserialization + remoting.
    // *********************************************

    /**
     * Checks if this valuetype has references (which is disallowed for boxed valuetypes).
     */
    bool HasReferencesForRemoting() const;

    /**
     * Accepts an object, a buffer to serialize the object to, the buffer's size.
     * Returns the amount of written bytes.
     * Aborts if the buffer isn't large enough.
     * Returns < 0 on error.
     * Implemented in Remoting.cpp
     */
    int SerializeForRemoting(void* soObj, char* buf, int bufSize, struct SSerializationContext* context) const;

    /**
     * Accepts a buffer and the buffer's size.
     * Returns the amount of read bytes.
     * Aborts if the buffer isn't large enough.
     * Returns < 0 on error.
     * 'output' is a pointer to an output buffer. The entire content will be dumped if the object
     * is a valuetype; otherwise, a pointer to a new allocated heap object is stored.
     * Implemented in Remoting.cpp
     */
    int DeserializeForRemoting(char* buf, int bufSize, void* output) const;

    /**
     * Verifies that versions of classes defined in different domains actually do match:
     * during domain loading, someone might have changed the classes on the hard drives.
     *
     * Implemented, as everything about remoting, in Remoting.cpp
     */
    bool MatchesForRemoting(const CClass* pForeignClass) const;

private:
    // Inits m_niceName. Called by CDomain::ClassByNiceName(..) which lazily constructs nice names of classes
	// only if CDomain::ClassByNiceName is used (for example, in reflection code) + used by STextBuilder
    // Also lazily init'd in CProfilingInfo::_dumpImpl(..)
	// See also ::m_niceName.
	// NOTE The returned string isn't ref'd
    const skizo::core::CString* makeSureNiceNameGenerated() const;

private:
    CDomain* m_declaringDomain;
    EPrimType m_primType;
    EClassFlags m_flags;
    ESpecialClass m_specialClass;
    STypeRef m_baseClass;
    STypeRef m_wrappedClass;
    EAccessModifier m_access;

    void* m_pRuntimeObj;
    mutable skizo::core::Auto<const skizo::core::CString> m_niceName;
    SMetadataSource m_source;

    SStringSlice m_flatName;
    SStringSlice m_structDef;

    void** m_pvtbl;
    CMethod* m_invokeMethod;
    skizo::core::Auto<skizo::collections::CHashMap<void*, void*> > m_pIntrfcMethodToImplPtr;

    skizo::core::Auto<skizo::collections::CArrayList<CField*> > m_instanceFields;
    skizo::core::Auto<skizo::collections::CArrayList<CField*> > m_staticFields;
    skizo::core::Auto<skizo::collections::CArrayList<CMethod*> > m_instanceCtors;
    skizo::core::Auto<CMethod> m_staticCtor;
    skizo::core::Auto<skizo::collections::CArrayList<CMethod*> > m_instanceMethods;
    // Used by CClass::DoesImplementInterface(..). Set by MakeSureMethodsFinalized.
    skizo::core::Auto<skizo::collections::CHashMap<SStringSlice, CMethod*> > m_instanceMethodMap;
    skizo::core::Auto<skizo::collections::CArrayList<CMethod*> > m_staticMethods;
    skizo::core::Auto<CMethod> m_instanceDtor;
    skizo::core::Auto<CMethod> m_staticDtor;
    // List of consts defined in this class.
    // @note Few classes have consts, so we save some memory by lazily allocating this list only when needed.
    skizo::core::Auto<skizo::collections::CArrayList<CConst*> > m_consts;
    skizo::core::Auto<skizo::collections::CHashMap<SStringSlice, CMember*> > m_nameSet;

    SGCInfo m_gcInfo;

    skizo::core::Auto<skizo::collections::CArrayList<CAttribute*> > m_attrs;
    // Precached pointer to the machine code implementation of the destructor (if any).
    // Populated by dtorImpl()
    // Don't use directly.
    mutable void* m_dtorImpl;
    skizo::core::Auto<skizo::collections::CArrayList<CField*> > m_eventFields;

    // A cache to speed up the DoesImplementInterface function.
    // Maps interfaces to true (implements) or false (doesn't implement).
    // Holds a collection of CClass's. Created on demand.
    mutable skizo::core::Auto<skizo::collections::CHashMap<const void*, bool> > m_interfaceCache;

    mutable skizo::core::SNullable<bool> m_hasReferencesForRemoting;

    // Cached for GetMapMethods(..)
    mutable FHashCode m_hashCodeImpl;
    mutable FEquals m_equalsImpl;
};

} }

#endif // CLASS_H_INCLUDED

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

#include "Template.h"
#include "ArrayList.h"
#include "Domain.h"
#include "HashMap.h"
#include "Method.h"
#include "RuntimeHelpers.h"
#include "StringBuilder.h"
#include "Variant.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

namespace
{

// CMethod::InvokeDynamic only reads data without retaining it. The trick to avoid stressing GC when boxing
// int values is to allocate the boxed int on the runtime heap.
// CMethod::InvokeDynamic won't see the difference between an object on the GC heap and the runtime heap.
struct SPseudoBoxedInt
{
    SPseudoBoxedInt()
        : m_boxedObject(nullptr)
    {
    }

    void TryInitialize(int value, CDomain* domain)
    {
        if(!m_boxedObject) {
            STypeRef intTypeRef;
            intTypeRef.SetPrimType(E_PRIMTYPE_INT);
            domain->ResolveTypeRef(intTypeRef);
            CClass* boxedIntClass = domain->BoxedClass(intTypeRef, true); // mustBeAlreadyCreated=true

            m_boxedObject = malloc(boxedIntClass->GCInfo().ContentSize);
            if(!m_boxedObject) {
                _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
            }

            ((SObjectHeader*)m_boxedObject)->vtable = boxedIntClass->VirtualTable();
        }

        void* boxedData = SKIZO_GET_BOXED_DATA(m_boxedObject);
        *((int*)boxedData) = value;
    }

    ~SPseudoBoxedInt()
    {
        free(m_boxedObject);
    }

    void* BoxedObject() const
    {
        return m_boxedObject;
    }

private:
    void* m_boxedObject;
};

// See SPseudoBoxedInt.
struct SPseudoArrayOfSingleAny
{
    SPseudoArrayOfSingleAny()
        : m_array(nullptr)
    {
    }

    void TryInitialize(CDomain* domain)
    {
        if(m_array) {
            return;
        }

        STypeRef anyArrayTypeRef;
        anyArrayTypeRef.SetObject(domain->NewSlice("any"));
        anyArrayTypeRef.ArrayLevel = 1;
        if(!domain->ResolveTypeRef(anyArrayTypeRef)) {
            domain->Abort("No runtime information is compiled in for [any]. Use `force [any]`.");
        }

        SArrayHeader* array = (SArrayHeader*)malloc(sizeof(SArrayHeader) + sizeof(void*));
        if(!array) {
            _soX_abort0(SKIZO_ERRORCODE_OUT_OF_MEMORY);
        }

        array->vtable = anyArrayTypeRef.ResolvedClass->VirtualTable();
        array->length = 1;
        memset(&array->firstItem, 0, sizeof(void*));

        m_array = array;
    }

    void SetElement(void* obj)
    {
        memcpy(&((SArrayHeader*)m_array)->firstItem, &obj, sizeof(void*));
    }

    ~SPseudoArrayOfSingleAny()
    {
        free(m_array);
    }

    void* Array() const
    {
        return m_array;
    }

private:
    void* m_array;
};

// A method with a captured argument (if any). For instance, {0} is translated to (get 0),
// so we have to remember the argument (0).
class CMethodWithArgument: public CObject
{
public:
    CMethodWithArgument(CMethod* method)
    {
        m_method.SetVal(method);
        m_domain = method->DeclaringClass()->DeclaringDomain();
    }

    CMethodWithArgument(CMethod* method, int arg)
        : CMethodWithArgument(method)
    {
        m_argument.SetInt(arg);
    }

    CMethodWithArgument(CMethod* method, const CString* arg)
        : CMethodWithArgument(method)
    {
        m_argument.SetObject(arg);
    }

    CMethod* Method() const
    {
        return m_method;
    }

    void* InvokeDynamic(void* obj)
    {
        const EVariantType argType = m_argument.Type();

        if(argType == E_VARIANTTYPE_NOTHING) {
            return m_method->InvokeDynamic(obj, nullptr);
        } else {
            m_anyArray.TryInitialize(m_domain);

            if(argType == E_VARIANTTYPE_OBJECT) {

                // String objects can be retained by the underlying method, so we have to create a real
                // GC-managed object.
                const CString* strArg = dynamic_cast<const CString*>(m_argument.ObjectValue());
                SKIZO_REQ_PTR(strArg);
                void* strObj = m_domain->CreateString(strArg, true);

                m_anyArray.SetElement(strObj);
                return m_method->InvokeDynamic(obj, m_anyArray.Array());
                
            } else if(argType == E_VARIANTTYPE_INT) {

                // Boxed integers are supposed to be temporary, they cannot be retained by the underlying method
                // (as it only sees by-value integers), hence we can use a pseudo-object outside of the GC heap.
                m_pseudoBoxedInt.TryInitialize(m_argument.IntValue(), m_domain);
                m_anyArray.SetElement(m_pseudoBoxedInt.BoxedObject());
                return m_method->InvokeDynamic(obj, m_anyArray.Array());

            } else {
                SKIZO_REQ_NEVER;
                return nullptr; // to shut up the compiler
            }
        }
    }
    
private:
    Auto<CMethod> m_method;
    SVariant m_argument;

    CDomain* m_domain;

    // Cached 
    SPseudoArrayOfSingleAny m_anyArray;
    SPseudoBoxedInt m_pseudoBoxedInt;
};

class CTemplatePart: public CObject
{
public:
    virtual void Output(void* obj, CStringBuilder* sb) const = 0;
};

class CStaticTemplatePart: public CTemplatePart
{
public:
    explicit CStaticTemplatePart(const CString* literal)
    {
        m_literal.SetVal(literal);
    }

    virtual void Output(void* /*obj*/, CStringBuilder* sb) const override
    {
        sb->Append(m_literal);
    }

private:
    Auto<const CString> m_literal;
};

class CDynamicTemplatePart: public CTemplatePart
{
public:
    CDynamicTemplatePart(CDomain* domain, const CArrayList<CMethodWithArgument*>* methods)
        : m_domain(domain)
    {
        m_methods.SetVal(methods);
    }

    virtual void Output(void* obj, CStringBuilder* sb) const override
    {
        void* tmp = obj;
        for(int i = 0; i < m_methods->Count(); i++) {
            CMethodWithArgument* methodWithArg = m_methods->Array()[i];

            // WARNING can abort
            tmp = methodWithArg->InvokeDynamic(tmp);
        }

        Auto<const CString> str (convertObjectToString(tmp));
        if(str) {
            sb->Append(str);
        }
    }

private:
    // Finds the `toString` method in a given class.
    static CMethod* toStringMethodInClass(CClass* objClass)
    {
        CMethod* toStringMethod;

        CDomain* domain = objClass->DeclaringDomain();
        const SStringSlice toStringSlice (domain->NewSlice("toString"));
        if(!objClass->TryGetInstanceMethodByName(toStringSlice, &toStringMethod)) {
            domain->Abort("Object has no `toString` method.");
        }
        toStringMethod->Unref();

        return toStringMethod;
    }

    // NOTE `obj` is always boxed if it's a valuetype so the logic is simpler than
    // that of the debugger.
    const CString* convertObjectToString(void* obj) const
    {
        if(!obj) {
            return nullptr;
        }
        
        const CString* retValue = nullptr;
        
        CClass* objClass = so_class_of(obj);
        CDomain* domain = objClass->DeclaringDomain();

        // In a lot of cases, the property is going to be a string. Return it directly.
        if(objClass == domain->StringClass()) {
            retValue = ((SStringHeader*)obj)->pStr;
            retValue->Ref();
            return retValue;
        }

        if(!m_classToFuncPtrCache) {
            m_classToFuncPtrCache.SetPtr(new CHashMap<void*, void*>());
        }

        void* pToString;
        if(!m_classToFuncPtrCache->TryGet(objClass, &pToString)) {
            CMethod* toStringMethod = toStringMethodInClass(objClass);
            pToString = domain->GetFunctionPointer(toStringMethod);

            if(!pToString
            || toStringMethod->ECallDesc().CallConv != E_CALLCONV_CDECL
            || toStringMethod->Signature().Params->Count() != 0
            || toStringMethod->Signature().ReturnType.ResolvedClass != domain->StringClass())
            {
                domain->Abort("Object has no method `toString` with an appropriate signature.");
            }

            m_classToFuncPtrCache->Set(objClass, pToString);
        }

        typedef SStringHeader* (SKIZO_API *FToStringMethod)(const void* self);

        // WARNING Can throw. Avoid RAII in this method.
        SStringHeader* repr = ((FToStringMethod)pToString)(obj);
        if(repr) {
            retValue = repr->pStr;
            retValue->Ref();
        }

        return retValue;
    }

    CDomain* m_domain;
    Auto<const CArrayList<CMethodWithArgument*>> m_methods;

    // Function pointer retrieval may be quite slow (for TCC, it's also under a lock),
    // so we cache retrieved pointers here instead.
    mutable Auto<CHashMap<void*, void*>> m_classToFuncPtrCache;
};

}

// Both 'int' and 'string' implement interfaces 'any' and 'MapKey'. These types are hardcoded
// to support generic scenarios, including the Map class.
static bool isSuitableGetMethodArgumentType(const CClass* klass, const char* paramClassName)
{
    const SStringSlice& flatName = klass->FlatName();
    return flatName.EqualsAscii(paramClassName)
        || flatName.EqualsAscii("any")
        || flatName.EqualsAscii("MapKey");
}

static CMethod* getMethodForClass(const CClass* targetClass, const char* paramClassName)
{
    CDomain* domain = targetClass->DeclaringDomain();
    const SStringSlice getSlice (domain->NewSlice("get"));

    CMethod* method = targetClass->MyMethod(getSlice, false, E_METHODKIND_NORMAL);
    if(!method) {
        domain->Abort("No `get` method found.");
    }

    if(method->ECallDesc().CallConv != E_CALLCONV_CDECL
    || method->Signature().Params->Count() != 1
    || !isSuitableGetMethodArgumentType(method->Signature().Params->Item(0)->Type.ResolvedClass, paramClassName)
    || method->Signature().ReturnType.IsVoid())
    {
        domain->Abort("Object has no method `get` with an appropriate signature.");
    }

    return method;
}

static bool tryParseSingleQuoteString(const CString* input, const CString** output)
{
    const so_char16 SINGLE_QUOTE = SKIZO_CHAR('\'');

    if(input->Length() < 3) {
        return false;
    }
    const so_char16* chars = input->Chars();
    const int length = input->Length();
    if(chars[0] != SINGLE_QUOTE || chars[length - 1] != SINGLE_QUOTE) {
        return false;
    }

    for(int i = 1; i < length - 1; i++) {
        if(chars[i] == SINGLE_QUOTE) {
            return false;
        }
    }

    *output = input->Substring(1, length - 2);
    return true;
}

// If the element is a quoted string or a number, it is considered to be a `get` method
// with an argument. For instance, {0} is translated to (obj get 0). This way we can support
// also arrays and maps in templates, as well as any other custom type with a `get` method.
static CMethodWithArgument* tryGetGetMethodWithArgument(const CClass* klass, const CString* elem)
{
    // int argument

    int intArg;
    if(elem->TryParseInt(&intArg)) {
        CMethod* getMethod = getMethodForClass(klass, "int");
        return new CMethodWithArgument(getMethod, intArg);
    }

    // string argument

    const CString* stringArg;
    if(tryParseSingleQuoteString(elem, &stringArg)) {
        CMethod* getMethod = getMethodForClass(klass, "string");
        CMethodWithArgument* r = new CMethodWithArgument(getMethod, stringArg);
        stringArg->Unref(); 
        return r;
    }

    // no argument

    return nullptr;
}

static void addObjectPart(CArrayList<CTemplatePart*>* parts, const CString* literal, const CClass* klass)
{
    Auto<CArrayList<const CString*>> split (literal->Split(SKIZO_CHAR(' ')));
    if(split->Count() == 0) {
        CDomain::Abort("Empty placeholder not allowed.");
    }

    Auto<CArrayList<CMethodWithArgument*>> methods (new CArrayList<CMethodWithArgument*>());
    const CClass* tmpClass = klass;
    CDomain* domain = klass->DeclaringDomain();
    
    for(int j = 0; j < split->Count(); j++) {
        const CString* elem = split->Array()[j];
        
        CMethod* method;

        Auto<CMethodWithArgument> methodWithArg (tryGetGetMethodWithArgument(tmpClass, elem));
        if(methodWithArg) {
            methods->Add(methodWithArg);
            method = methodWithArg->Method();
        } else {
            method = tmpClass->MyMethod(domain->NewSlice(elem), false, E_METHODKIND_NORMAL);
            if(!method) {
                CDomain::Abort("Unknown method.");
            }

            if(method->Signature().Params->Count() != 0
            || method->Signature().ReturnType.IsVoid())
            {
                domain->Abort("Placeholder refers to a method which is not property-like.");
            }

            Auto<CMethodWithArgument> methodWithArg (new CMethodWithArgument(method));
            methods->Add(methodWithArg);
        }

        tmpClass = method->Signature().ReturnType.ResolvedClass;
    }

    Auto<CDynamicTemplatePart> part (new CDynamicTemplatePart(domain, methods));
    parts->Add(part);
}

struct TemplatePrivate
{
    Auto<const CClass> m_klass;
    Auto<const CArrayList<CTemplatePart*>> m_parts;
    Auto<CStringBuilder> m_sb;
    
    TemplatePrivate(const CClass* klass, const CArrayList<CTemplatePart*>* parts)
        : m_sb(new CStringBuilder())
    {
        m_klass.SetVal(klass);
        m_parts.SetVal(parts);
    }

};

CTemplate::CTemplate(TemplatePrivate* p)
    : p(p)
{
}

CTemplate::~CTemplate()
{
    delete p;
}

CTemplate* CTemplate::CreateForClass(const CString* source, const CClass* klass)
{
    Auto<CArrayList<CTemplatePart*>> parts (new CArrayList<CTemplatePart*>());

    int lastIndex = 0;
    bool isStatic = true;
    for(int i = 0; i < source->Length(); i++) {
        so_char16 c = source->Chars()[i];
        
        if(c == SKIZO_CHAR('{')) {
            if(!isStatic) {
                CDomain::Abort("nested '{' not allowed");
            }
            
            if(i != lastIndex) {
                Auto<const CString> literal (source->Substring(lastIndex, i - lastIndex));
                Auto<CTemplatePart> part (new CStaticTemplatePart(literal));
                parts->Add(part);
            }
            
            isStatic = false;
            lastIndex = i + 1;
        } else if(c == SKIZO_CHAR('}')) {
            if(isStatic) {
                CDomain::Abort("Nested '}' not allowed.");
            }
            
            if(i == lastIndex) {
                CDomain::Abort("Empty placeholder not allowed.");
            }
            
            Auto<const CString> literal (source->Substring(lastIndex, i - lastIndex));
            isStatic = true;
            lastIndex = i + 1;

            addObjectPart(parts, literal, klass);
        }
    }
    
    if(!isStatic) {
        CDomain::Abort("Unclosed placeholder.");
    }
    
    if(lastIndex < source->Length()) {
        Auto<const CString> remaining (source->Substring(lastIndex, source->Length() - lastIndex));
        Auto<CTemplatePart> part (new CStaticTemplatePart(remaining));
        parts->Add(part);
    }

    return new CTemplate(new TemplatePrivate(klass, parts));
}

const CString* CTemplate::Render(void* obj) const
{
    if(so_class_of(obj) != p->m_klass.Ptr()) {
        CDomain::Abort("The rendered object is of a wrong type.");
    }
    
    CStringBuilder* sb = p->m_sb;
    const CArrayList<CTemplatePart*>* parts = p->m_parts;

    try {
        for(int i = 0; i < parts->Count(); i++) {
            CTemplatePart* part = parts->Array()[i];
            part->Output(obj, sb);
        }
    } catch (...) {
        sb->Clear();
        throw;
    }

    const CString* retValue = sb->ToString();
    sb->Clear();
    return retValue;
}

} }

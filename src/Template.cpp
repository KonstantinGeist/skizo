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

// The trick to avoid stressing GC with creating a temporary array is to allocate it outside of the GC heap.
// User code won't ever see it; CMethod::InvokeDynamic won't see the difference between an object on the GC
// heap and the runtime heap.
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
    enum EDirectMethodCall {
        E_DIRECTMETHODCALL_NONE,
        E_DIRECTMETHODCALL_INT_TO_PTR, // void* method(void* self, int arg)
        E_DIRECTMETHODCALL_PTR_TO_PTR  // void* method(void* self, void* arg)
    };

    CMethodWithArgument(CMethod* method)
        : m_directFuncPtr(nullptr),
          m_directMethodCall(E_DIRECTMETHODCALL_NONE)
    {
        m_method.SetVal(method);
        m_domain = method->DeclaringClass()->DeclaringDomain();
    }

    CMethodWithArgument(CMethod* method, int arg)
        : CMethodWithArgument(method)
    {
        m_argument.SetInt(arg);

        const CSignature& sig = method->Signature();

        if(sig.ReturnType.IsHeapClass()
        && sig.Params->Count() == 1
        && sig.Params->Array()[0]->Type.PrimType == E_PRIMTYPE_INT)
        {
            m_directFuncPtr = m_domain->GetFunctionPointer(method);
            m_directMethodCall = E_DIRECTMETHODCALL_INT_TO_PTR;
        }
    }

    CMethodWithArgument(CMethod* method, const CString* arg)
        : CMethodWithArgument(method)
    {
        m_argument.SetObject(arg);

        const CSignature& sig = method->Signature();

        if(sig.ReturnType.IsHeapClass()
        && sig.Params->Count() == 1
        && sig.Params->Array()[0]->Type.IsHeapClass())
        {
            m_directFuncPtr = m_domain->GetFunctionPointer(method);
            m_directMethodCall = E_DIRECTMETHODCALL_PTR_TO_PTR;
        }
    }

    CMethod* Method() const
    {
        return m_method;
    }

    // An optimized dynamic call.
    void* InvokeDynamic(void* obj)
    {
        const EVariantType argType = m_argument.Type();

        if(m_directMethodCall == E_DIRECTMETHODCALL_NONE) {
            // The slower, more generic codepath for signatures with arbitrary return types
            // (the reflection call will box it appropriately).

            if(argType == E_VARIANTTYPE_NOTHING) {
                return m_method->InvokeDynamic(obj, nullptr);
            } else {
                m_anyArray.TryInitialize(m_domain);

                if(argType == E_VARIANTTYPE_OBJECT) {

                    void* strObj = allocInternedString();
                    m_anyArray.SetElement(strObj);
                    return m_method->InvokeDynamic(obj, m_anyArray.Array());
                    
                } else if(argType == E_VARIANTTYPE_INT) {
                    
                    // The first optimized solution which was tried was allocating boxed integers outside of the heap.
                    // However, 2 problems were found a) the fake boxed int can be retained in the target method in certain
                    // scenarios, which will lead to crashes b) VTables of boxed classes are generated on demand in ThunkManager.

                    if(!m_directFuncPtr) {
                        STypeRef typeRef;
                        typeRef.SetPrimType(E_PRIMTYPE_INT);
                        m_domain->ResolveTypeRef(typeRef);

                        CClass* boxedClass = m_domain->BoxedClass(typeRef, true); // mustBeAlreadyCreated=true, relies on template.skizo forcing boxed int
                        CMethod* ctor = boxedClass->MyMethod(m_domain->NewSlice("create"), true, E_METHODKIND_CTOR);

                        m_directFuncPtr = m_domain->GetFunctionPointer(ctor);
                    }

                    typedef void* (SKIZO_API *FBoxedIntCtor)(int param);
                    void* boxedIntObj = ((FBoxedIntCtor)m_directFuncPtr)(m_argument.IntValue());

                    m_anyArray.SetElement(boxedIntObj);
                    return m_method->InvokeDynamic(obj, m_anyArray.Array());

                } else {
                    SKIZO_REQ_NEVER;
                    return nullptr; // to shut up the compiler
                }
            }

        } else {
            // The faster code path for most common signatures.

            typedef void* (SKIZO_API *FIntToPtrMethod)(const void* self, int param);
            typedef void* (SKIZO_API *FPtrToPtrMethod)(const void* self, void* param);

            assert(m_directFuncPtr);

            switch(m_directMethodCall) {
                case E_DIRECTMETHODCALL_INT_TO_PTR: // array access optimization
                {
                    return ((FIntToPtrMethod)m_directFuncPtr)(obj, m_argument.IntValue());
                }
                case E_DIRECTMETHODCALL_PTR_TO_PTR: // map access optimization
                {
                    void* strObj = allocInternedString();
                    return ((FPtrToPtrMethod)m_directFuncPtr)(obj, strObj);
                }
                default:
                {
                    SKIZO_REQ_NEVER;
                    return nullptr; // to shut up the compiler
                }
            }
        }
    }

private:
    // String objects can be retained by the underlying method, so we have to create a real
    // GC-managed object.
    void* allocInternedString() const
    {
        const CString* strArg = dynamic_cast<const CString*>(m_argument.ObjectValue());
        SKIZO_REQ_PTR(strArg);
        return m_domain->CreateString(strArg, true);
    }

    Auto<CMethod> m_method;
    SVariant m_argument;

    CDomain* m_domain;

    // Cached.
    SPseudoArrayOfSingleAny m_anyArray;

    // For some common scenarios, we can optimize even further and avoid
    // expensive reflection calls by calling the function pointer directly.
    void* m_directFuncPtr;
    EDirectMethodCall m_directMethodCall;
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

static CArrayList<const CString*>* split(const CString* source)
{
    Auto<CArrayList<const CString*>> result (new CArrayList<const CString*>());

    int lastIndex = 0;
    bool quote = false;

    const so_char16* chars = source->Chars();
    const int length = source->Length();
    for(int i = 0; i < length; i++) {
        const so_char16 c = chars[i];

        if(!quote && c == SKIZO_CHAR(' ')) {
            if(i != lastIndex) {
                Auto<const CString> stringPart (source->Substring(lastIndex, i - lastIndex));
                result->Add(stringPart);
            }

            lastIndex = i + 1;
        } else if(c == SKIZO_CHAR('\'')) {
            if(!quote && i > 0 && chars[i - 1] != SKIZO_CHAR(' ')) {
                CDomain::Abort("A space required before a quote.");
            } else if(quote && i < length - 1 && chars[i + 1] != SKIZO_CHAR(' ')) {
                CDomain::Abort("A space is required after a quote.");
            }

            quote = !quote;
        }
    }

    if(quote) {
        CDomain::Abort("Unclosed quotation.");
    }

    if(length != lastIndex) {
        Auto<const CString> stringPart (source->Substring(lastIndex, length - lastIndex));
        result->Add(stringPart);
    }

    result->Ref();
    return result;
}

static void addObjectPart(CArrayList<CTemplatePart*>* parts, const CString* literal, const CClass* klass)
{
    Auto<CArrayList<const CString*>> stringParts (split(literal));
    if(stringParts->Count() == 0) {
        CDomain::Abort("Empty placeholder not allowed.");
    }

    Auto<CArrayList<CMethodWithArgument*>> methods (new CArrayList<CMethodWithArgument*>());
    const CClass* tmpClass = klass;
    CDomain* domain = klass->DeclaringDomain();
    
    for(int j = 0; j < stringParts->Count(); j++) {
        const CString* elem = stringParts->Array()[j];
        
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

// NOTE Can use boxed classes, but the user can't refer to the boxed class directly, hence it's ommited here.
// TODO should we support E_SPECIALCLASS_FOREIGN, E_SPECIALCLASS_ALIAS?
static bool isRenderableClass(const CClass* klass)
{
    if(klass->IsAbstract() || klass->IsStatic()) {
        return false;
    }

    const ESpecialClass special = klass->SpecialClass();
    switch(special) {
        case E_SPECIALCLASS_NONE:
        case E_SPECIALCLASS_ARRAY:
        case E_SPECIALCLASS_FAILABLE:
        case E_SPECIALCLASS_METHODCLASS:
            return true;
        default:
            return false;
    }
}

CTemplate* CTemplate::CreateForClass(const CString* source, const CClass* klass)
{
    if(!isRenderableClass(klass)) {
        CDomain::Abort("The class is not renderable.");
    }

    Auto<CArrayList<CTemplatePart*>> parts (new CArrayList<CTemplatePart*>());

    int lastIndex = 0;
    bool isStatic = true;
    for(int i = 0; i < source->Length(); i++) {
        const so_char16 c = source->Chars()[i];

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
    CClass* objClass = so_class_of(obj);
    if(objClass->SpecialClass() == E_SPECIALCLASS_BOXED) {
        objClass = objClass->ResolvedWrappedClass();
    }
    if(objClass != p->m_klass) {
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

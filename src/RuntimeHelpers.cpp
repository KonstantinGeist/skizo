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

#include "RuntimeHelpers.h"
#include "Abort.h"
#include "Application.h"
#include "Class.h"
#include "Contract.h"
#include "Domain.h"
#include "SharedHeaders.h"
#include "Stack.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // WARNING don't introduce RAII in any of runtime helpers: longjmp will
        // ignore automatic C++ objects on stack
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

extern "C" {

// ******************
//   _soX_newarray.
// ******************

void* SKIZO_API _soX_newarray(int arraySize, void** vtable)
{
    return CDomain::ForCurrentThread()->CreateArray(arraySize, vtable);
}

// *****************
//   Type-related.
// *****************

void SKIZO_API _soX_regvtable(void* klass, void** vtable)
{
    ((CClass*)klass)->SetVirtualTable(vtable);
}

void* SKIZO_API _soX_downcast(void* targetClass, void* objptr)
{
    if(!objptr) {
        return nullptr;
    }

    const CClass* pClassToCheck = so_class_of(objptr);

    if(pClassToCheck->Is((CClass*)targetClass)) {
        return objptr;
    } else {
        CDomain::Abort("Downcast failed.");
        return nullptr;
    }
}

// TODO assert that vtSize == vtClass->m_sizeForByVal?
void SKIZO_API _soX_unbox(void* vt, int vtSize, void* vtClass, void* intrfcObj)
{
    SKIZO_NULL_CHECK(intrfcObj);

    const CClass* pInputClass = so_class_of(intrfcObj);
    if(pInputClass->SpecialClass() != E_SPECIALCLASS_BOXED
    || pInputClass->ResolvedWrappedClass() != vtClass)
    {
        CDomain::Abort("Can't unbox the value to the target valuetype (underlying types don't match).");
    }

    memcpy(vt, SKIZO_GET_BOXED_DATA(intrfcObj), vtSize);
}

_so_bool SKIZO_API _soX_is(void* objptr, void* type)
{
    if(!objptr) {
        return _so_FALSE;
    }

    const CClass* pClass = so_class_of(objptr);

    // Boxed values delegate their powers to their wrapped classes.
    if(pClass->SpecialClass() == E_SPECIALCLASS_BOXED) {
        pClass = pClass->ResolvedWrappedClass();
        SKIZO_REQ_PTR(pClass);
    }

    return pClass->Is((CClass*)type);
}

_so_bool SKIZO_API _soX_biteq(void* a, void* b, int sz)
{
    return memcmp(a, b, sz) == 0;
}

void SKIZO_API _soX_zero(void* a, int sz)
{
    memset(a, 0, sz);
}

// **********************
//   _soX_patchstrings.
// **********************

// see "icalls/string.cpp" for more information on how string literals are managed
void SKIZO_API _soX_patchstrings()
{
    CDomain* pDomain = CDomain::ForCurrentThread();
    SMemoryManager& mm = pDomain->MemoryManager();
    void* pvtbl = pDomain->StringClass()->VirtualTable();
    SKIZO_REQ_PTR(pvtbl);

    const CArrayList<void*>* stringLiterals = mm.StringLiterals();
    for(int i = 0; i < stringLiterals->Count(); i++) {
        SStringHeader* str = (SStringHeader*)stringLiterals->Array()[i];
        str->vtable = (void**)pvtbl;
    }
}

// ********************
//   _soX_findmethod
// ********************

void* SKIZO_API _soX_findmethod(void* objptr, void* _pMethod)
{
    SKIZO_NULL_CHECK(objptr);

    CMethod* pMethod = (CMethod*)_pMethod;
    CClass* pClass = so_class_of(objptr);

    void* methodImpl = pClass->TryGetMethodImplForInterfaceMethod(pMethod);
    if(!methodImpl) {
        CMethod* instanceMethod = nullptr;
        pClass->TryGetInstanceMethodByName(pMethod->Name(), &instanceMethod);
        SKIZO_REQ_PTR(instanceMethod);
        instanceMethod->Unref();

        SKIZO_REQ(instanceMethod->Signature().Equals(&pMethod->Signature()), EC_INVALID_STATE);
        SKIZO_REQ_NOT_EQUALS(instanceMethod->VTableIndex(), -1);

        methodImpl = so_virtmeth_of(objptr, instanceMethod->VTableIndex());
        pClass->SetMethodImplForInterfaceMethod(pMethod, methodImpl);
    }

    return methodImpl;
}

// ***************
//      Abort.
// ***************

void SKIZO_API _soX_abort0(int errCode)
{
    CDomain::Abort(errCode);
}

void SKIZO_API _soX_abort(void* msg)
{
    char* _msg = so_string_of(msg)->ToUtf8();
    CDomain::Abort(_msg, true);
}

void SKIZO_API _soX_abort_e(void* _errObj)
{
    SErrorHeader* errObj = (SErrorHeader*)_errObj;
    if(errObj->message) {
        _soX_abort(errObj->message);
    } else {
        _soX_abort0(SKIZO_ERRORCODE_FAILABLE_FAILURE);
    }
}

// **************
//   _soX_cctor
// **************

typedef void (SKIZO_API * FCCtor)(int stage);
void SKIZO_API _soX_cctor(void* _pClass, void* cctor)
{
    CClass* klass = (CClass*)_pClass;

    SVirtualUnwinder unwinder (klass->DeclaringDomain());
    unwinder.Remember();

    try {
        ((FCCtor)cctor)(1);
    } catch (const SoDomainAbortException& e) {
        klass->Flags() &= ~E_CLASSFLAGS_IS_INITIALIZED;
        unwinder.Unwind();
    }
}

// ******************
//   _soX_checktype
// ******************

void SKIZO_API _soX_checktype(void* pClass)
{
    if(!((CClass*)pClass)->IsInitialized()) {
        _soX_abort0(SKIZO_ERRORCODE_TYPE_INITIALIZATION_ERROR);
    }
}

// ****************************************************
//     Frame registration & stackoverflow detection.
// ****************************************************

#ifdef SKIZO_WIN
    // TODO x86 only? check for x64
    // NOTE: The standard stack size on Win32 is 1 MB. We consider a stack bigger than 900k a stack overflow
    // in order to leave some space for internal functions to run which will abort the domain, print
    // the stack trace etc.
    #define SO_DETECT_STACK_OVERFLOW \
                        if(((char*)domain->MemoryManager().StackBase() - (char*)&_domain) > (900 * 1024)) \
                            _soX_abort0(SKIZO_ERRORCODE_STACK_OVERFLOW);
#else
    #error "Not implemented."
#endif

void SKIZO_API _soX_pushframe(void* _domain, void* pMethod)
{
    CDomain* domain = (CDomain*)_domain;
    SO_DETECT_STACK_OVERFLOW
    domain->PushFrame((CMethod*)pMethod);
}

void SKIZO_API _soX_popframe(void* domain)
{
    ((CDomain*)domain)->PopFrame();
}

int SKIZO_API _soX_pushframe_prf(void* _domain, void* _pMethod)
{
    CDomain* domain = (CDomain*)_domain;
    SO_DETECT_STACK_OVERFLOW

    CMethod* pMethod = (CMethod*)_pMethod;
    pMethod->AddNumberOfCalls(1);
    domain->PushFrame(pMethod);
    return (int)Application::TickCount(); // TODO outside of Win32 tickCount can be >int32
}

void SKIZO_API _soX_popframe_prf(void* domain, int tc)
{
    CMethod* pMethod = ((CDomain*)domain)->PopFrame();

    // TODO int<=>long cast Win32 only?
    int delta = (int)Application::TickCount() - tc;
    pMethod->AddTotalTimeInMs(delta);
}

// ***********************
//   Helpers for events.
// ***********************

void SKIZO_API _soX_addhandler(void* _event, void* handler)
{
    // NOTE Explicit null check for _event was already emitted; not for 'handler', though.
    SKIZO_NULL_CHECK(handler);

    CDomain* domain = CDomain::ForCurrentThread();
    SEventHeader* event = (SEventHeader*)_event;

	// Retrieves the target typeref for the array.
    // NOTE The array as defined in the event class is an array of abstract method classes, not specific
    // closure classes. This code relies on the fact that a closure is always a subclass of its target
    // method class.
    const CClass* genericMethodClass = so_class_of(handler)->ResolvedBaseClass();
    SKIZO_REQ_PTR(genericMethodClass); // to be sure

	// Creates an array of the required size.
    const int origElemCount = event->array? event->array->length: 0;
    SArrayHeader* newArray;
    {
        const STypeRef handlerTypeRef (genericMethodClass->ToTypeRef());
        newArray = (SArrayHeader*)domain->CreateArray(handlerTypeRef, origElemCount + 1);
    }
    if(!newArray) { // a check just in case
        CDomain::Abort("Couldn't allocate a backing array for the event (::addHandler(..)).");
    }

    // Now, we need to copy the previous handlers to the new array.
    if(origElemCount) {
        memcpy(&newArray->firstItem, &event->array->firstItem, sizeof(void*) * origElemCount);
    }
    // Adds the new handler to the end of the list.
    ((void**)&newArray->firstItem)[origElemCount] = handler;
    // Sets the new array as the new handler array. The old one is going to be garbage collected eventually.
    event->array = newArray;
}

// ****************************************
//   Safe methods with preemptive checks.
// ****************************************

int SKIZO_API _so_int_op_divide(int a, int b)
{
    if(!b) {
        CDomain::Abort("Division by zero.");
    }

    return a / b;
}

// *******************
//   Soft debugging.
// *******************

void SKIZO_API _soX_reglocals(void** localRefs, int sz)
{
    CStack<void*>* ddStack = CDomain::ForCurrentThread()->DebugDataStack();

    ddStack->Push((void*)localRefs);
    ddStack->Push((void*)sz);
}

void SKIZO_API _soX_unreglocals()
{
    CStack<void*>* ddStack = CDomain::ForCurrentThread()->DebugDataStack();

    ddStack->Pop();
    ddStack->Pop();
}

CWatchIterator::CWatchIterator(const CMethod* _pMethod, void** localRefs, int size)
    : pMethod(_pMethod),
      CurIndex(-1),
      LocalRefs(localRefs),
      Size(size),
      Name(nullptr)
{
    if(!pMethod->Signature().IsStatic) {
        this->Size--;
    }
}

CWatchIterator::~CWatchIterator()
{
    if(this->Name) {
        CString::FreeUtf8(this->Name);
        this->Name = nullptr;
    }
}

// IMPORTANT the order of variables should be synchronized with the emitter
bool CWatchIterator::NextWatch(SKIZO_WATCHINFO* watchInfo)
{
    if(this->CurIndex >= this->Size) {
        return false;
    }

    if(this->CurIndex == -1 && !pMethod->Signature().IsStatic) {
        watchInfo->name = (char*)"this";
        watchInfo->klass = (skizo_class)pMethod->DeclaringClass();
        watchInfo->varPtr = this->LocalRefs[0];
    } else {
        if(this->CurIndex == -1) {
            this->CurIndex = 0;
        }

        const int paramLimit = pMethod->Signature().Params->Count();
        CLocal* local;
        if(this->CurIndex < paramLimit) {
            local = pMethod->Signature().Params->Item(this->CurIndex);
        } else {
            local = pMethod->LocalByIndex(this->CurIndex - paramLimit);
        }

        if(this->Name) {
            CString::FreeUtf8(this->Name);
            this->Name = nullptr;
        }
        Auto<const CString> tmp (local->Name.ToString());
        this->Name = tmp->ToUtf8();

        watchInfo->name = this->Name;
        watchInfo->klass = (skizo_class)local->Type.ResolvedClass;
        watchInfo->varPtr = this->LocalRefs[pMethod->Signature().IsStatic? this->CurIndex: this->CurIndex + 1];
    }

    this->CurIndex++;
    return (this->CurIndex - 1) < this->Size;
}

}

SVirtualUnwinder::SVirtualUnwinder(CDomain* domain)
    : m_domain(domain),
      m_stackFrameCnt(-1),
      m_debugDataStackCnt(-1)
{
}

void SVirtualUnwinder::Remember()
{
    m_stackFrameCnt = m_domain->m_stackFrames->Count();
    m_debugDataStackCnt = m_domain->m_debugDataStack->Count();
}

void SVirtualUnwinder::Unwind()
{
    // forgot to call ::Remember()
    SKIZO_REQ(m_stackFrameCnt != -1 && m_debugDataStackCnt != -1, EC_INVALID_STATE);

    while(m_domain->m_stackFrames->Count() > m_stackFrameCnt) {
        m_domain->m_stackFrames->Pop();
    }
    while(m_domain->m_debugDataStack->Count() > m_debugDataStackCnt) {
        m_domain->m_debugDataStack->Pop();
    }
}

} }

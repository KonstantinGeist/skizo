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

#include "ThunkManager.h"
#include "Application.h"
#include "ArrayList.h"
#include "Domain.h"
#include "ExecutablePageAllocator.h"
#include "FastByteBuffer.h"
#include "RuntimeHelpers.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

struct ThunkManagerPrivate
{
    // Methods registered for JIT compilation by this ThunkJIT.
    bool m_isDirty;
    Auto<CArrayList<CMethod*> > m_closureCtors;
    Auto<CArrayList<CMethod*> > m_boxedMethods;
    Auto<CArrayList<CMethod*> > m_boxedCtors;

    // Generated names, to free them only after the domain is destroyed
    Auto<CArrayList<char*> > m_names;

    Auto<CExecutablePageAllocator> m_execAllocator;

    ThunkManagerPrivate();
    ~ThunkManagerPrivate();

    void* allocExecutableMem(int sz);
    void freeExecutableMem(void* v);

    void* compileClosureCtor(const CMethod* method);
    void* compileBoxedCtor(const CMethod* method);
};

ThunkManagerPrivate::ThunkManagerPrivate()
    : m_isDirty(false),
      m_closureCtors(new CArrayList<CMethod*>()),
      m_boxedMethods(new CArrayList<CMethod*>()),
      m_boxedCtors(new CArrayList<CMethod*>()),
      m_names(new CArrayList<char*>()),
      m_execAllocator(new CExecutablePageAllocator())
{
}

ThunkManagerPrivate::~ThunkManagerPrivate()
{
    for(int i = 0; i < m_names->Count(); i++) {
        CString::FreeUtf8(m_names->Array()[i]);
    }
}

void* ThunkManagerPrivate::allocExecutableMem(int sz)
{
    return m_execAllocator->AllocatePage(sz);
}

void ThunkManagerPrivate::freeExecutableMem(void* v)
{
    m_execAllocator->DeallocatePage(v);
}

SThunkManager::SThunkManager()
    : p(new ThunkManagerPrivate())
{

}

SThunkManager::~SThunkManager()
{
    delete p;
}

void SThunkManager::FreeThunk(void* thunk)
{
    p->freeExecutableMem(thunk);
}

    // *****************************************************
    //   Converts Skizo callbacks into C callbacks.
    // *****************************************************

// TODO x86-only
// TODO don't use message boxes, use hookable runtime events
// If the closure is found to be called on a different domain than that it was created on,
// it shows a message box with this information.
// NOTE This code path is active only if SDomainCreation::SafeCallbacks=true (which is false by default)
static __cdecl void closureChecker(void* origDomain)
{
    CDomain* domain = CDomain::ForCurrentThreadRelaxed();
    if(domain != (CDomain*)origDomain) {
    #ifdef SKIZO_WIN
        int result = MessageBox(NULL,
                                TEXT("A Skizo closure was found to be called on a foreign domain or thread (via native code).\n"
                                     "The current process can become unstable from now on. Continue?"),
                                TEXT("Skizo"),
                                MB_YESNO | MB_ICONERROR);
        if(result == IDNO) {
            Application::Exit(1);
        }
    #else
        // TODO ?
        CDomain::Abort("A Skizo closure was found to be called on a foreign domain or thread (via native code).");
    #endif
    }
}

void* SThunkManager::GetClosureThunk(void* soObj) const
{
    if(!soObj) {
        return nullptr;
    }

    CClass* klass = so_class_of(soObj);
    if(klass->SpecialClass() != E_SPECIALCLASS_METHODCLASS) {
        CDomain::Abort("Marshal::codeOffset expects a method class instance.");
    }

    // Maybe the thunk was already generated?
    SClosureHeader* header = (SClosureHeader*)soObj;
    if(header->codeOffset) {
        return header->codeOffset;
    }

    CMethod* invokeMethod = klass->InvokeMethod();
    assert(invokeMethod);

    Utf8Auto cName (invokeMethod->GetCName());
    void* pImpl = klass->DeclaringDomain()->GetSymbolThreadSafe(cName);

    if(!pImpl) {
        CDomain::Abort("The passed method class instance has no body.");
    }

    // Verifies that the closure has no non-primitive valuetypes -- different compilers have
    // different implementations, so it's not safe to use valuetypes like that anyway.
    const char* sigError = "Marshal::codeOffset doesn't support signatures with non-primitive valuetypes.";
    for(int i = 0; i < invokeMethod->Signature().Params->Count(); i++) {
        const CClass* paramClass = invokeMethod->Signature().Params->Array()[i]->Type.ResolvedClass;
        assert(paramClass);
        if(paramClass->PrimitiveType() == E_PRIMTYPE_OBJECT && paramClass->IsValueType()) {
            CDomain::Abort(sigError);
        }
    }
    {
        const CClass* retType = invokeMethod->Signature().ReturnType.ResolvedClass;
        assert(retType);
        if(retType->PrimitiveType() == E_PRIMTYPE_OBJECT && retType->IsValueType()) {
            CDomain::Abort(sigError);
        }
    }

    // TODO x86-only
    // WARNING Also assumes that none of parameters are bigger than word size.
    // A pretty slow implementation, however very straightforward.

    SFastByteBuffer bb (128);

    // PROLOG
    const so_byte prolog[3] = {
        0x55,       // push	ebp
        0x89, 0xE5, // mov ebp, esp
    };
    bb.AppendBytes(prolog, sizeof(prolog));

    // DOMAIN CHECK
    // Special code to check whether the closure is called on a correct thread (correct domain).
    CDomain* domain = CDomain::ForCurrentThread();
    if(domain->SafeCallbacks()) {
        const void* origDomain = (void*)domain;
        const void* pClosureChecker = (void*)&closureChecker;
        so_byte domain_check[18] = {
            0x68, 0x00, 0x00, 0x00, 0x00,      // push THIS_DOMAIN
            0xB8, 0x00, 0x00, 0x00, 0x00,      // mov eax, closureChecker
            0xFF, 0xD0,                        // call eax
            0x81, 0xC4, 0x04, 0x00, 0x00, 0x00 // add esp, 4 ; because C callconv
        };
        memcpy(&domain_check[1], &origDomain, sizeof(void*));
        memcpy(&domain_check[6], &pClosureChecker, sizeof(void*));
        bb.AppendBytes(domain_check, sizeof(domain_check));
    }

    // ARGS
    for(int i = invokeMethod->Signature().Params->Count() - 1; i >= 0; i--) {
        so_byte arg_stuff[6] = {
            0xFF, 0xB5, 0x0, 0x0, 0x0, 0x0      // push [ebp+N]
        };
        const void* offset = (void*)(i * sizeof(void*) + 8);
        memcpy(&arg_stuff[2], &offset, sizeof(void*));
        bb.AppendBytes(arg_stuff, sizeof(arg_stuff));
    }

    // THIS POINTER (the whole point of this function)
    so_byte this_pointer[5] = {
        0x68, 0x00, 0x00, 0x00, 0x00,  // push THIS
    };
    memcpy(&this_pointer[1], &soObj, sizeof(void*));
    bb.AppendBytes(this_pointer, sizeof(this_pointer));

    // FUNCTION CALL
    so_byte function_call[7] = {
        0xB8, 0x00, 0x00, 0x00, 0x00, // mov eax, CODE
        0xFF, 0xD0,                   // call eax
    };
    memcpy(&function_call[1], &pImpl, sizeof(void*));
    bb.AppendBytes(function_call, sizeof(function_call));

    // STACK CLEANUP
    if(invokeMethod->ECallDesc().CallConv == E_CALLCONV_CDECL) {
        so_byte stack_cleanup[6] = {
            0x81, 0xC4, 0x00, 0x00, 0x00, 0x00 // add esp, ARGSIZE
        };

        // NOTE cleanup adjusts for "this" as well
        const void* argsz = (void*)((invokeMethod->Signature().Params->Count() + 1) * sizeof(void*));
        memcpy(&stack_cleanup[2], &argsz, sizeof(void*));
        bb.AppendBytes(stack_cleanup, sizeof(stack_cleanup));
    }

    // EPILOG
    const so_byte epilog[2] = {
        0xC9, // leave
        0xC3  // ret
    };
    bb.AppendBytes(epilog, sizeof(epilog));

    void* v = p->allocExecutableMem(bb.Size());
    memcpy(v, bb.Bytes(), bb.Size());

    // Uncomment for debugging.
    /*FILE* f = fopen("closure.bin", "wb");
    fwrite(v, bb.Size(), 1, f);
    fclose(f);*/

    header->codeOffset = v;
    return v;
}

    // *******************************
    //   ThunkJIT-generated methods.
    // *******************************

void SThunkManager::AddMethod(class CMethod* method)
{
    switch(method->SpecialMethod()) {
        case E_SPECIALMETHOD_CLOSURE_CTOR:
            p->m_closureCtors->Add(method);
            break;

        case E_SPECIALMETHOD_BOXED_METHOD:
            p->m_boxedMethods->Add(method);
            break;

        case E_SPECIALMETHOD_BOXED_CTOR:
            p->m_boxedCtors->Add(method);
            break;

        default:
            assert(false);
            break;
    }

    p->m_isDirty = true;
}

// ***************************************************************

static __cdecl void boxedMethodJit(void* trampoline, void* pMethod)
{
    const CMethod* wrapperMethod = (CMethod*)pMethod;
    assert(!wrapperMethod->Signature().IsStatic);

    CDomain* domain = wrapperMethod->DeclaringClass()->DeclaringDomain();
    const CClass* wrappedClass = wrapperMethod->DeclaringClass()->ResolvedWrappedClass();
    assert(wrappedClass);
    if(wrappedClass->SpecialClass() == E_SPECIALCLASS_BINARYBLOB) {
        CDomain::Abort("Boxed wrapper for this class cannot be created."); // TODO ?
    }

    const CMethod* wrappedMethod = wrappedClass->MyMethod(wrapperMethod->Name(),
                                                          false, // non static
                                                          E_METHODKIND_NORMAL);
    assert(wrappedMethod);
    void* pWrappedMethod = domain->GetFunctionPointer(wrappedMethod);
    if(!pWrappedMethod) {
        CDomain::Abort("Boxed wrapper for this method cannot be created (the method is always inlined?)"); // TODO ?
    }

    // TODO assert that never was compiled before just in case

    SFastByteBuffer bb (128);

    // NOTE Prolog is already implemented by the compilation trampoline.

    // ***********************
    //   RE-PUSHES ARGUMENTS
    // ***********************

    // Let's estimate the total size of arguments.
    // It doesn't matter how exactly we push it, what matters is that we need it laid out neatly on the stack.
    // The arguments are to be pushed in reverse order, as required by cdecl.

    int hiddenBufferSize = 0;

    // If the method returns a valuetype, TCC creates a hidden buffer argument.
    const CClass* retClass = wrappedMethod->Signature().ReturnType.ResolvedClass;
    if(retClass->PrimitiveType() == E_PRIMTYPE_OBJECT && retClass->IsValueType()) {
        hiddenBufferSize = sizeof(void*);
    }

    int totalArgSize = 0;
    for(int i = wrappedMethod->Signature().Params->Count() - 1; i >= 0; i--) {
        const CParam* param = wrappedMethod->Signature().Params->Array()[i];
        const CClass* paramClass = param->Type.ResolvedClass;
        assert(paramClass);

        totalArgSize += paramClass->GCInfo().SizeForUse;
    }

    assert(totalArgSize % 4 == 0);
    assert(sizeof(void*) == 4);
    const int argCount = totalArgSize / 4; // granularity is 4 bytes

    // TODO the code is dumb, we can copy it directly, but to be sure it works, I do it this way.
    for(int i = argCount - 1; i >= 0; i--) {
        so_byte pushGranule[6] = {
            0xFF, 0xB5, 0xFF, 0xFF, 0xFF, 0xFF // push [ebp+N]
        };
        const int offset = 12 + i * 4 + hiddenBufferSize; // skips "self"
        memcpy(&pushGranule[2], &offset, sizeof(int));
        bb.AppendBytes(pushGranule, sizeof(pushGranule));
    }

    // Now we need to push the valuetype itself (self).
    int wrappedSelfSize = wrappedClass->GCInfo().ContentSize;
    assert(wrappedSelfSize % 4 == 0);

    so_byte pushSelf[8] = {
        0x8B, 0x45, 0x08,                      // mov eax, [ebp+8] // gets boxed "this"
        0x5, 0x4, 0x0, 0x0, 0x0                // add eax, 4       // skips the vtable
    };
    if(hiddenBufferSize) {
        // If there's a hidden buffer, then "this" is actually the second argument.
        pushSelf[2] = 0x0C;
    }
    bb.AppendBytes(pushSelf, sizeof(pushSelf));

    // Now pushes the valuetype's fields, in reverse order again.
    int wrappedSelfGranuleCount = wrappedSelfSize / 4;
    for(int i = wrappedSelfGranuleCount - 1; i >= 0; i--) {
        so_byte pushField[6] = {
            0xFF, 0xB0, 0x0, 0x0, 0x0, 0x0     // push [eax+0] ; will be patched below
        };
        const int offset = i * 4;
        memcpy(&pushField[2], &offset, sizeof(int));
        bb.AppendBytes(pushField, sizeof(pushField));
    }

    // Pushes the hidden buffer, if any.
    if(hiddenBufferSize) {
        const so_byte pushBuffer[3] = {
            0xFF, 0x75, 0x8 // push [ebp+8] ; always the first argument
        };
        bb.AppendBytes(pushBuffer, sizeof(pushBuffer));
    }

    // ***************
    //   METHOD CALL
    // ***************

    so_byte methodCall[7] = {
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,           // mov eax, FFFFFFFF ; to be patched below
        0xFF, 0xD0                              // call eax
    };
    memcpy(&methodCall[1], &pWrappedMethod, sizeof(void*));
    bb.AppendBytes(methodCall, sizeof(methodCall));

    // Argument clean up according to cdecl.
    int cleanupSize = totalArgSize + wrappedSelfSize + hiddenBufferSize;
    if(cleanupSize) {
        so_byte cleanup[6] = {
            0x81, 0xC4, 0x00, 0x00, 0x00, 0x00 // add esp, N
        };
        memcpy(&cleanup[2], &cleanupSize, sizeof(int));
        bb.AppendBytes(cleanup, sizeof(cleanup));
    }

    // **********
    //   EPILOG
    // **********

    const so_byte epilog[2] = {
        0xC9, // leave
        0xC3  // ret
    };
    bb.AppendBytes(epilog, sizeof(epilog));

    void* v = domain->ThunkManager().p->allocExecutableMem(bb.Size());
    memcpy(v, bb.Bytes(), bb.Size());

    // *************************************
    //   Patches the original trampoline.
    // *************************************

    so_byte patch[7] = {
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF, // mov eax, compiled function
        0xFF, 0xE0,                   // jmp eax
    };
    memcpy(&patch[1], &v, sizeof(void*));

    // The patch area of the trampoline starts at offset 4, see ::GetCompilationTrampoline.
    memcpy(&(((char*)trampoline)[4]), patch, sizeof(patch));

    // Uncomment for debugging.
    /*FILE* f = fopen("thunk.bin", "wb");
    fwrite(v, bb.Size(), 1, f);
    fclose(f);*/
    //exit(1);
}

    // *************
    //   BoxedCtor
    // *************

static __cdecl void* boxedCtorHelper(CMethod* method, void* arg)
{
    CClass* declClass = method->DeclaringClass();
    CDomain* declDomain = declClass->DeclaringDomain();

    // The VTable is created on demand, on the first use of the constructor.
    if(!declClass->VirtualTable()) {
        const CArrayList<CMethod*>* instanceMethods = declClass->InstanceMethods();

        void** vtable = new void*[instanceMethods->Count() + 1]; // +1 for the class pointer
        vtable[0] = declClass; // first slot - class pointer

        for(int i = 0; i < instanceMethods->Count(); i++) {
            vtable[i + 1] = declDomain->GetFunctionPointer(instanceMethods->Array()[i]);
            assert(vtable[i + 1]);
        }

        declClass->SetVirtualTable(vtable);

        // The GC map calculated on demand as well.
        declClass->CalcGCMap();
    }

    const CClass* wrappedClass = declClass->ResolvedWrappedClass();
    assert(wrappedClass);

    assert(declClass->GCInfo().ContentSize);
    assert(wrappedClass->GCInfo().ContentSize);

    void* obj = declDomain->MemoryManager().Allocate(declClass->GCInfo().ContentSize, // vtable + content size
                                                     declClass->VirtualTable());
    void* objData = SKIZO_GET_BOXED_DATA(obj);
    memcpy(objData, arg, wrappedClass->GCInfo().ContentSize);

    return obj;
}

// A boxed class' ctor will simply take a reference to its argument and pass it to a helper function.
void* ThunkManagerPrivate::compileBoxedCtor(const CMethod* method)
{
    so_byte cc[27] = {
        0x55,                           // push ebp
        0x89, 0xE5,                     // mov ebp, esp
        0x8D, 0x45, 0x08,               // lea eax, [ebp+8] ; take the first argument's pointer
        0x50,                           // push eax		    ; push it to the helper function
        0x68, 0xFF, 0xFF, 0xFF, 0xFF,   // push CMethod* that describes the boxed class ctor (to be patched)
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,   // mov eax, helper function (to be patched)
        0xFF, 0xD0,                     // call helper function
        0x81, 0xC4, 0x8, 0x0, 0x0, 0x0, // add esp, 8
        0xC9,                           // leave
        0xC3                            // ret
    };

    // Patches the pointer to the ctorInfo.
    memcpy(&cc[8], &method, sizeof(void*));

    // Patches the pointer to the helper method.
    const void* funcPtr = (void*)&boxedCtorHelper;
    memcpy(&cc[13], &funcPtr, sizeof(void*));

    void* v = allocExecutableMem(sizeof(cc));
    memcpy(v, cc, sizeof(cc));

    // Uncomment for debugging.
    /*FILE* f = fopen("boxedctor.bin", "wb");
    fwrite(v, 27, 1, f);
    fclose(f);*/

    return v;
}

    // ***************
    //   ClosureCtor
    // ***************

static void* closureCtorHelper(CMethod* method, void* env)
{
    CClass* declClass = method->DeclaringClass();
    CDomain* declDomain = declClass->DeclaringDomain();

    // The VTable is created on demand, on the first use of the constructor.
    if(!declClass->VirtualTable()) {
        void** vtable = new void*[2];
        vtable[0] = declClass; // first slot - class pointer
        assert(declClass->InvokeMethod());
        // the second and the only method slot -- "invoke" method.
        vtable[1] = declDomain->GetFunctionPointer(declClass->InvokeMethod());
        assert(vtable[1]);

        declClass->SetVirtualTable(vtable);

        // To be sure...
        declClass->CalcGCMap();
    }

    SClosureHeader* obj = (SClosureHeader*)declDomain->MemoryManager().Allocate(sizeof(SClosureHeader), declClass->VirtualTable());
    obj->env = env;
    return obj;
}

// TODO x86-only
// Closure constructor forwards everything to a special helper, closureCtorHelper(..), see above.
void* ThunkManagerPrivate::compileClosureCtor(const CMethod* method)
{
    so_byte cc[26] = {
        0x55,                                   // push ebp
        0x89, 0xE5,                             // mov epb, esp

        0xFF, 0x75, 0x8,                        // push [ebp+8] ; pushes the "env" argument
        0x68, 0x0, 0x0, 0x0, 0x0,               // push 0 ; CMethod pointer, to be patched

        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,           // mov eax, FFFFFFFF ; to be patched below
        0xFF, 0xD0,                             // call eax
        0x81, 0xC4, 0x08, 0x00, 0x00, 0x00,     // add esp, 8 ; required by cdecl

        0xC9,                                   // leave
        0xC3                                    // ret
    };

    // Patches the pointer to the CMethod.
    memcpy(&cc[7], &method, sizeof(void*));
    // Patches the pointer to the helper method.
    void* funcPtr = (void*)&closureCtorHelper;
    memcpy(&cc[12], &funcPtr, sizeof(void*));

    void* v = allocExecutableMem(sizeof(cc));
    memcpy(v, cc, sizeof(cc));

    /*FILE* f = fopen("thunk.bin", "wb");
    fwrite(v, 26, 1, f);
    fclose(f);*/

    return v;
}

    // ***************
    //   Reflection.
    // ***************

// TODO x86 only
void* SThunkManager::GetReflectionThunk(const CMethod* method) const
{
    if(method->ECallDesc().CallConv != E_CALLCONV_CDECL) {
        return nullptr;
    }

    // Remembered stuff.
    {
        const SThunkInfo& thunkInfo = method->ThunkInfo();
        if(thunkInfo.pReflectionThunk) {
            return thunkInfo.pReflectionThunk;
        }
    }

    SFastByteBuffer bb (128);

        // ******
        // Prolog
        // ******

    const so_byte prolog[3] = {
        0x55,                       // push ebp
        0x89, 0xE5,                 // mov ebp, esp
    };
    bb.AppendBytes(prolog, sizeof(prolog));

        // ******************************************************************
        // Estimates the size of the arguments (excluding the hidden buffer).
        // ******************************************************************

    int argSize = 0;
    for(int i = 0; i < method->Signature().Params->Count(); i++) {
        size_t sizeForUse = method->Signature().Params->Array()[i]->Type.ResolvedClass->GCInfo().SizeForUse;
        if(sizeForUse < sizeof(void*)) {
            sizeForUse = sizeof(void*);
        }
        argSize += sizeForUse;
    }
        // *************************************
        // Considers the size of "this" as well.
        // *************************************

    if(!method->Signature().IsStatic && (method->MethodKind() != E_METHODKIND_CTOR)) {
        size_t sizeForUse = method->DeclaringClass()->GCInfo().SizeForUse;
        if(sizeForUse < sizeof(void*)) {
            sizeForUse = sizeof(void*);
        }
        argSize += sizeForUse;
    }
    int retOffset = argSize;

    // Moves the only argument, the passed arg buffer, to "eax" for faster retrieval.
    const so_byte prolog2[3] = {
        0x8B, 0x45, 0x08            // mov eax, [ebp+8]
    };
    bb.AppendBytes(prolog2, sizeof(prolog2));

        // ****************************************************************************************************
        // Now, emits the granules (word size elements), in reverse order, as required by the CDECL convention.
        // NOTE It includes both the arguments and "this" (if any).
        // ****************************************************************************************************

    assert(argSize % sizeof(void*) == 0);
    for(int i = argSize - sizeof(void*); i >= 0; i -= sizeof(void*)) {
        so_byte argcode[6] = {
            0xFF, 0xB0, 0x0, 0x0, 0x0, 0x0 // push [eax+0] ; will be patched below
        };
        memcpy(&argcode[2], &i, sizeof(void*));
        bb.AppendBytes(argcode, sizeof(argcode));
    }

    // For valuetype returns, TCC creates a hidden first argument. So we take the ret offset to the arg buffer
    // and push it.
    const CClass* retClass = method->Signature().ReturnType.ResolvedClass;
    assert(retClass);
    bool returnsNonPrimVT = (retClass->PrimitiveType() == E_PRIMTYPE_OBJECT) && retClass->IsValueType();
    if(returnsNonPrimVT) {
        so_byte hiddenBuf[7] = {
            0x8D, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, // lea eax, [eax+retOffset] ; to be patched below
            0x50                                // push eax
        };
        memcpy(&hiddenBuf[2], &retOffset, sizeof(4));
        bb.AppendBytes(hiddenBuf, sizeof(hiddenBuf));
    }

        // ****************
        // The call itself.
        // ****************

    so_byte callcode[7] = {
        0xB8, 0x00, 0x00, 0x00, 0x00,      // mov eax, functionptr ; to be patched
        0xFF, 0xD0                         // call eax
    };
    const void* implPtr = method->DeclaringClass()->DeclaringDomain()->GetFunctionPointer(method);
    if(!implPtr) {
        CDomain::Abort("Couldn't resolve the method.");
    }
    memcpy(&callcode[1], &implPtr, sizeof(4));
    bb.AppendBytes(callcode, sizeof(callcode));

    // Uncomment for debugging.
    /*FILE* f2 = fopen("wrapped.bin", "wb");
    fwrite(implPtr, 256, 1, f2);
    fclose(f2);*/

        // ***********************
        // Cleanup after the call.
        // ***********************

    // NOTE WARNING IMPORTANT
    // TCC implements cdecl just like GCC: it's the callee which is responsible for popping
    // the hidden buffer.
    if(argSize) {
        so_byte callcode2[6] = {
            0x81, 0xC4, 0x00, 0x00, 0x00, 0x00 // add esp, 0 ; required by cdecl, to be patched
        };

        memcpy(&callcode2[2], &argSize, 4);
        bb.AppendBytes(callcode2, sizeof(callcode2));
    };

        // *************************
        // Processing return values.
        // *************************

    if(method->Signature().ReturnType.PrimType == E_PRIMTYPE_VOID) {

        // Method::invoke returns null if the wrapped method returns nothing.
        // xor eax, eax == mov eax, 0
        bb.AppendByte(0x31);
        bb.AppendByte(0xC0);

    } else {
        // NOTE Valuetypes need to be converted to "any".
        CClass* boxedRetClass = nullptr;
        CDomain* domain = CDomain::ForCurrentThread();
        if(retClass->IsValueType() && retClass->PrimitiveType() != E_PRIMTYPE_VOID) {
            boxedRetClass = domain->BoxedClass(method->Signature().ReturnType);
            // To make sure it's registered in the ICall database & compiled in case the boxed class
            // was just generated.
            domain->ThunkManager().CompileAndLinkMethods(domain);
        }

        // Retrieves the constructor of the boxed wrapper (Method::invoke returns "any", so all returned
        // valuetypes should be pre-boxed).
        void* pBoxedCreate = nullptr;
        if(boxedRetClass) {
            SThunkInfo& thunkInfo = method->ThunkInfo();
            if(thunkInfo.pBoxedCreate) {
                pBoxedCreate = thunkInfo.pBoxedCreate;
            } else {
                // NOTE relies on the assumption that boxed wrappers have only one constructor
                assert(boxedRetClass->InstanceCtors()->Count() == 1);

                pBoxedCreate = domain->GetFunctionPointer(boxedRetClass->InstanceCtors()->Array()[0]);
                assert(pBoxedCreate);
                thunkInfo.pBoxedCreate = pBoxedCreate;
            }
        }

        // Floats is a special case, the wrapped method returns float values in st0, we need to fix it up
        // and move the value from st0 to eax to go the generic route afterwards.
        if(retClass->PrimitiveType() == E_PRIMTYPE_FLOAT) {
            // TODO can be optimized
            so_byte st0[18] = {
                0x81, 0xEC, 0x4, 0x0, 0x0, 0x0, // sub esp, 4
                0xD9, 0x1C, 0x24,               // fstp [esp]
                0x8B, 0x4, 0x24,                // mov eax, [esp]
                0x81, 0xC4, 0x4, 0x0, 0x0, 0x0  // add esp, 4
            };
            bb.AppendBytes(st0, sizeof(st0));
        }

        // NOTE For reference objects, do nothing: "eax" already holds what we need to return.
        if(retClass->IsValueType()) {
            // For valuetypes, on the other hand, the value is either in "eax" or inside the return buffer.
            if(returnsNonPrimVT) {

                const so_byte argBufAgain[3] = {
                    0x8B, 0x45, 0x08               // mov eax, [ebp+8] ; again remembers the arg buffer into eax
                };
                bb.AppendBytes(argBufAgain, sizeof(argBufAgain));

                // Arguments to the boxed ctor.
                // On the arg buffer: push the arg buffer's content entirely onto the stack.
                for(int i = retOffset + retClass->GCInfo().SizeForUse - sizeof(void*);
                    i >= retOffset;
                    i -= sizeof(void*))
                {
                    so_byte argcode[6] = {
                        0xFF, 0xB0, 0x0, 0x0, 0x0, 0x0 // push [eax+0] ; will be patched below
                    };
                    memcpy(&argcode[2], &i, sizeof(void*));
                    bb.AppendBytes(argcode, sizeof(argcode));
                }

                // The boxed ctor call and the cleanup.
                so_byte boxedCtorCall[13] = {
                    0xB8, 0x00, 0x00, 0x00, 0x00,      // mov eax, functionptr ; to be patched below
                    0xFF, 0xD0,                        // call eax
                    0x81, 0xC4, 0x00, 0x00, 0x00, 0x00 // add esp, 0 ; required by cdecl, to be patched
                };
                assert(pBoxedCreate);
                memcpy(&boxedCtorCall[1], &pBoxedCreate, 4);
                memcpy(&boxedCtorCall[9], &retClass->GCInfo().SizeForUse, 4);
                bb.AppendBytes(boxedCtorCall, sizeof(boxedCtorCall));

            } else {

                // Inside eax: push the eax and call the boxed ctor.
                so_byte ret1[14] = {
                    0x50,                              // push eax
                    0xB8, 0x00, 0x00, 0x00, 0x00,      // mov eax, functionptr ; to be patched below
                    0xFF, 0xD0,                        // call eax
                    0x81, 0xC4, 0x4, 0x0, 0x0, 0x0     // add esp, 4 ; cleanup
                };
                assert(pBoxedCreate);
                memcpy(&ret1[2], &pBoxedCreate, 4);
                bb.AppendBytes(ret1, sizeof(ret1));

            }
        }
    }

    // epilog
    const so_byte epilog[2] = {
        0xC9, // leave
        0xC3  // ret
    };
    bb.AppendBytes(epilog, sizeof(epilog));

    void* v = p->allocExecutableMem(bb.Size());
    memcpy(v, bb.Bytes(), bb.Size());

    // Uncomment for debugging.
    /*FILE* f = fopen("thunk.bin", "wb");
    fwrite(v, bb.Size(), 1, f);
    fclose(f);*/
    //exit(1);

    method->ThunkInfo().pReflectionThunk = v;

    return v;
}

// **********************
//   Compilation Thunk.
// **********************

void* SThunkManager::GetCompilationTrampoline(const CMethod* wrapperMethod, FJitFunction jitFunc) const
{
    so_byte cc[39] = {

        0x55,                               // push ebp
        0x89, 0xE5,                         // mov ebp, esp

        0x90,                               // nop (to align the jump target just in case)

        // patched: (label)

        // patch zone: 7 bytes NOP (as recommended by Intel,
        // see http://stackoverflow.com/questions/6776385/what-is-faster-jmp-or-string-of-nops)
        0x0F, 0x1F, 0x80, 0x0, 0x0, 0x0, 0x0,
        // will be patched with:
            //0xB8, 0xFF, 0xFF, 0xFF, 0xFF, // mov eax, compiled function
            //0xFF, 0xE0,                   // jmp eax

        0x68, 0xFF, 0xFF, 0xFF, 0xFF,       // push pointer to CMethod* (to be patched)
        0x68, 0xFF, 0xFF, 0xFF, 0xFF,       // push pointer to this trampoline (to be patched)
        0xB8, 0xFF, 0xFF, 0xFF, 0xFF,       // mov eax, jitfunc (to be patched)
        0xFF, 0xD0,                         // call eax

        0x81, 0xC4, 0x08, 0x00, 0x00, 0x00,     // add esp, 8 ; required by cdecl
        0xE9, 0xDD, 0xFF, 0xFF, 0xFF            // jumps back to "patched" label (see above)
                                                // this jump automatically flushes the instruction cache on x86

    };

    void* v = p->allocExecutableMem(sizeof(cc));
    // patches "push CMethod*"
    memcpy(&cc[12], &wrapperMethod, 4);
    // patches "push this trampoline"
    memcpy(&cc[17], &v, 4);
    // patches "push jitFunc"
    const void* jf = (void*)jitFunc;
    memcpy(&cc[22], &jf, 4);
    memcpy(v, &cc[0], sizeof(cc));

    // Uncomment for debugging.
    /*FILE* f = fopen("thunk.bin", "wb");
    fwrite(v, 39, 1, f);
    fclose(f);*/

    return v;
}

// *************************
//   CompileAndLinkMethods
// *************************

void SThunkManager::CompileAndLinkMethods(CDomain* domain)
{
    if(!p->m_isDirty) {
        return;
    }
    p->m_isDirty = false;

    for(int i = 0; i < p->m_closureCtors->Count(); i++) {
        const CMethod* method = p->m_closureCtors->Array()[i];
        void* implPtr = p->compileClosureCtor(method);
        char* cName (method->GetCName());
        p->m_names->Add(cName);

        domain->registerICall(cName, implPtr);
    }

    for(int i = 0; i < p->m_boxedCtors->Count(); i++) {
        const CMethod* method = p->m_boxedCtors->Array()[i];
        void* implPtr = p->compileBoxedCtor(method);
        char* cName (method->GetCName());
        p->m_names->Add(cName);

        domain->registerICall(cName, implPtr);
    }

    for(int i = 0; i < p->m_boxedMethods->Count(); i++) {
        const CMethod* method = p->m_boxedMethods->Array()[i];
        void* implPtr = this->GetCompilationTrampoline(method, boxedMethodJit);
        char* cName (method->GetCName());
        p->m_names->Add(cName);

        domain->registerICall(cName, implPtr);
    }

    // save some memory!
    p->m_closureCtors->Clear();
    p->m_boxedMethods->Clear();
    p->m_boxedCtors->Clear();
}

} }

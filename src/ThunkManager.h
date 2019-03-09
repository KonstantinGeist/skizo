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

#ifndef THUNKMANAGER_H_INCLUDED
#define THUNKMANAGER_H_INCLUDED

namespace skizo { namespace script {

class CDomain;    
class CMethod;

/**
 * A reflection thunk builds a correct native frame out of a pre-constructed argument buffer
 * and calls the target method.
 * Used by CMethod::invokeDynamic as defined in Reflection.cpp (see)
 */
typedef void* (__cdecl *FReflectionThunk)(void* argBuffer);

typedef void(__cdecl *FJitFunction)(void* trampoline, void* pMethod);

/**
 * To be embedded into CDomain.
 */
struct SThunkManager
{
    struct ThunkManagerPrivate* p;

    SThunkManager();
    ~SThunkManager();

    /**
     * WARNING Dangerous! Used in closure destruction process in MemoryManager.cpp to
     * get rid of generated closure thunks.
     */
    void FreeThunk(void* thunk);

        // *************
        //   Closures.
        // *************

    /**
     * For a given Skizo closure, creates a thunk which can be passed to native code as a simple C function.
     * The thunk re-pushes the passed arguments once again and then also adds the "this" pointer of the closure.
     * The thunk is alive as long as the closure is alive, so it's necessary to make sure the closure
     * is alive as long as native code makes use of the generated thunk. To achieve that, storing the closure
     * in a static field is an option, as well as using GC::addRoot(..)
     *
     * Used by Marshal::codeOffset
     * @note The returned value is saved inside the closure and automatically disposed on closure destruction
     * (no need to manually free this thunk).
     */
    void* GetClosureThunk(void* soObj) const;

        // ***************
        //   Reflection.
        // ***************

    /**
     * Reflection allows to call Skizo methods via a generic interface, which requires
     * converting an array of "any" to the actual types as expected by generated methods.
     * To achieve that, a special kind of thunks is introduced -- reflection thunks.
     * See also EReflectionThunk.
     */
    void* GetReflectionThunk(const CMethod* method) const;

        // *******************************
        //   JIT Compilation of methods.
        // *******************************

    /**
     * Creates a trampoline which compiles the wrapped method in a lazy fashion and then patches itself
     * to point to the actual compiled method body.
     *
     * @param jitFunc -- a specialized JIT function which will compile this method.
     */
    void* GetCompilationTrampoline(const CMethod* wrapperMethod, FJitFunction jitFunc) const;

        // *******************************
        //   ThunkJIT-generated methods.
        // *******************************

    /**
     * Used by code that wishes this method to be compiled using ThunkJIT instead of the baseline C compiler.
     * @note The method should be marked native so that the emitter didn't try to emit C code for this method.
     */
    void AddMethod(CMethod* method);

    /**
     * "Registers" methods added with ::AddMethod(..) (links with the rest of C code).
     * Used by CDomain::CreateDomain(..)
     */
    void CompileAndLinkMethods(CDomain* domain);
};

} }

#endif // THUNKMANAGER_H_INCLUDED

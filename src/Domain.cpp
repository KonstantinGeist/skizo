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

#include "Domain.h"
#include "Abort.h"
#include "Application.h"
#include "FileSystem.h"
#include "FileUtils.h"
#include "Emitter.h"
#include "icall.h"
#include "ModuleDesc.h"
#include "NativeHeaders.h"
#include "Parser.h"
#include "Path.h"
#include "RuntimeHelpers.h"
#include "ScriptUtils.h"
#include "String.h"
#include "StringBuilder.h"
#include "Transformer.h"
#include <assert.h>

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;
using namespace skizo::io;

// **************************
//       Static data.
// **************************

static SThreadLocal* g_domain = nullptr;  // Current domain is stored here.
static SThreadLocal* g_lastError = nullptr;
CMutex* CDomain::g_globalMutex = nullptr; // TCC isn't thread safe.

void __InitDomain()
{
    assert(!g_domain);
    g_domain = new SThreadLocal();
    assert(!g_lastError);
    g_lastError = new SThreadLocal();
    assert(!CDomain::g_globalMutex);
    CDomain::g_globalMutex = new CMutex();
}

void __DeinitDomain()
{
    assert(CDomain::g_globalMutex);
    CDomain::g_globalMutex->Unref();
    CDomain::g_globalMutex = nullptr;
    
    assert(g_lastError);
    delete g_lastError;
    g_lastError = nullptr;
    
    assert(g_domain);
    delete g_domain;
    g_domain = nullptr;
}

namespace
{

// Supporting functionality for SKIZOGetLastError(): every abort also fills in this
// CDomainError structure so that C code could extract the message without
// touching C++ exceptions.
// Note that the error is associated with the current thread (via thread locals) rathen
// the current domain as the error can happen during domain creation, i.e. there can be
// no domain for this thread at all.
class CDomainError: public CObject
{
public:
    char* Message; // WARNING Should be allocated with CString::ToUtf8(..) if ::Free is used.
    bool Free;

    CDomainError(char* msg, bool free)
        : Message(msg), Free(free)
    {
    }

    virtual ~CDomainError()
    {
        if(Free) {
            CString::FreeUtf8(Message);
        }
    }
};

// This class is used only as a placeholder for "gcobj" so that we could install its memory
// location as a GC root.
class CInternedString: public skizo::core::CObject
{
public:
    void* gcobj;
    CInternedString(void* _gcobj): gcobj(_gcobj) { }
};

}

#ifdef SKIZO_WIN
static LONG WINAPI unhandledExceptionFilter(struct _EXCEPTION_POINTERS *lpTopLevelExceptionFilter)
{
    switch(lpTopLevelExceptionFilter->ExceptionRecord->ExceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
        {
            // TODO estimate whether it's access violation or null reference check
            const uintptr_t addr = (uintptr_t)lpTopLevelExceptionFilter->ExceptionRecord->ExceptionInformation[1];

            // Tries to conservatively discern a random access violation from a simple null dereference.
            // NOTE The number is equal to ROTOR's value.
            if(addr < (64 * 1024)) {
                CDomain::Abort("Null dereference.");
            }
        }
        break;

        case EXCEPTION_STACK_OVERFLOW:
        {
            // Can get here if proactive stack overflow detection is disabled or there's a bug in the runtime.
            // Not recoverable.
            // TODO don't kill other threads
            printf("Stack overflow.");
            exit(1);
        }
        break;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void CDomain::chkstkHack()
{
    HMODULE hmodule = LoadLibrary(L"ntdll.dll");
    assert(hmodule);
    void* chkstck = (void*)GetProcAddress(hmodule, "_chkstk");
    assert(chkstck);
    registerICall("__chkstk", chkstck);
}
#endif

// **************************
//     SDomainCreation.
// **************************

void SDomainCreation::registerICall(const char* name, void* impl)
{
    iCalls->Set(name, impl);
}

void SDomainCreation::AddPermission(const CString* permission)
{
    permissions->Add(permission);
}

void SDomainCreation::AddSearchPath(const char* path)
{
    searchPaths->Add(path);
}

// ******************
//   Ctors & dtors.
// ******************

CDomain::CDomain()
   : m_runtimeVersion(SKIZO_RUNTIME_VERSION),
     m_thread(CThread::Current()),
     // NOTE automatically resets for new blocking calls to be made
     m_resultWaitObject(new CWaitObject(false, true)),
     m_modules(new CArrayList<CModuleDesc*>()),
     m_klasses(new CArrayList<CClass*>()),
     m_boxedClassMap(new CHashMap<SStringSlice, CClass*>()),
     m_foreignProxyMap(new CHashMap<SStringSlice, CClass*>()),
     m_aliases(new CArrayList<CClass*>()),
     m_forcedTypeRefs(new CArrayList<CForcedTypeRef*>()),
     m_extensions(new CArrayList<CClass*>()),
     m_identCompHelperMap(new CHashMap<SStringSlice, CClass*>()),
     m_stringClass(nullptr),
     m_boolClass(nullptr),
     m_charClass(nullptr),
     m_rangeClass(nullptr),
     m_errorClass(nullptr),
     m_searchPaths(new CArrayList<const CString*>()),
     m_arrayInitHelperRegistry(new CHashMap<CArrayInitializationType*, int>()),
     m_icallMethodSet(new CHashMap<void*, void*>()),
     m_eCalls(new CArrayList<void*>()),
     m_stackTraceEnabled(false),
     m_profilingEnabled(false),
     m_softDebuggingEnabled(false),
     m_explicitNullCheck(true),
     m_safeCallbacks(false),
     m_inlineBranching(true),
     m_stackFrames(new CStack<void*>()),
     m_disableBreak(false),
     m_debugDataStack(new CStack<void*>()),
     m_time(0),
     m_id(0),
     m_tccState(nullptr),
     m_readyForEpilog(false),
     m_uniqueIdCount(0)
{
    if(g_domain->BlobValue()) {
        SKIZO_THROW_WITH_MSG(EC_EXECUTION_ERROR, "More than one domain per thread not allowed.");
    }

    // NOTE The use of blob data avoids the problem when the threadlocal holds a reference to the domain
    // so that it's destroyed only when the thread is killed, not when the CDomain object itself
    // is out of scope.
    g_domain->SetBlob((void*)this);
}

CDomain::~CDomain()
{
    // **********************************************************************************
    //   Calls the epilog (static destructors).
    /* TODO I am not sure if it's safe to do it here, in a destructor. */
    if(m_tccState && m_readyForEpilog) {
        void (SKIZO_API *epilog)();
        SKIZO_LOCK_AB(CDomain::g_globalMutex) {
            epilog = (void(SKIZO_API *)())tcc_get_symbol(m_tccState, "_soX_epilog");
        } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);

        try {
            if(epilog)
                epilog();
        } catch(const SoDomainAbortException& e) {
            // All errors are swallowed as we're terminating anyway.
        }
    }
    // **********************************************************************************

    // Domain is deleted: performs "judgement day" garbage collection.
    m_memMngr.CollectGarbage(true);

    // Frees everything it can in advance.
    // IMPORTANT Not doing so leads to segfaults (something to do with the bump pointer allocator released too early (if compiled with it)).

    m_modules->Clear();
    m_klasses->Clear();
    m_klassMap.Clear();
    m_niceNameMap.Clear();
    m_primKlassMap.Clear();
    m_arrayClassMap.Clear();
    m_boxedClassMap->Clear();
    m_failableClassMap.Clear();
    m_foreignProxyMap->Clear();
    m_aliases->Clear();
    m_forcedTypeRefs->Clear();
    m_extensions->Clear();
    m_identCompHelperMap->Clear();

    TCCState* tccState = m_tccState;

    // FIX ::Unref() can be called in a separate thread, where g_domain is different from this one.
    if(CThread::Current() == m_thread) {
        g_domain->SetBlob(nullptr);
    }

    // WARNING DON'T do this. If an abort exception is thrown, the domain is destroyed,
    // destroying this message. When a handler is found, the message is corrupted.
    //g_lastError->SetNothing();

    if(tccState) {
        SKIZO_LOCK_AB(CDomain::g_globalMutex) {
            m_securityMngr.DeinitSecureIO();

            tcc_delete(tccState);
        } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);
    }
}

CDomain* CDomain::ForCurrentThread()
{
    CDomain* r = static_cast<CDomain*>(g_domain->BlobValue());
    if(!r) {
        ScriptUtils::Fail_("No domain associated with this thread.", 0, 0);
    }
    return r;
}

CDomain* CDomain::ForCurrentThreadRelaxed()
{
    return static_cast<CDomain*>(g_domain->BlobValue());
}

    // ************************
    //    Domain life cycle.
    // ************************

const CString* CDomain::readSource(const CString* _source, bool* out_isBaseModule) const
{
    Auto<const CString> source;
    source.SetVal(_source);

    if(!source->EndsWith(".skizo")) {
        source.SetPtr(source->Concat(const_cast<char*>(".skizo")));
    }

    bool found = false;

    // Consults the special base module directory first.
    // WARNING We inspect the 'modules' path before anything else so that malicious code could not overwrite
    // system code with an identically named files in other paths.
    Auto<const CString> targetPath (Path::Combine(m_securityMngr.BaseModuleFullPath(), source));
    if(FileSystem::FileExists(targetPath)) {
        found = true;
        source.SetVal(targetPath);
        *out_isBaseModule = true;
    }

    if(!found && !FileSystem::FileExists(source)) {
        // Consults the search path list.
        for(int i = 0; i < m_searchPaths->Count(); i++) {
            Auto<const CString> targetPath (Path::Combine(m_searchPaths->Array()[i], source));

            if(FileSystem::FileExists(targetPath)) {
                found = true;
                *out_isBaseModule = false;
                source.SetVal(targetPath);
                break;
            }
        }

        if(!found) {
            *out_isBaseModule = false;
            return nullptr;
        }
    }

    return FileUtils::ReadAllText(source);
}

static void reportProgress(const SDomainCreation& creation, float completeness)
{
    if(creation.CompilationCallback) {
        creation.CompilationCallback(completeness);
    }
}

CDomain* CDomain::CreateDomain(const SDomainCreation& creation)
{
    if(!creation.StackBase) {
        SKIZO_THROW_WITH_MSG(EC_ILLEGAL_ARGUMENT, "StackBase of the domain not specified.");
    }

    if(!creation.Source) {
        SKIZO_THROW_WITH_MSG(EC_ILLEGAL_ARGUMENT, "No source specified.");
    }

    if(g_domain->BlobValue()) {
        ScriptUtils::Fail_("A Skizo domain was already created for this thread.", 0, 0);
        return nullptr;
    }

    Auto<CDomain> domain (new CDomain());
    reportProgress(creation, 0.0f); // reports: compilation has just started

    // *******************************************************************************
    // Security stuff.
    if(creation.IsUntrusted) {
        domain->m_securityMngr.SetTrusted(!creation.IsUntrusted);
        for(int i = 0; i < creation.permissions->Count(); i++) {
            domain->m_securityMngr.AddPermission(creation.permissions->Array()[i]);
        }
    }
    // *******************************************************************************

    domain->m_memMngr.SetStackBase(creation.StackBase);
    domain->m_memMngr.SetMinGCThreshold(creation.MinGCThreshold);

    domain->m_entryPointClass.SetVal(creation.EntryPointClass);
    domain->m_entryPointMethod.SetVal(creation.EntryPointMethod);

    domain->m_stackTraceEnabled = creation.StackTraceEnabled;
    domain->m_profilingEnabled = creation.ProfilingEnabled;
    domain->m_memMngr.BumpPointerAllocator().EnableProfiling(creation.ProfilingEnabled);
    domain->m_softDebuggingEnabled = creation.SoftDebuggingEnabled;
    domain->m_explicitNullCheck = creation.ExplicitNullCheck;
    domain->m_safeCallbacks = creation.SafeCallbacks;
    domain->m_inlineBranching = creation.InlineBranching;
    domain->m_memMngr.EnableGCStats(creation.GCStatsEnabled);

    // _soX_reglocals & _soX_unreglocals rely on frames registered by _soX_pushframe/_soX_popframe
    // + debugger wants to know stack traces in any case.
    if(domain->m_softDebuggingEnabled && !domain->m_stackTraceEnabled) {
        domain->m_stackTraceEnabled = true;
    }

    // Untrusted domains always use stack traces because we must detect stackoverflows inside them.
    if(creation.IsUntrusted) {
        domain->m_stackTraceEnabled = true;
    }

    domain->m_breakpointCallback = creation.BreakpointCallback;

    for(int i = 0; i < creation.searchPaths->Count(); i++) {
        Auto<const CString> searchPath (CString::FromUtf8(creation.searchPaths->Array()[i]));
        domain->m_searchPaths->Add(searchPath);
    }

    if(creation.Name) {
        domain->m_domainName.SetVal(creation.Name);
    } else {
        // If no name was specified, generates one.
        //domain->m_id = g_domainCount;
        domain->m_domainName.SetPtr(CString::Format("<domain at %p>", (void*)domain));
    }

    domain->initBasicClasses();

    int dt = Application::TickCount(), tmpDt;
    int earliestDt = dt;

    {
        domain->m_sourceQueue.Enqueue(creation.Source); // initiates
        int sourceIndex = 0;
        while(!domain->m_sourceQueue.IsEmpty()) {
            Auto<const CString> source (domain->m_sourceQueue.Dequeue());

            bool isBaseModule = false;

            Auto<const CString> code;
            if(sourceIndex == 0 && !creation.UseSourceAsPath) {
                // Special case for the main module when creation.UseSourceAsPath == false (i.e. the passed string is
                // used as code rather than path).
                code.SetVal(source);
            } else {
                code.SetPtr(domain->readSource(source, &isBaseModule));
            }

            if(!code) {
                ScriptUtils::Fail_(domain->FormatMessage("Module '%o' not found.", (const CObject*)source), 0, 0);
                return nullptr;
            }

            // NOTE "filePath" isn't passed to tokens when the source is used as string instead of path:
            // otherwise ScriptUtils::Fail(..) would report the full code as the module name.
            SkizoParse(domain, creation.UseSourceAsPath? source: (const CString*)0, code, isBaseModule);

            domain->m_cachedCode.Add(code);
            domain->m_cachedCode.Add(source); // Tokens refer to the path of their declaring file
                                               // for nicer errors without acquiring them.

            sourceIndex++;
        }
    }

    if(domain->m_profilingEnabled) {
        tmpDt = Application::TickCount();
        printf("Parsing phase: %d ms.\n", tmpDt - dt);
        dt = tmpDt;
    }
    reportProgress(creation, 0.2f); // reports: parsing ready

    SkizoTransform(domain);
    domain->verifyIntrinsicClasses();

    if(domain->m_profilingEnabled) {
        tmpDt = Application::TickCount();
        printf("Transform phase: %d ms.\n", tmpDt - dt);
        dt = tmpDt;
    }
    reportProgress(creation, 0.4f); // reports: transformation ready

    STextBuilder cb;
    SkizoEmit(domain, cb);

    if(domain->m_profilingEnabled) {
        tmpDt = Application::TickCount();
        printf("Emit phase: %d ms.\n", tmpDt - dt);
        dt = tmpDt;
    }
    reportProgress(creation, 0.6f); // reports: emission ready

    {
        char* cCode = cb.Chars();

        // *******************************
        if(creation.DumpCCode) {
            FILE* f = fopen("skizodump.c", "w");
            fwrite(cCode, 1, strlen(cCode), f);
            fclose(f);
        }
        // *******************************

        SKIZO_LOCK_AB(CDomain::g_globalMutex) {
            domain->m_tccState = tcc_new();
            SKIZO_REQ_PTR(domain->m_tccState);

            tcc_set_output_type(domain->m_tccState, TCC_OUTPUT_MEMORY);

            if(tcc_compile_string(domain->m_tccState, cCode) == -1) {
                CDomain::Abort("Couldn't compile the output machine code (invalid inline C code or a bug in the backend).");
            }

            // **************************************************************
            domain->chkstkHack();

            domain->registerICall("_soX_mm", (void*)&domain->m_memMngr);
            domain->registerICall("_soX_gc_alloc", (void*)_soX_gc_alloc);
            domain->registerICall("_soX_gc_alloc_env", (void*)_soX_gc_alloc_env);
            domain->registerICall("_soX_gc_roots", (void*)_soX_gc_roots);

            domain->registerICall("_soX_regvtable", (void*)_soX_regvtable);
            domain->registerICall("_soX_patchstrings", (void*)_soX_patchstrings);
            domain->registerICall("_soX_downcast", (void*)_soX_downcast);
            domain->registerICall("_soX_unbox", (void*)_soX_unbox);
            domain->registerICall("_soX_findmethod", (void*)_soX_findmethod);
            domain->registerICall("_soX_findmethod2", (void*)_soX_findmethod2);
            domain->registerICall("_soX_is", (void*)_soX_is);
            domain->registerICall("_soX_biteq", (void*)_soX_biteq);
            domain->registerICall("_soX_zero", (void*)_soX_zero);
            domain->registerICall("_soX_abort0", (void*)_soX_abort0);
            domain->registerICall("_soX_abort", (void*)_soX_abort);
            domain->registerICall("_soX_abort_e", (void*)_soX_abort_e);
            domain->registerICall("_soX_cctor", (void*)_soX_cctor);
            domain->registerICall("_soX_checktype", (void*)_soX_checktype);
            domain->registerICall("_soX_newarray", (void*)_soX_newarray);
            domain->registerICall("_soX_addhandler", (void*)_soX_addhandler);
            domain->registerICall("_soX_msgsnd_sync", (void*)_soX_msgsnd_sync);
            domain->registerICall("_soX_unpack", (void*)_soX_unpack);
            domain->registerICall("_so_int_op_divide", (void*)_so_int_op_divide);

            if(domain->m_stackTraceEnabled) {
                domain->registerICall("_soX_pushframe", (void*)_soX_pushframe);
                domain->registerICall("_soX_popframe", (void*)_soX_popframe);
            }
            if(domain->m_profilingEnabled) {
                domain->registerICall("_soX_pushframe_prf", (void*)_soX_pushframe_prf);
                domain->registerICall("_soX_popframe_prf", (void*)_soX_popframe_prf);
            }
            if(domain->m_softDebuggingEnabled) {
                domain->registerICall("_soX_reglocals", (void*)_soX_reglocals);
                domain->registerICall("_soX_unreglocals", (void*)_soX_unreglocals);
                domain->registerICall("_soX_break", (void*)_soX_break);
            }

            // Registers icalls defined in SDomainCreation.
            {
                const char* iCallName;
                void* iCall;
                SHashMapEnumerator<const char*, void*> mapEnum (creation.iCalls);
                while(mapEnum.MoveNext(&iCallName, &iCall)) {
                    domain->registerICall(iCallName, iCall);
                }
            }

            domain->registerStandardICalls();

            // **************************************************************
            // Registers ecalls.
            for(int i = 0; i < domain->m_eCalls->Count(); i++) {
                CMethod* pMethod = (CMethod*)domain->m_eCalls->Array()[i];
                Utf8Auto methodName (pMethod->GetCName());

                tcc_add_symbol(domain->m_tccState,
                               methodName,
                               pMethod->ECallDesc().ImplPtr);
            }
            // **************************************************************

            // For ThunkManager
            domain->m_thunkMngr.CompileAndLinkMethods(domain);

            // ****************************************************
            // Checks if all the icalls have their impls linked in.
            // ****************************************************

            {
                SHashMapEnumerator<void*, void*> implSetEnum (domain->m_icallMethodSet);
                void* k, *v;
                while(implSetEnum.MoveNext(&k, &v)) {
                    domain->verifyICallIsRegistered((CMethod*)k);
                }

                // Not needed anymore.
                domain->m_icallMethodSet->Clear();
                // NOTE "m_icallImplSet" is still retained because CDomain::GetSymbol()
                // relies on it to find functions defined outside of the C code as TCC's tcc_get_symbol
                // can't locate them.
            }

            // ******************************

            if(tcc_relocate(domain->m_tccState, TCC_RELOCATE_AUTO) < 0) {
                //SKIZO_THROW(EC_EXECUTION_ERROR); // TODO ?
                CDomain::Abort("Relocation error (invalid inline C code or a bug in the backend).");
            }
            reportProgress(creation, 0.8f); // reports: relocation ready

        } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);

        if(domain->m_profilingEnabled) {
            tmpDt = Application::TickCount();
            printf("Compile phase: %d ms.\n\n", tmpDt - dt);
            dt = tmpDt;

            printf("Total startup time: %d ms.\n\n", tmpDt - earliestDt);
        }

    #ifdef SKIZO_WIN
        // Sets an exception handler to catch null reference errors without testing for them in runtime.
        // NOTE One handler for all threads. See MSDN:
        // "Issuing SetUnhandledExceptionFilter replaces the existing top-level exception filter for all
        // existing and all future threads in the calling process".
        static bool isHandlerSet = false;
        if(!isHandlerSet && !domain->m_explicitNullCheck) {
            isHandlerSet = true;
            //SetUnhandledExceptionFilter(unhandledExceptionFilter);
            AddVectoredExceptionHandler(0, unhandledExceptionFilter);
        }
    #else
        #error "Not implemented."
    #endif

        // **********************
        // After compiling the code, automatically calls "_soX_prolog" which
        // sets gc roots and calls static constructors.
        void (SKIZO_API *prolog)();
        SKIZO_LOCK_AB(CDomain::g_globalMutex) {
            domain->m_securityMngr.InitSecureIO(); // !!

            prolog = (void(SKIZO_API *)())tcc_get_symbol(domain->m_tccState, "_soX_prolog");
            SKIZO_REQ_PTR(prolog);
        } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);

        if(prolog) {
            prolog();
        }
        // **********************

        domain->m_readyForEpilog = true;
    }

    reportProgress(creation, 1.0f); // reports: prolog and secure IO complete; the domain is ready.

    domain->Ref();
    return domain;
}

bool CDomain::InvokeEntryPoint()
{
    // ***************************
    //   Safety area.
    //
    // Users of SkizoDomain usually acquire the stackBase pointer by taking a pointer to an argument on stack
    // in the top level function (such as "args" of the main function for example) or by using SKIZOGetStackBase().
    // This approach is a bit unsafe in systems that insert runtime stubs and other code before calling out to
    // SKIZOGetStackBase() (CLR being one example, through P/Invoke).
    // In that case, the pointer the SKIZOGetStackBase function returns will be at an offset deeper into the stack
    // than required. In certain situations (depending on how the runtime is compiled), this can lead to a
    // hypothetical situation when the stackBase pointer ends up _after_ the start of the stack data used by
    // the Skizo's compiled code, which means the GC won't see some of the stack roots.
    // To avoid this problem, just in case, we manually "shift" Skizo's stack data offset by allocating
    // a volatile buffer here.
    // ***************************
    volatile char safety_area[128];
    for(int i = 0; i < 128; i++) {
        // Shuts up the compiler about unused variables and also clears the area from garbage.
        safety_area[i] = 0;
        safety_area[i] = safety_area[i];
    }
    // ***************************

    bool r = true;
    try {

        // ******************************
        //   Extraction & Verification.
        // ******************************

        Auto<const CString> epClassName;
        Auto<const CString> epMethodName;

        // NOTE only when both the entrypoint class and the entrypoint method are specified,
        // this works. Otherwise, it's ignored and default values are used.
        if(m_entryPointClass && m_entryPointMethod) {
            epClassName.SetVal(m_entryPointClass);
            epMethodName.SetVal(m_entryPointMethod);
        } else {
            epClassName.SetPtr(CString::FromUtf8("Program"));
            epMethodName.SetPtr(CString::FromUtf8("main"));
        }

        CClass* epClass = this->ClassByNiceName(epClassName); // NOTE Nice names.
        if(!epClass) {
            ScriptUtils::Fail_("Entrypoint class not found.", 0, 0);
            return false;
        }

        SStringSlice _epMethodName (epMethodName);
        CMethod* epMethod = epClass->MyMethod(_epMethodName, true, E_METHODKIND_NORMAL);
        if(!epMethod) {
            ScriptUtils::Fail_("Entrypoint method not found.", 0, 0);
            return false;
        }

        if(!epMethod->IsValidEntryPoint()) {
            ScriptUtils::Fail_("Entrypoints must return nothing, accept 0 arguments and have CDECL convention.", 0, 0);
            return false;
        }

        // *******
        //   Go!
        // *******

        void (SKIZO_API *mainFunc)();
        {
            Utf8Auto szClass (epClassName->ToUtf8());
            Utf8Auto szMethod (epMethodName->ToUtf8());
            mainFunc = (void(*)())GetFunctionPointer(szClass, szMethod);
        }
        SKIZO_REQ_PTR(mainFunc);

        /*FILE* ftmp = fopen("codedump.bin", "wb");
        fwrite((char*)mainFunc, 1, 300, ftmp);
        fclose(ftmp);*/

        if(m_profilingEnabled) {
            m_time = Application::TickCount();
            mainFunc();
            m_time = Application::TickCount() - m_time;
        } else {
            mainFunc();
        }

    } catch (const SoDomainAbortException& e) {
        printf("ABORT (runtime): %s\n", e.Message); // TODO generic error/output interface
        _so_StackTrace_print();

        r = false;
    }

    return r;
}

const char* CDomain::GetLastError()
{
    SVariant vLastError (g_lastError->Get());
    if(vLastError.Type() == E_VARIANTTYPE_NOTHING) {
        return nullptr;
    } else {
        CDomainError* error = (CDomainError*)vLastError.ObjectValue();
        return error->Message;
    }
}

void CDomain::abortImpl(char* msg, bool free)
{
    // The message is also saved globally for the current thread.
    // SKIZOGetLastError() from the C interface relies on it, as C doesn't have the notion of
    // exceptions.
    SVariant vLastError (g_lastError->Get());
    if(vLastError.Type() == E_VARIANTTYPE_NOTHING) {
        Auto<CDomainError> daLastError (new CDomainError(msg, free));
        vLastError.SetObject(daLastError);
        g_lastError->Set(vLastError);
    } else {
        // Just in case.
        CDomainError* error = (CDomainError*)vLastError.ObjectValue();
        if(error->Message && error->Free) { // destroys the previous error, if any
            CString::FreeUtf8(error->Message);
        }
        error->Message = msg;
        error->Free = free;
    }

    // This trick unwinds the stack back to InvokeEntryPoint (or whatever the top function is) using
    // C++'s unwinding mechanism.
    throw SoDomainAbortException(msg);
}

void CDomain::Abort(char* msg, bool free)
{
    CDomain::abortImpl(msg, free);
}

void CDomain::Abort(const char* msg)
{
    CDomain::abortImpl(const_cast<char*>(msg), false);
}

void CDomain::Abort(int errorCode)
{
    switch(errorCode) {
        case SKIZO_ERRORCODE_RANGECHECK:
            CDomain::Abort("Range check failed.");
            break;
        case SKIZO_ERRORCODE_NULLABLENULLCHECK:
            CDomain::Abort("Attempt to get a value from a nullable which has no value.");
            break;
        case SKIZO_ERRORCODE_NULLDEREFERENCE:
            CDomain::Abort("Null dereference (accessed variable not set to an object instance).");
            break;
        case SKIZO_ERRORCODE_ASSERT_FAILED:
            CDomain::Abort("Assert failed.");
            break;
        case SKIZO_ERRORCODE_FAILABLE_FAILURE:
            CDomain::Abort("Attempt to get a value from a failure.");
            break;
        case SKIZO_ERRORCODE_OUT_OF_MEMORY:
            CDomain::Abort("Out of memory.");
            break;
        case SKIZO_ERRORCODE_DISALLOWED_CALL:
            CDomain::Abort("ECalls and unsafe code are disallowed in untrusted contexts in non-base modules.");
            break;
        case SKIZO_ERRORCODE_STACK_OVERFLOW:
            // NOTE We detect stackoverflows in a proactive manner, meaning there's still a lot
            // of stack space to run functions like these.
            CDomain::ForCurrentThread()->correctStackTraceAfterStackOverflow();
            CDomain::Abort("Stack overflow detected.");
            break;
        case SKIZO_ERRORCODE_TYPE_INITIALIZATION_ERROR:
            CDomain::Abort("Type initialization error (abort in the static constructor?)");
            break;
        default:
            SKIZO_REQ_NEVER
            break;
    }
}

void CDomain::correctStackTraceAfterStackOverflow()
{
    // Finds the repeating pattern.

    Auto<CArrayList<void*> > pattern (new CArrayList<void*>());

    for(int i = m_stackFrames->Count() - 1; i >= 0; i--) {
        void* item = m_stackFrames->Item(i);
        if(pattern->Count() > 0 && item == pattern->Array()[0]) {
            break;
        } else {
            pattern->Add(item);
        }
    }

    if(pattern->Count() == 0) {
        return;
    }

    // Removes the repeating pattern.

    int elementsToRemove = 0;
    int patternIndex = 0;
    for(int i = m_stackFrames->Count() - 1; i >= 0; i--) {
        void* item = m_stackFrames->Item(i);
        if(item == pattern->Array()[patternIndex]) {
            elementsToRemove++;
        } else {
            break;
        }

        if(++patternIndex == pattern->Count()) {
            patternIndex = 0;
        }
    }

    // We need to leave two loops on the stack so that the user could see who overflowed.

    int patternsToRemove = elementsToRemove / pattern->Count();
    patternsToRemove -= 2;
    if(patternsToRemove < 0) {
        patternsToRemove = 0;
    }

    elementsToRemove = patternsToRemove * pattern->Count();

    for(int i = 0; i < elementsToRemove; i++) {
        m_stackFrames->Pop();
    }
}

    // ******************
    //    Auxiliaries.
    // ******************

SStringSlice CDomain::NewSlice(const char* cs) const
{
    const CString* str = 0;
    if(!m_stringTable1.TryGet(cs, &str)) {
        str = CString::FromUtf8(cs);
        m_stringTable1.Set(cs, str);
    }
    str->Unref();

    assert(str);
    return SStringSlice(str, 0, str->Length());
}

SStringSlice CDomain::NewSlice(const CString* _str) const
{
    const CString* str;
    if(m_stringTable2.TryGet(_str, &str)) {
        str->Unref();
    } else {
        m_stringTable2.Set(_str, _str);
        str = _str;
    }

    assert(str);
    assert(str->Length() > 0);
    return SStringSlice(str, 0, str->Length());
}

SStringSlice CDomain::NewSlice(CStringBuilder* sb) const
{
    Auto<const CString> tmp (sb->ToString());
    return this->NewSlice(tmp);
}

int CDomain::NewUniqueId() const
{
    if(m_uniqueIdCount == SKIZO_INT32_MAX) {
		// Not going to happen anytime soon, as we have ~2 bln unique IDs per domain.
        SKIZO_THROW_WITH_MSG(EC_EXECUTION_ERROR, "Out of unique IDs.");
    }
    return m_uniqueIdCount++;
}

char* CDomain::FormatMessage(const char* format, ...) const
{
    char* r;

    va_list vl;
    va_start(vl, format);

    r = m_errorBuilder.ClearFormat(format, vl);

    va_end(vl);

    return r;
}

void* CDomain::GetSymbol(char* name) const
{
    void* r;
    if(m_icallImplSet.TryGet(name, &r)) {
        return r;
    } else {
        return tcc_get_symbol(m_tccState, name);
    }
}

void* CDomain::GetSymbolThreadSafe(char* name) const
{
    void* r;
    SKIZO_LOCK_AB(CDomain::g_globalMutex) {
        r = GetSymbol(name);
    } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);
    return r;
}

    // ******************
    //   Embedding API.
    // ******************

// WARNING don't introduce RAII
void* CDomain::CreateArray(int arrayLength, void** vtable)
{
    if(arrayLength < 0) {
        CDomain::Abort("Array size can't be negative.");
    }

    const CClass* pClass = ((CClass*)vtable[0]);
    assert(pClass->SpecialClass() == E_SPECIALCLASS_ARRAY);
    assert(pClass->ResolvedWrappedClass());
    const size_t itemSize = pClass->ResolvedWrappedClass()->GCInfo().SizeForUse;

    void* objptr = _soX_gc_alloc(&m_memMngr,
                                 offsetof(SArrayHeader, firstItem) + arrayLength * itemSize,
                                 vtable);
    ((SArrayHeader*)objptr)->length = arrayLength;
    return objptr;
}

void* CDomain::CreateArray(const STypeRef& elementTypeRef, int elementCount)
{
    if(elementCount < 0) {
        CDomain::Abort("Array length can't be negative.");
    }
    STypeRef arrayTypeRef = elementTypeRef;
    arrayTypeRef.ResolvedClass = nullptr; // otherwise ResolveTypeRef would've ignored it
    arrayTypeRef.ArrayLevel++;
    if(!ResolveTypeRef(arrayTypeRef)) {
        CDomain::Abort("Array class wasn't compiled into the domain because it was never used ('force' is required).");
    }
    assert(arrayTypeRef.ResolvedClass);

    CClass* pArrayClass = arrayTypeRef.ResolvedClass;
    assert(pArrayClass->HasVTable());
    assert(pArrayClass->VirtualTable());

    return CreateArray(elementCount, pArrayClass->VirtualTable());
}

void* CDomain::CreateArray(const CArrayList<const CString*>* daArray)
{
    const int count = daArray->Count();

    SKIZO_REQ_PTR(m_stringClass);
    void* soArray = this->CreateArray(m_stringClass->ToTypeRef(), count);

    for(int i = 0; i < count; i++) {
        if(daArray->Array()[i]) {
            void* soString = this->CreateString(daArray->Array()[i], false);
            this->SetArrayElement(soArray, i, soString);
        }
    }

    return soArray;
}

void CDomain::SetArrayElement(void* obj, int index, void* value)
{
    SKIZO_NULL_CHECK(obj);

	// Retrieves the class of the array.
    CClass* pClass = so_class_of(obj);
    if(pClass->SpecialClass() != E_SPECIALCLASS_ARRAY) {
        CDomain::Abort("The target object must be an array.");
    }

	// Retrieves the header of the array and verifies the index is not out of range.
    SArrayHeader* arrayHeader = (SArrayHeader*)obj;
    if(index < 0 || index >= arrayHeader->length) {
        _soX_abort0(SKIZO_ERRORCODE_RANGECHECK);
    }

	// Retrieves the wrapped class of the array and verifies the value is valid.
    CClass* pSubClass = pClass->ResolvedWrappedClass();
    assert(pSubClass->GCInfo().SizeForUse);
    if(pSubClass->IsValueType()) {
        if(!value) {
            CDomain::Abort("The value can't be null for valuetypes.");
        }
    } else {
        if(value && (pSubClass != so_class_of(value))) {
            CDomain::Abort("The types of the array and the value don't match.");
        }
    }

	// Sets the item of the array (the pointer for reference types or the actual data for valuetypes).
    void* offset = (char*)obj + offsetof(SArrayHeader, firstItem) + pSubClass->GCInfo().SizeForUse * index;
    if(pSubClass->IsValueType()) {
        memcpy(offset, value, pSubClass->GCInfo().SizeForUse);
    } else {
        memcpy(offset, &value, sizeof(void*));
    }
}

// NOTE String literals should be created with InternStringLiteral
void* CDomain::CreateString(const CString* source, bool intern)
{
    CObject* interned;
    if(intern && m_internedStrings.TryGet(source, &interned)) {
        interned->Unref();
        return static_cast<CInternedString*>(interned)->gcobj;
    }

    assert(m_stringClass->VirtualTable());
    void* objptr = _soX_gc_alloc(&m_memMngr, sizeof(SStringHeader), m_stringClass->VirtualTable());
    ((SStringHeader*)objptr)->pStr = source;
    source->Ref(); // Will be unref'd in the string's dtor.
    if(intern) {
        interned = new CInternedString(objptr);
        void* rootRef[1] = { &static_cast<CInternedString*>(interned)->gcobj };
        _soX_gc_roots(&m_memMngr, rootRef, 1);
        m_internedStrings.Set(source, interned);
        interned->Unref();
    }

    return objptr;
}

// see "icalls/string.cpp" for more information on how string literals are managed
void* CDomain::InternStringLiteral(const CString* source)
{
    CObject* interned;
    if(m_internedStrings.TryGet(source, &interned)) {
        interned->Unref();
        return static_cast<CInternedString*>(interned)->gcobj;
    }

    // Will be freed on domain teardown (see SMemoryManager::CollectGarbage)
    SStringHeader* strLiteral = (SStringHeader*)malloc(sizeof(SStringHeader));
    strLiteral->vtable = nullptr; // will be patched in prolog
    strLiteral->pStr = source;
    source->Ref(); // Will be unref'd in the string's dtor.

    // Adds to the intern cache.
    interned = new CInternedString(strLiteral);
    m_internedStrings.Set(source, interned);
    interned->Unref();

    // Adds to the memory manager-friendly list.
    m_memMngr.AddStringLiteral(strLiteral);

    return strLiteral;
}

    // ******************
    //    Reflection.
    // ******************

void CDomain::RegisterClass(CClass* klass)
{
    SKIZO_REQ(!klass->FlatName().IsEmpty(), EC_ILLEGAL_ARGUMENT);

    if(this->ClassByFlatName(klass->FlatName())) {
        ScriptUtils::FailC(this->FormatMessage("Type '%C' defined more than once.", klass),
                           klass);
    }

    m_klasses->Add(klass);
    m_klassMap.Set(klass->FlatName(), klass);
}

CClass* CDomain::ClassByFlatName(const SStringSlice& name) const
{
    CClass* klass = nullptr;
    if(m_klassMap.TryGet(name, &klass)) {
        klass->Unref();
        return klass;
    }

    return nullptr;
}

CClass* CDomain::ClassByNiceName(const CString* name) const
{
    // Checks if we need to lazily generate nice names.
    if(m_klassMap.Size() != m_niceNameMap.Size()) {
        m_niceNameMap.Clear(); // just in case.

        for(int i = 0; i < m_klasses->Count(); i++) {
            CClass* klass = m_klasses->Array()[i];

            // We don't add boxed classes to this map by spec, because otherwise, say, "int" would
            // be ambiguous: is it the original type or the boxed type (both share the same nice name)?
            if(klass->SpecialClass() != E_SPECIALCLASS_BOXED) {
                m_niceNameMap.Set(klass->NiceName(), klass);
            }
        }
    }

    CClass* klass = nullptr;
    if(m_niceNameMap.TryGet(name, &klass)) {
        klass->Unref();
        return klass;
    }

    return nullptr;
}

void CDomain::registerICall(const char* name, void* ptr)
{
    SKIZO_REQ(!m_icallImplSet.Contains(name), EC_ILLEGAL_ARGUMENT);

    tcc_add_symbol(m_tccState, name, ptr);
    m_icallImplSet.Set(name, ptr);
}

void CDomain::verifyICallIsRegistered(CMethod* pMethod) const
{
    // ECalls are to be loaded in runtime.
    if(pMethod->ECallDesc().IsValid()) {
        return;
    }

    Utf8Auto cName (pMethod->GetCName());
    if(!m_icallImplSet.Contains(cName)) {
        ScriptUtils::Fail_(this->FormatMessage("Native method '%C::%s' not registered as an icall inside the runtime.",
                                                pMethod->DeclaringClass(),
                                               &pMethod->Name()),
                           nullptr,
                           0);
    }
}

void* CDomain::GetFunctionPointer(const char* className, const char* methodName) const
{
    char* fullName = (char*)malloc(4 /* _so_ */ + strlen(className) + 1 /* _ */ + strlen(methodName) + 1 /* \0 */);
    strcpy(fullName, "_so_");
    strcat(fullName, className);
    strcat(fullName, "_");
    strcat(fullName, methodName);

    void* ptr;
    SKIZO_LOCK_AB(CDomain::g_globalMutex) {
        ptr = GetSymbol(fullName);
    } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);
    free(fullName);

    return ptr;
}

void* CDomain::GetFunctionPointer(const CMethod* method) const
{
    if(method->IsAbstract()) {
        SKIZO_THROW_WITH_MSG(EC_ILLEGAL_ARGUMENT, "Abstract methods don't have bodies.");
    }

    Utf8Auto fullName (method->GetCName());
    void* ptr;
    SKIZO_LOCK_AB(CDomain::g_globalMutex) {
        ptr = GetSymbol(fullName);
    } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);
    return ptr;
}

bool CDomain::IsBaseDomain() const
{
    return m_thread->IsMain();
}

// NOTE Checks if intrinsic classes are defined in the base module: if they're defined outside of base modules,
// that means a malicious user tries to trick the system by rewriting functions.
void CDomain::verifyIntrinsicClasses()
{
    CClass* mapClass = this->ClassByFlatName(this->NewSlice("Map"));
    if(mapClass) {
        m_memMngr.SetMapClass(mapClass);

        // Validates that the layout is not corrupted (defined externally in a *.skizo file).
        if(mapClass->InstanceFields()->Count() != 1
        || mapClass->InstanceFields()->Item(0)->Type.PrimType != E_PRIMTYPE_INTPTR
        || !mapClass->Source().Module->IsBaseModule)
        {
            SKIZO_THROW_WITH_MSG(EC_EXECUTION_ERROR, "Intrinsic Map class corrupted or redefined outside of base modules.");
        }
    }

    CClass* marshalClass = this->ClassByFlatName(this->NewSlice("Marshal"));
    if(marshalClass && !marshalClass->Source().Module->IsBaseModule) {
        SKIZO_THROW_WITH_MSG(EC_EXECUTION_ERROR, "Intrinsic Marshal class corrupted or redefined outside of base modules.");
    }

    CClass* hDomainClass = this->ClassByFlatName(this->NewSlice("DomainHandle"));
    if(hDomainClass) {
        // Validates that the layout is not corrupted (defined externally in a *.skizo file).
        if(hDomainClass->InstanceFields()->Count() != 1
        || hDomainClass->InstanceFields()->Item(0)->Type.PrimType != E_PRIMTYPE_INTPTR
        || !hDomainClass->Source().Module->IsBaseModule)
        {
            SKIZO_THROW_WITH_MSG(EC_EXECUTION_ERROR, "Intrinsic DomainHandle class corrupted or redefined outside of base modules.");
        }
    }
}

} }

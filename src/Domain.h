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

#ifndef DOMAIN_H_INCLUDED
#define DOMAIN_H_INCLUDED

#include "Activator.h"
#include "ArrayInitializationType.h"
#include "ArrayList.h"
#include "Class.h"
#include "DomainCreation.h"
#include "ECallCache.h"
#include "Mutex.h"
#include "HashMap.h"
#include "MemoryManager.h"
#include "Queue.h"
#include "Remoting.h"
#include "Security.h"
#include "SourceKind.h"
#include "Stack.h"
#include "skizoscript.h"
#include "StringSlice.h"
#include "TextBuilder.h"
#include "Thread.h"
#include "ThunkManager.h"
#include "TypeRef.h"
#include "third-party/tcc/libtcc.h"

// Invoke method is at offset 0 in a method class.
#define so_invokemethod_of(obj) so_virtmeth_of(obj, 0)

namespace skizo { namespace core {
    class CString;
    class CStringBuilder;
} }

namespace skizo { namespace script {

class CClass;
class CMethod;

/**
 * A domain is an isolated instance of the runtime that consists of a set of modules, a separate memory manager, its own thread,
 * and a set if permissions.
 * If you want to dynamically load new modules (chunks of code), you have to create new domains.
 * For convenience (as a domain is the root of everything, and it's easy to retrieve it via CDomain::ForCurrentThread()),
 * this class manages many things at once: it allows to register classes, create objects, etc. However, actual implementations
 * are split between separate files, which is fine.
 */
class CDomain: public skizo::core::CObject
{
    friend void __InitDomain();
    friend void __DeinitDomain();
    friend struct SVirtualUnwinder;
    friend struct SThunkManager;
    
public:
    CDomain();
    virtual ~CDomain();

    /**
     * Returns the domain associated with this thread. Throws if nothing is associated with this thread.
     */
    static CDomain* ForCurrentThread();

    /**
     * Returns the domain associated with this thread. Returns null if nothing is associated with this thread.
     */
    static CDomain* ForCurrentThreadRelaxed();

    const skizo::core::CString* Name() const { return m_domainName; }

    // **********************
    //       Managers.
    // **********************

    SMemoryManager& MemoryManager() { return m_memMngr; }
    const SMemoryManager& MemoryManager() const { return m_memMngr; }
    SThunkManager& ThunkManager() { return m_thunkMngr; }
    SSecurityManager& SecurityManager() { return m_securityMngr; }
    const SECallCache& ECallCache() const { return m_ecallCache; }
    SActivator& Activator() { return m_activator; }

    // ***********************
    //    Type resolution.
    // ***********************

    /**
     * This method should be called after performing parsing because resolution depends on a fully formed
	 * list of classes. Normally, a resolved typeref becomes "flat", i.e. its ArrayLevel is 0 and it's not
	 * a failable -- it was resolved as a new simple ('flat') generated class that implements the
	 * array/failable etc.
     * @note Fails with an error if no class with the given name is found.
     * @note Implemented in TypeResolution.cpp
     * @warning For use in the transformation phase.
     */
    bool ResolveTypeRef(STypeRef& typeRef);

    /**
     * Creates a boxed class of a struct class, or returns an existing one.
     * From the point of view of the emitter/reflection, generated boxed classes are average classes
     * with the name "0Boxed_%d" where %d is a unique ID.
     * @note Implemented in TypeResolution.cpp
     */
    CClass* BoxedClass(const STypeRef& typeRef, bool mustBeAlreadyCreated = false);

    // ************************
    //    Domain life cycle.
    // ************************

    /**
     * Creates a new domain for the current thread ("injects" it). If you want a domain in another thread, use CreateRemoteDomain(..)
     * @note Throws EC_EXECUTION_ERROR if there is already a domain associated with this thread.
     */
    static CDomain* CreateDomain(const SDomainCreation& creation);

    /**
     * Creates a non-base domain from under this domain. The function assumes that there's already a domain created
     * in this thread; the function automatically creates a thread for the new domain and returns a handle to it
     * to communicate with it.
     *
     * If `permissions` is null, the domain is trusted. Otherwise, it's untrusted.
     *
     * Implemented in Remoting.cpp, wrapped by Domain::runString(..) and Domain::runPath(..) and others in Skizo.
     */
    static class CDomainHandle* CreateRemoteDomain(const skizo::core::CString* source,
                                                   ESourceKind sourceKind,
                                                   const skizo::collections::CArrayList<const skizo::core::CString*>* permissions);

    /**
     * Invokes the main method of the domain, which must be a static parameterless method which returns nothing,
     * called "main" and defined in the class "Program".
     * Returns true if no errors (aborts); false otherwise.
     */
    bool InvokeEntryPoint();

    /**
     * Aborts the current domain with a message.
     * @note The new implementation allows no domain set for this thread. The function merely throws
     * SoDomainAbortException to unwind the stack. This allows to abort domain creation as well (when domain
     * isn't yet constructed).
     *
     * @param free If true, uses CString::FreeUtf8 to destroy the string once the stack unwinds and the message
     * is printed.
     */
    static void Abort(char* msg, bool free = false);
    static void Abort(const char* msg);
    static void Abort(int errorCode);

    // ******************
    //    Reflection.
    // ******************

    /**
     * Returns a class by its internal (flat) name. Returns null if nothing found.
     */
    CClass* ClassByFlatName(const SStringSlice& name) const;

    /**
     * Returns a class by its nice name.
     * @note If "int" is specified, the the actual int is returned. Boxed classes (whose nice names are identical to the
     * nice names of their corresponding plain valuetypes) report same nice names, and we ignore them in this function
     * (for a good reason).
     */
    CClass* ClassByNiceName(const skizo::core::CString* name) const;

    /**
     * Puts this class to the domain's internal containers.
     * Preferred to call this method after fully constructing a class.
     */
    void RegisterClass(CClass* klass);

    // ******************
    //    Auxiliaries.
    // ******************

    /**
     * Used to format messages for ::Abort(..) and others.
     * Returns an UTF8-encoded string which should be released with CString::FreeUtf8(..)
     */
    char* FormatMessage(const char* format, ...) const;

    // Slice creation convenience methods.
    SStringSlice NewSlice(const char* cs) const;
    SStringSlice NewSlice(const skizo::core::CString* str) const;
    SStringSlice NewSlice(skizo::core::CStringBuilder* sb) const;

    /**
     * A workaround: as it turns out, TCC's tcc_get_symbol doesn't report extern functions that were registered from outside
	 * the C code.
	 * @warning The method isn't automatically guarded with g_globalMutex
     */
    void* GetSymbol(char* name) const;

    /**
     * Thread-safe variant of GetSymbol(..)
     */
    void* GetSymbolThreadSafe(char* name) const;

    /**
     * The id is guaranteed to be unique across the entire domain (until we hit the 2 bln limit, which isn't going to
     * happen anytime soon).
     */
    int NewUniqueId() const;

    // ******************
    //   Embedding API.
    // ******************

    /**
     * Creates a GC-allocated Skizo string from a string.
     * If intern is true, the string is interned, i.e. it's added as a GC root and the runtime makes sure
     * there's only one instance of this string for the given string literal.
     */
    void* CreateString(const skizo::core::CString* source, bool intern = false);

    /**
     * Creates a GC-allocated Skizo array from a list of strings.
     * For convenience.
     */
    void* CreateArray(const skizo::collections::CArrayList<const skizo::core::CString*>* array);

    /**
     * Creates an interned string literal.
     * @note When this function is created before the code emission, the string's vtable is zero.
     * It should be patched with the correct vtable pointer after registering vtables.
     * NOTE Unlike createString/internString, this function doesn't alter allocated memory statistics,
	 * neither it triggers garbage collection.
     * @warning Internal code should not schedule garbage collection before code emission.
     */
    void* InternStringLiteral(const skizo::core::CString* source);

    /**
     * Creates a new GC-allocated array.
     * Returns null in case of an error.
     * @note May fail if the array specialization was never used in the code. For example, if [int] was never used,
     * then the internal class to describe [int] was never generated: we can't allocate an instance of this class.
     * @note vtable is the vtable of the generated array class, not the element type!
     * @note used by _soX_newarray
     */
    void* CreateArray(int arrayLength, void** vtable);
    void* CreateArray(const STypeRef& elementTypeRef, int elementCount);

    /**
     * For reference types, the value is a pointer to the object.
     * For valuetypes, the value is a pointer to the variable which holds the object.
     */
    void SetArrayElement(void* obj, int index, void* value);

    /**
     * Gets a function pointer to the machine code implementation of the method defined by its className
     * and methodName.
     */
    void* GetFunctionPointer(const char* className, const char* methodName) const;
    void* GetFunctionPointer(const CMethod* method) const;

    // **************
    //   Debugging.
    // **************

    /**
     * Implements _soX_break
     */
    void Break();

    /**
     * Converts a Skizo object into its string representation by dynamically executing the
     * "toString" method (if any).
     * Parameter "objClass": valuetypes don't embed class information; pass a class reference to
     * supply such information.
     * @note Returns null if the object is null.
     * @note Doesn't support primitive valuetypes.
     * Implemented in Debugging.cpp
     */
    const skizo::core::CString* GetStringRepresentation(const void* soObj, const CClass* objClass = nullptr) const;

    // Implemented in Debugging.cpp
    const skizo::core::CString* GetStackTraceInfo() const;

    // Implemented in Debugging.cpp
    void PrintStackTrace() const;

    // Retrieves values from object's properties (a property is defined as a parameterless
    // method that returns a value).
    bool GetBoolProperty(void* obj, const char* propName) const;
    float GetFloatProperty(void* obj, const char* propName) const;
    void* GetIntptrProperty(void* obj, const char* propName) const;
    const skizo::core::CString* StringProperty(void* obj, const char* propName) const;
    void* GetPropertyImpl(void* obj, const char* propName, const STypeRef& targetType) const;

    /**
     * Retrieves the last error message (on abort) for the current domain/thread.
     * The retrieved value is guaranteed to exist as long as the domain's thread.
     * Returns null if there were no errors.
     */
    static const char* GetLastError();

    /**
     * Returns an object which allows to access profiling data (see Profiling.h)
     * @note Only methods that were called at least once are reported here.
     * See Profiling.h for more info.
     */
    class CProfilingInfo* GetProfilingInfo() const;

    /**
     * The thread the domain is attached to.
     */
    skizo::core::CThread* Thread() const { return m_thread; }

    // Frame management. Do not call directly. TODO try to hide it
    void PushFrame(CMethod* method) { m_stackFrames->Push((void*)method); }
    CMethod* PopFrame() { return (CMethod*)m_stackFrames->Pop(); }
    skizo::collections::CStack<void*>* DebugDataStack() const { return m_debugDataStack; }
    int FrameCount() const { return m_stackFrames->Count(); }

    // *************
    //   Remoting.
    // *************

    /**
     * Used to verify runtime versions of different domains match.
     */
    int RuntimeVersion() const { return m_runtimeVersion; }

    /**
     * Exports the given GC-allocated object, allowing other domains to be able to import it as a foreign object.
     * Implemented in Remoting.cpp
     */
    void ExportObject(const skizo::core::CString* name, void* soObj);

    /**
     * Dispatches incoming message requests from other domains. Accepts a GC-allocated predicate to stop listening
     * when necessary.
     * Implemented in Remoting.cpp
     */
    void Listen(void* soStopPred);

    /**
     * Enqueues a message (for remoting).
     */
    void EnqueueMessage(CDomainMessage* msg) { m_msgQueue.Enqueue(msg); }

    skizo::core::CWaitObject* ResultWaitObject() const { return m_resultWaitObject; }

    // *************
    //   Security.
    // *************

    /**
     * ICalls can demand permissions by their name.
     * Implemented in Security.cpp
     */
    static void DemandPermission(const char* name);

    /**
     * Demands FileIOPermission and verifies that the path is safe (for untrusted domains).
     */
    static void DemandFileIOPermission(const skizo::core::CString* path);

    /**
     * Returns whether the current domain is trusted (i.e. can run unsafe code).
     *
     * Implemented in Security.cpp
     */
    bool IsTrusted() const;

    /**
     * Returns true if this domain is the base domain, i.e. it occupies the main thread of the process and is
     * the root of the domain hierarchy in the current application.
     * When the base domain dies, all other domains die as well.
     * Only the base domain is allowed to call Application::exit(..)
     *
     * @warning This API is unstable if the domain is injected into a non-Skizo thread (see CThread::Current())
     */
    bool IsBaseDomain() const;

    /**
     * Returns a list of permissions given to the current domain.
     */
    const skizo::collections::CArrayList<const skizo::core::CString*>* GetPermissions() const;

    // **********************
    //    Active settings.
    // **********************

    bool ProfilingEnabled() const { return m_profilingEnabled; }
    bool StackTraceEnabled() const { return m_stackTraceEnabled; }
    bool ExplicitNullCheck() const { return m_explicitNullCheck; }
    bool SoftDebuggingEnabled() const { return m_softDebuggingEnabled; }
    bool InlineBranching() const { return m_inlineBranching; }
    bool SafeCallbacks() const { return m_safeCallbacks; }
    const skizo::collections::CArrayList<const skizo::core::CString*>* SearchPaths() const { return m_searchPaths; }

    // **********************
    //       Classes.
    // **********************

    const skizo::collections::CArrayList<CClass*>* Classes() const { return m_klasses; }

    CClass* BoolClass() const { return m_boolClass; }
    CClass* CharClass() const { return m_charClass; }
    CClass* ErrorClass() const { return m_errorClass; }
    CClass* StringClass() const { return m_stringClass; }

    const skizo::collections::CHashMap<SStringSlice, CClass*>* BoxedClasses() const { return m_boxedClassMap; }
    const skizo::collections::CHashMap<SStringSlice, CClass*>* ForeignProxies() const { return m_foreignProxyMap; }
    const skizo::collections::CArrayList<CClass*>* Extensions() const { return m_extensions; }
    const skizo::collections::CArrayList<CClass*>* Aliases() const { return m_aliases; }

    // ********************
    //      Unsorted.
    // ********************

    // TODO clean up this mess

    skizo::collections::CHashMap<CArrayInitializationType*, int>* ArrayInitHelperRegistry() const { return m_arrayInitHelperRegistry; }
    skizo::collections::CHashMap<SStringSlice, CClass*>* IdentityComparisonHelpers() const { return m_identCompHelperMap; }
    void AddForcedTypeRef(CForcedTypeRef* forcedTypeRef) { m_forcedTypeRefs->Add(forcedTypeRef); } // TODO delete
    skizo::collections::CArrayList<CForcedTypeRef*>* ForcedTypeRefs() const { return m_forcedTypeRefs; }
    void AddAlias(CClass* alias) { m_aliases->Add(alias); }
    void AddExtension(CClass* klass) { m_extensions->Add(klass); }
    bool ContainsSource(const skizo::core::CString* source) { return m_sourceSet.Contains(source); }
    void EnqueueSource(const skizo::core::CString* newSource)
    {
        m_sourceQueue.Enqueue(newSource);
        m_sourceSet.Set(newSource, newSource);
    }
    void AddModule(CModuleDesc* module) { m_modules->Add(module); }
    void AddECall(void* ecall) { m_eCalls->Add(ecall); }
    void MarkMethodAsICall(CMethod* method) { m_icallMethodSet->Set((void*)method, (void*)method); }

private:
    // Registers all the standard icalls defined in icalls and elsewhere.
    void registerStandardICalls();

#ifdef SKIZO_WIN
    // When a function frame is larger than 4K, TCC emits calls to __chkstsk to make sure the stack uses properly commited
    // memory. We redirect it to NTDLL on Windows. TCC used to linked in lots of garbage, we removed it, however we still have
    // to deal with this one.
    // TODO make sure NTDLL.DLL's function is actually what TCC expects (arg count etc.)
    void chkstkHack();
#endif

    // An ICall is a subtype of a native method which is implemented internally in the runtime.
    // On the other hand, eCalls (external methods) are implemented outside, in external dynamically linked modules.
    // WARNING The method isn't automatically guarded with g_globalMutex
    void registerICall(const char* name, void* ptr);

    // Verifies all native methods defined in Skizo code have actual machine code implementations linked in.
    void verifyICallIsRegistered(CMethod* pMethod) const;

    // Generates an array class if none is found. Array classes are generated lazily, on demand.
    // From the point of view of the emitter/reflection, generated array classes are average classes
    // with the name "0Array_%d" where %d is a unique ID.
    // Array classes are specially treated by the GC, though. A "GC map" in an array class is dynamic
    // and depends on the number of entries in a given instance. Therefore, instead of generated GC maps
    // per instance, GC is able to recognize array classes and inspect them depending on their length.
    // WARNING Internally used by ::ResolveTypeRef, don't use directly.
    // NOTE Implemented in TypeResolution.cpp
    // WARNING For use in the transformation phase.
    bool resolveArrayClass(STypeRef& typeRef);

    // Creates a failable wrapper for a given type.
    // WARNING Internally used by ::ResolveTypeRef, don't use directly.
    // NOTE Implemented in TypeResolution.cpp
    // WARNING For use in the transformation phase.
    void resolveFailableStruct(STypeRef& typeRef);

    // Creates a foreign proxy for a given type.
    // WARNING Internally used by ::ResolveTypeRef, don't use directly.
    // NOTE Implemented in TypeResolution.cpp
    // WARNING For use in the transformation phase.
    bool resolveForeignProxy(STypeRef& typeRef);

    // Initializes metadata for basic types.
    void initBasicClasses();
    void initStringClass();
    void initRangeStruct();
    void initPredicateClass();
    void initRangeLooperClass();
    void initActionClass();
    void initErrorClass();

    // Used by registerStandardICalls() to avoid linking icalls for classes that were never loaded.
    bool isClassLoaded(const char* className) const;

    // A source may be found in different directories. This method takes the search pathes into
	// consideration.
	// out_isBaseModule returns if the source was found in the base module directory.
    const skizo::core::CString* readSource(const skizo::core::CString* source, bool* out_isBaseModule) const;

    // Aborts the current domain with a message.
    // @param free If true, uses CString::FreeUtf8 to destroy the string once the stack unwinds and
	// the message is printed.
    static void abortImpl(char* msg, bool free);

    // Gets a property's machine code implementation.
    // Used by CDomain::GetBoolProperty & friends.
    void* getPropertyImpl(void* obj, const char* propName, const STypeRef& targetType) const;

    // Some intrinsic classes (like Map and DomainHandle) have too complex definitions to describe them from C++,
    // so we have to describe it in Skizo. However, those files may become corrupted/get out of sync
    // with the native representation. Hence this function to check it.
    void verifyIntrinsicClasses();

    // After a stack overflow is detected, this function removes repeated elements from the stack trace.
    // Used by _soX_abort0 right before aborting.
    void correctStackTraceAfterStackOverflow();

private:
    // Just to be sure (used in remoting).
    int m_runtimeVersion;

    // **************************
    //   Synchronization stuff.
    // **************************

    // TCC (the C backend) was found to be potentially thread-unsafe according to various reports.
	// We have to wrap the code generator with a mutex.
    static skizo::core::CMutex* g_globalMutex;

    skizo::core::CThread* m_thread;
    // Every domain possesses a special wait object which allows the receiver
    // domain to signal to the sender thread that the function is ready (used in blocking
    // cross-domain calls).
    skizo::core::Auto<skizo::core::CWaitObject> m_resultWaitObject;

    // Message queue for inter-domain communication.
    SDomainMessageQueue m_msgQueue;

    // **************************

    // List of all modules in the domain. The container is used to simply keep them alive.
    skizo::core::Auto<skizo::collections::CArrayList<CModuleDesc*>> m_modules;

    // List of all classes in the domain. Don't use directly, use ::RegisterClass(..) instead.
    skizo::core::Auto<skizo::collections::CArrayList<CClass*>> m_klasses;

    // Same as "m_klasses", but allows to find classes by names much faster.
    // Don't use directly, use ::RegisterClass instead.
    // NOTE The classmap maps _internal_ (float, resolved) names to classes. To map _nice_ names to classes,
	// use m_niceNameMap (for reflection).
    skizo::collections::CHashMap<SStringSlice, CClass*> m_klassMap;

    // Unlike m_klassMap, uses nice names as keys instead of internal names (flat).
    // For reflection.
    mutable skizo::collections::CHashMap<const skizo::core::CString*, CClass*> m_niceNameMap;

    // A class map for primitives. It's separate from m_klassMap for faster resolution.
    skizo::collections::CHashMap<int, CClass*> m_primKlassMap;

    // Map of all generated array classes (NOTE array classes are present in m_klasses and m_klassMap as well).
    skizo::collections::CHashMap<STypeRef, CClass*> m_arrayClassMap;

    // Map of all generated boxing wrappers (NOTE boxing wrappers are present in m_klasses and m_klassMap as well).
    skizo::core::Auto<skizo::collections::CHashMap<SStringSlice, CClass*>> m_boxedClassMap;

    // Map of all generated failable wrappers (NOTE failable wrappers are present in m_klasses and m_klassMap
	// as well).
    skizo::collections::CHashMap<SStringSlice, CClass*> m_failableClassMap;

    // Map of all generated foreign proxies (NOTE failable wrappers are present in m_klasses and m_klassMap
	// as well).
    skizo::core::Auto<skizo::collections::CHashMap<SStringSlice, CClass*>> m_foreignProxyMap;

    // Map of all defined aliases (NOTE aliases are present in m_klasses and m_klassMap as well).
    skizo::core::Auto<skizo::collections::CArrayList<CClass*>> m_aliases;

    // Implements the "force" feature.
    skizo::core::Auto<skizo::collections::CArrayList<CForcedTypeRef*>> m_forcedTypeRefs;

    // A list of extensions. Classes are merged with them after parsing is finished.
    // NOTE The classes are incomplete just for the metadata and can't be used directly (for allocating etc.)!
    skizo::core::Auto<skizo::collections::CArrayList<CClass*>> m_extensions;

    // A list of valuetype classes that require the emitter to emit special code to compare their identities
    // bitwise. Populated by the transformer in STransformer::inferIdentCompExpr(..) and then used by
    // SEmitter::emitIdentCompExpr(..)
    skizo::core::Auto<skizo::collections::CHashMap<SStringSlice, CClass*>> m_identCompHelperMap;

    // Direct references to a few built-in classes for faster access in certain parts of the runtime.
    CClass* m_stringClass;
    CClass* m_boolClass;
    CClass* m_charClass;
    CClass* m_rangeClass;
    CClass* m_errorClass;

    // Tokens refer to string slices that refer to the original code.
    // So we must save a reference to the original code so that it wasn't destroyed.
    skizo::collections::CArrayList<const skizo::core::CString*> m_cachedCode;

    // Often, the VM generates new names. Class, method etc. names expect string slices everywhere
    // that usually refer to the original script code. We don't have new names in the old script code,
    // so we have to generate new Skizo strings and make string slices refer to them.
    mutable skizo::collections::CHashMap<const char*, const skizo::core::CString*> m_stringTable1;
    mutable skizo::collections::CHashMap<const skizo::core::CString*, const skizo::core::CString*> m_stringTable2;

    // All interned strings are stored here.
    // This container is used only as a cache. Algorithms should not depend on its context.
    skizo::collections::CHashMap<const skizo::core::CString*, skizo::core::CObject*> m_internedStrings;

    // A list of search paths.
    skizo::core::Auto<skizo::collections::CArrayList<const skizo::core::CString*> > m_searchPaths;

    // Map "specific ArrayInitExprType => uniqueId of a generated method".
    // Every ArrayInitExpression has a generated method associated. Those methods differ
    // from each other in their IDs.
    skizo::core::Auto<skizo::collections::CHashMap<CArrayInitializationType*, int>> m_arrayInitHelperRegistry;

    // ***********************************************************************************
    //   ICalls & ECalls.
    // a) ICalls -- implemented inside the runtime itself, linked statically.
    // b) ECalls -- implemented externally in a separate native module, loaded dynamically.
    // ***********************************************************************************

    // Remembers which icalls were added. Used by verifyICallIsRegistered(..), populated
    // by registerICall.
    skizo::collections::CHashMap<const char*, void*> m_icallImplSet;

    // Remembers during transformation which Skizo methods were marked as ICalls.
    // After all the ICalls are registered, walks through this list and checks in m_icallImplSet
    // if all the icalls have implementations assigned.
    skizo::core::Auto<skizo::collections::CHashMap<void*, void*>> m_icallMethodSet;

    skizo::core::Auto<skizo::collections::CArrayList<void*>> m_eCalls;
    // ***********************************************************************************

    // Memory Manager.
    SMemoryManager m_memMngr;

    // Security Manager.
    SSecurityManager m_securityMngr;

    // Thunk Manager.
    SThunkManager m_thunkMngr;

    // External call (nmodules) cache.
    SECallCache m_ecallCache;

    SActivator m_activator;

    // *************************************************************
    //   Various flags and states remembered from SDomainCreation.
    // *************************************************************

    skizo::core::Auto<const skizo::core::CString> m_entryPointClass; // can be null, "Program" is assumed
    skizo::core::Auto<const skizo::core::CString> m_entryPointMethod; // can be null, "main" is assumed

    bool m_stackTraceEnabled;
    bool m_profilingEnabled;
    bool m_softDebuggingEnabled;
    bool m_explicitNullCheck;
    bool m_safeCallbacks;
    bool m_inlineBranching;

    // ******************************************************
    //   Supporting structures for the "import" expression.
    // ******************************************************

    // As we parse a source file, we find requests to add a new source to the program. All the new
	// requests are enqueued to this queue. In CDomain::CreateDomain, after SParser finishes
	// with a source, it checks if there are new sources added and parses again until the queue is empty.
    skizo::collections::CQueue<const skizo::core::CString*> m_sourceQueue;

    // Different files may want to request the same source for import, and we don't want to reparse
	// the same file multiple times. This hashset remembers which sources were already imported.
    skizo::collections::CHashMap<const skizo::core::CString*, const skizo::core::CString*> m_sourceSet;

    // *****************************************************

    // A stack of references to CMethod*, populated at runtime by _soX_pushframe/
    // _soX_popframe only, and if only SDomainCreation::StackTraceEnabled is set to true.
    // Allows to print nicer errors.
    skizo::core::Auto<skizo::collections::CStack<void*>> m_stackFrames;

    // *******************
    //   Soft debugging.
    // *******************

    // to avoid infinite recursion when calling random "toString"'s from inside _soX_break
    bool m_disableBreak;

    // A stack of registered locals. Used only if SoftDebuggingEnabled==true.
    // The stack should be correctly balanced by the emitted code.
    // The order is this for a single frame:
    //  a) this (if the method is not static)
    //  b) N params according to pMethod
    //  c) N locals according to pMethod
    //  d) the total size of the frame (so that _soX_unreglocals could quickly unwind it)
    //
    // The current pMethod is found via m_stackFrames because SoftDebuggingEnabled guarantees
    // stack tracing is enabled (see CDomain::CreateDomain).
    skizo::core::Auto<skizo::collections::CStack<void*>> m_debugDataStack;

    SKIZO_BREAKPOINTCALLBACK m_breakpointCallback;

    // *************************
    //   Used by the profiler.
    // *************************
    so_long m_time;
    skizo::core::Auto<const skizo::core::CString> m_domainName;
    // *************************

    // Each domain has a process-unique id. API for domain communication employs id's.
    int m_id;

    // Code generator.
    TCCState* m_tccState;
    bool m_readyForEpilog; // If the state is not ready at the time of an abort, don't call the epilog!

    // Used by NewUniqueId().
    mutable int m_uniqueIdCount;

    // A buffer for generating nice errors.
    mutable STextBuilder m_errorBuilder;
};

// Do not call directly.
void __InitDomain();
void __DeinitDomain();

} }

#endif // DOMAIN_H_INCLUDED

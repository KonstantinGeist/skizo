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

#include "Remoting.h"
#include "Abort.h"
#include "Contract.h"
#include "Domain.h"
#include "icall.h"
#include "Marshal.h"
#include "MemoryManager.h"
#include "ModuleDesc.h"
#include "RuntimeHelpers.h"

#define DOMAIN_TIMEOUT 3000
#define MESSAGEQUEUE_TIMEOUT 100
#define REMOTECALL_TIMEOUT 2000

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

static void coreStringToFlatString(so_char16* buf, int bufSize, const CString* str, const char* errorMsg)
{
    if(str->Length() >= bufSize + 1) {
        CDomain::Abort(errorMsg);
    }
    so_wcscpy_16bit(buf, str->Chars());
}

static void stringSliceToFlatString(so_char16* buf, int bufSize, const SStringSlice& slice, const char* errorMsg)
{
    const int length = slice.End - slice.Start;
    if(length >= bufSize + 1) {
        CDomain::Abort(errorMsg);
    }
    const so_char16* src = slice.String->Chars() + slice.Start;
    memcpy(buf, src, length * sizeof(so_char16));
    buf[length] = 0;
}

// ***********************
//      CDomainHandle
// ***********************

CDomainHandle::CDomainHandle()
    : DomainMutex(new CMutex()),
      ReadinessWaitObject(new CWaitObject(false, false)), // Non-signaled, manual reset.
      Domain(nullptr)
{
}

bool CDomainHandle::waitForDomainReadiness(bool doThrowOnTimeout) const
{
    if(!this->ReadinessWaitObject) {
        return true;
    }
    const bool b = CThread::Wait(this->ReadinessWaitObject, DOMAIN_TIMEOUT);
    if(doThrowOnTimeout && !b) {
        CDomain::Abort("Target domain does not respond.");
    }
    if(b) {
        this->ReadinessWaitObject.SetPtr(nullptr);
    }
    return b;
}

void CDomainHandle::SetDomain(class CDomain* domain)
{
    SKIZO_LOCK_AB(this->DomainMutex) {
        this->Domain = domain;
    } SKIZO_END_LOCK_AB(this->DomainMutex);
}

void CDomainHandle::SignalDomainIsReady()
{
    this->ReadinessWaitObject->Pulse();
}

CDomain* CDomainHandle::getDomain() const
{
    waitForDomainReadiness();

    CDomain* r = nullptr;
    SKIZO_LOCK_AB(this->DomainMutex) {
        r = this->Domain;
        if(r) {
            r->Ref();
        }
    } SKIZO_END_LOCK_AB(this->DomainMutex);

    return r;
}

bool CDomainHandle::IsAlive() const
{
    Auto<CDomain> domain (getDomain());
    return domain? true: false;
}

bool CDomainHandle::Wait(int timeout)
{
    const bool b = waitForDomainReadiness(false);
    if(!b) {
        return b;
    }

    CThread* domainThread = nullptr;
    SKIZO_LOCK_AB(this->DomainMutex) {
        if(this->Domain) {
            domainThread = this->Domain->Thread();
            domainThread->Ref();
        }
    } SKIZO_END_LOCK_AB(this->DomainMutex);
    // **********************************************

    if(domainThread) { // The domain might have been destroyed.
        try {

            CThread::Join(domainThread, timeout);

        } catch (SException& e) {
            if(domainThread) {
                domainThread->Unref();
                domainThread = nullptr;
            }

            CDomain::Abort(const_cast<char*>(e.Message()));
        }

        if(domainThread) {
            domainThread->Unref();
            domainThread = nullptr;
        }
    }

    return true;
}

void CDomainHandle::SendMessageSync(CDomainMessage* msg, void* blockingRet, int timeout)
{
    Auto<CDomain> domain (getDomain());
    if(domain) { // The domain might have been destroyed.
        domain->EnqueueMessage(msg);

        const bool b = CThread::Wait(msg->ResultWaitObject, timeout);
        if(!b) {
            CDomain::Abort("Cross-domain method call timed out (target domain too busy, terminated or never enters Domain::listen(..))");
        }

        if(msg->ErrorMessage) {
            CDomain::Abort((char*)msg->ErrorMessage, msg->FreeErrorMessage);
        }
    } else {
        CDomain::Abort("Can't make a cross-domain method call because the target domain was destroyed.");
    }
}

// ***********************
//   RemoteDomainThread
// ***********************

#define REMOTE_DOMAIN_COOKIE 1234

// Each new domain corresponds to a separate thread.
class CRemoteDomainThread: public CThread
{
public:
    // WARNING Used to pass flags from the original thread to the remote thread.
    // Don't save objects with non-atomic reference counting here to avoid thread sharing. 
    SDomainCreation DomainCreation;

    // NOTE UTF8 strings are used to avoid unsafe thread sharing of CString's.
    char *Source, *EntryPointClass, *EntryPointMethod;
    Auto<CArrayList<char*>> SearchPaths;
    Auto<CArrayList<char*>> Permissions;

    Auto<CDomainHandle> DomainHandle;

    CRemoteDomainThread()
        : Source(nullptr), EntryPointClass(nullptr), EntryPointMethod(nullptr),
          SearchPaths(new CArrayList<char*>()),
          Permissions(new CArrayList<char*>())
    {
    }

    virtual ~CRemoteDomainThread()
    {
        CString::FreeUtf8(this->Source);
        CString::FreeUtf8(this->EntryPointClass);
        CString::FreeUtf8(this->EntryPointMethod);

        for(int i = 0; i < this->SearchPaths->Count(); i++) {
            CString::FreeUtf8(this->SearchPaths->Array()[i]);
        }

        for(int i = 0; i < this->Permissions->Count(); i++) {
            CString::FreeUtf8(this->Permissions->Array()[i]);
        }
    }

    CDomainHandle* PrepareOnCurrentThreadAndStart(const CString* source, ESourceKind sourceKind, const CArrayList<const CString*>* permArray)
    {
        this->DomainHandle.SetPtr(new CDomainHandle());

        prepareDomainCreationOnCurrentThread(source, sourceKind, permArray);
        this->Start();

        // The domain handle will be wrapped by a Skizo object. It will hold a strong reference to this object (tied to GC collections).
        this->DomainHandle->Ref();
        return this->DomainHandle;
    }

protected:
    virtual void OnStart() override
    {
        remoteMain(REMOTE_DOMAIN_COOKIE);
    }

private:
    void remoteMain(int stackBase)
    {
        prepareDomainCreationOnRemoteThread(&stackBase);

        Auto<CDomain> domain;
        try {
            domain.SetPtr(CDomain::CreateDomain(this->DomainCreation));
        } catch (const SoDomainAbortException& e) {
            printf("ABORT (domain creation): %s\n", e.Message); // TODO generic error/output interface
            _so_StackTrace_print();
            freeOnRemoteThread();
            return;
        }

        this->DomainHandle->SetDomain(domain);
        this->DomainHandle->SignalDomainIsReady();
        domain->InvokeEntryPoint();
        this->DomainHandle->SetDomain(nullptr);

        freeOnRemoteThread();
    }

    void prepareDomainCreationOnCurrentThread(const CString* source, ESourceKind sourceKind, const CArrayList<const CString*>* permArray)
    {
        setSources(sourceKind, source);
        this->DomainCreation.UseSourceAsPath = (sourceKind == E_SOURCEKIND_PATH || sourceKind == E_SOURCEKIND_METHODNAME);

        // NOTE The new domain inherits some of the settings.
        const CDomain* pCurDomain = CDomain::ForCurrentThread();
        this->DomainCreation.StackTraceEnabled = pCurDomain->StackTraceEnabled();
        this->DomainCreation.ExplicitNullCheck = pCurDomain->ExplicitNullCheck();
        this->DomainCreation.InlineBranching = pCurDomain->InlineBranching();
        this->DomainCreation.SoftDebuggingEnabled = pCurDomain->SoftDebuggingEnabled();

        // Inherits the search paths.
        const CArrayList<const CString*>* searchPaths = pCurDomain->SearchPaths();
        for(int i = 0; i < searchPaths->Count(); i++) {
            this->SearchPaths->Add(searchPaths->Array()[i]->ToUtf8());
        }

        // Passed from Domain::runGenericImpl(..) which treats the domain as untrusted if permArray is non-null.
        if(permArray) {
            this->DomainCreation.IsUntrusted = true;

            for(int i = 0; i < permArray->Count(); i++) {
                this->Permissions->Add(permArray->Array()[i]->ToUtf8());
            }
        }
    }

    void prepareDomainCreationOnRemoteThread(void* stackBase)
    {
        this->DomainCreation.StackBase = stackBase;
        this->DomainCreation.Source = CString::FromUtf8(this->Source);
        this->DomainCreation.EntryPointClass = this->EntryPointClass? CString::FromUtf8(this->EntryPointClass): nullptr;
        this->DomainCreation.EntryPointMethod = this->EntryPointMethod? CString::FromUtf8(this->EntryPointMethod): nullptr;

        for(int i = 0; i < this->SearchPaths->Count(); i++) {
            char* searchPath = this->SearchPaths->Array()[i];
            this->DomainCreation.AddSearchPath(searchPath);
        }

        for(int i = 0; i < this->Permissions->Count(); i++) {
            char* permission = this->Permissions->Array()[i];
            Auto<const CString> soPermission (CString::FromUtf8(permission));
            this->DomainCreation.AddPermission(soPermission);
        }
    }

    void freeOnRemoteThread()
    {
        this->DomainCreation.Source->Unref();
        if(this->DomainCreation.EntryPointClass) {
            this->DomainCreation.EntryPointClass->Unref();
            this->DomainCreation.EntryPointClass = nullptr;
        }
        if(this->DomainCreation.EntryPointMethod) {
            this->DomainCreation.EntryPointMethod->Unref();
            this->DomainCreation.EntryPointMethod = nullptr;
        }
    }

    // If the source is a method name, parse it, get the declaring module of the specified method.
    // Implements Domain::runMethod(..) and Domain::runMethodUntrusted(..)
    void setSources(ESourceKind sourceKind, const CString* source)
    {
        if(sourceKind == E_SOURCEKIND_METHODNAME) {
            // Extracts the class and method names.
            Auto<CArrayList<const CString*> > parts (source->Split(SKIZO_CHAR('/')));
            if(parts->Count() != 2) {
                CDomain::Abort("Method name must be in the form \"Class/method\".");
            }

            const CString* entryPointClass = parts->Item(0);
            const CString* entryPointMethod = parts->Item(1);

            // Finds the class and the method in the metadata of the current domain.
            const CDomain* curDomain = CDomain::ForCurrentThread();
            const CClass* klass = curDomain->ClassByNiceName(entryPointClass); // NOTE Nice names are used.
            if(!klass) {
                abortValidEntryPointNotFound();
            }

            const SStringSlice methodName (entryPointMethod);
            const CMethod* method = klass->MyMethod(methodName, true, E_METHODKIND_NORMAL);
            if(!method || !method->IsValidEntryPoint()) {
                abortValidEntryPointNotFound();
            }

            if(!method->Source().Module || !method->Source().Module->FilePath) { // just in case
                abortValidEntryPointNotFound();
            }

            // Finally: the corrected source path is set, the entrypoint is remembered.
            this->Source = method->Source().Module->FilePath->ToUtf8();
            this->EntryPointClass = entryPointClass->ToUtf8();
            this->EntryPointMethod = entryPointMethod->ToUtf8();
        } else {
            this->Source = source->ToUtf8();
        }
    }

    void abortValidEntryPointNotFound()
    {
        CDomain::Abort("Domain creation fail: valid entrypoint not found.");
    }
};

CDomainHandle* CDomain::CreateRemoteDomain(const CString* source, ESourceKind sourceKind, const CArrayList<const CString*>* permArray)
{
    Auto<CRemoteDomainThread> domainThread (new CRemoteDomainThread());
    return domainThread->PrepareOnCurrentThreadAndStart(source, sourceKind, permArray);
}

// *****************
//   DomainMessage
// *****************

// TODO refactor

bool CClass::HasReferencesForRemoting() const
{
    SKIZO_REQ(this->IsValueType(), EC_INVALID_STATE);

    if(!m_hasReferencesForRemoting.HasValue()) {
        for(int i = 0; i < m_instanceFields->Count(); i++) {
            const CField* field = m_instanceFields->Array()[i];
            SKIZO_REQ_PTR(field->Type.ResolvedClass);
            const CClass* fieldClass = field->Type.ResolvedClass;

            if(fieldClass->PrimitiveType() == E_PRIMTYPE_OBJECT) {
                if(fieldClass->IsValueType()) {
                    if(fieldClass->HasReferencesForRemoting()) {
                        m_hasReferencesForRemoting.SetValue(true);
                        return true;
                    }
                } else {
                    m_hasReferencesForRemoting.SetValue(true);
                    return true;
                }
            }
        }

        m_hasReferencesForRemoting.SetValue(false);
        return false;
    } else {
        return m_hasReferencesForRemoting.Value();
    }
}

#define MSG_TOO_LARGE (-1)
#define UNSAFE_TYPES_NOT_SUPPORTED (-2)
#define INVALID_MSG (-3)
#define NON_VALUETYPE_SERIALIZATION (-4)
#define UNKNOWN_UNDERLYING_TYPE (-5)
#define INCOMING_FOREIGN_NOT_FOUND (-6)
#define FOREIGN_TO_FOREIGN_DISALLOWED (-7)
#define SPECIAL_CLASS_NOT_SUPPORTED (-8)
#define CANT_DESERIALIZE_BOXED_VALUETYPE (-9)
#define REFERENCES_IN_BOXED_VALUETYPES_DISALLOWED (-10)
static const char* msgtbl[10] = {
    "Cross-domain message too large.",
    "Type 'intptr' and native layouts not supported in cross-domain calls.",
    "Invalid message.",
    "Local non-valuetype objects can't pass domain boundaries by default (export required).",
    "Unknown underlying type cast to interface during deserialization.",
    "Incoming foreign object under this name not found in the domain (unregistered between calls?)",
    "Foreign objects can't travel from foreign domain to foreign domain (only foreign-to-local and local-to-foreign allowed).",
    "Special class not supported.",
    "Can't deserialize a boxed valuetype object because the domain lacks compiled code for the boxed version of the valuetype ('force boxed T' required).",
    "Boxed valuetypes serialized across domain boundaries aren't allowed to contain references in the current implementation."
};
#define MSG_FROM_RETURN(r) msgtbl[(r * -1) - 1]

// Passed to ::serialize(..)
struct SSerializationContext
{
    // Contains a reference to the domain handle we're dealing with so that we could compare it to serialized foreign objects
    // and make sure the foreign objects are passed to the same domain which created them.
    CDomainHandle* TargetHDomain;

    SSerializationContext(CDomainHandle* hdomain)
        : TargetHDomain(hdomain)
    {
    }
};

// NOTE Don't abort here as we ref' our strings here. If we abort, they will never be unref'd, leaking memory.
int CClass::SerializeForRemoting(void* _soObj, char* buf, int bufSize, struct SSerializationContext* context) const
{
    char* soObj = (char*)_soObj;

    const ESpecialClass specialClass = this->SpecialClass();
    if(specialClass == E_SPECIALCLASS_FOREIGN) {

        if(_soObj) {

            if(!context->TargetHDomain) { // if context->TargetHDomain == null, we're serializing the return,
                                          // meaning we're trying to return a foreign object to a foreign domain
										  // (which is explicitly disallowed by the spec)
                return FOREIGN_TO_FOREIGN_DISALLOWED;
            }

            // Checks if the target domain is identical to the owner domain of this foreign object
            // (basically, the foreign object travels back to the original domain it stems from).
            // We don't allow yet to pass a foreign object to a foreign domain which is not the original
            // home of said foreign object.
            const SForeignProxyHeader* proxyHeader = (SForeignProxyHeader*)_soObj;
            const CDomainHandle* proxyOwner = proxyHeader->hDomain->wrapped;
            if(proxyOwner != context->TargetHDomain) {
                return FOREIGN_TO_FOREIGN_DISALLOWED;
            }

            // Foreign classes are dealt with differently: they are marshaled as string names.
            // When the target domain receives the message, it deserializes it by matching the name against
            // the dictionary of exported objects.
            // Note that only _foreign_ non-valuetypes are allowed to pass through domain boundaries, meaning
            // there's no need for a special header to differiantiate between passed foreigns and non-foreigns,
            // as any non-valuetype (except interfaces/strings which are dealt with differently) is foreign by default
            // here.

            const CString* objectName = so_string_of(proxyHeader->name);

            // Embeds the nice name of the type.
            const int nameSize = sizeof(int) // length header
                               + objectName->Length() * sizeof(so_char16); // content
            if(bufSize < nameSize) {
                return MSG_TOO_LARGE;
            }
            *((int*)buf) = objectName->Length(); // length header again
            memcpy(buf + sizeof(int),
                   objectName->Chars(),
                   objectName->Length() * sizeof(so_char16)); // content again

            return nameSize;

        } else {

            // If the object is null, no runtime information is extractable: serialize zero-length nice name header
            // meaning the value is nothing.
            // TODO?

            const int nullSize = sizeof(int);
            if(bufSize < nullSize) {
                return MSG_TOO_LARGE;
            }

            memset(buf, 0, nullSize);
            return nullSize;

        }

    } else if(specialClass == E_SPECIALCLASS_INTERFACE) {

        // Special case for interfaces: interfaces are a mere compile time feature; in runtime the actual type
        // depends on the object. Objects cast to interfaces are always reference-type. What we do here is to
        // find the actual type of the argument and redirect serialization to it.
        // It's all good and all, but the target domain can't know what the underlying type is in order to correctly
        // read serialized data. So we have to also serialize the nice name of the type in front of serialized
        // data.

        if(_soObj) {
            CClass* pActualClass = so_class_of(_soObj);

            const CString* niceName = pActualClass->NiceName();

            // Embeds the nice name of the type.
            const int intrfcHdrSize = sizeof(int) // length header
                                    + niceName->Length() * sizeof(so_char16); // content
            if(bufSize < intrfcHdrSize) {
                return MSG_TOO_LARGE;
            }
            *((int*)buf) = niceName->Length(); // length header again
            memcpy(buf + sizeof(int),
                   niceName->Chars(),
                   niceName->Length() * sizeof(so_char16)); // content again

            // Redirects serialization.
            const int r = pActualClass->SerializeForRemoting(_soObj, buf + intrfcHdrSize, bufSize - intrfcHdrSize, context);
            if(r < 0) {
                return r; // don't forget about potential errors signaled as negative sizes
            } else {
                return r + intrfcHdrSize;
            }
        } else {
            // If the object is null, no runtime information is extractable: serialize zero-length nice name header
            // meaning the value is nothing.

            const int nullIntrfcSize = sizeof(int);
            if(bufSize < nullIntrfcSize) {
                return MSG_TOO_LARGE;
            }

            memset(buf, 0, nullIntrfcSize);
            return nullIntrfcSize;
        }

    } else if(specialClass == E_SPECIALCLASS_BOXED) {

        const CClass* boxedClass = so_class_of(soObj)->m_wrappedClass.ResolvedClass;
        SKIZO_REQ_PTR(boxedClass);

        // NOTE The current implementation doesn't allow boxed valuetypes with references in them.
        // Checks if this is the case.
        if(boxedClass->HasReferencesForRemoting()) {
            return REFERENCES_IN_BOXED_VALUETYPES_DISALLOWED;
        }

        const int dataSize = boxedClass->GCInfo().SizeForUse;
        if(bufSize < dataSize) {
            return MSG_TOO_LARGE;
        }

        // Boxed values are only created through interfaces. We have just serialized the interface up in the call stack
		// by outputting the nice name of the actual type (boxed type), so at this point, the deserializer knows what the
		// actual type is.
        memcpy(buf, SKIZO_GET_BOXED_DATA(soObj), dataSize);
        return dataSize;

    } else if(specialClass != E_SPECIALCLASS_NONE) {

        return SPECIAL_CLASS_NOT_SUPPORTED;

    } else if(this == m_declaringDomain->StringClass()) {

        if(bufSize < (int)sizeof(const CString*)) {
            return MSG_TOO_LARGE;
        }

        // Strings can be quite large and aren't sharable anymore, so a pointer to cloned contents is passed instead.
        if(soObj) {
            const CString* str = so_string_of(soObj);
            so_char16* strContents = new so_char16[str->Length() + 1];
            so_wcscpy_16bit(strContents, str->Chars());
            *((const void**)buf) = strContents;
        } else {
            *((const void**)buf) = nullptr;
        }

        return sizeof(CString*);

    } else if(!m_structDef.IsEmpty() || this->PrimitiveType() == E_PRIMTYPE_INTPTR) {

        // We don't support native layouts just like we don't support intptr's as their values are most likely
        // meaningless in foreign domains.
        // NOTE: must follow the check for string above, since strings' layouts are implemented via structDef
        // (although we do support string marshalling).
        return UNSAFE_TYPES_NOT_SUPPORTED;

    } else if(this->PrimitiveType() == E_PRIMTYPE_OBJECT) {
        // A normal type, serializes its contents recursively according to the fields.

        // Checks if it's a valuetype. Only valuetypes are serializable by default.
        if(this->IsValueType()) {

            // Iterates over the instance fields of this valuetype object.
            int finalSize = 0;
            int srcFieldOffset = 0;
            for(int i = 0; i < m_instanceFields->Count(); i++) {
                const CField* field = m_instanceFields->Array()[i];
                SKIZO_REQ_PTR(field->Type.ResolvedClass);
                const CClass* fieldClass = field->Type.ResolvedClass;
                SKIZO_REQ_POS(fieldClass->GCInfo().SizeForUse);

                // NOTE We pass the buffer as it is for valuetypes, while reference types require
                // dereference of the buffer so that it pointed directly to the reference type.
                const int bytesWritten = fieldClass->SerializeForRemoting(fieldClass->IsValueType()?
                                                                            soObj + srcFieldOffset
                                                                          : *((void**)(soObj + srcFieldOffset)),
                                                                          buf + finalSize,
                                                                          bufSize - finalSize,
                                                                          context);
                if(bytesWritten < 0) {
                    return bytesWritten;
                }

                // As per documentation: sizeof(void*) for references classes; equals to m_contentSize for valuetypes.
                srcFieldOffset += fieldClass->GCInfo().SizeForUse;
                finalSize += bytesWritten;
            }

            return finalSize;

        } else { // !m_isValueType

            // Allows only null non-valuetype objects.
            if(soObj) {
                return NON_VALUETYPE_SERIALIZATION;
            } else {

                if(bufSize < (int)sizeof(void*)) {
                    return MSG_TOO_LARGE;
                }

                memset(buf, 0, sizeof(void*));
                return sizeof(void*);
            }

        }

    } else {

		SKIZO_REQ_NOT_EQUALS(this->PrimitiveType(), E_PRIMTYPE_VOID);

        const size_t sizeForUse = m_gcInfo.SizeForUse;
        SKIZO_REQ_POS(sizeForUse);
        if(bufSize < (int)sizeForUse) {
            return MSG_TOO_LARGE;
        }

        // Primitive types are emitted directly.
        memcpy(buf, soObj, sizeForUse);
        return sizeForUse;

    }
}

// WARNING Never abort here, or foreign domains would crash.
int CClass::DeserializeForRemoting(char* buf, int bufSize, void* output) const
{
    const ESpecialClass specialClass = this->SpecialClass();
    if(specialClass == E_SPECIALCLASS_FOREIGN) {

        // Foreign objects are serialized as strings (names).

        // Probes the length of the argument.
        if(bufSize < (int)sizeof(int)) {
            return MSG_TOO_LARGE;
        }
        const int objectNameSize = *((int*)buf);
        if(objectNameSize < 0) {
            return INVALID_MSG;
        }
        bufSize -= sizeof(int);

        if(objectNameSize) {

            // Checks to make sure there are no deliberate buffer overflows.
            if(bufSize < (int)(objectNameSize * sizeof(so_char16))) {
                return MSG_TOO_LARGE;
            }

            // Reads the name of the reimported foreign object.
            // TODO Over-allocation?
            so_char16* objectNameCs;
            Auto<const CString> objectName (CString::CreateBuffer(objectNameSize, &objectNameCs));
            memcpy(objectNameCs, buf + sizeof(int), objectNameSize * sizeof(so_char16));

            // After the name is ready, let's find this object (if any).
            void* resultObj = nullptr;
            SMemoryManager& memMgr = m_declaringDomain->MemoryManager();

            SKIZO_LOCK_AB(memMgr.ExportedObjsMutex) {
                memMgr.ExportedObjs->TryGet(objectName, &resultObj);
            } SKIZO_END_LOCK_AB(memMgr.ExportedObjsMutex);

            if(resultObj) {
                *((void**)output) = resultObj;

                return sizeof(int)                         // header
                     + objectNameSize * sizeof(so_char16); // content
            } else {
                return INCOMING_FOREIGN_NOT_FOUND;
            }

        } else {

            // No value.
            *((void**)output) = nullptr;
            return sizeof(int);

        }

    } else if(specialClass == E_SPECIALCLASS_BOXED) {

        SKIZO_REQ_PTR(this->ResolvedWrappedClass());
        const int dataSize = this->ResolvedWrappedClass()->GCInfo().SizeForUse;
        SKIZO_REQ_POS(dataSize);

        if(bufSize < dataSize) {
            return MSG_TOO_LARGE;
        }

        void* soBoxed = _soX_gc_alloc(&m_declaringDomain->MemoryManager(),
                                      m_gcInfo.ContentSize,
                                      this->VirtualTable());
        void* data = SKIZO_GET_BOXED_DATA(soBoxed);
        memcpy(data, buf, dataSize);

        *((void**)output) = soBoxed;
        return dataSize;

    } else if(specialClass == E_SPECIALCLASS_INTERFACE) {

        // Probes the name of the underlying type this interface pointer wraps.
        if(bufSize < (int)sizeof(int)) {
            return MSG_TOO_LARGE;
        }
        const int niceNameSize = *((int*)buf);
        if(niceNameSize < 0) {
            return INVALID_MSG;
        }

        if(niceNameSize) {
            // There is a value.

            // Moves the pointer forward.
            buf += sizeof(int);
            bufSize -= sizeof(int);
            // Checks to make sure there are no deliberate buffer overflows.
            if(bufSize < (int)(niceNameSize * sizeof(so_char16))) {
                return MSG_TOO_LARGE;
            }

            // Reads the nice name.
            // TODO Over-allocation?
            so_char16* niceNameCs;
            Auto<const CString> niceName (CString::CreateBuffer(niceNameSize, &niceNameCs));
            memcpy(niceNameCs, buf, niceNameSize * sizeof(so_char16));

            // Don't forget to move the pointer forward.
            buf += niceNameSize * sizeof(so_char16);
            bufSize -= niceNameSize * sizeof(so_char16);

            CClass* actualClass = m_declaringDomain->ClassByNiceName(niceName);
            if(actualClass) {
                // A special fixup for boxed stuff. Both valuetypes and their boxed classes share the same
                // nice name, so ::ClassByNiceName(..) was designed to always return the class of the actual valuetype
                // to avoid ambiguity and instability. This means that "actualClass" right now contains the actual valuetype
                // while the object in the buffer, since we're dealing with interfaces here, is certainly a boxed class.
                // What we do here is check if it's a valuetype, and then redirect it to the correct boxed class.
                if(actualClass->IsValueType()) {
                    const SStringSlice niceNameSlice (niceName, 0, niceName->Length());
                    if(m_declaringDomain->BoxedClasses()->TryGet(niceNameSlice, &actualClass)) {
                        actualClass->Unref();
                    } else {
                        // Boxed class not found.
                        return CANT_DESERIALIZE_BOXED_VALUETYPE;
                    }
                }

                // Redirect to the actual class.
                const int r = actualClass->DeserializeForRemoting(buf, bufSize, output);
                if(r < 0) {
                    return r; // don't forget about errors.
                } else {
                    return r + sizeof(int) + niceNameSize * sizeof(so_char16);
                }
            } else {
                // The wrapped type not found.
                return UNKNOWN_UNDERLYING_TYPE;
            }

        } else {
            // No value.
            *((void**)output) = 0;
            return sizeof(int);
        }

    } else if(specialClass != E_SPECIALCLASS_NONE) {

        // Should never be the case, as serialize(..) wasn't allowed to pass it.
        return INVALID_MSG;

    } else if(this == m_declaringDomain->StringClass()) {

        if(bufSize < (int)sizeof(CString*)) {
            return MSG_TOO_LARGE;
        }

        so_char16* strContents = *((so_char16**)buf);
        if(strContents) {
            Auto<const CString> str (CString::FromUtf16(strContents));
            delete [] strContents;
            *((void**)output) = m_declaringDomain->CreateString(str);
        } else {
            *((void**)output) = nullptr;
        }

        return sizeof(CString*);

    } else if(this->PrimitiveType() == E_PRIMTYPE_OBJECT) {

        if(this->IsValueType()) {

            // Iterates over the instance fields of this valuetype object.
            int finalSize = 0;
            int srcFieldOffset = 0;
            for(int i = 0; i < m_instanceFields->Count(); i++) {
                const CField* field = m_instanceFields->Array()[i];
                SKIZO_REQ_PTR(field->Type.ResolvedClass);
                const CClass* fieldClass = field->Type.ResolvedClass;
                SKIZO_REQ_POS(fieldClass->GCInfo().SizeForUse);

                const int bytesWritten = fieldClass->DeserializeForRemoting(buf + finalSize,
                                                                            bufSize - finalSize,
                                                                           (char*)output + srcFieldOffset);
                if(bytesWritten < 0) {
                    return bytesWritten;
                }

                // As per documentation: // sizeof(void*) for references classes; equals to m_contentSize for valuetypes.
                srcFieldOffset += fieldClass->GCInfo().SizeForUse;
                finalSize += bytesWritten;
            }

            return finalSize;

        } else { // !m_isValueType

            if(bufSize < (int)sizeof(void*)) {
                return MSG_TOO_LARGE;
            }
            void* soObj = *((void**)buf);

            // Before, we allowed only null non-valuetypes (in serialize(..))
            if(soObj) {
                return INVALID_MSG;
            } else {
                *((void**)output) = nullptr;
                return sizeof(void*);
            }

        }

    } else {
        // Primitive types.

        SKIZO_REQ_NOT_EQUALS(this->PrimitiveType(), E_PRIMTYPE_VOID);

        const size_t sizeForUse = m_gcInfo.SizeForUse;
        SKIZO_REQ_POS(sizeForUse);
        if(bufSize < (int)sizeForUse) {
            return MSG_TOO_LARGE;
        }

        // Primitive types are emitted directly.
        memcpy(output, buf, sizeForUse);
        return sizeForUse;
    }
}

void SKIZO_API _soX_unpack(void** args, void* _daMsg, void* _pMethod)
{
    CDomainMessage* msg = (CDomainMessage*)_daMsg;
    const CMethod* pMethod = (CMethod*)_pMethod;
    const CDomain* pDomain = CDomain::ForCurrentThread();
    const CSignature& sig = pMethod->Signature();
    int newLength = 0;
    const int paramCount = sig.Params->Count();

    for(int i = 0; i < paramCount; i++) {
        const CParam* param = sig.Params->Array()[i];

        SKIZO_REQ_PTR(param->Type.ResolvedClass);
        const CClass* pParamClass = param->Type.ResolvedClass;

        const int bytesRead = pParamClass->DeserializeForRemoting(msg->Buffer + newLength, SKIZO_DOMAINMESSAGE_SIZE - newLength, args[i]);
        if(bytesRead < 0) {

            // Unref all possible string parameters we've ref'd so far (see CClass::serialize for more info).
            for(int j = i; j < paramCount; j++) {
                if(args[j] && (sig.Params->Array()[j]->Type.ResolvedClass == pDomain->StringClass())) {
                    so_string_of(args[j])->Unref();
                }
            }

            // Instead of aborting a remote domain, we pass the error message to the domain message object,
            // the user code will check the error message after the remote call is complete and will abort
            // appropriately in the context of itself.
            msg->ErrorMessage = MSG_FROM_RETURN(bytesRead);
            return;

        }

        newLength += bytesRead;
    }

    SKIZO_REQ_EQUALS(newLength, msg->BufferLength);
}

void SKIZO_API _soX_msgsnd_sync(void* _hDomain, void* soObjName, void* _pMethod, void** args, void* blockingRet)
{
    const CMethod* pMethod = (const CMethod*)_pMethod;
    CDomain* pDomain = CDomain::ForCurrentThread();
    SKIZO_REQ_PTR(soObjName);
    CDomainHandle* hDomain = ((SDomainHandleHeader*)_hDomain)->wrapped;

    Auto<CDomainMessage> msg (new CDomainMessage());
    coreStringToFlatString(&msg->ObjectName[0], SKIZO_OBJECTNAME_SIZE, so_string_of(soObjName), "Object name too large.");
    stringSliceToFlatString(&msg->MethodName[0], SKIZO_METHODNAME_SIZE, pMethod->Name(), "Method name too large.");
    msg->ResultWaitObject.SetVal(pDomain->ResultWaitObject());

    SSerializationContext context (hDomain);

    const CSignature& sig = pMethod->Signature();
    int newLength = 0;
    for(int i = 0; i < sig.Params->Count(); i++) {
        const CParam* param = sig.Params->Array()[i];

        SKIZO_REQ_PTR(param->Type.ResolvedClass);
        const CClass* paramClass = param->Type.ResolvedClass;

        const int bytesWritten = paramClass->SerializeForRemoting(args[i],
                                                                  msg->Buffer + newLength,
                                                                  SKIZO_DOMAINMESSAGE_SIZE - newLength,
                                                                 &context);
        if(bytesWritten < 0) {

            // Unref all possible string parameters we've ref'd so far (see CClass::serialize for more info).
            for(int j = 0; j <= i; j++) {
                if(args[j] && (sig.Params->Array()[j]->Type.ResolvedClass == pDomain->StringClass())) {
                    so_string_of(args[j])->Unref();
                }
            }

            CDomain::Abort(MSG_FROM_RETURN(bytesWritten));
            return;

        }

        newLength += bytesWritten;
    }

    msg->BufferLength = newLength;

    // Uncomment for testing/debugging.
    /*for(int i = 0; i < newLength; i++) {
        printf("%d ", msg->Buffer[i]);
        if((i + 1) % 4 == 0)
            printf("  ");
    }*/

    hDomain->SendMessageSync(msg, blockingRet, REMOTECALL_TIMEOUT);

    // Extract the value.
    if(!sig.ReturnType.IsVoid()) {
        SKIZO_REQ_PTR(sig.ReturnType.ResolvedClass);
        SKIZO_REQ_PTR(blockingRet);

        const int bytesRead = sig.ReturnType.ResolvedClass->DeserializeForRemoting(msg->Buffer,
                                                                                   SKIZO_DOMAINMESSAGE_SIZE,
                                                                                   blockingRet);
        if(bytesRead < 0) {
            CDomain::Abort(MSG_FROM_RETURN(bytesRead));
        }
    }
}

// ********************
//   Exports/imports.
// ********************

// Called in the target/service domain.
void CDomain::ExportObject(const CString* name, void* soObj)
{
    void* prevObj = nullptr;

    if(soObj) {
        const CClass* pClass = so_class_of(soObj);
        if(pClass->SpecialClass() == E_SPECIALCLASS_FOREIGN) {
            CDomain::Abort("Attempt to export a foreign object.");
        }
    }

    // A flag used to abort outside of SKIZO_LOCK_AB just to be safer.
    bool noObjectExported = false;
    SKIZO_LOCK_AB(m_memMngr.ExportedObjsMutex) {
        if(soObj) { // actually exports the object
            if(m_memMngr.ExportedObjs->TryGet(name, &prevObj)) {
                // Unregisters whatever was before under this name, if any.
                m_memMngr.RemoveGCRoot(prevObj);
            }

            if(prevObj != soObj) { // a guard against doing it twice
                m_memMngr.AddGCRoot(soObj);
                m_memMngr.ExportedObjs->Set(name, soObj);
            }
        } else { // unregisters it if null is passed
            if(m_memMngr.ExportedObjs->TryGet(name, &prevObj)) {
                m_memMngr.RemoveGCRoot(prevObj);
                m_memMngr.ExportedObjs->Remove(name);
            } else {
                noObjectExported = true;
            }
        }
    } SKIZO_END_LOCK_AB(m_memMngr.ExportedObjsMutex);

    if(noObjectExported) {
        CDomain::Abort("No object was exported under this name.");
    }
}

bool CClass::MatchesForRemoting(const CClass* pForeignClass) const
{
    // Module-less classes are dangerous.
    if(!this->m_source.Module || !pForeignClass->m_source.Module) {
        return false;
    }

    // Verifies classes stem from the same module.
    if(!this->m_source.Module->Matches(pForeignClass->m_source.Module)) {
        return false;
    }

    // Verifies vtable indices and types match.
    if(this->m_instanceMethods->Count() != pForeignClass->m_instanceMethods->Count()) {
        return false;
    }
    for(int i = 0; i < pForeignClass->m_instanceMethods->Count(); i++) {
        CMethod* meth1 = pForeignClass->m_instanceMethods->Array()[i];
        CMethod* meth2 = this->m_instanceMethods->Array()[i];

        if(!meth1->Name().Equals(meth2->Name())) {
            return false;
        }
        if(!meth1->Signature().Equals(&meth2->Signature())) {
            return false;
        }
    }

    return true;
}

// Called in the user domain.
// This method checks if the object exists at all and what type it is of, then it constructs a foreign proxy
// for the target type.
void* CDomainHandle::ImportObject(void* soHandle, void* soName)
{
    SKIZO_NULL_CHECK(soName);
    const CString* name = so_string_of(soName);

    CDomain* localDomain = CDomain::ForCurrentThread();

    // NOTE As long as "Auto" holds a reference to the target domain, it's not deleted, so we're safe
    // touching it here, from the client domain.
    Auto<CDomain> foreignDomain (getDomain());
    if(!foreignDomain) {
        return nullptr; // Target domain terminated or is not available.
    }

    if(localDomain == foreignDomain) {
        return nullptr; // can't import from itself
    }

    // To be 100% sure.
    if(localDomain->RuntimeVersion() != foreignDomain->RuntimeVersion()) {
        return nullptr;
    }

    SMemoryManager& memMgr = foreignDomain->MemoryManager();
    CClass* pForeignClass = nullptr; // Lo and behold! A pointer from a parallel world!

    SKIZO_LOCK_AB(memMgr.ExportedObjsMutex) {
        // NOTE We're inside the mutex right now (CDomain::ExportObject(..) depends on it, too), meaning
        // the target domain can't unregister objects in parallel thereby removing them from the root set
        // and possibly garbage collecting them. So we're safe here accessing foreign pointers directly.
        void* soObj = nullptr; // Lo and behold! A pointer from a parallel world!

        if(memMgr.ExportedObjs->TryGet(name, &soObj)) {
            pForeignClass = so_class_of(soObj);
        }
    } SKIZO_END_LOCK_AB(memMgr.ExportedObjsMutex);

    // No object under this name was found.
    if(!pForeignClass) {
        return nullptr;
    }

    // Foreign class metadata of the imported object have been extracted. We've left the mutex region
    // meaning the target domain is now free to garbage collect the imported object -- we don't care as
    // its class metadata depend on the lifetime of the whole domain (and it's guaranteed to exist here until
    // this function ends).

    // Extracts the nice name of the foreign class. We use this because flat names can be different across
    // domains while nice names are always same provided domains reference same modules.
    const CString* niceName = pForeignClass->NiceName();

    // Let's see if the local domain actually has such type.
    const CClass* localClass = localDomain->ClassByNiceName(niceName);
    // No such class was found.
    if(!localClass) {
        return nullptr;
    }

    // Verifies the types match.
    if(!localClass->MatchesForRemoting(pForeignClass)) {
        return nullptr;
    }

    // Now, we need to find the foreign proxy class for the found class.
    CClass* proxyClass;
    if(!localDomain->ForeignProxies()->TryGet(localDomain->NewSlice(niceName), &proxyClass)) {
        // No proxy class was generated in the local domain (probably "force T*" is required?)
        return nullptr;
    }
    proxyClass->Unref();

    // Actual allocation.
    SKIZO_REQ_PTR(proxyClass->VirtualTable());
    SForeignProxyHeader* objptr = (SForeignProxyHeader*)_soX_gc_alloc(&localDomain->MemoryManager(),
                                                                       proxyClass->GCInfo().ContentSize,
                                                                       proxyClass->VirtualTable());
    objptr->hDomain = (SDomainHandleHeader*)soHandle;
    objptr->name = (SStringHeader*)soName;

    return objptr;
}

// *********************
//   _soX_findmethod2
// *********************

void* SKIZO_API _soX_findmethod2(void* objptr, void* _msg)
{
    // objptr is guaranted to be non-null in the usual emitted function prolog of a server stub.
    SKIZO_REQ_PTR(objptr);

    const CClass* pClass = so_class_of(objptr);
    const CDomainMessage* msg = (CDomainMessage*)_msg;

    if(msg->ErrorMessage) {
        return nullptr;
    }

    const CArrayList<CMethod*>* instanceMethods = pClass->InstanceMethods();
    for(int i = 0; i < instanceMethods->Count(); i++) {
        const CMethod* pMethod = instanceMethods->Array()[i];

        if(pMethod->Name().Equals(&msg->MethodName[0])) {
            return so_virtmeth_of(objptr, pMethod->VTableIndex());
        }
    }

    // By spec, we don't abort the current domain just because another domain requested a bad method.
    return nullptr;
}

// *******************
//   Message queues.
// *******************

struct DomainMessageQueuePrivate
{
    Auto<CMutex> m_queueMutex;
    Auto<CWaitObject> m_newMessageWaitObject;
    Auto<CQueue<CDomainMessage*> > m_backingQueue;

    DomainMessageQueuePrivate()
        : m_queueMutex(new CMutex()),
          m_newMessageWaitObject(new CWaitObject()), // automatic/non-signaled by default
          m_backingQueue(new CQueue<CDomainMessage*>())
    {
    }
};

SDomainMessageQueue::SDomainMessageQueue()
    : p(new DomainMessageQueuePrivate())
{
}

SDomainMessageQueue::~SDomainMessageQueue()
{
    delete p;
}

void SDomainMessageQueue::Enqueue(CDomainMessage* msg)
{
    SKIZO_LOCK_AB(p->m_queueMutex)
    {
        p->m_backingQueue->Enqueue(msg);
    }
    SKIZO_END_LOCK_AB(p->m_queueMutex);

    p->m_newMessageWaitObject->Pulse();
}

CDomainMessage* SDomainMessageQueue::tryRetrieveMessage()
{
    Auto<CDomainMessage> msg;

    SKIZO_LOCK_AB(p->m_queueMutex)
    {
        if(!p->m_backingQueue->IsEmpty()) {
            msg.SetPtr(p->m_backingQueue->Dequeue());
        }
    }
    SKIZO_END_LOCK_AB(p->m_queueMutex);

    if(msg) {
        msg->Ref();
    }
    return msg;
}

CDomainMessage* SDomainMessageQueue::Poll(int timeout)
{
    // Maybe already something in the queue.
    CDomainMessage* msg = tryRetrieveMessage();
    if(msg) {
        return msg;
    }

    // If not -- wait for a new message.
    const bool b = CThread::Wait(p->m_newMessageWaitObject, timeout);
    if(!b) {
        return nullptr;
    }

    return tryRetrieveMessage();
}

typedef _so_bool (SKIZO_API * _FPredicate)(void*);
typedef void (SKIZO_API * _FServerStubFunc)(void*, void*, void*);

static _so_bool SKIZO_API nullPred(void* self)
{
    return true;
}

void* CMethod::GetServerStubImpl() const
{
    if(!m_serverStubImpl) {
        CClass* pClass = this->DeclaringClass();
        CDomain* pDomain = pClass->DeclaringDomain();

        STextBuilder textBuilder;
        textBuilder.Emit("_soX_server_%s_%s", &pClass->FlatName(), &this->Name());

        m_serverStubImpl = pDomain->GetSymbolThreadSafe(textBuilder.Chars());
    }

    return m_serverStubImpl;
}

void CDomain::Listen(void* soStopPred)
{
    _FPredicate predFunc = soStopPred? (_FPredicate)so_invokemethod_of(soStopPred): nullPred;

    while(predFunc(soStopPred)) {
        Auto<CDomainMessage> msg (m_msgQueue.Poll(MESSAGEQUEUE_TIMEOUT));
        if(msg) {
            const bool isBlocking = msg->ResultWaitObject? true: false;

            // Extracts the target object by its name.
            // WARNING `targetObj` may get garbage-collected while inside serverStubImpl.
            void* targetObj = nullptr;
            {
                Auto<const CString> objectName (CString::FromUtf16(&msg->ObjectName[0]));
                SKIZO_LOCK_AB(m_memMngr.ExportedObjsMutex) {
                    m_memMngr.ExportedObjs->TryGet(objectName, &targetObj);
                } SKIZO_END_LOCK_AB(m_memMngr.ExportedObjsMutex);
            }

            if(targetObj) {
                const CClass* pClass = so_class_of(targetObj);
                const CMethod* targetMethod;
                {
                    Auto<const CString> methodName (CString::FromUtf16(&msg->MethodName[0]));
                    targetMethod = pClass->MyMethod(SStringSlice(methodName), false, E_METHODKIND_NORMAL);
                }

                if(targetMethod) {

                    void* serverStubImpl = targetMethod->GetServerStubImpl();
                    if(serverStubImpl) {

                        // The server stub places stuff into this buffer as it is (without serialization).
                        char buf[SKIZO_DOMAINMESSAGE_SIZE];

                        try {
                            ((_FServerStubFunc)serverStubImpl)(targetObj, msg, buf); // <== ACTUAL CALL TO THE SERVER STUB
                            // Aborts that originate here are caught a bit below (see).

                            // Finds the type of the method.
                            SKIZO_REQ_PTR(targetMethod->Signature().ReturnType.ResolvedClass);
                            const CClass* retType = targetMethod->Signature().ReturnType.ResolvedClass;

                            SSerializationContext context (nullptr); // TODO

                            // Serializes the value from the temporary buffer to the message buffer.
                            if(retType->PrimitiveType() != E_PRIMTYPE_VOID) {
                                // Again and again, for valuetypes, the whole buffer is passed, for reference types, the direct pointer contained
                                // in the buffer.
                            #pragma GCC diagnostic push
                            #pragma GCC diagnostic ignored "-Wstrict-aliasing"
                                retType->SerializeForRemoting(retType->IsValueType()? buf: *(reinterpret_cast<void**>(buf)),
                                                              msg->Buffer,
                                                              SKIZO_DOMAINMESSAGE_SIZE,
                                                             &context);
                            #pragma GCC diagnostic pop
                            }

                        } catch (const SoDomainAbortException& e) {
                            // NOTE aborts are redirected to the caller.
                            // The current target domain is responsible for deleting the error message
                            // (see g_lastError in SkizoDomain.cpp), we can't safely share the message
                            // with the caller, so we copy it.

                            msg->ErrorMessage = CString::CloneUtf8(e.Message);
                            msg->FreeErrorMessage = true;
                        }

                    } else {
                        msg->ErrorMessage = "Cross-domain method implementation not found ('force T*' required?)";
                    }
                } else {
                    msg->ErrorMessage = "Method not found (versioning problem?)";
                }

                // *******************************
            } else {
                msg->ErrorMessage = "Foreign object not found.";
            }

            if(isBlocking) {
                msg->ResultWaitObject->Pulse();
            } else {
                // TODO add the result to the queue of the original domain
                SKIZO_REQ_NEVER
            }
        }
    }
}

} }

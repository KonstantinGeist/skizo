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

#ifndef REMOTING_H_INCLUDED
#define REMOTING_H_INCLUDED

#include "Mutex.h"
#include "SharedHeaders.h"
#include "StringSlice.h"
#include "WaitObject.h"

namespace skizo { namespace core {
    class CThread;
} }

namespace skizo { namespace script {

/**
 * Domains communicate via domain handles.
 * Domain::runPath(..)/Domain::runString(..) in Skizo code return a domain handle.
 *
 * The handle is stored in two places: the CRemoteDomainThread instance of the target domain (see icalls/Domain.cpp)
 * and a wrapper object in Skizo. So, the handle is destroyed when the domain finishes execution and
 * no other Skizo objects refer to it.
 */
class CDomainHandle: public skizo::core::CAtomicObject
{
    friend class CRemoteDomainThread;

public:
    class CDomain* GetDomain() const;

    skizo::core::CThread* GetThread() const;

	/**
     * Waits for the domain to finish its job and terminate (stop naturally by exiting from Program::main(..) or aborting
     * on error).
     */
    bool Wait(int timeout);

	/**
     * Checks if the domain is alive.
	 * @warning For debugging only, the information is very unreliable.
     */
    bool IsAlive() const;

	/**
     * Imports a new object.
	 * Used by ICalls, hence 'soDomainHandle' (which wraps this domain handle on the C++ side) and 'soName'.
     */
    void* ImportObject(void* soDomainHandle, void* soName);

    /**
     * Adds the message to the domain queue of the referenced domain.
     * The message blocks the thread until the function return the result (if any) in a memory area referenced by blockingRet.
     * Thread-safe.
     */
    void SendMessageSync(class CDomainMessage* msg, void* blockingRet, int timeout);

protected: // internal
    CDomainHandle();

    /**
     * Updates the domain value in a thread-safe manner. It can be something or nothing -- depending on whether
     * the domain exists or not anymore.
     */
    void UpdateDomainValue(class CDomain* domain);

    void Pulse();
    
private:
    /**
     * Synchronizes access to the handle. The domain reference is weak and is set to zero when the domain is destroyed.
     * In order to make sure the domain handle never tries to access a dangling pointer, we force all access to the domain
     * field through this mutex, by Ref()'ing the domain in the guarded code to make sure an immediately following code
     * never deals with a released domain.
     */
    skizo::core::Auto<skizo::core::CMutex> Mutex;

    /**
     * All access to the domain handle pass through this wait object, to make sure the target domain is ready at the point
	 * we try to do something to it.
     * When the remote domain is created, it calls WaitObject::Pulse()
     */
    skizo::core::Auto<skizo::core::CWaitObject> WaitObject;

    /**
     * @note When the domain finishes execution, this reference is set to zero.
     * API should always check for Domain=null (via the mutex).
     */
    class CDomain* Domain;

    bool waitForDomain(bool doThrow = true) const;
};

/**
 * @warning Currently, the size of a message sent by domains to each other is limited to SKIZO_DOMAINMESSAGE_SIZE bytes.
 */
#define SKIZO_DOMAINMESSAGE_SIZE 1024

/**
 * A binary message is sent by domains to each other to implement cross-domain method calls.
 * Used by _soX_msgsnd_sync(..) and _soX_msgsnd_async(..)
 * Implemented in: Remoting.cpp
 */
class CDomainMessage: public skizo::core::CAtomicObject
{
public:
    /**
     * Object name.
     */
    skizo::core::Auto<const skizo::core::CString> ObjectName;

    /**
     * The name of the method. We reuse string slices as strings they refer to are sharable across
     * domains without having to allocate new strings or marshal them through the buffer. However,
     * string slices don't manage lifetime of strings by themselves, so the string this field references
     * is ref'd upon creation and unref'd in the dtor manually.
     */
    SStringSlice MethodName;

    /**
     * In a blocking cross-domain call, the local domain waits on this object.
     * When the target domain finishes executing a remote call, it pulses this object.
     * The object is borrowed from Domain::m_resultWaitObject, i.e. all messages
     * in a domain share the same wait object.
     * @note The object is independent, i.e. if the original domain is destroyed, other domains
     * pulsing it will not result in a crash.
     */
    skizo::core::Auto<skizo::core::CWaitObject> ResultWaitObject;

    /**
     * Null if no error; an error message otherwise.
     */
    const char* ErrorMessage;
    
    /**
     * Some error messages can be passed from SoDomainAbortException
     * where they are constructed with CString::ToUtf8(..) and should
     * be manually destroyed.
     */
    bool FreeErrorMessage;    

    /**
     * The actual length of the buffer.
     */
    int BufferLength;

    /**
     * The argument buffer.
     */
    char Buffer[SKIZO_DOMAINMESSAGE_SIZE];

    CDomainMessage()
        : ErrorMessage(nullptr),
          FreeErrorMessage(false),
          BufferLength(0)
    {
    }

    virtual ~CDomainMessage();
};

/**
 * A domain's message queue which is polled in Domain::listen for incoming cross-domain method calls.
 */
class SDomainMessageQueue
{
    friend class CDomain;

public:
    SDomainMessageQueue();
    virtual ~SDomainMessageQueue();

    void Enqueue(CDomainMessage* msg);

    /**
     * The returned message is Ref()'d and removed from the queue.
     * Call this method in a loop.
     * @warning Returns null if the queue is empty.
     */
    CDomainMessage* Poll(int timeout);

private:
    struct SkizoDomainMessageQueue* p;
};

} }

#endif // REMOTING_H_INCLUDED

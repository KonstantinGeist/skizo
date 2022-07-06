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

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#ifdef SKIZO_X
    #include <pthread.h>
#endif

#ifdef SKIZO_SINGLE_THREADED
    // We need to guarantee atomicity of threads even when compiled in 
    // single-threaded mode.
    #include "AtomicObject.h"
#else
    #include "Object.h"
#endif

#include "ArrayList.h"
#include "Variant.h"

namespace skizo { namespace core {
class CWaitObject;

/**
 * Exposes the native handle of a thread.
 */
struct SThreadHandle
{
#ifdef SKIZO_WIN
    HANDLE Value;
#else
    pthread_t Value;
#endif
};

/**
 * Specifies the execution state of a CThread.
 *
 * @warning Most states are for debugging only; ABORT_REQUESTED is used to signal
 * that a thread should be aborted (in a cooperative way).
 */
enum EThreadState
{
    /**
     * The thread is unstarted and ready to go.
     */
    E_THREADSTATE_UNSTARTED = 0,

    /**
     * The thread is already running.
     */
    E_THREADSTATE_RUNNING = 1,

    /**
     * The thread is being aborted: it's still running,
     * but terminating (some cleanup).
     */
    E_THREADSTATE_ABORT_REQUESTED = 2,

    /**
     * The thread was stopped.
     */
    E_THREADSTATE_STOPPED = 3
};

/**
 * A thread of execution in a program.
 *
 * @warning To use threads under MinGW, each library must be compiled with
 * -mthreads flag set! Or expect random crashes. Use -pthreads for Unix.
 *
 * Declare a class to be a subclass of CThread and override CThread::OnStart.
 */
#ifdef SKIZO_SINGLE_THREADED
class CThread: public CAtomicObject
#else
class CThread: public CObject
#endif
{
    friend struct SThreadLocal;

#ifdef SKIZO_X
    friend void* PosixToSkizo(void* arg);
#endif
#ifdef SKIZO_WIN
    friend __stdcall DWORD WinToSkizo(LPVOID arg);
#endif

public:
    virtual ~CThread();

    /**
     * See skizo::core::EThreadState.
     */
    EThreadState State() const;

    /**
     * @throw EC_INVALID_STATE if the thread is already running
     */
    void SetAffinity(int procId);

    /**
     * Expected priorities are given in the range from 0 (minimal priority) to
     * 100 (maximum priority).
     *
     * @note An underlying platform may have a much narrower number of priorities
     * available (10 levels, for example) which means that similar priorities
     * (90 vs. 100) may be mapped to the same priority by the OS.
     * @throw EC_ILLEGAL_ARGUMENT if priority is less than 0 or greater than 100.
     * @throw EC_INVALID_STATE if the thread is already running
     */
    void SetPriority(int priority);

    /**
     * Sets the name of the thread. For debugging.
     */
    void SetName(const char* name);

    /**
     * Gets the name of the thread. For debugging.
     */
    const char* Name() const;

    /**
     * Causes this thread to begin execution and to change its state to
     * E_THREADSTATE_RUNNING.
     *
     * @throws EC_INVALID_STATE the thread was already started
     * @throws EC_PLATFORM_DEPENDENT run out of thread limit
     */
    void Start();

    /**
     * Gets the currently running thread.
     *
     * @warning If the current thread is non-skizo (it wasn't created via
     * CThread::Start() or it isn't the main thread), this API has undefined behavior.
     */
    static CThread* Current();

    /**
     * Waits for a thread to exit.
     *
     * @param timeout The time-out interval, in milliseconds. The function waits
     * until the object is signaled or the interval elapses. If the parameter is
     * 0, the function will return only after the thread stops execution.
     */
    static void Join(const CThread* thread, int timeout = 0);

    // NOTE Implemented in native/WaitObject.unix.cpp for Linux.
    /**
     * Blocks the current thread until the wait object receives a signal (gets
     * CWaitObject::Pulse() called) using a 32-bit signed integer to specify the
     * time interval.
     *
     * @param timeout The time-out interval, in milliseconds. The function waits
     * until the object is signaled or the interval elapses. If the parameter is
     * 0, the function will return only when the object is signaled.
     * @return Returns true if the thread was woken up; returns false if the
     * timeout has expired.
     * @throw EC_ILLEGAL_ARGUMENT if ms is less than 1.
     */
    static bool Wait(const CWaitObject* waitObject, int timeout = 0);

    /**
     * Aborts the thread by setting the thread state to E_THREADSTATE_ABORT_REQUESTED.
     * User code should check for this flag repeatedly in a loop:
     *
     * \code{.cpp} while(this->State() != E_THREADSTATE_ABORT_REQUESTED) { } \endcode
     * and then simply return from from OnStart if it was implemented via subclassing,
     * allowing the thread to die on its own.
     */
    void Abort();

    /**
     * The internal handle used by the underlying system. For debugging only.
     */
    SThreadHandle Handle() const;

    /**
     * An "owned" thread was created from under skizo wrappers. A "non-owned"
     * thread was created by foreign code and later a reference to it was
     * retrieved via methods such as ::GetThreads(). A non-owned thread cannot
     * be started, aborted etc. because it belongs to foreign code and we have
     * a reference to it only for debugging/profiling purposes.
     */
    bool IsOwned() const;

    /**
     * Causes the currently executing thread to sleep (temporarily cease execution)
     * for the specified number of milliseconds.
     */
    static void Sleep(int ms);

    /**
     * Returns true if this thread is main (automatically created by the
     * operating system).
     */
    bool IsMain() const;

    /**
     * Gets the processor time of this thread (total time spent on this thread)
     * in milliseconds.
     * For debugging/profiling purposes only!
     */
    so_long GetProcessorTime() const;

    /**
     * Returns a list of all threads available to the implementation.
     * Depending on the implementation, it can be a list of all known threads in
     * this process (including created by non-skizo code) or only those threads
     * which were created explicitly through skizo wrappers.
     */
    static skizo::collections::CArrayList<CThread*>* GetThreads();

    // **************
    //   Dangerous.
    // **************

    /**
     * Disassociates the main thread from its skizo wrapper. Unsafe; to be used
     * for memory diagnostics to ensure absolutely all skizo objects are freed
     * before calling certain functions. Ignored if the current thread isn't main.
     */
    static void DisassociateMainThreadUnsafe();

protected:
    /**
     * Initializes a new instance of the CThread class. A thread does not begin
     * executing when it is created. To schedule the thread for execution, call
     * the Start method.
     *
     * @note The framework guarantees that the object is alive as long as the
     * underlying OS thread is alive. It's not necessary to hold a reference to
     * a CThread to keep it alive.
     */
    CThread();

    /**
     * Override this method to execute code on the thread.
     */
    virtual void OnStart() { }

protected: // internal
    // Used by SThreadLocal::Set
    void SetThreadLocal(int id, const SVariant& v);
    bool TryGetThreadLocal(int id, SVariant* v) const;

private:
    struct ThreadPrivate* p;
};

/**
* Defines a thread-local variable.
*
* @note Required on Windows because many MinGW versions silently ignore thread
* local modifiers which also leads to unsafe exception handling when several
* threads are used (even with -mthreads specified).
* @param type the type of the variable
* @param name the name of the variable
*/
struct SThreadLocal
{
public:
    SThreadLocal();

    /**
     * @brief Gets the value stored in a thread-local variable.
     * @param name the name of the variable
     */
    SVariant Get() const;

    /**
     * @brief Sets the value stored in a thread-local variable.
     * @param name the name of the variable
     * @param value the value to store in the thread-local variable
     */
    void Set(const SVariant& v);

    // ***************************
    //   Type-dependent methods.
    // ***************************

    void SetNothing();

    void SetInt(int i);
    void SetBool(int b);
    void SetBlob(void* ptr);

    int IntValue() const;
    int BoolValue() const;
    void* BlobValue() const;

    // ***************************

    template <typename T>
    T ObjectValue() const
    {
        return static_cast<T>(objectValue());
    }

    template <typename T>
    void SetObject(T v)
    {
        setObject(static_cast<const CObject*>(v));
    }

private:
    int m_id;
    CObject* objectValue() const;
    void setObject(const CObject* v);
};

// WARNING Do not call directly.
void __InitThread();
void __DeinitThread();
void __InitThreadNative();
void __DeinitThreadNative();

} }

#endif // THREAD_H_INCLUDED

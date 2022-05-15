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

#include "../../Thread.h"
#include "../../ArrayList.h"
#include "../../Contract.h"
#include "../../CoreUtils.h"
#include "../../Exception.h"
#include "../../HashMap.h"
#include "../../Log.h"
#include "../../Mutex.h"
#include "../../String.h"
#include "../../WaitObject.h"

#include <assert.h>
#include <errno.h>

// *************************************************************************
// See the comments for the Win32 implementation for additional information.
// *************************************************************************

namespace skizo { namespace core {
using namespace skizo::collections;

static __thread CThread* g_currentThread = nullptr;

static CThread* g_MainThread = nullptr;

static pthread_mutex_t g_cs; // To secure access to the thread list (CoreCLR
                             // does the same).

// The list of known skizo threads. Required for Thread::GetThreads()
// void* is used as a weak reference with no ref counting.
static CArrayList<void*>* g_knownThreadList = nullptr;

struct ThreadPrivate
{
    pthread_t m_handle;

    // NOTE Mostly for debugging + to signal that a thread should be aborted.
    std::atomic<EThreadState> m_state;

    int m_procId, m_priority;

    char* m_name;

    // The main thread's handle should not be disposed as it's managed by the OS.
    bool m_isMain;

    Auto<CHashMap<int, SVariant> > m_tlsData;

    ThreadPrivate()
        : m_handle(0),
          m_state(E_THREADSTATE_UNSTARTED), m_procId(0), m_priority(50),
          m_name(nullptr),
          m_isMain(false),
          m_tlsData(new CHashMap<int, SVariant>())
    {
    }

    ~ThreadPrivate()
    {
        if(m_name) {
            delete [] m_name;
            m_name = nullptr;
        }
    }

    void freeHandle();
};

void __InitThreadNative()
{
    int r = pthread_mutex_init(&g_cs, 0);
    if(r != 0) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }

    SKIZO_REQ_EQUALS(g_knownThreadList, nullptr);
    g_knownThreadList = new CArrayList<void*>();

    // Forces registration of the main thread as a skizo thread.
    CThread::Current();
}

void __DeinitThreadNative()
{
    if(g_MainThread)
        g_MainThread->Unref();

    // Destroys the thread list.
    pthread_mutex_lock(&g_cs);
        CArrayList<void*>* threadList = g_knownThreadList;
        g_knownThreadList = nullptr;
        SKIZO_REQ_PTR(threadList);
        threadList->Unref();
    pthread_mutex_unlock(&g_cs);
}

CThread* CThread::Current()
{
    CThread* r = g_currentThread;
    if(!r) {
        // It's a native thread!
        // The skizo wrapper is "injected" in-place.
        r = new CThread();
        r->p->m_state = E_THREADSTATE_RUNNING;
        r->p->m_handle = pthread_self();

        // The first non-attached native thread is thought to be the main thread.
        if(!g_MainThread) {
            r->p->m_isMain = true;
            r->SetName("Main");
            g_MainThread = r;
        }

        g_currentThread = r;
    }

    return r;
}

void CThread::DisassociateMainThreadUnsafe()
{
    CThread* curThread = g_currentThread;

    if(curThread && curThread == g_MainThread) {
        g_MainThread = nullptr;
        curThread->Unref();
        g_currentThread = nullptr;
    }
}

CThread::CThread()
    : p (new ThreadPrivate())
{

}

CThread::~CThread()
{
    if(!p->m_isMain) {
        // Main thread's handle is automatically created and freed by the
        // operating system.
        p->freeHandle();
    }

    delete p;
}

EThreadState CThread::State() const
{
    return p->m_state;
}

bool CThread::IsMain() const
{
    return p->m_isMain;
}

void CThread::SetAffinity(int procId)
{
    if(p->m_state != E_THREADSTATE_UNSTARTED) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "The thread is already running.");
    }

    p->m_procId = procId;
}

void CThread::SetPriority(int priority)
{
    if(priority < 0 || priority > 100) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }
    if(p->m_state != E_THREADSTATE_UNSTARTED) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "The thread is already running.");
    }

    p->m_priority = priority;
}

void ThreadPrivate::freeHandle()
{
    if(m_handle != 0) {
        pthread_detach(m_handle);

        m_handle = 0;
    }
}

SThreadHandle CThread::Handle() const
{
    SThreadHandle r;
    r.Value = p->m_handle;
    return r;
}

bool CThread::IsOwned() const
{
    // Everything is owned in the current implementation
    return true;
}

void* PosixToSkizo(void* arg)
{
    CThread* thread = static_cast<CThread*>(arg);
    g_currentThread = (thread);

    // We set it to running only here because we must ensure
    // that all thread-dependent data has been set up.
    if(thread->p->m_state != E_THREADSTATE_ABORT_REQUESTED) { // already aborted?
        thread->p->m_state = E_THREADSTATE_RUNNING;
    }

    // Sets an additional reference on itself. Why? To prevent deallocating
    // the thread while it's still running.
    thread->Ref();

    // *********************************************
    // Adds the instance to the global thread list.
    pthread_mutex_lock(&g_cs);
        SKIZO_REQ_PTR(g_knownThreadList);
        g_knownThreadList->Add(static_cast<void*>(thread));
    pthread_mutex_unlock(&g_cs);
    // *********************************************

    try {

        // The final call into the user implementation of the thread.
        thread->OnStart();

    } catch (const SException& e) {

        // Exceptions need to be caught, or they will propagate to foreign
        // OS stacks. Also we need to unregister the thread instance from the
        // thread list below.
        printf("Uncaught thread exception: '%s'.", e.Message());

    } catch (const std::bad_alloc& e) {

        printf("Out of memory (in Thread::OnStart()).\n");

    }

    g_currentThread = nullptr;
    thread->p->m_state = E_THREADSTATE_STOPPED;

    // ***************************************************************************
    // Removes the instance from the global thread list.
    pthread_mutex_lock(&g_cs);
        if(g_knownThreadList) {
            g_knownThreadList->Remove(static_cast<void*>(thread));
        } else {
            fprintf(stderr, "Thread list destroyed before a thread instance was.");
        }
    pthread_mutex_unlock(&g_cs);
    // ***************************************************************************

    // Detaches the additional reference set here.
    thread->Unref();

    // Detaches the additional reference set in Start()
    thread->Unref();

    return 0;
}

void CThread::Start()
{
    if(p->m_state != E_THREADSTATE_UNSTARTED) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Thread was already started.");
    }

    // WARNING
    // It's important to ref the instance BEFORE it's scheduled as an OS thread:
    // due to race conditions, the thread may finish before we get to ref'ing
    // it, by that time it can be already unref'd on another thread in the
    // onStart sub and deleted.
    this->Ref();

    int r;
    r = pthread_create(&p->m_handle,
                        0,
                        &PosixToSkizo,  /* Wrapper. */
                        (void*)this);

    if(r) {
        this->Unref();

        if(r == EAGAIN) {
            SKIZO_THROW_WITH_MSG(EC_PLATFORM_DEPENDENT, "Run out of thread limit.");
        } else {
            SKIZO_THROW(EC_PLATFORM_DEPENDENT); // TODO add message:
                                             // "Thread creation failed with error code '%d'", r
        }
    }

    // ******************************************
    // Sets the thread affinity and the priority.
    // ******************************************

    // TODO Thread priorities ignored for Linux yet.
}

void CThread::Join(const CThread* thread, int timeout)
{
    if(timeout < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }
    if(thread == Current()) {
        SKIZO_THROW_WITH_MSG(EC_ILLEGAL_ARGUMENT, "Can't join itself.");
    }

    // The thread has already terminated...
    if(thread->State() == E_THREADSTATE_STOPPED) {
        return;
    }

    if(thread->State() == E_THREADSTATE_UNSTARTED) {
        /*
           At this point, even if we have called CThread::Start,
           the thread state still may be UNSTARTED because the thread's internal
           data aren't guaranteed to have been already set up.

           There may be one of two situations:
           1) The user forgot to call CThread::Start. In this case, raise an exception.
           2) The user called CThread::Start, but the thread isn't still ready
              to be used, it's still initializing its data. In this case, we must just wait.

           To distinguish these two cases, all we have to do is to check thread->m_handle. It's != 0
           in the second case.
        */

        /* The user forgot to call CThread::Start. */
        if(thread->p->m_handle == 0) {
            SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Can't join an unstarted thread.");
        }

        /*
           Although the thread is unstarted in the managed context, it's fully
           usable in the native context. That's why we may proceed without any
           errors (i.e. join a semantically unstarted thread).
         */
    }

    if(timeout != 0) {
        fprintf(stderr, "WARNING: Timeouts not supported under Linux!\n");
    }

    pthread_join(thread->p->m_handle, 0);
}

void CThread::Sleep(int ms)
{
    if(ms < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    struct timespec ts;
    ts.tv_sec = (long)ms / 1000;              // Seconds.
    ts.tv_nsec = ((long)ms % 1000) * 1000000; // Milliseconds.
    nanosleep(&ts, NULL);
}

void CThread::Abort()
{
    p->m_state = E_THREADSTATE_ABORT_REQUESTED;
}

void CThread::SetThreadLocal(int id, const SVariant& v)
{
    p->m_tlsData->Set(id, v);
}

bool CThread::TryGetThreadLocal(int id, SVariant* v) const
{
    return p->m_tlsData->TryGet(id, v);
}

so_long CThread::GetProcessorTime() const
{
    struct timespec ts;
    clockid_t cid;

    if(pthread_getcpuclockid(pthread_self(), &cid)) {
        return 0;
    }
    if(clock_gettime(cid, &ts) == -1) {
        return 0;
    }

    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

// NOTE The Linux implementation only lists the threads explicitly created
// from under skizo wrappers.
CArrayList<CThread*>* CThread::GetThreads()
{
    CArrayList<CThread*>* r =  new CArrayList<CThread*>();

    // Main thread is a special case, not listed inside g_knownThreadList.
    if(g_MainThread) {
        r->Add(g_MainThread);
    }

    pthread_mutex_lock(&g_cs);
        for(int i = 0; i < g_knownThreadList->Count(); i++) {
            r->Add(static_cast<CThread*>(g_knownThreadList->Array()[i]));
        }
    pthread_mutex_unlock(&g_cs);

    return r;
}

void CThread::SetName(const char* name)
{
    SKIZO_REQ_PTR(name);

    if(p->m_name)
        delete [] p->m_name;
    p->m_name = new char[strlen(name) + 1];
    strcpy(p->m_name, name);
}

const char* CThread::Name() const
{
    return p->m_name;
}

} }

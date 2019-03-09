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
#include "../../CoreUtils.h"
#include "../../Exception.h"
#include "../../HashMap.h"
#include "../../String.h"
#include "../../WaitObject.h"

#include <assert.h>
#include <TlHelp32.h>

namespace skizo { namespace core {
using namespace skizo::collections;

// Thread local implementation.
// Windows has a limitation of 1088 tls indices per process. We create another
// indirection which allows us to store 2^32 tls indices with just 1 Windows tls index
// (although access is slower).
static DWORD g_ActualIndex = 0;
static CThread* g_MainThread = nullptr;
static DWORD g_MainThreadId = 0;

// Guards the list of threads.
// Does not use CMutex here for less overhead.
// NOTE CoreCLR uses a critical section for adding threads to a thread list as well.
static CRITICAL_SECTION g_cs;

// Thread list is needed to keep a list of skizo-created threads.
// Thread::GetThreads() then compares ID's of known threads to captured threads
// (via WinAPI's CreateToolhelp32Snapshot(..)) to inject there previously created
// skizo wrappers.
// It's needed because a skizo wrapper may have a name attached (Windows threads
// do not have names, so we have to emulate it), so, when Thread::GetThreads()
// returns thread instances, we can retrieve the previously assigned names.
// The list has void* to avoid ref'ing the Thread instance which would prevent
// its deallocation.
// NOTE Threads are added here only when they actually get started, and removed
// when they actually stop executing.
static CArrayList<void*>* g_knownThreadList = nullptr;

struct ThreadPrivate
{
    HANDLE m_handle;
    bool m_isOwned;

    // NOTE Mostly for debugging + to signal that a thread should be aborted.
    std::atomic<EThreadState> m_state;
    int m_procId, m_priority;

    char* m_name;

    // The main thread's handle should not be disposed as it's managed by the OS.
    bool m_isMain;

    Auto<CHashMap<int, SVariant> > m_tlsData;

    ThreadPrivate()
        : m_handle(NULL),
          m_isOwned(true),
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

static void setCurrentThread(CThread* thread)
{
    TlsSetValue(g_ActualIndex, reinterpret_cast<void*>(thread));
}

static CThread* currentThread()
{
    return reinterpret_cast<CThread*>(TlsGetValue(g_ActualIndex));
}

void __InitThreadNative()
{
    g_ActualIndex = TlsAlloc();
    assert(g_ActualIndex != TLS_OUT_OF_INDEXES);

    // TODO never deinitialized?
    // For some safety we do not do it (destruction order not clear)
    // However if the engine is to be repeatedly created/destroyed in the same
    // process, it can become problematic.
    InitializeCriticalSection(&g_cs);

    SKIZO_REQ_EQUALS(g_knownThreadList, nullptr);
    g_knownThreadList = new CArrayList<void*>();

    // Makes this thread "main" (see implementation). Initialization should
    // be done on the main thread in the first place.
    CThread::Current();
}

void __DeinitThreadNative()
{
    // This produces AppVerifier error.
    // We don't really have to free it anyway, as it's always 1 per process.
    //TlsFree(ActualIndex);

    if(g_MainThread) {
        g_MainThread->Unref();
        g_MainThread = nullptr;
        g_MainThreadId = 0;
    }

    // Destroys the thread list.
    EnterCriticalSection(&g_cs);
        CArrayList<void*>* threadList = g_knownThreadList;
        g_knownThreadList = nullptr;
        SKIZO_REQ_PTR(threadList);
        threadList->Unref();
    LeaveCriticalSection(&g_cs);
}

CThread* CThread::Current()
{
    // TODO dangerous if Current() is suddenly called on a native method, how
    // to check for it?
    CThread* r = currentThread();
    if(!r) {
        // It's a non-attached thread!
        // The skizo wrapper is "injected" in-place.
        r = new CThread();
        r->p->m_state = E_THREADSTATE_RUNNING;
        r->p->m_handle = GetCurrentThread();

        // The first non-attached native thread is thought to be the main thread.
        if(!g_MainThread) {
            r->p->m_isMain = true;
            r->SetName("Main");
            g_MainThread = r;
            g_MainThreadId = GetCurrentThreadId();
        }

        setCurrentThread(r);
    }
    return r;
}

void CThread::DisassociateMainThreadUnsafe()
{
    CThread* curThread = currentThread();
    if(curThread && curThread == g_MainThread) {
        g_MainThread = nullptr;
        curThread->Unref();
        g_MainThread = nullptr;
        setCurrentThread(nullptr);
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
        // operating system. If it's a non-owned thread, the reference counted
        // CloseHandle() will simply close the handle.
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
    if(!p->m_isOwned) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Cannot set affinity of a non-owned thread.");
    }

    if(p->m_state != E_THREADSTATE_UNSTARTED) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "The thread is already running.");
    }

    p->m_procId = procId;
}

void CThread::SetPriority(int priority)
{
    if(!p->m_isOwned) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Cannot set priority of a non-owned thread.");
    }

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
    if(m_handle != NULL) {
        CloseHandle(m_handle); // Do not assert, who knows what if it will fail on shutdown?

        m_handle = NULL;
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
    return p->m_isOwned;
}

// FIX for O3+vectorization, on entrypoint, it's not 16-byte aligned, which can
// wreck havoc when using SSE down the stack.
SKIZO_FORCE_ALIGNMENT __stdcall DWORD WinToSkizo(LPVOID arg)
{
    // Required for ShellExecuteEx(..)
    //CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    CThread* thread = static_cast<CThread*>(arg);
    setCurrentThread(thread);

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
    EnterCriticalSection(&g_cs);
        if(g_knownThreadList) { // ?
            g_knownThreadList->Add(static_cast<void*>(thread));
        }
    LeaveCriticalSection(&g_cs);
    // *********************************************

    try {

        // The final call into the user implementation of the thread.
        thread->OnStart();

    } catch (const SException& e) {

        // Exceptions need to be caught, or they will propagate to foreign
        // Windows OS stacks. Also we need to unregister the thread instance
        // from the thread list below.
        printf("Uncaught thread exception: '%s'.", e.Message());

    } catch (const std::bad_alloc& e) {

        printf("Out of memory (in Thread::OnStart()).\n");

    }

    setCurrentThread(nullptr);
    thread->p->m_state = E_THREADSTATE_STOPPED;

    // ***************************************************************************
    // Removes the instance from the global thread list.
    EnterCriticalSection(&g_cs);
        if(g_knownThreadList) {
            g_knownThreadList->Remove(static_cast<void*>(thread));
        } else {
            fprintf(stderr, "Thread list destroyed before a thread instance was.");
        }
    LeaveCriticalSection(&g_cs);
    // ***************************************************************************

    // Detaches the additional reference set here.
    thread->Unref();

    // Detaches the additional reference set in Start()
    thread->Unref();

    // Paired with CoInitializeEx(..) above.
    //CoUninitialize();

    return 0;
}

void CThread::Start()
{
    if(!p->m_isOwned) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Cannot start a non-owned thread.");
    }

    if(p->m_state != E_THREADSTATE_UNSTARTED) {
        SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Thread was already started.");
    }

    // WARNING
    // It's important to ref the instance BEFORE it's scheduled as an OS thread:
    // due to race conditions, the thread may finish before we get to ref'ing
    // it, by that time it can be already unref'd on another thread in the
    // onStart sub and deleted.
    this->Ref();

    p->m_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WinToSkizo, (LPVOID)this, 0, 0); //-V513
    if(p->m_handle == 0) {
        this->Unref();
        CoreUtils::ThrowWin32Error();
    }

    // ******************************************
    // Sets the thread affinity and the priority.
    // ******************************************

    if(p->m_procId) {
        DWORD_PTR mask = 0;
        mask |= (1 << p->m_procId);
        DWORD_PTR r = SetThreadAffinityMask(p->m_handle, mask);
        if(r == 0) {
            fprintf(stderr, "CThread::SetAffinity(int) failed.\n");
            // TODO use something like so_warn which can be turned off via macros
        }
    }

    if(p->m_priority != 50) { // 50 == normal
        DWORD winPriority;
        if(p->m_priority < 30) {
            winPriority = BELOW_NORMAL_PRIORITY_CLASS;
        /*else if(m_priority < 95)
            winPriority = REALTIME_PRIORITY_CLASS;*/
        } else if(p->m_priority > 80) {
            winPriority = HIGH_PRIORITY_CLASS;
        } else {
            winPriority = NORMAL_PRIORITY_CLASS;
        }

        SetPriorityClass(p->m_handle, winPriority);
    }
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
           1) The user forgot to call CThread::Start. In this case, raise an
              exception.
           2) The user called CThread::Start, but the thread isn't still ready
              to be used, it's still
              initializing its data. In this case, we must just wait.

           To distinguish these two cases, all we have to do is to check
           thread->m_handle. It's != 0 in the second case.
        */

        /* The user forgot to call CThread::Start. */
        if(thread->p->m_handle == 0) {
            SKIZO_THROW_WITH_MSG(EC_INVALID_STATE, "Can't join an unstarted thread.");
        }

        /*
           Although the thread is unstarted in the managed context, it's fully
           usable in the native context.
           That's why we may proceed without any errors (i.e. join a semantically
           unstarted thread).
         */
    }

    WaitForSingleObject(thread->p->m_handle, timeout? timeout: INFINITE);
}

bool CThread::Wait(const CWaitObject* waitObject, int timeout)
{
    if(timeout < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    // Makes sure the object isn't deleted while we're waiting for it.
    waitObject->Ref();

    DWORD r = WaitForSingleObject(waitObject->m_handle, timeout? timeout: INFINITE);

    waitObject->Unref();
    return r == WAIT_OBJECT_0;
}

void CThread::Sleep(int ms)
{
    if(ms < 0) {
        SKIZO_THROW(EC_ILLEGAL_ARGUMENT);
    }

    ::Sleep(ms);
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
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if(GetThreadTimes(p->m_handle, &creationTime, &exitTime, &kernelTime, &userTime)) {
        // MSDN: "Do not cast a pointer to a FILETIME structure to either a
        // ULARGE_INTEGER* or __int64* value because it can cause alignment
        // faults on 64-bit Windows."

        SKIZO_REQ_EQUALS(sizeof(FILETIME), sizeof(so_long)); // Just in case...

        so_long lKernelTime, lUserTime;
        memcpy(&lKernelTime, &kernelTime, sizeof(so_long));
        memcpy(&lUserTime, &userTime, sizeof(so_long));

        // Combines user+kernel mode, converts to milliseconds.
        return (so_long)((double)(lKernelTime + lUserTime) / 10000);
    } else {
        // Thread may not exist anymore if it's a non-owned thread.
        return 0;
    }
}

static CThread* matchCapturedThreadToKnownThread(DWORD capturedThreadId)
{
    CThread* r = nullptr;

    EnterCriticalSection(&g_cs);
        SKIZO_REQ_PTR(g_knownThreadList);

        for(int i = 0; i < g_knownThreadList->Count(); i++) {
            // Threads cannot be destroyed while we're running here, because
            // we're inside the critical section, and all threads need to deregister
            // themselves through this list. I.e. if a thread suddenly decides
            // it needs to stop while we're here, it will enter the critical
            // section to deregister from the list, and so it will wait.
            CThread* knownThread = static_cast<CThread*>(g_knownThreadList->Array()[i]);

        #if _WIN64 
            SThreadHandle knownThreadHandle = knownThread->Handle();
            DWORD knownThreadId = GetThreadId(knownThreadHandle.Value);
        #else
            // FIX for older compilers, which essentially means we cannot
            // recognize threads we themselves created in Thread::GetThreads() TODO
            // GetThreadId(..) is a Vista-only function
            DWORD knownThreadId = 0;
        #endif

            if(knownThreadId == capturedThreadId) {
                r = knownThread;
                // Do not forget add reference count, as expected by the name of
                // the function.
                r->Ref();
                // We need to safely quit because we're inside a critical section.
                break;
            }
        }

    LeaveCriticalSection(&g_cs);

    return r;
}

CArrayList<CThread*>* CThread::GetThreads()
{
    CArrayList<CThread*>* r = new CArrayList<CThread*>();

    DWORD dwOwnerPID;
    HANDLE hThreadSnap;
    THREADENTRY32 te32; 

    // Only the current process.
    dwOwnerPID = GetCurrentProcessId();
 
    // Takes a snapshot of all running threads.
    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
    if(hThreadSnap == INVALID_HANDLE_VALUE) {
        return r;
    }
 
    // Fills in the size of the structure before using it. 
    te32.dwSize = sizeof(THREADENTRY32); 
 
    // Retrieves information about the first thread, and exits if unsuccessful.
    if(!Thread32First(hThreadSnap, &te32))  {
        CloseHandle(hThreadSnap);            // Must clean up the snapshot object.
        return r;
    }

    // Now walks the thread list of the system, and retrieves handles of each thread
    // associated with the current process.

    do {
        if(te32.th32OwnerProcessID == dwOwnerPID) {

            // Main thread? Special case.
            if(te32.th32ThreadID == g_MainThreadId) {
                SKIZO_REQ_PTR(g_MainThread);
                r->Add(g_MainThread);
                continue;
            }

            // Searches if this thread is already known to us (if it was us who
            // created it in the first place).
            Auto<CThread> knownThread (matchCapturedThreadToKnownThread(te32.th32ThreadID));

            if(knownThread) {
                // Known thread? Just add it to the list.

                r->Add(knownThread);
            } else {
                // Unknown thread? Create a new non-owned wrapper.

                // MSDN for the permission set: "If an application compiled for
                // Windows Server 2008 and Windows Vista is run on Windows Server
                // 2003 or Windows XP, the THREAD_ALL_ACCESS flag contains access
                // bits that are not supported and the function specifying this
                // flag fails with ERROR_ACCESS_DENIED.
                // NOTE The handle returned by OpenThread needs to be closed by
                // CloseHandle, and the thread's destructor already does exactly
                // that.
                HANDLE capturedThreadHandle = OpenThread(THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION,
                                                         FALSE, // ?
                                                         te32.th32ThreadID);

                Auto<CThread> capturedThread (new CThread());
                capturedThread->p->m_handle = capturedThreadHandle;
                capturedThread->p->m_state = E_THREADSTATE_RUNNING;
                // Important. See CThread::IsOwned() for more information.
                capturedThread->p->m_isOwned = false;

                r->Add(capturedThread);
            }
        }
    } while( Thread32Next(hThreadSnap, &te32));

    // Don't forget to clean up the snapshot object.
    CloseHandle( hThreadSnap );

    return r;
}

void CThread::SetName(const char* name)
{
    SKIZO_REQ_PTR(name);

    if(p->m_name) {
        delete [] p->m_name;
    }
    p->m_name = new char[strlen(name) + 1];
    strcpy(p->m_name, name);
}

const char* CThread::Name() const
{
    return p->m_name;
}

} }

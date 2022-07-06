// *****************************************************************************
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

#ifndef WAITOBJECT_H_INCLUDED
#define WAITOBJECT_H_INCLUDED

#ifdef SKIZO_SINGLE_THREADED
    // We need to guarantee atomicity of wait objects even when compiled in
    // single-threaded mode.
    #include "AtomicObject.h"
#else
    #include "Object.h"
#endif

#ifdef SKIZO_X
    #include <pthread.h>
#endif

namespace skizo { namespace core {

/**
 * Tells the thread that waits for a certain event to wake up.
 * Use CThread::Wait(CWaitObject*) to wait on this wait object.
 */
#ifdef SKIZO_SINGLE_THREADED
class CWaitObject final: public CAtomicObject
#else
class CWaitObject final: public CObject
#endif
{
    friend class CThread;

public:
    CWaitObject(bool initialState = false, bool resetAutomatically = true);
    virtual ~CWaitObject();

    /**
     * Sets the state of the object to "signaled", allowing the waiting thread to
     * proceed. Automatically resets to non-signaled once the thread is released
     * (resetAutomatically == true)
     *
     * @note Only one thread at a time is guaranteed to proceed.
     */
    void Pulse();

private:
#ifdef SKIZO_WIN
    HANDLE m_handle;
#endif
#ifdef SKIZO_X
    mutable bool m_autoReset;
    mutable pthread_cond_t m_cvariable;
    mutable pthread_mutex_t m_mutex;
    mutable bool m_state;

    int setEvent();
    int resetEvent();
    int unlockedWaitForEvent(int timeout) const;
#endif
};

} }

#endif // WAITOBJECT_H_INCLUDED

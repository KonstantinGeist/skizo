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

#include "WaitObject.h"
#include "Thread.h"
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

// Based on:
/*
 * WIN32 Events for POSIX
 * Author: Mahmoud Al-Qudsi <mqudsi@neosmart.net>
 * Copyright (C) 2011 - 2013 by NeoSmart Technologies
 * This code is released under the terms of the MIT License
 */

namespace skizo { namespace core {

#define CHECK_RESULT if(result != 0) { SKIZO_THROW(EC_PLATFORM_DEPENDENT); }

int CWaitObject::setEvent()
{
    int result = pthread_mutex_lock(&m_mutex);
    CHECK_RESULT

    m_state = true;

    if(m_autoReset) {
        if(m_state) {
            result = pthread_mutex_unlock(&m_mutex);
            CHECK_RESULT

            result = pthread_cond_signal(&m_cvariable);
            CHECK_RESULT

            return 0;
        }
    } else {
        result = pthread_mutex_unlock(&m_mutex);
        CHECK_RESULT

        result = pthread_cond_broadcast(&m_cvariable);
        CHECK_RESULT
    }

    return 0;
}

int CWaitObject::resetEvent()
{
    int result = pthread_mutex_lock(&m_mutex);
    CHECK_RESULT

    m_state = false;

    result = pthread_mutex_unlock(&m_mutex);
    CHECK_RESULT

    return 0;
}

CWaitObject::CWaitObject(bool initialState, bool resetAutomatically)
{
    int result = pthread_cond_init(&m_cvariable, 0);
    CHECK_RESULT

    result = pthread_mutex_init(&m_mutex, 0);
    CHECK_RESULT

    m_state = false;
    m_autoReset = resetAutomatically;

    if(initialState) {
        result = setEvent();
        CHECK_RESULT
    }
}

CWaitObject::~CWaitObject()
{
    int result = 0;

    result = pthread_cond_destroy(&m_cvariable);
    CHECK_RESULT

    result = pthread_mutex_destroy(&m_mutex);
    CHECK_RESULT
}

void CWaitObject::Pulse()
{
    int result = setEvent();
    CHECK_RESULT
    result = resetEvent();
    CHECK_RESULT
}

int CWaitObject::unlockedWaitForEvent(int _timeout) const
{
    uint64_t timeout = (uint64_t)_timeout;

    int result = 0;
    if(!m_state) {
        //Zero-timeout event state check optimization
        if(timeout == 0) {
            return ETIMEDOUT;
        }

        timespec ts;
        if(timeout != (uint64_t) -1) {
            timeval tv;
            gettimeofday(&tv, NULL);

            uint64_t nanoseconds = ((uint64_t) tv.tv_sec) * 1000 * 1000 * 1000
                      + timeout * 1000 * 1000 + ((uint64_t) tv.tv_usec) * 1000;

            ts.tv_sec = nanoseconds / 1000 / 1000 / 1000;
            ts.tv_nsec = (nanoseconds - ((uint64_t) ts.tv_sec) * 1000 * 1000 * 1000);
        }

        do {
            // Regardless of whether it's an auto-reset or manual-reset event:
            // wait to obtain the event, then lock anyone else out.
            if(timeout != (uint64_t) -1) {
                result = pthread_cond_timedwait(&m_cvariable, &m_mutex, &ts);
            } else {
                result = pthread_cond_wait(&m_cvariable, &m_mutex);
            }

        } while(result == 0 && !m_state);

        if(result == 0 && m_autoReset) {
            // We've only accquired the event if the wait succeeded.
            m_state = false;
        }

    } else if(m_autoReset) {
        // It's an auto-reset event that's currently available;
        // we need to stop anyone else from using it.
        result = 0;
        m_state = false;
    }

    // Else we're trying to obtain a manual reset event with a signaled state;
    // don't do anything.

    return result;
}

bool CThread::Wait(const CWaitObject* waitObject, int timeout)
{
    // Makes sure the object isn't deleted while we're waiting for it.
    waitObject->Ref();

    int tempResult;
    if(timeout == 0) {
        tempResult = pthread_mutex_trylock(&waitObject->m_mutex);
        if(tempResult == EBUSY) {
            waitObject->Unref();
            return false; // "returns false if the timeout has expired."
        }
    } else {
        tempResult = pthread_mutex_lock(&waitObject->m_mutex);
    }

    assert(tempResult == 0);

    int result = waitObject->unlockedWaitForEvent(timeout);

    tempResult = pthread_mutex_unlock(&waitObject->m_mutex);
    assert(tempResult == 0);

    waitObject->Unref();
    return result == ETIMEDOUT? false: true; // "returns false if the timeout has expired."
}

} }

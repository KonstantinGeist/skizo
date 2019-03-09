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

#ifndef MUTEX_H_INCLUDED
#define MUTEX_H_INCLUDED

// WARNING Make sure that none of the objects the thread depends on make use of
// non-atomic objects to guarantee stability.

// ********

#ifdef SKIZO_SINGLE_THREADED
    // We need to guarantee atomicity of mutexes even when compiled in
    // single-threaded mode.
    #include "AtomicObject.h"
#else
    #include "Object.h"
#endif

#include "Exception.h"
#ifdef SKIZO_X
    #include <pthread.h>
#endif

namespace skizo { namespace core {

/**
 * Used together with SKIZO_END_LOCK to restrict access to shared data to 1 thread
 * at a time.
 *
 * Example:
 * \code{.cpp}
 * SKIZO_LOCK(&mutexName) {
 * } SKIZO_END_LOCK(&mutexName);
 * \endcode
 */
#define SKIZO_LOCK(mu) (mu)->Lock(); try {

/**
 * See SKIZO_LOCK
 */
#define SKIZO_END_LOCK(mu) } catch(skizo::core::SException& e) { (mu)->Unlock(); throw; } (mu)->Unlock()

/**
 * Implements a lightweight semaphore that can be used to coordinate access to
 * shared data from multiple concurrent threads.
 */
#ifdef SKIZO_SINGLE_THREADED
class CMutex: public CAtomicObject
#else
class CMutex: public CObject
#endif
{
private:
#ifdef SKIZO_X
    pthread_mutex_t m_data;
#endif
#ifdef SKIZO_WIN
    CRITICAL_SECTION m_data;
#endif

public:
    CMutex();
    virtual ~CMutex();

    /**
     * Attempts to grab the lock and waits if it isn't available: it locks the
     * current thread and unlocks it only if the mutex becomes available
     * (another thread releases the lock).
     *
     * @warning Don't use directly. Use SKIZO_LOCK/SKIZO_END_LOCK macros instead.
     */
    void Lock();

    /**
     * Releases the lock. If another thread was waiting on this mutex, it will
     * take ownership of this mutex.
     *
     * @warning Don't use directly. Use SKIZO_LOCK/SKIZO_END_LOCK macros instead.
     */
    void Unlock();
};

} }

#endif // MUTEX_H_INCLUDED

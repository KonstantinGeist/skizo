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

#ifndef QUEUE_H_INCLUDED
#define QUEUE_H_INCLUDED

// #define SKIZO_QUEUE_AS_LINKED_LIST

#include "Object.h"
#include "Contract.h"
#include "Enumerator.h"
#include "Exception.h"
#include "ref.h"

#ifdef SKIZO_QUEUE_AS_LINKED_LIST
    #include "LinkedList.h"
#endif

namespace skizo { namespace collections {

#ifdef SKIZO_QUEUE_AS_LINKED_LIST

// *********************************************************
//   LinkedList implementation.
// (skip below to the default ring buffer implementation).
// *********************************************************

template <class T>
class CQueue: public CLinkedList<T>
{
public:
    explicit CQueue(int /*initCap*/) // initCap unused
    {
    }

    void Enqueue(T value)
    {
        this->Add(value);
    }

    T Dequeue()
    {
        if(this->Count() == 0) {
            SKIZO_THROW_WITH_MSG(skizo::core::EC_INVALID_STATE, "Queue is empty.");
        }

        T r = this->FirstNode()->Value;
        SKIZO_REF(r);
        this->Remove(this->FirstNode());
        return r;
    }

    T Peek() const
    {
        if(this->Count() == 0) {
            SKIZO_THROW_WITH_MSG(skizo::core::EC_INVALID_STATE, "Queue is empty.");
        }

        return this->FirstNode()->Value;
    }

    bool IsEmpty() const
    {
        return this->Count() == 0;
    }
};

#define SQueueEnumerator SLinkedListEnumerator

#else

// ******************************
//   RingBuffer implementation.
// ******************************

/**
 * Default queue capacity.
 */
#define SKIZO_DEF_QUEUE_CAP 16

/**
 * Default grow factor.
 */
#define SKIZO_QUEUE_GROW_FACTOR 2

template <class T>
class SQueueEnumerator;

/**
 * Represents a first-in, first-out collection of objects.
 */
template <class T>
class CQueue: public skizo::core::CObject
{
    friend class SQueueEnumerator<T>;

private:
    int m_size, m_front, m_back, m_theArrayLength;
#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif
    T* m_theArray;

    void doubleQueue()
    {
        T* newArray = new T[m_theArrayLength * SKIZO_QUEUE_GROW_FACTOR];

        // Copy elements that are logically in the queue
        for(int i = 0; i < m_size; i++, m_front = increment(m_front)) {
            newArray[i] = m_theArray[m_front];
        }

        delete [] m_theArray;
        m_theArray = newArray;
        m_theArrayLength = m_theArrayLength * SKIZO_QUEUE_GROW_FACTOR;
        m_front = 0;
        m_back = m_size - 1;
    }

    int increment(int x)
    {
        if(++x == m_theArrayLength) {
            x = 0;
        }
        return x;
    }

public:
    /**
     * Initializes a new instance of the CQueue class that is empty and
     * has the specified initial capacity.
     *
     * @param initCap initial capacity. Optional.
     */
    explicit CQueue(int initCap = SKIZO_DEF_QUEUE_CAP)
    {
        // Pre-condition.
        SKIZO_REQ_NOT_NEG(initCap);

        m_theArrayLength = initCap;
        m_theArray = new T[m_theArrayLength];

        m_size = 0;
        m_front = 0;
        m_back = -1;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount = 0;
    #endif
    }

    virtual ~CQueue()
    {
        Clear();

        delete [] m_theArray;
    }

    /**
     * Adds an object to the end of the queue.
     *
     * @param v the object to add to the queue. The value can be NULL.
     */
    void Enqueue(T v)
    {
        if(m_size == m_theArrayLength) {
            doubleQueue();
        }
        m_back = increment(m_back);
        m_theArray[m_back] = v;
        m_size++;
        SKIZO_REF(v);

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif
    }

    /**
     * Removes and returns the object at the beginning of the queue.
     *
     * @throws EC_INVALID_STATE The queue is empty.
     */
    T Dequeue()
    {
        // Pre-condition.
        SKIZO_REQ_WITH_MSG(m_size > 0, skizo::core::EC_INVALID_STATE, "Queue is empty.");

        m_size--;

        T r = m_theArray[m_front];
        m_front = increment(m_front);

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // No SKIZO_REF since it's balanced by array[head] being virtually set to
        // null (i.e. SKIZO_UNREF).
        return r;
    }

    /**
     * Gets the number of elements contained in the queue.
     *
     * @note Guarantees to be thread-safe. Note however that the value can
     * be stale without flushing CPU caches if accessed from several threads, so
     * use this value in an unlocked fashion only for probing/heuristics.
     */
    int Count() const
    {
        return m_size;
    }

    /**
     * Checks if the queue is empty. Same as \code{.cpp}Count() == 0\endcode
     *
     * @note Guarantees to be thread-safe. Note however that the value can
     * be stale without flushing CPU caches if accessed from several threads, so
     * use this value in an unlocked fashion only for probing/heuristics.
     */
    bool IsEmpty() const
    {
        return m_size == 0;
    }

    /**
     * Returns the object at the beginning of the queue without removing it.
     * \warning Despite the rules established in CObject, this method
     * doesn't call Ref() on the object it returns, although it is a verb.
     *
     * @throws EC_INVALID_STATE The queue is empty.
     * @return the object at the beginning of the Queue.
     */
    T Peek() const
    {
        // Pre-condition.
        SKIZO_REQ_WITH_MSG(m_size > 0, skizo::core::EC_INVALID_STATE, "Queue is empty.");

        return m_theArray[m_front];
    }

    /**
     * Removes all objects from the queue.
     */
    void Clear()
    {
        for(int i = 0; i < m_size; i++, m_front = increment(m_front)) {
            SKIZO_UNREF(m_theArray[m_front]);
        }

        m_size = 0;
        m_front = 0;
        m_back = -1;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // Post-condition.
        SKIZO_REQ_EQUALS(this->Count(), 0);
    }
};

// **************************************

/**
 * The enumerator for the CQueue<T> class.
 */
template <class T>
class SQueueEnumerator: public SEnumerator<T>
{
private:
    const CQueue<T>* m_queue;
    int m_index;
    int m_cur;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

public:
    explicit SQueueEnumerator(const CQueue<T>* queue)
        : m_queue(queue)
    {
        Reset();
    }

    virtual void Reset() override
    {
        m_index = 0;
        m_cur = m_queue->m_front;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        this->m_modCount = m_queue->m_modCount;
    #endif
    }

    inline int CurrentIndex() const
    {
        return m_index - 1;
    }

    virtual bool MoveNext(T* out) override
    {
    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        if(this->m_modCount != m_queue->m_modCount) {
            SKIZO_THROW(skizo::core::EC_CONCURRENT_MODIFICATION);
        }
    #endif

        if(m_index == m_queue->Count()) {
            return false;
        }

        *out = m_queue->m_theArray[m_cur];
        if(++m_cur == m_queue->m_theArrayLength) {
            m_cur = 0;
        }

        m_index++;
        return true;
    }
};

#endif

} }

#endif // QUEUE_H_INCLUDED

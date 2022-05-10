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

#ifndef STACK_H_INCLUDED
#define STACK_H_INCLUDED

#include "Object.h"
#include "Contract.h"
#include "Exception.h"
#include "ref.h"

namespace skizo { namespace collections {

/**
 * The default size of the stack if no initial capacity is specified explicitly
 * in the CStack's constructor.
 */
#define SKIZO_DEFAULT_STACK_SIZE 32

/**
 * Represents a last-in-first-out (LIFO) stack of objects.
 */
template <class T>
class CStack: public skizo::core::CObject
{
private:
    int m_cap, m_index;
    T* m_items;

public:
    /**
     * Initializes a new instance of the CStack class that is empty and
     * has the specified initial capacity.
     *
     * @param initCap initial capacity. Optional. If ommited, the default value
     * is SKIZO_DEFAULT_STACK_SIZE.
     */
    explicit CStack(int initCap = 0)
    {
        SKIZO_REQ_NOT_NEG(initCap)

        m_cap = (initCap == 0)? SKIZO_DEFAULT_STACK_SIZE: initCap;
        m_items = new T[m_cap];
        m_index = 0;
    }

    virtual ~CStack()
    {
        this->Clear();

        delete [] m_items;
    }

	/**
	 * Gets the number of elements contained in the stack.
	 */
    inline int Count() const
    {
        return m_index;
    }

    /**
     * Pushes an item onto the top of this stack.
     * @param item the item to be pushed onto this stack.
     */
    void Push(T item)
    {
        if((float)m_index / (float)m_cap >= SKIZO_LOAD_FACTOR) {
            m_items = skizo::core::CoreUtils::ReallocArray<T>(m_items, m_index, m_cap *= 2);
        }

        m_items[m_index] = item;
        SKIZO_REF(item);
        m_index++;
    }

    /**
     * Removes the object at the top of this stack and returns that object
     * as the value of this function.
     * @return the object at the top of this stack
     * @throws EC_INVALID_STATE The stack is empty.
     */
    T Pop()
    {
        if(m_index == 0) {
            SKIZO_THROW_WITH_MSG(skizo::core::EC_INVALID_STATE, "Stack is empty.");
        }

        return m_items[--m_index];

        /* Balance:

           SKIZO_REF(v_out);
           SKIZO_UNREF(v_out); */
    }

    /**
     * Looks at the object at the top of this stack without removing it from the
     * stack.
     *
     * @warning Despite the rules established in CObject, this method
     * doesn't call Ref() on the object it returns, although it is a verb.
     * @throws EC_INVALID_STATE the stack is empty.
     * @return the object at the top of this stack
     */
    T Peek() const
    {
        // must be m_index > 0
        SKIZO_REQ_WITH_MSG(m_index, skizo::core::EC_INVALID_STATE, "Stack is empty.");

        return m_items[m_index - 1];
    }

    /**
     * Retrieves an object at the specified index in this stack without removing
     * it from the stack.
     *
     * @param index the specified index to retrieve the item at
     * @return the object at the specified index
     */
    T Item(int index) const
    {
        SKIZO_REQ_RANGE_D(index, 0, m_index);

        return m_items[index];
    }

    /**
     * Checks if the stack is empty.
     */
    inline bool IsEmpty() const
    {
        return m_index == 0;
    }

    /**
     * Removes all objects from the stack.
     */
    void Clear()
    {
        for(int i = 0; i < m_index; i++) {
            SKIZO_UNREF(m_items[i]);
        }

        m_index = 0;
    }
};

} }

#endif // STACK_H_INCLUDED

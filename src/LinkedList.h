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

#ifndef LINKEDLIST_H_INCLUDED
#define LINKEDLIST_H_INCLUDED

#include "Object.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "Enumerator.h"
#include "ref.h"

namespace skizo { namespace collections {

/**
 * Represents a node in a CLinkedList<T>.
 */
template <class T>
class SLinkedListNode
{
public:
    SLinkedListNode<T> *Prev, *Next;
    T Value;

    explicit SLinkedListNode(T value)
        : Prev(nullptr),
          Next(nullptr),
          Value(value)
    {
        SKIZO_REF(value);
    }

    ~SLinkedListNode()
    {
        SKIZO_UNREF(Value);
    }
};

template <class T>
class SLinkedListEnumerator;

/**
 * Represents a doubly-linked list. Efficient for frequent inserts/removals;
 * inefficient in regards to memory use.
 */
template <class T>
class CLinkedList: public skizo::core::CObject
{
    friend class SLinkedListEnumerator<T>;

private:
    SLinkedListNode<T> *m_first, *m_last;
    int m_count;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

    SLinkedListNode<T>* findNode(T item) const
    {
        for(SLinkedListNode<T>* node = m_first; node; node = node->Next) {
            if(SKIZO_EQUALS(node->Value, item)) {
                return node;
            }
        }

        return nullptr;
    }

public:
    /**
     * Initializes a new instance of the CLinkedList<T> class that is empty.
     */
    CLinkedList()
        : m_first(nullptr),
          m_last(nullptr),
          m_count(0)
    {
    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount = 0;
    #endif
    }

    virtual ~CLinkedList()
    {
        this->Clear();
    }

    /**
     * Returns the first node. If the list is empty, returns NULL.
     */
    SLinkedListNode<T>* FirstNode()  const
    {
        return m_first;
    }

    /**
     * Returns the last node. If the list is empty, returns NULL.
     */
    SLinkedListNode<T>* LastNode() const
    {
        return m_last;
    }

    /**
     * Finds a list node that corresponds to the value.
     */
    SLinkedListNode<T>* FindNode(T value) const
    {
        return findNode(value);
    }

    /**
     * Adds a new node that contains the specified value at the end of the linked
     * list.
     *
     * @param item the value to add at the end of the linked list
     */
    SLinkedListNode<T>* Add(T item)
    {
        SLinkedListNode<T>* node = new SLinkedListNode<T>(item);

        if(!m_first) {
            m_first = m_last = node;
        } else {
            m_last->Next = node;
            node->Prev = m_last;
            m_last = node;
        }

        m_count++;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        return node;
    }

    /**
     * Adds a new value after the specified existing node in the CLinkedList<T>.
     *
     * @param node the node after which to add the value
     * @param item the new value to add
     */
    SLinkedListNode<T>* InsertAfter(SLinkedListNode<T>* node, T item)
    {
        SLinkedListNode<T>* newNode = new SLinkedListNode<T>(item);

        newNode->Prev = node;
        newNode->Next = node->Next;
        if(node->Next) {
            node->Next->Prev = newNode;
        }
        node->Next = newNode;

        if(node == m_last) {
            m_last = newNode;
        }

        m_count++;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        return newNode;
    }

    /**
     * Adds a new value before the specified existing node in the CLinkedList<T>.
     *
     * @param node the node before which to add the value
     * @param item the new value to add
     */
    SLinkedListNode<T>* InsertBefore(SLinkedListNode<T>* node, T item)
    {
        SLinkedListNode<T>* newNode = new SLinkedListNode<T>(item);

        newNode->Next = node;
        newNode->Prev = node->Prev;
        if(node->Prev) {
            node->Prev->Next = newNode;
        }
        node->Prev = newNode;

        if(node == m_first) {
            m_first = newNode;
        }

        m_count++;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        return newNode;
    }

    /**
     * Removes the first occurrence of the specified item from this list, if it
     * is present.
     *
     * @param item item to be removed from this list, if present
     * @return true if this list contained the specified item; false otherwise.
     */
    bool Remove(T item)
    {
        SLinkedListNode<T>* node = findNode(item);
        if(node) {
            this->Remove(node); // count is updated here
            return true;
        } else {
            return false;
        }
    }

    /**
     * Removes the specified node from the CLinkedList<T>.
     */
    void Remove(SLinkedListNode<T>* node)
    {
        SKIZO_REQ_PTR(node);

        if(node->Prev) {
            node->Prev->Next = node->Next;
        } else {
            m_first = node->Next;
        }

        if(node->Next) {
            node->Next->Prev = node->Prev;
        } else {
            m_last = node->Prev;
        }

        delete node;
        m_count--;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif
    }

    /**
     * Removes all of the items from this list.
     */
    void Clear()
    {
        SLinkedListNode<T>* cur = m_first;

        while(cur) {
            SLinkedListNode<T>* next = cur->Next;
            delete cur;
            cur = next;
        }

        m_first = m_last = nullptr;
        m_count = 0;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // Post-condition.
        SKIZO_REQ_EQUALS(this->Count(), 0);
    }

    /**
     * Gets the number of nodes contained in the CLinkedList<T>.
     */
    inline int Count() const
    {
        return m_count;
    }
};

// ***********************
//     Enumerator.
// ***********************

/**
 * The enumerator for the CLinkedList<T> class.
 */
template <class T>
class SLinkedListEnumerator: public SEnumerator<T>
{
private:
    const CLinkedList<T>* m_list;
    SLinkedListNode<T>* m_node;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

public:
    explicit SLinkedListEnumerator(const CLinkedList<T>* list)
        : m_list(list)
    {
        Reset();
    }

    virtual void Reset() override
    {
        m_node = m_list->m_first;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        this->m_modCount = m_list->m_modCount;
    #endif
    }

    virtual bool MoveNext(T* out) override
    {
    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        if(this->m_modCount != m_list->m_modCount) {
            SKIZO_THROW(skizo::core::EC_CONCURRENT_MODIFICATION);
        }
    #endif

        if(!m_node) {
            return false;
        }
        *out = m_node->Value;
        m_node = m_node->Next;
        return true;
    }
};

} }

#endif // LINKEDLIST_H_INCLUDED

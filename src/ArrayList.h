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

#ifndef ARRAYLIST_H_INCLUDED
#define ARRAYLIST_H_INCLUDED

#include "Object.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "Enumerator.h"
#include "ref.h"

namespace skizo { namespace collections {

template <class T>
class SArrayListEnumerator;

/**
 * Implements a list using an array whose size is dynamically increased as
 * required.
 */
template <class T>
class CArrayList: public skizo::core::CObject
{
    friend class SArrayListEnumerator<T>;

private:
    T* m_items;
    int m_count;
    int m_cap;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

    int findItem(T item) const
    {
        for(int i = 0; i < m_count; i++) {
            if(SKIZO_EQUALS(m_items[i], item)) {
                return i;
            }
        }

        return -1;
    }

    void qsort(int(*compareFunc)(T obj1, T obj2), int left, int right)
    {
          int i = left, j = right;
          T tmp;
          T pivot = m_items[(left + right) / 2];

          // Partition.
          while (i <= j) {
                while (compareFunc(m_items[i], pivot) < 0) {
                      i++;
                }
                while (compareFunc(m_items[j], pivot) > 0) {
                      j--;
                }
                if (i <= j) {
                      tmp = m_items[i];
                      m_items[i] = m_items[j];
                      m_items[j] = tmp;
                      i++;
                      j--;
                }
          };

          // Recursion.
          if (left < j) {
                qsort(compareFunc, left, j);
          }
          if (i < right) {
                qsort(compareFunc, i, right);
          }
    }

public:
    /**
     * Initializes a new instance of the CArrayList class that is empty and has
     * the specified capacity.
     *
     * @param initCap the initial capacity of the list
     */
    explicit CArrayList(int initCap = 0)
    {
        // Pre-condition.
        SKIZO_REQ_NOT_NEG(initCap);

        m_cap = initCap? initCap: 16;
        m_count = 0;
        m_items = new T[m_cap];

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount = 0;
    #endif
    }

    virtual ~CArrayList()
    {
        this->Clear();

        delete [] m_items;
    }

    /**
     * Determines whether an element is in the ArrayList.
     *
     * @return true if this list contains at least one item that
     *         is equal to item
     */
    bool Contains(T item) const
    {
        return findItem(item) != -1;
    }

    /**
     * Returns a shallow copy of this CArrayList instance.
     */
    CArrayList<T>* Clone() const
    {
        CArrayList* clone = new CArrayList(m_cap);
        clone->m_count = this->m_count;

        for(int i = 0; i < m_count; i++) {
            clone->m_items[i] = this->m_items[i];
            SKIZO_REF(clone->m_items[i]);
        }

        // Post-condition.
        SKIZO_REQ_EQUALS(this->Count(), clone->Count());
        return clone;
    }

    /**
     * Removes all items from the list.
     */
    void Clear()
    {
        for(int i = 0; i < m_count; i++) {
            SKIZO_UNREF(m_items[i]);
        }

        m_count = 0;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // Post-condition.
        SKIZO_REQ_EQUALS(this->Count(), 0);
    }

    /**
     * Replaces the item at the specified position in this list with
     * the specified item.
     *
     * @param index index of the item to replace
     * @param item item to be stored at the specified position
     * @throws EC_OUT_OF_RANGE if the index is out of range (index < 0 || index >= Count())
     */
    void Set(int index, T item)
    {
        // Pre-condition.
        SKIZO_REQ_RANGE_D(index, 0, m_count);

        SKIZO_REF(item);
        SKIZO_UNREF(m_items[index]);
        m_items[index] = item;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // Post-condition.
        //SKIZO_REQ_EQUALS(this->FindItem(item), index);
    }

    /**
     * Appends the specified item to the end of this list.
     *
     * @param item item to be appended to this list
     */
    void Add(T item)
    {
        if(((float)m_count / (float)m_cap) >= SKIZO_LOAD_FACTOR) {
            m_items = skizo::core::CoreUtils::ReallocArray<T>(m_items, m_count, m_cap *= 2);
        }

        m_items[m_count] = item;
        SKIZO_REF(item);

        m_count++;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // Post-condition.
        //SKIZO_REQ_EQUALS(this->FindItem(item), m_count - 1);
    }

    /**
     * Inserts an element into the CArrayList<T> at the specified index.
     *
     * @throws EC_OUT_OF_RANGE if the index is out of range (index < 0 || index > Count())
     */
    void Insert(int index, T item)
    {
        if(index < 0 || index > m_count) {
            SKIZO_THROW(skizo::core::EC_OUT_OF_RANGE);
        }

        if(((float)m_count / (float)m_cap) >= SKIZO_LOAD_FACTOR) {
            m_items = skizo::core::CoreUtils::ReallocArray<T>(m_items, m_count, m_cap *= 2);
        }

        memmove(&m_items[index + 1], &m_items[index], m_count * sizeof(T));
        m_items[index] = item;
        SKIZO_REF(item);
        m_count++;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        // Post-condition.
        SKIZO_REQ_EQUALS(this->FindItem(item), index);
    }

    /**
     * Appends all of the items in the specified unsafe buffer to the end of this
     * list, preserving the order.
     *
     * @warning There is no mechanism to prevent invalid access if the specified
     * count is greater than the passed buffer.
     *
     * @param arr buffer containing items to be added to this list
     * @param count the number of items to copy
     */
    void AddUnsafeRange(const T* arr, int count)
    {
        if(count < 0) {
            SKIZO_THROW(skizo::core::EC_OUT_OF_RANGE);
        }

        if(count == 0) {
            return;
        }

        if(((float)(m_count + count) / (float)m_cap) >= SKIZO_LOAD_FACTOR) {
            m_items = skizo::core::CoreUtils::ReallocArray<T>(m_items, m_count, m_cap = m_cap * 2 + count);
        }

        for(int i = 0; i < count; i++) {
            m_items[m_count + i] = arr[i];
            SKIZO_REF(arr[i]);
        }

        m_count += count;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif
    }

    /**
     * Appends all of the items in the specified arraylist to the end of this list,
     * preserving the order.
     *
     * @param arr arraylist containing items to be added to this list
     */
    void AddRange(const CArrayList<T>* arr)
    {
        this->AddUnsafeRange(arr->Array(), arr->Count());
    }

    /**
     * Returns the item at the specified position in this list.
     *
     * @param index index of the item to return
     * @return the item at the specified position in this list
     * @throws EC_OUT_OF_RANGE if the index is out of range (index < 0 || index >= Count())
     */
    T Item(int index) const
    {
        // Pre-condition.
        SKIZO_REQ_RANGE_D(index, 0, m_count);

        return m_items[index];
    }

    /**
     * Direct access to the underlying array. Typically used in loops where one
     * does not usually need to check for out of range exceptions.
     */
    inline T* Array() const
    {
        return m_items;
    }

    /**
     * Returns the number of items in this list.
     */
    inline int Count() const
    {
        return m_count;
    }

    /**
     * Removes the item at the specified position in this list. Shifts any
     * subsequent items to the left.
     *
     * @param index the index of the item to be removed
     * @return false if out of range; true otherwise
     */
    bool RemoveAt(int index)
    {
        if(index < 0 || index >= m_count) {
            return false;
        }

        T item = m_items[index];

        for(int i = index; i < m_count - 1; i++) {
            m_items[i] = m_items[i + 1];
        }

        m_count--;
        SKIZO_UNREF(item);

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        return true;
    }

    /**
     * Removes the first occurrence of the specified item from this list, if present.
     *
     * @param item item to be removed from this list, if present
     * @return true if this list contained the specified item; false otherwise.
     */
    bool Remove(T item)
    {
        const int found = findItem(item);
        if(found == -1) {
            return false;
        }

        return this->RemoveAt(found);
    }

    /**
     * Expands the array with values.
     *
     * @param count number of times to insert the default value
     * @param def the value to insert
     */
    void Expand(int count, T def)
    {
        for(int i = 0; i < count; i++) {
            this->Add(def);
        }
    }

    /**
     * Finds the specified item in the list and returns its index (position).
     * Returns -1 if the item was not found in the list.
     */
    int FindItem(T item) const
    {
        return findItem(item);
    }

    /**
     * Sorts the items in the entire arraylist using the quicksort algorithm:
     * if two items are equal, their original order might not be preserved.
     * The compare function returns an abstract integer which tells the order
     * of two given objects in respect to each other (<0 would mean obj1 is less than 
     * obj2, >0 would mean obj1 is greater than obj2, and a return value of 0 would
     * mean the values equal).
     */
    void Sort(int(*compareFunc)(T obj1, T obj2))
    {
        SKIZO_REQ_PTR(compareFunc);

        if(m_count > 1) {
            qsort(compareFunc, 0, m_count - 1);
        }

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif
    }
};

/**
 * The enumerator class for CArrayList<T>
 */
template <class T>
class SArrayListEnumerator: public SEnumerator<T>
{
private:
    CArrayList<T>* m_list;
    int m_index;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

public:
    explicit SArrayListEnumerator(CArrayList<T>* list)
        : m_list(list)
    {
        Reset();
    }

    virtual void Reset() override
    {
        m_index = 0;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        this->m_modCount = m_list->m_modCount;
    #endif
    }

    /**
     * Returns the current index inside the CArrayList.
     */
    inline int CurrentIndex() const
    {
        return m_index - 1;
    }

    virtual bool MoveNext(T* out) override
    {
    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        if(this->m_modCount != m_list->m_modCount) {
            SKIZO_THROW(skizo::core::EC_CONCURRENT_MODIFICATION);
        }
    #endif

        if(m_index == m_list->Count()) {
            return false;
        }
        *out = m_list->Array()[m_index];
        m_index++;
        return true;
    }
};

} }

#endif // ARRAYLIST_H_INCLUDED

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

#ifndef ORDEREDHASHMAP_H_INCLUDED
#define ORDEREDHASHMAP_H_INCLUDED

#include "HashMap.h"
#include "LinkedList.h"

// TODO inherit OrderedHashMap from HashMap or AbstractMap

namespace skizo { namespace collections {

/**
 * Implementation detail of TOrderedHashMap<K, V>.
 */
template <class K, class V>
class COrderedHashMapEntry: public skizo::core::CObject
{
public:
    // The values are weak because references are retained in m_map anyway.
    K Key;
    V Value;

    COrderedHashMapEntry(K key, V value)
        : Key(key), Value(value)
    {
    }
};

template <class K, class V>
class SOrderedHashMapEnumerator;

/**
 * A COrderedHashMap is a CHashMap with one additional property: it maintains a
 * list of the map's keys in the order they were added to the map, which allows
 * iteration with a defined order (unlike CHashMap).
 */
template <class K, class V>
class COrderedHashMap: public skizo::core::CObject
{
    friend class SOrderedHashMapEnumerator<K, V>;

private:
    skizo::core::Auto<CHashMap<K, V> > m_map;
    skizo::core::Auto<CLinkedList<COrderedHashMapEntry<K, V>*> > m_list;

public:
    COrderedHashMap()
        : m_map(new CHashMap<K, V>()),
          m_list(new CLinkedList<COrderedHashMapEntry<K, V>*>())
    {
    }

    /**
     * Returns the first added item.
     */
    SLinkedListNode<COrderedHashMapEntry<K, V>*>* FirstItem() { return m_list->FirstNode(); }

    /**
     * Returns the last added item.
     */
    SLinkedListNode<COrderedHashMapEntry<K, V>*>* LastItem() { return m_list->LastNode(); }

    /**
     * Associates the specified value with the specified key in this map.
     * If the map previously contained a mapping for the key, the old value is
     * replaced.
     *
     * @param key key with which the specified value is to be associated
     * @param value value to be associated with the specified key. Can be NULL.
     */
    void Set(K key, V value)
    {
        if(!m_map->Contains(key)) {
            m_map->Set(key, value);
            skizo::core::Auto<COrderedHashMapEntry<K, V> > entry (new COrderedHashMapEntry<K, V>(key, value));
            m_list->Add(entry);
        }
    }

    /**
     * Gets the value associated with the specified key.
     * Same as ::Item(..), however the value's reference count is increased.
     *
     * @param key the key of the value to get
     * @return the found value
     * @throws SKIZO_KEY_NOT_FOUND if the key is not found
     */
    V Get(K key) const
    {
        return m_map->Get(key);
    }

    /**
     * Gets or sets the value associated with the specified key.
     * Same as ::Get(..), however the value's reference count is not increased.
     */
    V Item(K key) const
    {
        return m_map->Item(key);
    }

    /**
     * Gets the value associated with the specified key.
     *
     * @param key the key of the value to get
     * @param out_value a pointer to a variable that will contain the value
     * associated with the specified key, if the key is found; otherwise, the
     * value is undefined.
     * @return true if the value is found; false otherwise.
     */
    bool TryGet(K key, V* out) const
    {
        return m_map->TryGet(key, out);
    }

    /**
     * Removes the key/value pair for the specified key from this map if present.
     *
     * @param key key whose mapping is to be removed from the map
     * @return true if the pair was removed; false if the pair was not found in
     * the map.
     */
    void Remove(K key)
    {
        m_map->Remove(key);

        for(SLinkedListNode<COrderedHashMapEntry<K, V>*>* node = m_list->FirstNode(); node; node = node->Next) {
            if(SKIZO_EQUALS(node->Value->Key, key)) {
                m_list->Remove(node);
                return;
            }
        }
    }


    /**
     * Determines whether the hashmap contains the key-value pair specified by
     * the key.
     *
     * @return true if the hashmap contains the key; false otherwise
     */
    bool Contains(K key) const
    {
        return m_map->Contains(key);
    }

    /**
     * Gets the number of key/value pairs contained in the hashmap.
     */
    inline int Size() const { return m_map->Size(); }

    /**
     * Removes all keys and values from the hashmap.
     */
    void Clear()
    {
        m_map->Clear();
        m_list->Clear();
    }

    /**
     * Imports all keys and values from the specified hashmap.
     */
    void Import(const COrderedHashMap<K, V>* hm)
    {
        // TODO copypaste

        SOrderedHashMapEnumerator<K, V> hmEnum(hm);
        K k;
        V v;
        while(hmEnum.MoveNext(&k, &v)) {
            this->Set(k, v);
        }
    }
};

// ****************
//   Enumerator.
// ****************

/**
 * The enumerator for the COrderedHashMap<K, V> class.
 */
template <class K, class V>
class SOrderedHashMapEnumerator
{
private:
    const COrderedHashMap<K, V>* m_map;
    SLinkedListEnumerator<COrderedHashMapEntry<K, V>*> m_listEnum;

public:
    explicit SOrderedHashMapEnumerator(const COrderedHashMap<K, V>* map)
        : m_map(map), m_listEnum(map->m_list)
    {
        Reset();
    }

    void Reset()
    {
        m_listEnum.Reset();
    }

    bool MoveNext(K* kout, V* vout)
    {
        COrderedHashMapEntry<K, V>* v;
        bool b = m_listEnum.MoveNext(&v);
        if(b) {
            *kout = v->Key;
            *vout = v->Value;
        }
        return b;
    }
};

} }

#endif // ORDEREDHASHMAP_H_INCLUDED

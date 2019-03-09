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

#ifndef HASHMAP_H_INCLUDED
#define HASHMAP_H_INCLUDED

#include "Object.h"
#include "Contract.h"
#include "CoreUtils.h"
#include "Exception.h"
#include "ref.h"

namespace skizo { namespace collections {

/**
 * Implementation detail of CHashMap<K, V>.
 */
template <class K, class V>
struct SHashMapEntry
{
    SHashMapEntry<K, V>* Next;
    K Key;
    V Value;

    SHashMapEntry(): Next(nullptr), Key(0), Value(0) { }
};

template <class K, class V>
class CHashMap;

/**
 * Enumerates the elements of a hashmap. The order is not guaranteed.
 */
template <class K, class V>
class SHashMapEnumerator
{
private:
    const CHashMap<K, V>* m_map;
    int m_bucketIndex;
    SHashMapEntry<K, V>* m_mapEntry;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

public:
    /**
     * Sets the enumerator to its initial position, which is before the first
     * element in the hashmap.
     */
    void Reset()
    {
        m_bucketIndex = 0;
        m_mapEntry = 0;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        this->m_modCount = m_map->m_modCount;
    #endif
    }

    /**
     * Creates an enumerator.
     * @param map the hashmap to enumerate over
     */
    explicit SHashMapEnumerator(const CHashMap<K, V>* map)
        : m_map(map)
    {
        Reset();
    }

    /**
     * Advances the enumerator to the next element of the collection.
     *
     * @warning Do not call Unref() on elements returned via kout and vout
     * parameters.
     * @param kout pointer to a variable which will contain the next key
     * @param vout pointer to a variable which will contain the next value
     * @return true if the enumerator was successfully advanced to the next
     * element; false if the enumerator has passed the end of the collection.
     * @note Usage:
     * \code{.cpp}SHashMapEnumerator<K, V> enumerator(hashmap);
     * K k;
     * V v;
     * while(enumerator.MoveNext(&k, &v)) {
     *  // Do something with k and v.
     * }\endcode
     */
    bool MoveNext(K* kout, V* vout)
    {
    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        if(this->m_modCount != m_map->m_modCount) {
            SKIZO_THROW(skizo::core::EC_CONCURRENT_MODIFICATION);
        }
    #endif

    try_again:
        if(m_bucketIndex < m_map->m_bucketCount) {
            if(m_mapEntry == 0) {
                m_mapEntry = m_map->m_buckets[m_bucketIndex];
                if(m_mapEntry == 0) {
                    m_bucketIndex++;
                    goto try_again;
                }
            } else {
                m_mapEntry = m_mapEntry->Next;
                if(m_mapEntry == 0) {
                    m_bucketIndex++;
                    goto try_again;
                }
            }

            if(kout) {
                *kout = m_mapEntry->Key;
            }
            if(vout) {
                *vout = m_mapEntry->Value;
            }

            return true;
        } else {
            return false;
        }
    }
};

/**
 * Represents a collection of key/value pairs that are organized based on the hash
 * code of the key.
 *
 * @note Provides constant-time performance for the basic operations (Get and Set),
 * assuming the hash function disperses the elements properly among the buckets.
 * If many key value pairs are to be stored in a hashmap, creating it with a
 * sufficiently large capacity will allow the pairs to be stored more efficiently
 * than letting it perform automatic rehashing as needed to grow the table.
 * @warning This class makes no guarantees as to the order of the map.
 * @param K type of the key
 * @param V type of the value
 */
template <class K, class V>
class CHashMap: public skizo::core::CObject
{
    friend class SHashMapEnumerator<K, V>;

private:
    SHashMapEntry<K, V>** m_buckets;
    int m_bucketCount, m_size, m_threshold;

#ifdef SKIZO_COLLECTIONS_MODCOUNT
    int m_modCount;
#endif

    void addEntry(K key, V value, int idx)
    {
        SHashMapEntry<K, V>* e = new SHashMapEntry<K, V>();

        e->Key = key;
        e->Value = value;

        SKIZO_REF(key);
        SKIZO_REF(value);

        e->Next = m_buckets[idx];
        m_buckets[idx] = e;
    }

    int getIdx(K key) const
    {
        if(SKIZO_IS_NULL(key)) {
            return 0;
        } else {
            return abs(SKIZO_HASHCODE(key) % m_bucketCount);
        }
    }

    void rehash()
    {
        SHashMapEntry<K, V>** oldBuckets = m_buckets;
        int oldBucketCount = m_bucketCount;

        int newcapacity = (m_bucketCount * 2) + 1;
        m_threshold = (int) (newcapacity * SKIZO_LOAD_FACTOR);
        m_buckets = (SHashMapEntry<K, V>**)calloc(newcapacity, sizeof(void*));
        m_bucketCount = newcapacity;

        int i;
        for (i = oldBucketCount; i-- > 0; ) {
            SHashMapEntry<K, V>* e = oldBuckets[i];
            while (e) {
                int idx = getIdx(e->Key);
                SHashMapEntry<K, V>* dest = m_buckets[idx];

                if (dest) {
                    while (dest->Next) {
                        dest = dest->Next;
                    }

                    dest->Next = e;
                } else {
                    m_buckets[idx] = e;
                }

                SHashMapEntry<K, V>* next = e->Next;
                e->Next = 0;
                e = next;
            }
        }

        free(oldBuckets);
    }

public:
    /**
     * Gets the number of key/value pairs contained in the hashmap.
     */
    inline int Size() const
    {
        return m_size;
    }

    virtual ~CHashMap()
    {
        Clear();

        free(m_buckets);
    }

    /**
     * Constructs an empty hashmap with the specified initial capacity.
     *
     * @param initCap the initial capacity
     */
    explicit CHashMap(int initCap = 0)
    {
        // Pre-condition.
        SKIZO_REQ_NOT_NEG(initCap);

        int capacity = initCap? initCap: 11;

        m_buckets = (SHashMapEntry<K, V>**)calloc(capacity, sizeof(void*));
        m_bucketCount = capacity;

        m_size = 0;
        m_threshold = (int)(capacity * 0.75f);

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount = 0;
    #endif
    }

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
        int idx = getIdx(key);
        SHashMapEntry<K, V>* e = m_buckets[idx];

        while (e) {
            if (SKIZO_EQUALS(key, e->Key)) {
                SKIZO_REF(value);
                SKIZO_UNREF(e->Value);

                e->Value = value;
                return;
            }
            else
                e = e->Next;
        }

        // At this point, we know we need to add a new entry.
        if (++m_size > m_threshold) {
            rehash();
            // Need a new idx to suit the bigger table.
            idx = getIdx(key);
        }

        addEntry(key, value, idx);

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif
    }

    /**
     * Removes the key/value pair for the specified key from this map if present.
     *
     * @param key key whose mapping is to be removed from the map
     * @return true if the pair was removed; false if the pair was not found in
     * the map.
     */
    bool Remove(K key)
    {
        int idx = getIdx(key);
        SHashMapEntry<K, V>* e = m_buckets[idx];
        SHashMapEntry<K, V>* prev_e = nullptr;

        while(e) {
            if(SKIZO_EQUALS(key, e->Key)) {
                SKIZO_UNREF(e->Key);
                SKIZO_UNREF(e->Value);

                if(prev_e == nullptr) {
                    m_buckets[idx] = e->Next;
                } else {
                    prev_e->Next = e->Next;
                }

                m_size--;
                delete e;

            #ifdef SKIZO_COLLECTIONS_MODCOUNT
                m_modCount++;
            #endif

                // Post-condition.
                SKIZO_REQ_EQUALS(this->Contains(key), false);

                return true;
            }

            prev_e = e;
            e = e->Next;
        }

        return false;
    }

    /**
     * Gets the value associated with the specified key.
     *
     * @param key the key of the value to get
     * @param out_value a pointer to a variable that will contain the value
     * associated with the specified key, if the key is found; otherwise, the
     * value is undefined. The returned value is Ref()'d if the value is
     * reference-counted.
     * @return true if the value is found; false otherwise.
     */
    bool TryGet(K key, V* out_value) const
    {
        int idx = getIdx(key);
        SHashMapEntry<K, V>* e = m_buckets[idx];

        while(e) {
            if(SKIZO_EQUALS(key, e->Key)) {
                SKIZO_REF(e->Value);
                *out_value = e->Value;

                return true;
            }

            e = e->Next;
        }

        return false;
    }

    /**
     * Gets the value associated with the specified key.
     * Same as ::Item(..), however the value's reference count is increased.
     * @param key the key of the value to get
     * @return the found value
     * @throws EC_KEY_NOT_FOUND if the key is not found
     */
    V Get(K key) const
    {
        V r;
        if(!TryGet(key, &r)) {
            SKIZO_THROW(skizo::core::EC_KEY_NOT_FOUND);
            return 0;
        }
        return r;
    }

    /**
     * Gets or sets the value associated with the specified key.
     * Same as ::Get(..), however the value's reference count is not increased.
     */
    V Item(K key) const
    {
        V r = Get(key);
        SKIZO_UNREF(r);
        return r;
    }

    /**
     * Determines whether the hashmap contains the key-value pair specified by
     * the key.
     *
     * @return true if the hashmap contains the key; false otherwise
     */
    bool Contains(K key) const
    {
        V out_v;
        bool exists = TryGet(key, &out_v);
        if(exists) {
            SKIZO_UNREF(out_v);
        }
        return exists;
    }

    /**
     * Removes all keys and values from the hashmap.
     */
    void Clear()
    {
        for(int i = 0; i < m_bucketCount; i++) {
            SHashMapEntry<K, V>* e = m_buckets[i];

            while(e) {
                SHashMapEntry<K, V>* next = e->Next;

                SKIZO_UNREF(e->Key);
                SKIZO_UNREF(e->Value);

                delete e;

                e = next;
            }
            m_buckets[i] = 0;
        }

        m_size = 0;

    #ifdef SKIZO_COLLECTIONS_MODCOUNT
        m_modCount++;
    #endif

        SKIZO_REQ_EQUALS(this->Size(), 0);
    }

    /**
     * Imports all keys and values from the specified hashmap.
     */
    void Import(const CHashMap<K, V>* hm)
    {
        SHashMapEnumerator<K, V> hmEnum(hm);
        K k;
        V v;
        while(hmEnum.MoveNext(&k, &v)) {
            this->Set(k, v);
        }
    }
};

} }

#endif // HASHMAP_H_INCLUDED

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

#ifndef POINTERSET_H_INCLUDED
#define POINTERSET_H_INCLUDED

// A faster reimplementation of CHashMap specialized for storing pointers.

namespace skizo { namespace script {

struct SPointerSetEnumerator;

struct SPointerSetEntry
{
    SPointerSetEntry* Next;
    void* Value;

    SPointerSetEntry()
        : Next(nullptr),
          Value(nullptr)
    {
    }
};

class SPointerSet
{
    friend struct SPointerSetEnumerator;

private:
    SPointerSetEntry** m_buckets;
    int m_bucketCount, m_size, m_threshold;

    void addEntry(void* value, int idx)
    {
        SPointerSetEntry* e = new SPointerSetEntry();
        e->Value = value;
        e->Next = m_buckets[idx];
        m_buckets[idx] = e;
    }

    int getIdx(void* value) const
    {
        int hash = (size_t)(value) >> 3;
        return abs(hash % m_bucketCount);
    }

    void rehash()
    {
        SPointerSetEntry** oldBuckets = m_buckets;
        int oldBucketCount = m_bucketCount;

        const int newcapacity = (m_bucketCount * 2) + 1;
        m_threshold = (int) (newcapacity * SKIZO_LOAD_FACTOR);
        m_buckets = (SPointerSetEntry**)calloc(newcapacity, sizeof(void*));
        m_bucketCount = newcapacity;

        for (int i = oldBucketCount; i-- > 0; ) {
            SPointerSetEntry* e = oldBuckets[i];
            while (e) {
                int idx = getIdx(e->Value);
                SPointerSetEntry* dest = m_buckets[idx];

                if (dest) {
                    while (dest->Next) {
                        dest = dest->Next;
                    }

                    dest->Next = e;
                } else {
                    m_buckets[idx] = e;
                }

                SPointerSetEntry* next = e->Next;
                e->Next = nullptr;
                e = next;
            }
        }

        free(oldBuckets);
    }

public:
    int Size() const
    {
        return m_size;
    }

    ~SPointerSet()
    {
        Clear();

        free(m_buckets);
    }

    SPointerSet()
    {
        int capacity = 547;

        m_buckets = (SPointerSetEntry**)calloc(capacity, sizeof(void*));
        m_bucketCount = capacity;

        m_size = 0;
        m_threshold = (int)(capacity * 0.75f);
    }

    void Set(void* value)
    {
        int idx = getIdx(value);
        SPointerSetEntry* e = m_buckets[idx];

        while (e) {
            if (value == e->Value) {
                e->Value = value;
                return;
            }
            else {
                e = e->Next;
            }
        }

        if (++m_size > m_threshold) {
          rehash();
          idx = getIdx(value);
        }

        addEntry(value, idx);
    }

    bool Remove(void* value)
    {
        const int idx = getIdx(value);
        SPointerSetEntry* e = m_buckets[idx];
        SPointerSetEntry* prev_e = nullptr;

        while(e) {
            if(value == e->Value) {
                if(prev_e == nullptr) {
                    m_buckets[idx] = e->Next;
                } else {
                    prev_e->Next = e->Next;
                }

                m_size--;
                delete e;

                return true;
            }

            prev_e = e;
            e = e->Next;
        }

        return false;
    }

    bool Contains(void* value) const
    {
        const int idx = getIdx(value);
        const SPointerSetEntry* e = m_buckets[idx];

        while(e) {
            if(value == e->Value) {
                return true;
            }
            e = e->Next;
        }

        return false;
    }

    void Clear()
    {
        for(int i = 0; i < m_bucketCount; i++) {
            SPointerSetEntry* e = m_buckets[i];

            while(e) {
                SPointerSetEntry* next = e->Next;
                delete e;
                e = next;
            }
            m_buckets[i] = nullptr;
        }

        m_size = 0;
    }
};

class SPointerSetEnumerator
{
private:
    const SPointerSet& m_set;
    int m_bucketIndex;
    SPointerSetEntry* m_entry;

public:
    explicit SPointerSetEnumerator(const SPointerSet& set)
        : m_set(set),
          m_bucketIndex(0),
          m_entry(nullptr)
    {
    }

    bool MoveNext(void** value_out)
    {
    try_again:
        if(m_bucketIndex < m_set.m_bucketCount) {
            if(!m_entry) {
                m_entry = m_set.m_buckets[m_bucketIndex];
                if(!m_entry) {
                    m_bucketIndex++;
                    goto try_again;
                }
            } else {
                m_entry = m_entry->Next;
                if(!m_entry) {
                    m_bucketIndex++;
                    goto try_again;
                }
            }

            *value_out = m_entry->Value;
            return true;
        } else {
            return false;
        }
    }
};

} }

#endif /* POINTERSET_H_INCLUDED */

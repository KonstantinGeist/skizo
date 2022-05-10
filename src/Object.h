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

#ifndef OBJECT_H_INCLUDED
#define OBJECT_H_INCLUDED

#include "options.h"
#include "basedefs.h"

namespace skizo { namespace core {

class CString;

    // *******************
    //       Auto
    // *******************

/**
 * A smart pointer to manage lifetime of CObject's and its subclasses.
 *
 * Usage is simple:
 * \code{.cpp}Auto<CObject> obj (new CObject());\endcode
 *
 * @note Doesn't automatically Ref() the object it consumes. However, it does
 * automatically Unref() when the object goes out of the scope (including
 * destructors).
 */
template <class T>
class Auto
{
public:
    Auto(): m_ptr(nullptr) { }
    explicit Auto(T* p) : m_ptr(p) { }
    ~Auto() { if(m_ptr) m_ptr->Unref(); }

    // Allows use of SetPtr(..)/SetVal(..) only.
    Auto(const Auto& other) = delete;
    Auto& operator=(const Auto& other) = delete;

    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr; }

    operator T*() const { return m_ptr; }

    T* Ptr() const { return m_ptr; }

    /**
     * Updates the Auto variable with a new value.
     * @warning Does not increase the reference count of the new value.
     */
    void SetPtr(T* ptr)
    {
        if(m_ptr) {
            m_ptr->Unref();
        }
        m_ptr = ptr;
    }

    /**
     * Updates the Auto variable with a new value.
     * @warning Does increase the reference count of the new value.
     */
    void SetVal(T* obj)
    {
        if(obj) {
            obj->Ref();
        }
        if(m_ptr) {
            m_ptr->Unref();
        }
        m_ptr = obj;
    }

private:
    T* m_ptr;
};

    // *******************
    //      CObject
    // *******************

/**
 * Class CObject is the root of the class hierarchy. Every class except interfaces
 * should have CObject as a superclass.
 *
 * @note Every object that inherits from CObject, is reference-counted.
 * <ol><li>When an object is acquired, call Ref().</li>
 * <li>When an object is no more used, call Unref().</li>
 * <li>An object that is returned by new operator, already has the effect
 *     as if Ref() was called, i.e. it already has one reference.</li>
 * <li>A byref object, that is returned by a function, has different logic
 * depending on the signature:
 *    <ol>
 *       <li>If the method uses an imperative verb, including such verbs as Get
 *     or Load, then the returned object is automatically referenced, i.e.
 *     there is no need to call Ref(). However, after the actual use,
 *     you must call Unref(), to tell the system you don't need it anymore.</li>
 *       <li> If the method doesn't have a verb in its name, and it has a name of
 *    a variable/field instead with no params, then it is a "viewer getter",
 *    which simply returns a reference to one of the fields. You don't need
 *    to invoke either Ref() or Unref() inside the function.
 *    However, if you want to keep the object for later use, call Ref()
 *     explicitly.</li>
 *    </ol>
 * </li>
 * <li>If one of the functions may throw an exception, to avoid memory leaks,
 * you have to use an Auto<T> smart pointer. Syntax is:
 *
 * \code{.cpp}Auto<CObject> obj (anotherObj->GetObject());\endcode</li>
 * </ol>
 *
 * @warning Circular references are to be avoided.
 */
class CObject
{
public:
    CObject();
    virtual ~CObject();

    /**
     * Increments the reference count by one.
     *
     * @warning It's advised not to use this method directly; use the Auto
     * smart pointer instead.
     */
    SKIZO_VIRTUAL_REF void Ref() const;

    /**
     * Decrements the reference count by one. If reference count is zero,
     * the object is automatically destroyed.
     *
     * @warning It's advised not to use this method directly; use the Auto
     * smart pointer instead.
     *
     * @return true if this object is disposed; false otherwise.
     */
    SKIZO_VIRTUAL_REF bool Unref() const;

    /**
     * Indicates whether some other object is "equal to" this one.
     * \note It is generally necessary to override the GetHashCode method whenever
     * this method is overridden, so as to maintain the general contract for the
     * GetHashCode method, which states that equal objects must have equal hash codes.
     * @return true if this object is the same as the obj argument; false otherwise.
     */
    virtual bool Equals(const CObject* obj) const;

    /**
     * Returns a hash code value for the object. This method is supported for
     * the benefit of hashmaps such as those provided by skizo::collections::CHashMap.
     * The general contract of GetHashCode is:
     * \li Whenever it is invoked on the same object more than once during an
     *     execution of a skizo application, the GetHashCode method must
     *     consistently return the same integer, provided no information used in
     *     Equals comparisons on the object is modified.
     * \li If two objects are equal according to the Equals method, then calling
     *     the GetHashCode method on each of the two objects must produce the same
     *     integer result.
     * \li It is not required that if two objects are unequal, then calling the
     *     GetHashCode method on each of the two objects must produce distinct integer results.
     *
     * @return A hash code value for this object.
     */
    virtual int GetHashCode() const;

    /**
     * Returns a string representation of the object. In general, the ToString method
     * returns a string that textually represents this object. The result should be
     * a concise but informative representation that is easy for a person to read.
     * @return a string representation of the object.
     */
    virtual const CString* ToString() const;

    /**
     * Retrieves the current reference count of the object. For debugging purposes
     * only.
     */
    inline int ReferenceCount() const
    {
        return (int)m_refCount;
    }

private:
#ifdef SKIZO_SINGLE_THREADED
    mutable int m_refCount;
#else
    mutable so_atomic_int m_refCount; // Should be also ideally aligned.
#endif
};

    // *******************
    //    CBoxedStruct
    // *******************

/**
 * A wrapper for any valuetype object, allows to store structs and classes which
 * do not derive from CObject directly in skizo collections.
 * Expects the wrapped value to contain implementations for GetHashCode() and ToString()
 */
template <class T>
class CBoxedStruct final: public CObject
{
public:
    /**
     * The wrapped value.
     */
    T Value;

    explicit CBoxedStruct(const T& value): Value(value) { }

    virtual const CString* ToString() const override { return Value.ToString(); }
    virtual int GetHashCode() const override { return Value.GetHashCode(); }
};

/**
 * Returns a string value which tells the object's type, its reference count and
 * other data. For debugging only.
 */
const CString* GetDebugStringInfo(const CObject* obj);

#ifdef SKIZO_BASIC_LEAK_DETECTOR
/**
 * Sometimes the static destructor that makes the leak detector work isn't called.
 * The reason is unknown, as a workaround one can use this function.
 */
void PrintLeakInfo();

/**
 * Suppresses the basic leak detector (doesn't print anything).
 */
void SuppressBasicLeakDetector(bool b);
#endif

} }

#endif // OBJECT_H_INCLUDED

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

#ifndef VARIANT_H_INCLUDED
#define VARIANT_H_INCLUDED

namespace skizo { namespace core {
class CObject;
class CString;

/**
 * An enumeration that describes the class type of a particular value (field,
 * parameter or return value).
 */
enum EVariantType
{
    /**
     * No value. Typical use: method returns.
     */
    E_VARIANTTYPE_NOTHING = 0,

    /**
     * A 32-bit integer.
     */
    E_VARIANTTYPE_INT,

    /**
     * A boolean.
     */
    E_VARIANTTYPE_BOOL,

    /**
     * A 32-bit float.
     */
    E_VARIANTTYPE_FLOAT,

    /**
     * A CObject-derived type.
     */
    E_VARIANTTYPE_OBJECT,

    /**
     * A binary blob.
     */
    E_VARIANTTYPE_BLOB
};

/**
 * A variant value.
 */
struct SVariant
{
public:
    // A hack to null it.
    explicit SVariant(void* v);

    SVariant();
    ~SVariant();
    SVariant(const SVariant& copy);
    SVariant& operator=(const SVariant& rhs);

    /**
     * Sets an integer value. The class type is set to E_VARIANTTYPE_INT.
     */
    void SetInt(int value);

    /**
     * Sets a boolean value. The class type is set to E_VARIANTTYPE_BOOL.
     */
    void SetBool(bool value);

    /**
     * Sets a float value. The class type is set to E_VARIANTTYPE_FLOAT.
     */
    void SetFloat(float value);

    /**
     * Sets an instance of a CObject-derived class as a value, including CString and
     * TReflectedEnum classes. The class type is set to E_VARIANTTYPE_OBJECT.
     */
    void SetObject(const CObject* value);

    /**
     * Makes the variant a nothing value.
     */
    void SetNothing();

    /**
     * Makes the variant a blob value.
     */
    void SetBlob(void* blob);

    /**
     * Returns an integer value.
     * @throws EC_TYPE_MISMATCH if the class type is not E_VARIANTTYPE_INT.
     */
    int IntValue() const;

    /**
     * Returns a boolean value.
     * @throws EC_TYPE_MISMATCH if the class type is not E_VARIANTTYPE_BOOL.
     */
    bool BoolValue() const;

    /**
     * Returns a float value.
     * @throws EC_TYPE_MISMATCH if the class type is not E_VARIANTTYPE_FLOAT.
     */
    float FloatValue() const;

    /**
     * Returns a blob.
     */
    void* BlobValue() const;

    /**
     * Returns an object value.
     * @throws EC_TYPE_MISMATCH if the class type is not E_VARIANTTYPE_OBJECT.
     */
    CObject* ObjectValue() const;

    /**
     * The subtype of this variant value.
     */
    EVariantType Type() const { return m_type; }

    bool Equals(const SVariant& other) const;
    int GetHashCode() const;

    const CString* ToString() const;

private:
    EVariantType m_type;
    union UVariant {
        int m_asInt;
        bool m_asBool;
        float m_asFloat;
        const CObject* m_asObject;
        void* m_asBlob;
    } m_union;
};

} }

#endif // VARIANT_H_INCLUDED

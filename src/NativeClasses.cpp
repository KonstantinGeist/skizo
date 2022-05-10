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

#include "Domain.h"
#include "Class.h"
#include "icall.h"

namespace skizo { namespace script {
using namespace skizo::core;

// TODO use an API for creating classes
// TODO use api to create method classes from signatures

void CDomain::initPredicateClass()
{
    Auto<CClass> klass (CClass::CreateIncompleteMethodClass(this));
    klass->SetFlatName(this->NewSlice("Predicate"));
    klass->InvokeMethod()->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
    this->RegisterClass(klass);
}

void CDomain::initRangeLooperClass()
{
    Auto<CClass> klass (CClass::CreateIncompleteMethodClass(this));
    klass->SetFlatName(this->NewSlice("RangeLooper"));
    {
        Auto<CParam> param (new CParam());
        param->Type.SetPrimType(E_PRIMTYPE_INT);
        klass->InvokeMethod()->Signature().Params->Add(param);
    }

    this->RegisterClass(klass);
}

void CDomain::initActionClass()
{
    Auto<CClass> klass (CClass::CreateIncompleteMethodClass(this));
    klass->SetFlatName(this->NewSlice("Action"));
    this->RegisterClass(klass);
}

void CDomain::initRangeStruct()
{
    // ************************************************
    //    Range struct
    // (built-in because ranges are required for loops)
    // ************************************************

    Auto<CClass> rangeClass (new CClass(this));
    m_rangeClass = rangeClass;
    rangeClass->SetPrimitiveType(E_PRIMTYPE_OBJECT);
    rangeClass->SetFlatName(this->NewSlice("Range"));
    rangeClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
    RegisterClass(rangeClass);

    Auto<CField> fromField (new CField());
    fromField->DeclaringClass = rangeClass;
    fromField->Name = this->NewSlice("from");
    fromField->Type.SetPrimType(E_PRIMTYPE_INT);
    rangeClass->RegisterInstanceField(fromField);

    Auto<CField> toField (new CField());
    toField->DeclaringClass = rangeClass;
    toField->Name = this->NewSlice("to");
    toField->Type.SetPrimType(E_PRIMTYPE_INT);
    rangeClass->RegisterInstanceField(toField);

    // *****************
    //   Range::create
    // *****************

    Auto<CMethod> ctor (new CMethod(rangeClass));
    ctor->SetMethodKind(E_METHODKIND_CTOR);
    ctor->SetName(this->NewSlice("create"));
    ctor->Signature().ReturnType = rangeClass->ToTypeRef();

    {
        Auto<CParam> param1 (new CParam());
        param1->Name = this->NewSlice("_from");
        param1->Type.SetPrimType(E_PRIMTYPE_INT);
        ctor->Signature().Params->Add(param1);

        Auto<CParam> param2 (new CParam());
        param2->Name = this->NewSlice("_to");
        param2->Type.SetPrimType(E_PRIMTYPE_INT);
        ctor->Signature().Params->Add(param2);
    }

    ctor->SetCBody("self.from = l__from;\n"
                   "self.to = l__to;\n");
    rangeClass->RegisterInstanceCtor(ctor);

    // **********************************
    //   Range::loop(li: RangeLooper)
    // **********************************

    {
        Auto<CMethod> nMethod (new CMethod(rangeClass));
        nMethod->SetName(this->NewSlice("loop"));
        nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
        {
            Auto<CParam> param1 (new CParam());
            param1->Type.SetObject(this->NewSlice("RangeLooper"));
            nMethod->Signature().Params->Add(param1);
        }
        rangeClass->RegisterInstanceMethod(nMethod);
    }

    // ****************************************
    //   Range::step(s:int li: RangeLooper)
    // ****************************************

    {
        Auto<CMethod> nMethod (new CMethod(rangeClass));
        nMethod->SetName(this->NewSlice("step"));
        nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
        {
            Auto<CParam> param1 (new CParam());
            param1->Type.SetPrimType(E_PRIMTYPE_INT);
            nMethod->Signature().Params->Add(param1);

            Auto<CParam> param2 (new CParam());
            param2->Type.SetObject(this->NewSlice("RangeLooper"));
            nMethod->Signature().Params->Add(param2);
        }
        rangeClass->RegisterInstanceMethod(nMethod);
    }
}

void CDomain::initErrorClass()
{
    // ***************************************************************
    //    Error class
    // base for all errors, part of implementation of failables.
    // WARNING Keep in sync with the layout specified by SErrorHeader.
    // ***************************************************************

    Auto<CClass> errorClass (new CClass(this));
    m_errorClass = errorClass;
    errorClass->SetPrimitiveType(E_PRIMTYPE_OBJECT);
    errorClass->SetFlatName(this->NewSlice("Error"));

    Auto<CField> msgField (new CField());
    msgField->DeclaringClass = errorClass;
    msgField->Name = this->NewSlice("m_message");
    msgField->Type = m_stringClass->ToTypeRef();
    errorClass->RegisterInstanceField(msgField);

    // *****************************************************
    // NOTE toString instead of "message" to be printable.
    //   message (getter).
    // *****************************************************
    {
        Auto<CMethod> nMethod (new CMethod(errorClass));
        nMethod->SetName(this->NewSlice("toString"));
        nMethod->Signature().ReturnType = m_stringClass->ToTypeRef();
        nMethod->SetCBody("return self->m_message;\n");
        errorClass->RegisterInstanceMethod(nMethod);
    }

    // *********************
    //   message (setter).
    // *********************
    {
        Auto<CMethod> nMethod (new CMethod(errorClass));
        nMethod->SetAccess(E_ACCESSMODIFIER_PROTECTED);
        nMethod->SetName(this->NewSlice("setMessage"));
        {
            Auto<CParam> param (new CParam());
            param->Name = this->NewSlice("_msg");
            param->Type = m_stringClass->ToTypeRef();
            nMethod->Signature().Params->Add(param);
        }
        nMethod->SetCBody("self->m_message = l__msg;\n");
        errorClass->RegisterInstanceMethod(nMethod);
    }

    // *********************

    RegisterClass(errorClass);
}

void CDomain::initStringClass()
{
    // *********************************************************
    //    string
    //  (not really a primitive, a built-in class nonetheless)
    //  NOTE should be before int/float because those refer to
    //  "string" in methods like "toString"
    // *********************************************************

    {
        Auto<CClass> stringClass (new CClass(this));
        SStringSlice stringClassName (this->NewSlice("string"));
        stringClass->SetPrimitiveType(E_PRIMTYPE_OBJECT);
        stringClass->SetFlatName(stringClassName);

        stringClass->SetStructDef(this->NewSlice("void** _soX_vtable;\n"
                                           "void* pStr _soX_ALIGNED;\n"));

        SGCInfo& gcInfo = stringClass->GCInfo();
        gcInfo.SizeForUse = sizeof(void*);
        gcInfo.ContentSize = 2 * sizeof(void*); // see above
        stringClass->Flags() |= E_CLASSFLAGS_IS_SIZE_CALCULATED;

        // ****************
        //   String::dtor
        // ****************

        {
            Auto<CMethod> dtor (new CMethod(stringClass));
            dtor->SetMethodKind(E_METHODKIND_DTOR);
            dtor->SetSpecialMethod(E_SPECIALMETHOD_NATIVE); // Implemented as an icall.
            stringClass->SetInstanceDtor(dtor);
        }

        // *******************
        //   String::length #1
        // *******************

        stringClass->DefICall(this->NewSlice("length"), "i", false);

        // *******************
        //   String::get #2
        // *******************

        stringClass->DefICall(this->NewSlice("get"), "ci", false);

        // *******************
        //    String::add #3
        // *******************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("op_add"));
            nMethod->Signature().ReturnType = stringClass->ToTypeRef();
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            Auto<CParam> param1 (new CParam());
            param1->Type.SetObject(stringClassName);
            param1->Type.ResolvedClass = stringClass;
            nMethod->Signature().Params->Add(param1);
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *************************
        //    String::toString #4
        // *************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("toString"));
            nMethod->Signature().ReturnType.SetObject(stringClassName);
            nMethod->Signature().ReturnType.ResolvedClass = stringClass;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *************************
        //    String::print #5
        // *************************

        stringClass->DefICall(this->NewSlice("print"), "v", false);

        // *************************
        //    String::substring #6
        // *************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("substring"));
            nMethod->Signature().ReturnType.SetObject(stringClassName);
            nMethod->Signature().ReturnType.ResolvedClass = stringClass;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);

            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetPrimType(E_PRIMTYPE_INT);
                nMethod->Signature().Params->Add(param1);

                Auto<CParam> param2 (new CParam());
                param2->Type.SetPrimType(E_PRIMTYPE_INT);
                nMethod->Signature().Params->Add(param2);
            }

            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *************************
        //    String::hashCode #7
        // *************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("hashCode"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *************************
        //    String::op_equals #8
        // *************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("op_equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);

            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(stringClassName);
                param1->Type.ResolvedClass = stringClass;
                nMethod->Signature().Params->Add(param1);
            }

            stringClass->RegisterInstanceMethod(nMethod);
        }

        // **********************
        //    String::equals #9
        // **********************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);

            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("any"));
                nMethod->Signature().Params->Add(param1);
            }

            stringClass->RegisterInstanceMethod(nMethod);
        }

        // **********************
        //    String::split #10
        // **********************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("split"));
            nMethod->Signature().ReturnType.SetObject(stringClassName);
            nMethod->Signature().ReturnType.ArrayLevel++;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);

            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(stringClassName);
                param1->Type.ResolvedClass = stringClass;
                nMethod->Signature().Params->Add(param1);
            }

            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *****************************
        //    String::toLowerCase #11
        // *****************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("toLowerCase"));
            nMethod->Signature().ReturnType.SetObject(stringClassName);
            nMethod->Signature().ReturnType.ResolvedClass = stringClass;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *****************************
        //    String::toUpperCase #12
        // *****************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("toUpperCase"));
            nMethod->Signature().ReturnType.SetObject(stringClassName);
            nMethod->Signature().ReturnType.ResolvedClass = stringClass;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *******************************
        //    String::findSubstring #13
        // *******************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("findSubstring"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(stringClassName);
                param1->Type.ResolvedClass = stringClass;
                nMethod->Signature().Params->Add(param1);
            }
            {
                Auto<CParam> param2 (new CParam());
                param2->Type.SetPrimType(E_PRIMTYPE_INT);
                nMethod->Signature().Params->Add(param2);
            }
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // ****************************
        //    String::startsWith #14
        // ****************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("startsWith"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(stringClassName);
                param1->Type.ResolvedClass = stringClass;
                nMethod->Signature().Params->Add(param1);
            }
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *************************
        //    String::endsWith #15
        // *************************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("endsWith"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(stringClassName);
                param1->Type.ResolvedClass = stringClass;
                nMethod->Signature().Params->Add(param1);
            }
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // **********************
        //    String::trim #16
        // **********************

        {
            Auto<CMethod> nMethod (new CMethod(stringClass));
            nMethod->SetName(this->NewSlice("trim"));
            nMethod->Signature().ReturnType.SetObject(stringClassName);
            nMethod->Signature().ReturnType.ResolvedClass = stringClass;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            stringClass->RegisterInstanceMethod(nMethod);
        }

        // *******************

        RegisterClass(stringClass);
        m_stringClass = stringClass;
    }
}

// TODO init each built-in class in its own method
void CDomain::initBasicClasses()
{
    initStringClass();
    initRangeStruct();
    initPredicateClass();
    initRangeLooperClass();
    initActionClass();
    initErrorClass();

    // **********
    //    int
    // **********

    {
        Auto<CClass> intClass (new CClass(this));
        SStringSlice intClassName (this->NewSlice("int"));
        m_primKlassMap.Set((int)E_PRIMTYPE_INT, intClass);
        intClass->SetPrimitiveType(E_PRIMTYPE_INT);
        intClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
        intClass->SetFlatName(intClassName);
        intClass->DefICall(this->NewSlice("op_add"), "ii", true);
        intClass->DefICall(this->NewSlice("op_subtract"), "ii", true);
        intClass->DefICall(this->NewSlice("op_multiply"), "ii", true);
        intClass->DefICall(this->NewSlice("op_divide"), "ii", true);
        intClass->DefICall(this->NewSlice("op_modulo"), "ii", true);
        intClass->DefICall(this->NewSlice("op_and"), "ii", true);
        intClass->DefICall(this->NewSlice("op_or"), "ii", true);
        intClass->DefICall(this->NewSlice("op_equals"), "bi", true);
        intClass->DefICall(this->NewSlice("op_greaterThan"), "bi", true);
        intClass->DefICall(this->NewSlice("op_lessThan"), "bi", true);
        intClass->DefICall(this->NewSlice("toFloat"), "f", true);

        // ******************
        //    int::equals
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(intClass));
            nMethod->SetName(this->NewSlice("equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("any"));
                nMethod->Signature().Params->Add(param1);
            }
            intClass->RegisterInstanceMethod(nMethod);
        }

        // ******************
        //    int::toString
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(intClass));
            nMethod->SetName(this->NewSlice("toString"));
            nMethod->Signature().ReturnType = m_stringClass->ToTypeRef();
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            intClass->RegisterInstanceMethod(nMethod);
        }

        // ******************
        //    int::hashCode
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(intClass));
            nMethod->SetName(this->NewSlice("hashCode"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            intClass->RegisterInstanceMethod(nMethod);
        }

        // ***************
        //    int::to
        // ***************

        {
            Auto<CMethod> nMethod (new CMethod(intClass));
            nMethod->SetName(this->NewSlice("to"));
            nMethod->Signature().ReturnType.SetObject(this->NewSlice("Range"));
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            nMethod->Flags() |= E_METHODFLAGS_FORCE_NO_HEADER;
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetPrimType(E_PRIMTYPE_INT);
                nMethod->Signature().Params->Add(param1);
            }
            intClass->RegisterInstanceMethod(nMethod);
        }

        // ***************
        //    int::upto
        // ***************

        {
            Auto<CMethod> nMethod (new CMethod(intClass));
            nMethod->SetName(this->NewSlice("upto"));
            nMethod->Signature().ReturnType.SetObject(this->NewSlice("Range"));
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            nMethod->Flags() |= E_METHODFLAGS_FORCE_NO_HEADER;
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetPrimType(E_PRIMTYPE_INT);
                nMethod->Signature().Params->Add(param1);
            }
            intClass->RegisterInstanceMethod(nMethod);
        }

        this->RegisterClass(intClass);
    }

    // ***********
    //    float
    // ***********

    {
        Auto<CClass> floatClass (new CClass(this));
        SStringSlice floatClassName (this->NewSlice("float"));
        m_primKlassMap.Set((int)E_PRIMTYPE_FLOAT, floatClass);
        floatClass->SetPrimitiveType(E_PRIMTYPE_FLOAT);
        floatClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
        floatClass->SetFlatName(floatClassName);
        floatClass->DefICall(this->NewSlice("op_add"), "ff", true);
        floatClass->DefICall(this->NewSlice("op_subtract"), "ff", true);
        floatClass->DefICall(this->NewSlice("op_multiply"), "ff", true);
        floatClass->DefICall(this->NewSlice("op_divide"), "ff", true);
        floatClass->DefICall(this->NewSlice("op_greaterThan"), "bf", true);
        floatClass->DefICall(this->NewSlice("op_lessThan"), "bf", true);
        floatClass->DefICall(this->NewSlice("toInt"), "i", true);
        floatClass->DefICall(this->NewSlice("op_equals"), "bf", true);

        // ******************
        //    float::equals
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(floatClass));
            nMethod->SetName(this->NewSlice("equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("any"));
                nMethod->Signature().Params->Add(param1);
            }
            floatClass->RegisterInstanceMethod(nMethod);
        }

        // ********************
        //    float::toString
        // ********************

        {
            Auto<CMethod> anotherMethod (new CMethod(floatClass));
            anotherMethod->SetName(this->NewSlice("toString"));
            anotherMethod->Signature().ReturnType = m_stringClass->ToTypeRef();
            anotherMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            floatClass->RegisterInstanceMethod(anotherMethod);
        }

        // *******************
        //   float::hashCode
        // *******************

        {
            Auto<CMethod> nMethod (new CMethod(floatClass));
            nMethod->SetName(this->NewSlice("hashCode"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            floatClass->RegisterInstanceMethod(nMethod);
        }

        this->RegisterClass(floatClass);
    }

    // **************************************************
    //    void
    //
    // An actual struct which simply lacks any fields.
    // Theoretically embeddable into T? and other things.
    // **************************************************

    {
        Auto<CClass> voidClass (new CClass(this));
        SStringSlice voidClassName (this->NewSlice("void"));
        m_primKlassMap.Set((int)E_PRIMTYPE_VOID, voidClass);
        voidClass->SetPrimitiveType(E_PRIMTYPE_VOID);
        voidClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
        voidClass->SetFlatName(voidClassName);
        this->RegisterClass(voidClass);
    }

    // **********
    //    bool
    // **********

    {
        Auto<CClass> boolClass (new CClass(this));
        m_boolClass = boolClass;
        SStringSlice boolClassName (this->NewSlice("bool"));
        m_primKlassMap.Set((int)E_PRIMTYPE_BOOL, boolClass);
        boolClass->SetPrimitiveType(E_PRIMTYPE_BOOL);
        boolClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
        boolClass->SetFlatName(boolClassName);
        boolClass->DefICall(this->NewSlice("or"), "bb", true);
        boolClass->DefICall(this->NewSlice("and"), "bb", true);
        boolClass->DefICall(this->NewSlice("op_equals"), "bb", true);
        boolClass->DefICall(this->NewSlice("not"), "b", true);

        // *******************
        //    bool::toString
        // *******************

        {
            Auto<CMethod> nMethod (new CMethod(boolClass));
            nMethod->SetName(this->NewSlice("toString"));
            nMethod->Signature().ReturnType = m_stringClass->ToTypeRef();
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            boolClass->RegisterInstanceMethod(nMethod);
        }

        // *****************
        //    bool::equals
        // *****************

        {
            Auto<CMethod> nMethod (new CMethod(boolClass));
            nMethod->SetName(this->NewSlice("equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("any"));
                nMethod->Signature().Params->Add(param1);
            }
            boolClass->RegisterInstanceMethod(nMethod);
        }

        // ******************
        //   bool::hashCode
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(boolClass));
            nMethod->SetName(this->NewSlice("hashCode"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            boolClass->RegisterInstanceMethod(nMethod);
        }

        // ****************
        //    bool::then
        // ****************

        {
            Auto<CMethod> nMethod (new CMethod(boolClass));
            nMethod->SetName(this->NewSlice("then"));
            // Returns the same value so that the construction could be chained:
            // ((a == a) then ^{ /* do something */ }) else ^{ /* do something */ };
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("Action"));
                nMethod->Signature().Params->Add(param1);
            }
            boolClass->RegisterInstanceMethod(nMethod);
        }

        // ****************
        //    bool::else
        // ****************

        {
            Auto<CMethod> nMethod (new CMethod(boolClass));
            nMethod->SetName(this->NewSlice("else"));
            // Returns an inverted value.
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("Action"));
                nMethod->Signature().Params->Add(param1);
            }
            boolClass->RegisterInstanceMethod(nMethod);
        }

        // ****************
        //    bool::while
        // ****************

        {
            Auto<CMethod> nMethod (new CMethod(boolClass));
            nMethod->SetName(this->NewSlice("while"));
            nMethod->Signature().IsStatic = true;
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("Predicate"));
                nMethod->Signature().Params->Add(param1);

                Auto<CParam> param2 (new CParam());
                param2->Type.SetObject(this->NewSlice("Action"));
                nMethod->Signature().Params->Add(param2);
            }
            boolClass->RegisterStaticMethod(nMethod);
        }

        this->RegisterClass(boolClass);
    }

    // **********
    //    char
    // **********

    {
        Auto<CClass> charClass (new CClass(this));
        m_charClass = charClass;
        SStringSlice charClassName (this->NewSlice("char"));
        m_primKlassMap.Set((int)E_PRIMTYPE_CHAR, charClass);
        charClass->SetPrimitiveType(E_PRIMTYPE_CHAR);
        charClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
        charClass->SetFlatName(charClassName);
        charClass->DefICall(this->NewSlice("op_equals"), "bc", true);

        // ********************
        //    char::toString
        // ********************

        {
            Auto<CMethod> nMethod (new CMethod(charClass));
            nMethod->SetName(this->NewSlice("toString"));
            nMethod->Signature().ReturnType = m_stringClass->ToTypeRef();
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            charClass->RegisterInstanceMethod(nMethod);
        }

        // *****************
        //    char::toInt
        // *****************

        /*{
            Auto<CMethod> nMethod (new CMethod(charClass));
            nMethod->SetName(this->NewSlice("toInt"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->m_isNative = true;
            charClass->RegisterInstanceMethod(nMethod);
        }*/

        // ******************
        //    char::equals
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(charClass));
            nMethod->SetName(this->NewSlice("equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("any"));
                nMethod->Signature().Params->Add(param1);
            }
            charClass->RegisterInstanceMethod(nMethod);
        }

        // ******************
        //   char::hashCode
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(charClass));
            nMethod->SetName(this->NewSlice("hashCode"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            charClass->RegisterInstanceMethod(nMethod);
        }

        this->RegisterClass(charClass);
    }

    // **********
    //   intptr
    // **********

    {
        Auto<CClass> intptrClass (new CClass(this));
        SStringSlice intptrClassName (this->NewSlice("intptr"));
        m_primKlassMap.Set((int)E_PRIMTYPE_INTPTR, intptrClass);
        intptrClass->SetPrimitiveType(E_PRIMTYPE_INTPTR);
        intptrClass->Flags() |= E_CLASSFLAGS_IS_VALUETYPE;
        intptrClass->SetFlatName(intptrClassName);

        intptrClass->DefICall(this->NewSlice("op_equals"), "bp", true);

        // ******************
        //   intptr::equals
        // ******************

        {
            Auto<CMethod> nMethod (new CMethod(intptrClass));
            nMethod->SetName(this->NewSlice("equals"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_BOOL);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            {
                Auto<CParam> param1 (new CParam());
                param1->Type.SetObject(this->NewSlice("any"));
                nMethod->Signature().Params->Add(param1);
            }
            intptrClass->RegisterInstanceMethod(nMethod);
        }

        // ********************
        //   intptr::hashCode
        // ********************

        {
            Auto<CMethod> nMethod (new CMethod(intptrClass));
            nMethod->SetName(this->NewSlice("hashCode"));
            nMethod->Signature().ReturnType.SetPrimType(E_PRIMTYPE_INT);
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            intptrClass->RegisterInstanceMethod(nMethod);
        }

        // **********************
        //    intptr::toString
        // **********************

        {
            Auto<CMethod> nMethod (new CMethod(intptrClass));
            nMethod->SetName(this->NewSlice("toString"));
            nMethod->Signature().ReturnType = m_stringClass->ToTypeRef();
            nMethod->SetSpecialMethod(E_SPECIALMETHOD_NATIVE);
            intptrClass->RegisterInstanceMethod(nMethod);
        }

        this->RegisterClass(intptrClass);
    }

    // *******************
    //    any interface
    // *******************

    {
        Auto<CClass> anyClass (new CClass(this));
        anyClass->SetPrimitiveType(E_PRIMTYPE_OBJECT);
        anyClass->Flags() |= E_CLASSFLAGS_IS_ABSTRACT;
        anyClass->SetSpecialClass(E_SPECIALCLASS_INTERFACE);
        anyClass->SetFlatName(this->NewSlice("any"));
        this->RegisterClass(anyClass);
    }
}

} }

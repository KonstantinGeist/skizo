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

#ifndef APPLICATIONOPTIONS_H_INCLUDED
#define APPLICATIONOPTIONS_H_INCLUDED

#include "ArrayList.h"

namespace skizo { namespace core {

/**
 * An object which describes a command-line option: its name, its default value
 * and the description. Passed to CApplicationOptions::GetOptions(..) and
 * CApplicationOptions::GetEmpty(..)
 */
class CApplicationOptionDescription final: public skizo::core::CObject
{
    friend class CApplicationOptions;

public:
    /**
     * The default value can be null.
     */
    CApplicationOptionDescription(const char* name, const char* description, const char* defaultValue);

protected: // internal
    Auto<const CString> m_name;
    Auto<const CString> m_description;
    Auto<const CString> m_defaultValue;
};

/**
 * A CApplicationOptions object represents an application's command-line arguments.
 *
 * Only Microsoft-style options are supported (no GNU), for example:
 * "myprogram.exe /option1:123 /option2:namedEnum"
 * It's simply a collection of mappings key=>value.
 *
 * @note Bool options can be of two sorts: implicit: "/doSomething" defaults to
 * the result provided in CApplicationOptionDescription; or explicit:
 * "/doSomething:true" or "/doSomething:false"
 *
 * @note Values support double quotes (allows to insert spaces).
 */
class CApplicationOptions final: public skizo::core::CObject
{
public:
    virtual ~CApplicationOptions();

    /**
     * Creates an options object by invoking Application::GetCommandLineArgs()
     * and parsing the string it returns.
     *
     * @param descrs Option descriptions. NOTE The array is copied.
     * @param injectedOptions Optional value for testing: uses the string instead
     * of Application::GetCommandLineArgs() to process options.
     *
     * @throws EC_BAD_FORMAT if the options are ill-formed.
     * @throws EC_KEY_NOT_FOUND Non-existing option is mentioned.
     */
    static CApplicationOptions* GetOptions(const skizo::collections::CArrayList<CApplicationOptionDescription*>* descrs,
                                           const CString* injectedOptions = nullptr);

    /**
     * Returns an empty application options object. Useful when it's required to
     * force the application to ignore any passed arguments (if it's called in
     * the context of a VM as a DLL, for example).
     */
    static CApplicationOptions* GetEmpty(const skizo::collections::CArrayList<CApplicationOptionDescription*>* descrs);

    /**
     * Treats an option's value as a string.
     *
     * @note Returns NULL if no such option was specified.
     */
    const CString* GetStringOption(const char* optionName) const;

    /**
     * Treats an option's value as an integer.
     *
     * @note Returns -1 if the option was not specified.
     * @throw EC_BAD_FORMAT
     */
    int GetIntOption(const char* optionName) const;

    /**
     * Treats an option's value as a float.
     *
     * @note Returns -1 if the option was not specified.
     * @throw EC_BAD_FORMAT
     */
    float GetFloatOption(const char* optionName) const;

    /**
     * Treats an option's value as a bool. Parsable values are "true" and "false".
     *
     * @note Returns false if the option was not specified.
     * @throw EC_BAD_FORMAT
     */
    bool GetBoolOption(const char* optionName) const;

    /**
     * Returns the number of options.
     */
    int Size() const;

    /**
     * Enumerates the possible options by printing them to the console.
     */
    void PrintHelp() const;

private:
    struct ApplicationOptionsPrivate* p;

    CApplicationOptions();
};

} }

#endif // APPLICATIONOPTIONS_H_INCLUDED

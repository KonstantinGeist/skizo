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

#include "ApplicationOptions.h"
#include "Application.h"
#include "Contract.h"
#include "OrderedHashMap.h"
#include "String.h"

// **********************
//   TODO buggy parsing
// **********************

namespace skizo { namespace core {
using namespace skizo::core;
using namespace skizo::collections;

struct ApplicationOptionsPrivate
{
    Auto<CHashMap<const CString*, const CString*> > m_backingMap;
    Auto<COrderedHashMap<const CString*, CApplicationOptionDescription*> > m_descrs;

    ApplicationOptionsPrivate()
        : m_backingMap(new CHashMap<const CString*, const CString*>()),
          m_descrs(new COrderedHashMap<const CString*, CApplicationOptionDescription*>())
    {
    }
};

CApplicationOptionDescription::CApplicationOptionDescription(const char* name,
                                                             const char* description,
                                                             const char* defaultValue)
{
    // Pre-condition.
    SKIZO_REQ_PTR(name);
    SKIZO_REQ_PTR(description);
    // DefaultValue can be null.

    m_name.SetPtr(CString::FromUtf8(name));
    m_description.SetPtr(CString::FromUtf8(description));
    if(defaultValue) {
        m_defaultValue.SetPtr(CString::FromUtf8(defaultValue));
    }
}

CApplicationOptions::CApplicationOptions()
    : p (new ApplicationOptionsPrivate())
{
}

CApplicationOptions::~CApplicationOptions()
{
    delete p;
}

// substrings removing quotes if any
static const CString* appOptionsSubstring(const CString* cmd, int start, int length)
{
    Auto<const CString> trimmed (cmd->Trim());
    SKIZO_REQ_RANGE_D(start, 0, trimmed->Length());

    if(length > 1 && trimmed->Chars()[start] == SKIZO_CHAR('"')) {
        return trimmed->Substring(start + 1, length - 2);
    } else {
        Auto<const CString> substr (trimmed->Substring(start, length));
        return substr->Trim();
    }
}

// TODO support "target" which is a nameless option
CApplicationOptions* CApplicationOptions::GetOptions(const CArrayList<CApplicationOptionDescription*>* descrs,
                                                     const CString* injectedOptions)
{
    SKIZO_REQ_POS(descrs->Count());

    Auto<CApplicationOptions> r (new CApplicationOptions());
    for(int i = 0; i < descrs->Count(); i++) {
        CApplicationOptionDescription* desc = descrs->Array()[i];
        r->p->m_descrs->Set(desc->m_name, desc);
    }

    Auto<const CString> cmd;
    if(injectedOptions) {
        cmd.SetVal(injectedOptions);
    } else {
        cmd.SetPtr(Application::GetCommandLineArgs());
    }

    // TODO not secured from space-based attacks if the shell removes quotes.
    bool quote = false;
    int lastStart = 0;
    int lastColon = -1;
    const so_char16* const cs = cmd->Chars();

    for(int i = 0; i < cmd->Length() + 1; i++) {
        const so_char16 c = i < cmd->Length()? cs[i]: SKIZO_CHAR('/');
        if(c == SKIZO_CHAR('"')) {
            quote = !quote;
        } else {
            if(!quote) {
                if(c == SKIZO_CHAR('/')
                    && (i == 0 || cs[i - 1] == SKIZO_CHAR(' ') || i == cmd->Length())) {
                    // ^^ FIX If a value contains a normalized path with "/" in it,
                    // to tell it from / as a marker of a new key, we relied on
                    // quotes, but, as it turns out, many shells remove them. The
                    // solution is to consider / as a marker of a new key only
                    // if it's either a) the first character b) after a space
                    // c) the last character.

                    if(i - lastStart != 0) {
                        Auto<const CString> key;
                        Auto<const CString> value;
                        if(lastColon > lastStart) {
                            if(lastColon - lastStart - 1 < 1) {
                                SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Option name expected.");
                            }
                            if(i - lastColon - 1 < 1) {
                                SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Option value expected.");
                            }

                            key.SetPtr(appOptionsSubstring(cmd, lastStart + 1, lastColon - lastStart - 1));
                            value.SetPtr(appOptionsSubstring(cmd, lastColon + 1, i - lastColon - 1));
                        } else {
                            if(i - lastStart - 1 < 1) {
                                SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Option name expected.");
                            }

                            key.SetPtr(appOptionsSubstring(cmd, lastStart + 1, i - lastStart - 1));
                        }

                        if(!r->p->m_descrs->Contains(key)) {
                            SKIZO_THROW_WITH_MSG(EC_KEY_NOT_FOUND, "Unrecognized option."); // TODO specify which one.
                        }
                        r->p->m_backingMap->Set(key, value);
                    }

                    lastStart = i;
                    
                } else if(c == SKIZO_CHAR(':')) {
                    
                    if(i - lastStart - 1 < 1) {
                        SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Option name expected.");
                    }

                    lastColon = i;
                    
                }
            }
        }
    }

    if(quote) {
        SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Quote expected.");
    }

    r->Ref();
    return r;
}

CApplicationOptions* CApplicationOptions::GetEmpty(const CArrayList<CApplicationOptionDescription*>* descrs)
{
    SKIZO_REQ_POS(descrs->Count());

    CApplicationOptions* r (new CApplicationOptions());
    for(int i = 0; i < descrs->Count(); i++) {
        CApplicationOptionDescription* const desc = descrs->Array()[i];
        r->p->m_descrs->Set(desc->m_name, desc);
    }

    return r;
}

const CString* CApplicationOptions::GetStringOption(const char* _optionName) const
{
    Auto<const CString> optionName (CString::FromUtf8(_optionName));
    const CString* raw = nullptr;

    if(p->m_backingMap->TryGet(optionName, &raw)) {
        // Nothing.
    } else {
        CApplicationOptionDescription* descr;
        if(p->m_descrs->TryGet(optionName, &descr)) {
            descr->Unref();

            raw = descr->m_defaultValue;
            if(!raw) {
                return nullptr;
            }

            raw->Ref();
        } else {
            SKIZO_THROW_WITH_MSG(EC_KEY_NOT_FOUND, "Unrecognized option."); // TODO specify which
        }
    }

    return raw;
}

bool CApplicationOptions::GetBoolOption(const char* _optionName) const
{
    Auto<const CString> raw (GetStringOption(_optionName));
    if(!raw) {
        return true;
    }
    bool b;
    if(!raw->TryParseBool(&b)) {
        SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Boolean value expected (ApplicationOptions::GetBoolOption).");
    }
    return b;
}

int CApplicationOptions::GetIntOption(const char* _optionName) const
{
    Auto<const CString> raw (GetStringOption(_optionName));
    if(!raw) {
        return -1;
    }
    int i;
    if(!raw->TryParseInt(&i)) {
        SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Integer value expected (ApplicationOptions::GetIntOption).");
    }
    return i;
}

float CApplicationOptions::GetFloatOption(const char* _optionName) const
{
    Auto<const CString> raw (GetStringOption(_optionName));
    if(!raw) {
        return -1;
    }
    float f;
    if(!raw->TryParseFloat(&f)) {
        SKIZO_THROW_WITH_MSG(EC_BAD_FORMAT, "Float value expected (ApplicationOptions::GetFloatOption).");
    }
    return f;
}

int CApplicationOptions::Size() const
{
    return p->m_backingMap->Size();
}

void CApplicationOptions::PrintHelp() const
{
    SOrderedHashMapEnumerator<const CString*, CApplicationOptionDescription*> mapEnum (p->m_descrs);
    const CString* key;
    CApplicationOptionDescription* descr;
    while(mapEnum.MoveNext(&key, &descr)) {
        if(descr->m_defaultValue) {
            Auto<const CString> format (CString::Format("/%o -- %o (default: %o)\n",
                                                  static_cast<const CObject*>(descr->m_name.Ptr()),
                                                  static_cast<const CObject*>(descr->m_description.Ptr()),
                                                  static_cast<const CObject*>(descr->m_defaultValue.Ptr())));
            format->DebugPrint();
        } else {
            Auto<const CString> format (CString::Format("/%o -- %o\n",
                                                  static_cast<const CObject*>(descr->m_name.Ptr()),
                                                  static_cast<const CObject*>(descr->m_description.Ptr())));
            format->DebugPrint();
        }
    }
}

} }

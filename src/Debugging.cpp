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

#include "Abort.h"
#include "ArrayList.h"
#include "Console.h"
#include "Domain.h"
#include "HashMap.h"
#include "icall.h"
#include "ModuleDesc.h"
#include "Profiling.h"
#include "RuntimeHelpers.h"
#include "ScriptUtils.h"
#include "StringBuilder.h"

namespace skizo { namespace script {
using namespace skizo::core;
using namespace skizo::collections;

namespace
{

class CLocalDebugInfo: public CObject
{
public:
    void* ptr;
    CClass* klass; // NOTE the real underlying class of the object, not the interface
};
typedef CHashMap<const CString*, CLocalDebugInfo*> CLocalDebugInfoMap;

struct SDebuggerCommandDesc
{
    const char* Name;
    const int ArgumentCount;
    const char* Help;
    bool ShouldEndPromptLoop;

    SDebuggerCommandDesc(const char* name, int argumentCount, const char* help, bool shouldEndPromptLoop = false)
        : Name(name),
          ArgumentCount(argumentCount),
          Help(help),
          ShouldEndPromptLoop(shouldEndPromptLoop)
    {
    }
};

struct SDebuggerCommandContext
{
    const CDomain* Domain;
    const CArrayList<const CString*>* CmdArgs;
    const CLocalDebugInfoMap* DebugInfoMap;
    const class CDebuggerCommandList* CommandList;

    SDebuggerCommandContext(const CDomain* domain, const CArrayList<const CString*>* cmdArgs, const CLocalDebugInfoMap* debugInfoMap, const CDebuggerCommandList* cmdList)
        : Domain(domain),
          CmdArgs(cmdArgs),
          DebugInfoMap(debugInfoMap),
          CommandList(cmdList)
    {
    }
};

class CDebuggerCommand: public CObject
{
public:
    virtual SDebuggerCommandDesc GetDesc() const = 0;
    virtual const CString* Process(const SDebuggerCommandContext& context) const = 0;
};

class CDebuggerCommandList: public CObject
{
public:
    CDebuggerCommandList()
        : m_commands(new CArrayList<CDebuggerCommand*>())
    {
    }

    template <class T>
    void AddCommand()
    {
        Auto<T> cmd (new T());
        m_commands->Add(cmd);
    }

    CDebuggerCommand* MatchedCommand(const CArrayList<const CString*>* cmdArgs) const
    {
        const int argCount = cmdArgs->Count();
        if(argCount == 0) {
            return nullptr;
        }

        const CString* head = cmdArgs->Array()[0];

        for(int i = 0; i < m_commands->Count(); i++) {
            CDebuggerCommand* candidate = m_commands->Array()[i];
            const SDebuggerCommandDesc desc = candidate->GetDesc();

            if((argCount - 1) == desc.ArgumentCount // argCount without the head
            && head->EqualsASCII(desc.Name))
            {
                return candidate;
            }
        }

        return nullptr;
    }

    void PrintHelp() const
    {
        Auto<CStringBuilder> sb (new CStringBuilder());

        for(int i = 0; i < m_commands->Count(); i++) {
            const SDebuggerCommandDesc desc = m_commands->Array()[i]->GetDesc();

            Auto<const CString> name (CString::FromASCII(desc.Name));
            Auto<const CString> help (CString::FromASCII(desc.Help));

            const int tabIndex = help->FindChar(SKIZO_CHAR('\t'));

            sb->Append(name);
            sb->Append(SKIZO_CHAR(' '));
            if(tabIndex != -1) {
                sb->Append(help, 0, tabIndex);
            }

            constexpr int MAX_NAME = 40;
            const int padCount = MAX_NAME - (name->Length() + 1 + (tabIndex > 0? tabIndex: 0));
            for(int i = 0; i < padCount; i++) {
                sb->Append(SKIZO_CHAR(' '));
            }

            sb->Append(help, tabIndex + 1);
            sb->AppendLine();
        }

        Auto<const CString> str (sb->ToString());
        Console::WriteLine(str);
    }

private:
    Auto<CArrayList<CDebuggerCommand*>> m_commands;
};

class CContinueCommand: public CDebuggerCommand
{
public:
    virtual SDebuggerCommandDesc GetDesc() const override
    {
        return SDebuggerCommandDesc("cont", 0, "continue (exit this breakpoint)", true); // shouldEndPromptLoop=true
    }

    virtual const CString* Process(const SDebuggerCommandContext& /*context*/) const override
    {
        return nullptr;
    }
};

class CShowInstancePropertyCommand: public CDebuggerCommand
{
public:
    virtual SDebuggerCommandDesc GetDesc() const override
    {
        return SDebuggerCommandDesc("show-inst-prop", 2, "$local$ $prop$\tshow instance property");
    }

    virtual const CString* Process(const SDebuggerCommandContext& context) const override;
};

class CListInstancePropertiesCommand: public CDebuggerCommand
{
public:
    virtual SDebuggerCommandDesc GetDesc() const override
    {
        return SDebuggerCommandDesc("list-inst-props", 1, "$local$\tlist instance properties");
    }

    virtual const CString* Process(const SDebuggerCommandContext& context) const override;
};

class CHelpCommand: public CDebuggerCommand
{
public:
    virtual SDebuggerCommandDesc GetDesc() const override
    {
        return SDebuggerCommandDesc("help", 0, "print this help information");
    }

    virtual const CString* Process(const SDebuggerCommandContext& context) const override;
};

}

static const CString* getStringRepr_failable(const void* obj, const CClass* objClass, const CDomain* domain)
{
    auto failable = (const SFailableHeader*)obj;

    if(failable->error) {
        const SErrorHeader* errorHeader = (SErrorHeader*)failable->error;
        CClass* errorClass = so_class_of(failable->error);

        return CString::Format("<%o: \"%o\">", (const CObject*)errorClass->NiceName(), errorHeader->message? (const CObject*)errorHeader->message->pStr: nullptr);
    } else {
        const CClass* wrappedClass = objClass->ResolvedWrappedClass();
        SKIZO_REQ_PTR(wrappedClass);

        if(wrappedClass->IsRefType()) {
            return domain->GetStringRepresentation(failable->refData, wrappedClass);
        } else {
            return domain->GetStringRepresentation(&failable->valData, wrappedClass);
        }
    }
}

static const CString* getStringRepr_array(const void* obj, const CClass* objClass, const CDomain* domain)
{
    // Arrays are represented like: [item1 item2 item3]
    const SArrayHeader* array = (SArrayHeader*)obj;
    const CClass* wrappedClass = objClass->ResolvedWrappedClass();
    SKIZO_REQ_PTR(wrappedClass);
    const size_t itemSize = wrappedClass->GCInfo().SizeForUse;

    Auto<CStringBuilder> sb (new CStringBuilder());
    sb->Append("[");
    for(int i = 0; i < array->length; i++) {
        const void* pStart = &array->firstItem + itemSize * i;

        Auto<const CString> strElement;
        if(wrappedClass->IsRefType()) {
            strElement.SetPtr(domain->GetStringRepresentation(*((const void**)pStart), wrappedClass));
        } else {
            strElement.SetPtr(domain->GetStringRepresentation(pStart, wrappedClass));
        }
        sb->Append(strElement);

        if(i < array->length - 1) {
            sb->Append(" ");
        }
    }
    sb->Append("]");
    return sb->ToString();
}

static const CString* getStringRepr_map(const void* obj, const CClass* objClass, const CDomain* domain)
{
    const SkizoMapObject* mapObj = ((SMapHeader*)obj)->mapObj;

    Auto<CStringBuilder> sb (new CStringBuilder());
    sb->Append("{");

    SHashMapEnumerator<SkizoMapObjectKey, void*> mapEnum (mapObj->BackingMap);
    SkizoMapObjectKey key;
    void* val;
    int sz = mapObj->BackingMap->Size();
    while(mapEnum.MoveNext(&key, &val)) {
        sz--;

        Auto<const CString> keyRepr (domain->GetStringRepresentation(key.Key, nullptr));
        Auto<const CString> valueRepr (domain->GetStringRepresentation(val, nullptr));
        keyRepr.SetPtr(ScriptUtils::UnescapeString(keyRepr));
        valueRepr.SetPtr(ScriptUtils::UnescapeString(valueRepr));

        sb->Append(SKIZO_CHAR('('));
        sb->Append(keyRepr);
        sb->Append(SKIZO_CHAR(' '));
        sb->Append(valueRepr);
        sb->Append(SKIZO_CHAR(')'));

        if(sz) {
            sb->Append(" ");
        }
    }

    sb->Append("}");
    return sb->ToString();
}

const CString* CDomain::GetStringRepresentation(const void* obj, const CClass* objClass) const
{
    if(!obj) {
        return CString::FromUtf8("null");
    }

    if(objClass) {
        // Types of reference objects can be specialized.
        if(objClass->IsRefType()) {
            objClass = so_class_of(obj);
        }
    } else {
        objClass = so_class_of(obj);
    }

    const CDomain* domain = objClass->DeclaringDomain();

    // ***************************************************
    //   Special cases for strings & failables & arrays.
    // ***************************************************

    switch(objClass->SpecialClass()) {
        case E_SPECIALCLASS_FAILABLE: return getStringRepr_failable(obj, objClass, this);
        case E_SPECIALCLASS_ARRAY: return getStringRepr_array(obj, objClass, this);
        default: break;
    }

    if(objClass == domain->m_stringClass) {
        // The string class is treated specially: we add quote marks.
        return CString::Format("\"%o\"", (const CObject*)((SStringHeader*)obj)->pStr);
    } else if(objClass == domain->m_memMngr.MapClass()) {
        return getStringRepr_map(obj, objClass, this);
    }

    // **************************************
    //   Special cases for primitive types.
    // **************************************

    switch(objClass->PrimitiveType()) {
        case E_PRIMTYPE_INT:
            return CoreUtils::IntToString(*((int*)obj));
        case E_PRIMTYPE_FLOAT:
            return CoreUtils::FloatToString(*((float*)obj));
        case E_PRIMTYPE_BOOL:
            return CoreUtils::BoolToString(*((_so_bool*)obj));
        case E_PRIMTYPE_CHAR:
        {
            const so_char16 ch = (so_char16) *((_so_char*)obj);
            const so_char16 buf[4] = { SKIZO_CHAR('\''), ch, SKIZO_CHAR('\''), 0 };
            return CString::FromUtf16(buf);
        }
        case E_PRIMTYPE_INTPTR:
            return CoreUtils::PtrToString(*((void**)obj));
        case E_PRIMTYPE_OBJECT:

            // *********************************************************************************************
            // NOTE TODO Non-primitive valuetypes not supported yet, as GCC<=>TCC valuetype argument passing
            // is fragile.
            if(objClass->IsValueType()) {
                return CString::Format("<val-obj at %p>", obj);
            }
            // *********************************************************************************************

            // Otherwise skipped.
            break;
        default:
            SKIZO_REQ_NEVER
            break;
    }

    // **************************************

    const CString* retValue = nullptr;

    // Emulating reflection...
    const SStringSlice toStringSlice (domain->NewSlice("toString"));
    CMethod* toStringMethod = nullptr;
    if(objClass->TryGetInstanceMethodByName(toStringSlice, &toStringMethod)) {
        toStringMethod->Unref();
    }

    void* pToString = nullptr;
    if(toStringMethod) {
        pToString = domain->GetFunctionPointer(toStringMethod);
    }

    if(pToString
    && toStringMethod->ECallDesc().CallConv == E_CALLCONV_CDECL
    && toStringMethod->Signature().Params->Count() == 0
    && toStringMethod->Signature().ReturnType.ResolvedClass == domain->m_stringClass)
    {
        typedef SStringHeader* (SKIZO_API *FToStringMethod)(const void* self);

        SStringHeader* repr = nullptr;

        // NOTE Catches Skizo exceptions, we don't want them to propagate.
        try {
            repr = ((FToStringMethod)pToString)(obj);
        } catch (const SoDomainAbortException& soErr) {
            retValue = CString::Format("<Error: \"%s\">", soErr.Message);
        }

        if(!retValue) {
            if(repr) {
                retValue = CString::Format("<%o>", (const CObject*)repr->pStr);
            } else {
                retValue = CString::FromUtf8("<null>");
            }
        }

    } else {

        // Can't find "toString": print the pointer.
        retValue = CString::Format("<ref-obj at %p>", obj);
    }

    return retValue;
}

void* CDomain::getPropertyImpl(void* obj, const char* propName, const STypeRef& targetType) const
{
    CClass* pClass = so_class_of(obj);

    CMethod* pFoundMethod = nullptr;
    const CArrayList<CMethod*>* instanceMethods = pClass->InstanceMethods();
    for(int i = 0; i < instanceMethods->Count(); i++) {
        CMethod* pMethod = instanceMethods->Array()[i];

        if(pMethod->Name().EqualsAscii(propName)) {
            pFoundMethod = pMethod;
            break;
        }
    }

    if(!pFoundMethod || (pFoundMethod->Signature().Params->Count() != 0)) {
        ScriptUtils::Fail_(this->FormatMessage("Property '%S' not found.", propName), 0, 0);
    }
    if(!pFoundMethod->Signature().ReturnType.Equals(targetType)) {
        ScriptUtils::Fail_("Property type mismatch.", 0, 0); // TODO
    }

    Utf8Auto cName (pFoundMethod->GetCName());
    void* ptr;
    SKIZO_LOCK_AB(CDomain::g_globalMutex) {
        ptr = this->GetSymbol(cName);
    } SKIZO_END_LOCK_AB(CDomain::g_globalMutex);

    if(!ptr) {
        ScriptUtils::Fail_(this->FormatMessage("Property '%S' not found.", propName), 0, 0); // TODO?
    }

    return ptr;
}

void* CDomain::GetPropertyImpl(void* obj, const char* propName, const STypeRef& targetType) const
{
    return getPropertyImpl(obj, propName, targetType);
}

bool CDomain::GetBoolProperty(void* obj, const char* propName) const
{
    typedef _so_bool (SKIZO_API * _FBoolGetter)(void* self);

    STypeRef targetType;
    targetType.SetPrimType(E_PRIMTYPE_BOOL);

    _FBoolGetter impl = (_FBoolGetter)getPropertyImpl(obj, propName, targetType);
    return impl(obj);
}

float CDomain::GetFloatProperty(void* obj, const char* propName) const
{
    typedef float (SKIZO_API * _FFloatGetter)(void* self);

    STypeRef targetType;
    targetType.SetPrimType(E_PRIMTYPE_FLOAT);

    _FFloatGetter impl = (_FFloatGetter)getPropertyImpl(obj, propName, targetType);
    return impl(obj);
}

void* CDomain::GetIntptrProperty(void* obj, const char* propName) const
{
    typedef void* (SKIZO_API * _FIntPtrGetter)(void* self);

    STypeRef targetType;
    targetType.SetPrimType(E_PRIMTYPE_INTPTR);

    _FIntPtrGetter impl = (_FIntPtrGetter)getPropertyImpl(obj, propName, targetType);
    return impl(obj);
}

const CString* CDomain::StringProperty(void* obj, const char* propName) const
{
    typedef void* (SKIZO_API * _FStringGetter)(void* self);

    STypeRef targetType;
    targetType.SetObject(this->NewSlice("string"));

    _FStringGetter impl = (_FStringGetter)getPropertyImpl(obj, propName, targetType);

    void* r = impl(obj);
    if(r) {
        return so_string_of(r);
    } else {
        return nullptr;
    }
}

CProfilingInfo* CDomain::GetProfilingInfo() const
{
    Auto<CProfilingInfo> prfInfo (new CProfilingInfo(this));
    prfInfo->m_totalTime = m_time;

    for(int i = 0; i < m_klasses->Count(); i++) {
        CClass* klass = m_klasses->Array()[i];
        if(klass->PrimitiveType() != E_PRIMTYPE_OBJECT) {
            continue;
        }

        SHashMapEnumerator<SStringSlice, CMember*> mapEnum (klass->GetNameSetEnumerator());
        CMember* member;
        while(mapEnum.MoveNext(nullptr, &member)) {
            if(member->MemberKind() == E_RUNTIMEOBJECTKIND_METHOD) {
                CMethod* method = static_cast<CMethod*>(member);

                if(method->SpecialMethod() != E_SPECIALMETHOD_NATIVE && (method->NumberOfCalls() != 0)) {
                    prfInfo->m_methods->Add(method);
                }
            }
        }
    }

    prfInfo->Ref();
    return prfInfo;
}

const CString* CDomain::GetStackTraceInfo() const
{
    // NOTE This can be used when domain creation fails.
    if(m_stackTraceEnabled || m_profilingEnabled) {
        STextBuilder textBuilder;

        for(int i = m_stackFrames->Count() - 1; i >= 0; i--) {
            const CMethod* pMethod = (CMethod*)m_stackFrames->Item(i);

            if(pMethod->Source().Module) {
                textBuilder.Emit("  at %C::%s (\"%o\":%d)\n",
                                  pMethod->DeclaringClass(),
                                 &pMethod->Name(),
                                 (const CObject*)pMethod->Source().Module->FilePath,
                                  pMethod->Source().LineNumber);
            } else {
                textBuilder.Emit(" at %C::%s (%S)\n",
                                  pMethod->DeclaringClass(),
                                 &pMethod->Name(),
                                  pMethod->DeclaringClass()->IsCompilerGenerated()? "compiler-generated": "source unknown");
            }
        }

        return textBuilder.ToString();
    } else {
        return nullptr;
    }
}

void CDomain::PrintStackTrace() const
{
    Auto<const CString> info (this->GetStackTraceInfo());
    if(info) {
        Console::WriteLine(info);
    } else {
        printf("<stack trace information not available>\n");
    }
}

static CField* instancePropertyFieldByName(const CClass* klass, const CString* name)
{
    const SStringSlice slice (name);
    CMethod* method;
    if(klass->TryGetInstanceMethodByName(slice, &method)) {
        method->Unref();
        return method->TargetField();
    } else {
        return nullptr;
    }
}

const CString* CShowInstancePropertyCommand::Process(const SDebuggerCommandContext& context) const
{
    const CArrayList<const CString*>* cmdArgs = context.CmdArgs;
    SKIZO_REQ(cmdArgs->Count() >= 3, EC_ILLEGAL_ARGUMENT);
    const CString* localName = cmdArgs->Array()[1];
    const CString* fieldName = cmdArgs->Array()[2];

    CLocalDebugInfo* debugInfo;

    if(context.DebugInfoMap->TryGet(localName, &debugInfo)) {
        debugInfo->Unref();

        const CField* field = instancePropertyFieldByName(debugInfo->klass, fieldName);
        if(field) {
            SKIZO_REQ_NOT_EQUALS(field->Offset, -1);
            const CClass* fieldType = field->Type.ResolvedClass;
            SKIZO_REQ_PTR(fieldType);

            Auto<const CString> repr;
            if(fieldType->IsRefType()) {
                repr.SetPtr(context.Domain->GetStringRepresentation(*((void**)((char*)debugInfo->ptr + field->Offset)), fieldType));
            } else {
                repr.SetPtr(context.Domain->GetStringRepresentation((char*)debugInfo->ptr + field->Offset, fieldType));
            }

            return ScriptUtils::UnescapeString(repr);
        } else {
            printf("Unrecognized instance property.\n");
        }
    } else {
        printf("Unrecognized variable.\n");
    }

    return nullptr;
}

const CString* CListInstancePropertiesCommand::Process(const SDebuggerCommandContext& context) const
{
    const CArrayList<const CString*>* cmdArgs = context.CmdArgs;
    SKIZO_REQ(cmdArgs->Count() >= 2, EC_ILLEGAL_ARGUMENT);
    const CString* localName = cmdArgs->Array()[1];

    CLocalDebugInfo* debugInfo;

    if(context.DebugInfoMap->TryGet(localName, &debugInfo)) {
        debugInfo->Unref();
        const CClass* objClass = debugInfo->klass;

        Auto<CStringBuilder> sb (new CStringBuilder());
        const CArrayList<CMethod*>* instanceMethods = objClass->InstanceMethods();
        for(int i = 0; i < instanceMethods->Count(); i++) {
            const CMethod* potentialProp = instanceMethods->Array()[i];

            if(potentialProp->TargetField()) {
                Auto<const CString> propName (potentialProp->Name().ToString());
                sb->Append(propName);
                sb->AppendLine();
            }
        }
        if(sb->Length() == 0) {
            sb->Append("<none>\n");
        }
        return sb->ToString();
    } else {
        printf("Unrecognized variable.\n");
    }

    return nullptr;
}

const CString* CHelpCommand::Process(const SDebuggerCommandContext& context) const
{
    context.CommandList->PrintHelp();
    return nullptr;
}

static void promptLoop(const CDomain* domain, const CLocalDebugInfoMap* debugInfoMap)
{
    // ****************
    //   Prompt loop.
    // *****************

    Auto<CDebuggerCommandList> cmdList (new CDebuggerCommandList());
    cmdList->AddCommand<CContinueCommand>();
    cmdList->AddCommand<CShowInstancePropertyCommand>();
    cmdList->AddCommand<CListInstancePropertiesCommand>();
    cmdList->AddCommand<CHelpCommand>();

    while(true) {
        printf("> "); // prompt
        Auto<const CString> cmd (Console::ReadLine());

        Auto<CArrayList<const CString*> > cmdArgs (cmd? cmd->Split(SKIZO_CHAR(' ')): new CArrayList<const CString*>());
        const SDebuggerCommandContext context (domain, cmdArgs, debugInfoMap, cmdList);

        const CDebuggerCommand* matchedCommand = cmdList->MatchedCommand(cmdArgs);
        if(matchedCommand) {

            Auto<const CString> result (matchedCommand->Process(context));
            if(result) {
                Console::WriteLine(result);
            }
            if(matchedCommand->GetDesc().ShouldEndPromptLoop) {
                break;
            }

        } else {
            printf("Unrecognized or ill-formed command. Print 'help' for more information.\n");
        }
    }
}

// TODO add windowed version as well
static void SKIZO_API builtin_callback_console(SKIZO_BREAKPOINTINFO* info)
{
    Auto<CStringBuilder> sb (new CStringBuilder());
    Auto<CLocalDebugInfoMap> debugInfoMap (new CLocalDebugInfoMap()); // to be passed to promptLoop(..)

    printf("\n== BP START ==\n\n");

    ((CDomain*)info->Domain)->PrintStackTrace();

    SKIZO_WATCHINFO watchInfo;
    while(SKIZONextWatch(info->WatchIterator, &watchInfo) == SKIZO_SUCCESS) {
        CClass* objClass = (CClass*)watchInfo.klass;
        void* obj = watchInfo.varPtr;
        sb->AppendFormat("%s: ", watchInfo.name);

        // A pointer to a pointer for reference types, should be dereferenced.
        // A pointer to the start of actual data, should not be touched for valuetypes.
        if(objClass->IsRefType()) {
            obj = *((void**)watchInfo.varPtr);
        }

        // ******************************************************
        // Registers the environment, to be used in promptLoop(..)
        Auto<CLocalDebugInfo> ldi (new CLocalDebugInfo());
        ldi->ptr = obj;
        // The type can be given as "any"; but the underlying type is "string", for example.
        ldi->klass = (obj && objClass->IsRefType())? so_class_of(obj): objClass;
        Auto<const CString> daName (CString::FromUtf8(watchInfo.name));
        debugInfoMap->Set(daName, ldi);
        // ******************************************************

        if(obj) {
            // prints the type
            sb->AppendFormat("%o = ", (const CObject*)objClass->NiceName());

            // prints the value
            Auto<const CString> stringRepr (objClass->DeclaringDomain()->GetStringRepresentation(obj, objClass));
            stringRepr.SetPtr(ScriptUtils::UnescapeString(stringRepr));
            sb->Append(stringRepr);
            sb->Append("\n");
        } else { // the object is null: no type, no value
            sb->Append("? = null\n");
        }
    }

    Auto<const CString> r (sb->ToString());
    // IMPORTANT Console::WriteLine(..) respects the current codepage unlike printf (under Windows).
    Console::WriteLine(r);

    promptLoop(((CDomain*)info->Domain), debugInfoMap);

    // *****************

    printf("\n== BP END ==\n");
}

void CDomain::Break()
{
   if(m_disableBreak) {
        return;
    }

    SKIZO_BREAKPOINTINFO bInfo;
    bInfo.Domain = this;

    const int sz = (int)m_debugDataStack->Pop();
    void** localRefs = (void**)m_debugDataStack->Pop();
    Auto<CWatchIterator> watchIterator (new CWatchIterator((CMethod*)m_stackFrames->Peek(),
                                                            localRefs,
                                                            sz));
    m_debugDataStack->Push((void*)localRefs);
    m_debugDataStack->Push((void*)sz);
    bInfo.WatchIterator = (skizo_watchiterator)watchIterator.Ptr();

    SKIZO_BREAKPOINTCALLBACK callback;
    if(m_breakpointCallback) {
        callback = m_breakpointCallback;
    } else {
        callback = builtin_callback_console;
    }

    // Temporarily disables breakpoints so that the user callback didn't try enter a new breakpoint while we're still
    // inside this break (stackoverflow could happen).
    m_disableBreak = true;

    try {
        callback(&bInfo);
    } catch (...) {
        m_disableBreak = false;
        throw;
    }

    m_disableBreak = false;
}

extern "C" {

void SKIZO_API _soX_break()
{
    CDomain::ForCurrentThread()->Break();
}

}

} }

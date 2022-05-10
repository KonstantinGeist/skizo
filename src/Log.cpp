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

#include "Log.h"
#include "ArrayList.h"
#include "Console.h"
#include "Contract.h"
#include "FileStream.h"
#include "HashMap.h"
#include "DateTime.h"
#include "Mutex.h"
#include "String.h"
#include "TextWriter.h"
#include "Thread.h"
#include <stdarg.h>

namespace skizo { namespace core {
using namespace skizo::collections;
using namespace skizo::io;

    // **************
    //   LogHandler
    // **************

void CLogHandler::SetEnabled(bool value)
{
    m_isEnabled = value;
}

static const char* stringForPriority(ELogPriority priority)
{
    switch(priority) {
        case E_LOGPRIORITY_DEBUG:
            return "DBG";
        case E_LOGPRIORITY_INFO:
            return "INF";
        case E_LOGPRIORITY_WARNING:
            return "WRN";
        case E_LOGPRIORITY_ERROR:
            return "ERR";
        default:
            return "???";
    }
}

const CString* CLogHandler::FormatMessage(ELogPriority priority,
                                          const char* domain,
                                          const CString* message)
{
    Auto<const CString> strDateTime (SDateTime::Now().ToString());
    return CString::Format("[%o] <%s@%s> %o",  static_cast<const CObject*>(strDateTime.Ptr()),
                                               stringForPriority(priority),
                                               domain,
                                               static_cast<const CObject*>(message));
}

    // *******************
    //   HandlerSelector
    // *******************

// Used to map priority (not a mask!) + domain (not a domain list!) to registered
// handlers.
struct CHandlerSelector: public CObject
{
    ELogPriority Priority;
    Auto<const CString> Domain;

    CHandlerSelector(ELogPriority priority, const CString* domain)
        : Priority(priority)
    {
        this->Domain.SetVal(domain);
    }

    virtual int GetHashCode() const override
    {
        return this->Domain->GetHashCode() + (int)this->Priority * 31;
    }

    virtual bool Equals(const CObject* _other) const override
    {
        const CHandlerSelector* const other = dynamic_cast<const CHandlerSelector*>(_other);
        if(!other) {
            return false;
        }

        if(this->Priority != other->Priority) {
            return false;
        }

        return this->Domain->Equals(other->Domain);
    }
};

    // *********************
    //   DelayedLogMessage
    // *********************

class CDelayedLogMessage final: public CObject
{
public:
    ELogPriority Priority;
    const char* Domain;
    Auto<const CString> Message;

    CDelayedLogMessage(ELogPriority priority, const char* domain, const CString* msg)
        : Priority(priority), Domain(domain)
    {
        this->Message.SetVal(msg);
    }
};

    // *******
    //   Log
    // *******

struct LogPrivate
{
    Auto<CHashMap<CHandlerSelector*, CArrayList<CLogHandler*>*> > m_logHandlerMap;
    Auto<CHashMap<const char*, const CString*> > m_st;

    // The base thread is the main thread where log handlers are called.
    // It's the thread which creates the logger.
    // Also, see CLog::Flush() for more information.
    CThread* m_baseThread;

    // Messages written on non-base threads are saved to a delayed list which
    // is then flushed on the base thread.
    Auto<CMutex> m_delLogMsgMutex;
    Auto<CArrayList<CDelayedLogMessage*> > m_delLogMsgList_nonBase,    // list for non-base threads
                                           m_delLogMsgList_base;       // list for the base thread

    LogPrivate()
        : m_logHandlerMap(new CHashMap<CHandlerSelector*, CArrayList<CLogHandler*>*>()),
          m_st(new CHashMap<const char*, const CString*>()),
          m_baseThread(CThread::Current()),
          m_delLogMsgMutex(new CMutex()),
          m_delLogMsgList_nonBase(new CArrayList<CDelayedLogMessage*>()),
          m_delLogMsgList_base(new CArrayList<CDelayedLogMessage*>())
    {
    }

    const CString* cachedString(const char* cString)
    {
        const CString* r;
        if(!m_st->TryGet(cString, &r)) {
            r = CString::FromUtf8(cString);

            // Must a have copy of the original string because its lifetime is uncertain.
            // To be deleted in ~LogPrivate().
            char* const cStringCopy = new char[strlen(cString) + 1];
            strcpy(cStringCopy, cString);

            m_st->Set(cStringCopy, r);
        }

        r->Unref();
        return r;
    }

    ~LogPrivate()
    {
        SHashMapEnumerator<const char*, const CString*> stEnum (m_st);
        const char* k;
        while(stEnum.MoveNext(&k, nullptr)) {
            delete [] const_cast<char*>(k);
        }
    }

    void writeThreadUnsafe(ELogPriority priority, const char* domain, const CString* message);
};

CLog::CLog()
    : p (new LogPrivate())
{

}

CLog::~CLog()
{
    delete p;
}

void CLog::AddLogHandler(ELogPriority priorMask, const char* domainList, CLogHandler* logHandler)
{
    // Entries are registered for every priority mask separately.
    #define LOG_PRIORITY_COUNT 4
    ELogPriority priorities[LOG_PRIORITY_COUNT] = {
        E_LOGPRIORITY_DEBUG,
        E_LOGPRIORITY_INFO,
        E_LOGPRIORITY_WARNING,
        E_LOGPRIORITY_ERROR
    };

    // Pre-splits in advance not to repeat it for every priority in the mask.
    Auto<const CString> daDomainList (CString::FromUtf8(domainList));
    Auto<CArrayList<const CString*> > splitDomains (daDomainList->Split(SKIZO_CHAR(';')));
    
    for(int i = 0; i < LOG_PRIORITY_COUNT; i++) {

        // Checks if such a priority exists in the mask.
        if((int)priorMask & (int)(priorities[i])) {

            for(int k = 0; k < splitDomains->Count(); k++) {
                const CString* const domain = splitDomains->Array()[k];

                Auto<CHandlerSelector> selector (new CHandlerSelector(priorities[i], domain));
                CArrayList<CLogHandler*>* regHandlers;

                // Creates a list if there's none.
                if(!p->m_logHandlerMap->TryGet(selector, &regHandlers)) {
                    regHandlers = new CArrayList<CLogHandler*>();
                    p->m_logHandlerMap->Set(selector, regHandlers);
                }

                regHandlers->Add(logHandler);

                // Don't forget to drop it.
                regHandlers->Unref();
            }

        }

    }
}

void CLog::Write(ELogPriority priority, const char* domain, const CString* message)
{
    if(CThread::Current() == p->m_baseThread) {
        // Automatically flushes stuff written by other threads.
        this->Flush();

        // Immediate mode for the base thread.
        p->writeThreadUnsafe(priority, domain, message);
    } else {
        // Non-base threads have to go through a delayed message list. Use
        // Flush(..) to call handlers on the base thread.

        SKIZO_LOCK(p->m_delLogMsgMutex)
        {
            // Makes a copy in case reference counting is non-atomic ('message'
            // may be used, after exiting from this function, by the non-base
            // thread, simultanously calling Ref()/Unref() while CLog::Flush()
            // works, potentially corrupting reference counting).
            //
            // See comments in CLog::Flush() for more info.
            Auto<const CString> messageCopy (message->Clone());
            Auto<CDelayedLogMessage> msg (new CDelayedLogMessage(priority, domain, messageCopy));

            p->m_delLogMsgList_nonBase->Add(msg);
        }
        SKIZO_END_LOCK(p->m_delLogMsgMutex);
    }
}

void LogPrivate::writeThreadUnsafe(ELogPriority priority, const char* domain, const CString* message)
{
    // Finds out the registered handlers.
    Auto<CHandlerSelector> selector (new CHandlerSelector(priority, cachedString(domain)));
    CArrayList<CLogHandler*>* regHandlers;

    // Note that there may be 0 handlers associated for the current combination.
    if(m_logHandlerMap->TryGet(selector, &regHandlers)) {
        regHandlers->Unref();

        for(int i = 0; i < regHandlers->Count(); i++) {
            CLogHandler* const handler = regHandlers->Array()[i];

            if(handler->m_isEnabled) {
                handler->Handle(priority, domain, message);
            }
        }
    }
}

void CLog::WriteImpl(ELogPriority priority, const char* domain, const char* message, va_list vl)
{
    Auto<const CString> formatted;
    formatted.SetPtr(CString::FormatImpl(message, vl));
    this->Write(priority, domain, formatted);
}

void CLog::Write(ELogPriority priority, const char* domain, const char* message, ...)
{
    va_list vl;
    va_start(vl, message);

    this->WriteImpl(priority, domain, message, vl);

    va_end(vl);
}

void CLog::Flush()
{
    if(p->m_delLogMsgList_nonBase->Count() > 0) {
        SKIZO_REQ_EQUALS(CThread::Current(), p->m_baseThread);

        // NOTE Log handlers may take too much time, so we don't call them under
        // the lock. Instead, we copy the list to another list to use it outside
        // of the lock. If messages have non-atomic reference count, it must be
        // safe because the messages in this delayed list are cloned
        // (see ::Write(..)) and are afterwards accessed either via the mutex, or 
        // on the base thread only (after m_delLogMsgList_nonBase is cleared (!),
        // so another thread cannot call Ref/Unref on the message during array
        // list relocation while log handlers on the message are being called
        // below, because the message has been removed from the list already).

        SKIZO_LOCK(p->m_delLogMsgMutex)
        {
            p->m_delLogMsgList_base->AddRange(p->m_delLogMsgList_nonBase);
            p->m_delLogMsgList_nonBase->Clear();
        }
        SKIZO_END_LOCK(p->m_delLogMsgMutex);

        // NOTE An enumerator is used because, if SKIZO_COLLECTIONS_MODCOUNT is
        // defined, it tests for concurrent modifications for additional
        // guarantees of thread-safety.
        SArrayListEnumerator<CDelayedLogMessage*> baseListEnum(p->m_delLogMsgList_base);
        CDelayedLogMessage* msg;
        while(baseListEnum.MoveNext(&msg)) {
            p->writeThreadUnsafe(msg->Priority, msg->Domain, msg->Message);
        }
        p->m_delLogMsgList_base->Clear();
    }
}

    // ************************
    //   Predefined handlers.
    // ************************

class CStreamLogHandler final: public CLogHandler
{
private:
    Auto<CTextWriter> m_textWriter;

public:
    explicit CStreamLogHandler(CStream* stream)
    {
        m_textWriter.SetPtr(new CTextWriter(stream));
    }

    virtual void Handle(ELogPriority priority, const char* domain, const CString* message) override
    {
        Auto<const CString> line (CLogHandler::FormatMessage(priority, domain, message));

        m_textWriter->Write(line);
        m_textWriter->WriteLine();
    }
};

CLogHandler* CLogHandler::CreateFromStream(CStream* stream)
{
    return new CStreamLogHandler(stream);
}

CLogHandler* CLogHandler::CreateFromFile(const CString* path)
{
    Auto<CFileStream> fileStream (CFileStream::Open(path, E_FILEACCESS_WRITE));
    return new CStreamLogHandler(fileStream);
}

// *******************

class CConsoleLogHandler final: public CLogHandler
{
public:
    virtual void Handle(ELogPriority priority, const char* domain, const CString* message) override
    {
        Auto<const CString> line (CLogHandler::FormatMessage(priority, domain, message));

        EConsoleColor foreColor;
        switch(priority) {
            case E_LOGPRIORITY_DEBUG:
                foreColor = E_CONSOLECOLOR_WHITE;
                break;

            case E_LOGPRIORITY_INFO:
                foreColor = E_CONSOLECOLOR_GREEN;
                break;

            case E_LOGPRIORITY_WARNING:
                foreColor = E_CONSOLECOLOR_YELLOW;
                break;

            case E_LOGPRIORITY_ERROR:
                foreColor = E_CONSOLECOLOR_RED;
                break;

            default:
                foreColor = E_CONSOLECOLOR_WHITE;
                break; // to shut up the compiler
        }
        Console::SetForeColor(foreColor);
        Console::WriteLine(line);

        // Returns the color back to default which is white usually.
        if(priority != E_LOGPRIORITY_DEBUG) {
            Console::SetForeColor(E_CONSOLECOLOR_WHITE);
        }
    }
};

CLogHandler* CLogHandler::CreateForConsole()
{
    return new CConsoleLogHandler();
}

} }

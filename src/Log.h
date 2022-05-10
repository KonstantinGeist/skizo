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

#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include "Object.h"

#ifdef SKIZO_SINGLE_THREADED
    #include "AtomicObject.h"
#endif

/**
 * Use this to log debug messages.
 */
#define SKIZO_LOG_DEBUG(_log, domain, message, args...) \
        if(_log) _log->Write(E_LOGPRIORITY_DEBUG, domain, message, ## args)

/**
 * @note Won't fail if log is null.
 */
#define SKIZO_LOG_INFO(_log, domain, message, args...) \
            if(_log) _log->Write(E_LOGPRIORITY_INFO, domain, message, ## args)

#define SKIZO_LOG_INFO_S(_log, domain, message) \
            if(_log) _log->Write(E_LOGPRIORITY_INFO, domain, message)

#define SKIZO_LOG_DEBUG(_log, domain, message, args...) \
            if(_log) _log->Write(E_LOGPRIORITY_DEBUG, domain, message, ## args)

#define SKIZO_LOG_DEBUG_S(_log, domain, message) \
            if(_log) _log->Write(E_LOGPRIORITY_DEBUG, domain, message)

#define SKIZO_LOG_WARNING(_log, domain, message, args...) \
            if(_log) _log->Write(E_LOGPRIORITY_WARNING, domain, message, ## args)

#define SKIZO_LOG_WARNING_S(_log, domain, message) \
            if(_log) _log->Write(E_LOGPRIORITY_WARNING, domain, message)

#define SKIZO_LOG_ERROR(_log, domain, message, args...) \
            if(_log) _log->Write(E_LOGPRIORITY_ERROR, domain, message, ## args)

#define SKIZO_LOG_ERROR_S(_log, domain, message) \
            if(_log) _log->Write(E_LOGPRIORITY_ERROR, domain, message)

namespace skizo { namespace io {
    class CStream;
} }

namespace skizo { namespace core {
class CString;

/**
 * @note Can be used as a mask.
 */
enum ELogPriority
{
    /**
     * A debug message is something which should be removed from the release.
     */
    E_LOGPRIORITY_DEBUG = 1,

    /**
     * An informational message is useful when diagnosing a problem post mortem.
     */
    E_LOGPRIORITY_INFO = 2,

    /**
     * A warning is something which is tolerable but is of caution.
     */
    E_LOGPRIORITY_WARNING = 4,

    /**
     * An error is a "show stopper".
     */
    E_LOGPRIORITY_ERROR = 8,

    /**
     * All priorities combined.
     */
    E_LOGPRIORITY_ALL = E_LOGPRIORITY_DEBUG
                      | E_LOGPRIORITY_INFO
                      | E_LOGPRIORITY_WARNING
                      | E_LOGPRIORITY_ERROR
};

/**
 * Handles log messages.
 */
class CLogHandler: public CObject
{
    friend class CLog;
    friend struct LogPrivate;

private:
    bool m_isEnabled;

public:
    CLogHandler()
        : m_isEnabled(true)
    {
    }

    /**
     * @see CLog::Write(..) for more info about the arguments.
     */
    virtual void Handle(ELogPriority priority, const char* domain, const CString* message) = 0;

    /**
     * A handler can be temporarily disabled: for example, too many warnings are
     * posted from one of the domains, not allowing to see the real problem (a
     * few error messages). Only enabled log handlers are called the "Handle()"
     * method. By default, log handlers are enabled.
     */
    void SetEnabled(bool value);

    /**
     * There's no concept of "formatters", as it's easier to provide a set of
     * static methods. It's the responsibility of a handler to format the message.
     * If an implementation does not care about formatting, this method can be used.
     */
    static const CString* FormatMessage(ELogPriority priority, const char* domain, const CString* message);

    // ************************
    //   Predefined handlers.
    // ************************

    /**
     * A built-in log handler which writes all messages to a stream.
     */
    static CLogHandler* CreateFromStream(skizo::io::CStream* stream);

    /**
     * A built-in log handler which writes all messages to a file specified by
     * a path.
     */
    static CLogHandler* CreateFromFile(const CString* path);

    /**
     * A built-in log handler which writes all messages to the console.
     */
    static CLogHandler* CreateForConsole();
};

/**
 * A log is a collection of messages with different priorities.
 * The messages can be dumped to a stream (for example, a file on disk), or
 * rerouted to a console or an in-game terminal.
 *
 * @note There's no global log; implementations should agree on their own global log.
 * @note The implementation is thread-safe, see ::Flush()
 */
#ifdef SKIZO_SINGLE_THREADED
class CLog: public CAtomicObject
#else
class CLog: public CObject
#endif
{
private:
    struct LogPrivate* p;

public:
    CLog();
    virtual ~CLog();

    /**
     * Writes the specified message (which supports %-formatting just like
     * CString::Format) to the log. Handlers associated with the given priority
     * (not a mask!) and the domain are called to handle the message. "Domain"
     * is a specialized part of the application. Separate handlers can be
     * registered for separate domains, enabling the user to selectively disable
     * messages for specific parts of the application.
     */
    void Write(ELogPriority priority, const char* domain, const char* message, ...);
    void WriteImpl(ELogPriority priority, const char* domain, const char* message, va_list vl);

    void Write(ELogPriority priority, const char* domain, const CString* message);

    /**
     * Adds a new handler which is registered for the specified priorities (a mask)
     * and the specified domain list. The domain list should be a string
     * containing domains separated by semicolon, for example:
     * "Model;View;Controller"
     *
     * @note There can be several log handlers for the same priority mask and
     * domain list: for example, there can be a scenario where the same message
     * should be both printed on screen and saved to a LOG file on disk.
     */
    void AddLogHandler(ELogPriority priorMask, const char* domainList, CLogHandler* logHandler);

    /**
     * Handlers are called immediatelly when Log::Write(..) is called on the
     * base thread (the thread which created this object), but are delayed when
     * called on other threads.
     *
     * Flush(..) should be called to force delayed log data to be logged on the
     * base thread; that is, Flush(..) should be called on the base thread.
     *
     * @warning This function can be called only on the base thread.
     */
    void Flush();
};

} }

#endif // LOG_H_INCLUDED

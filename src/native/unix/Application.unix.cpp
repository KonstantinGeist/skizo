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

#include "../../Application.h"
#include "../../ArrayList.h"
#include "../../Exception.h"
#include "../../String.h"
#include "../../StringBuilder.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <time.h>
#define MAX_PATH 4096 // As in Linux kernel.

// For Application::GetOSVersion(..)
#include <sys/utsname.h>

namespace skizo { namespace core { namespace Application {
using namespace skizo::core;
using namespace skizo::collections;

// TODO doesn't restore quotes yet
const CString* GetCommandLineArgs()
{
    FILE* f;
    // WARNING works only with arguments <1024 in UTF8
    char BUF[1024];
    pid_t proc_id = getpid();
    sprintf(BUF, "/proc/%i/cmdline", proc_id);

    f = fopen(BUF, "rb");
    if(!f) {
        return CString::CreateEmptyString();
    }

    memset(BUF, 0, 1024);
    size_t sz = fread(BUF, 1, 1024, f);

    // Replace zeros with spaces
    // (reconstructs the command line).
    int zeroCount = 0;
    const char* final = BUF;
    for(size_t i = 0; i < sz; i++) {
        if(BUF[i] == 0) {
            BUF[i] = ' ';
            // The API contract ignores the executable name.
            if(zeroCount == 0) {
                final = BUF + i + 1;
            }
            zeroCount++;
        }
    }

    fclose(f);

    Auto<const CString> tmp (CString::FromUtf8(final));
    return tmp->Trim();
}

const CString* GetSpecialFolder(ESpecialFolder specialFolder)
{
    switch(specialFolder) {
        case E_SPECIALFOLDER_APPDATA:
        case E_SPECIALFOLDER_HOME:
        {
            const char* homedir;
            if ((homedir = getenv("HOME")) == NULL) {
                homedir = getpwuid(getuid())->pw_dir;
            }

            return homedir? CString::FromUtf8(homedir): CString::CreateEmptyString();
        }
        break;

        default:
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
            break;
    }

    return nullptr; // to shut up the compiler
}

so_long TickCount()
{
    struct timespec ts;
    unsigned int theTick = 0U;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    theTick = ts.tv_nsec / 1000000;
    theTick += ts.tv_sec * 1000;

    return theTick;
}

   // *******************
   //      Launch
   // *******************

struct SLaunchArray
{
    int xArgSize;
    char** xArgs;

    SLaunchArray(const CString* path, const CString* args)
        : xArgSize(0),
          xArgs(nullptr)
    {
        Auto<CArrayList<const CString*> > args2 (splitArgs(args));
        this->xArgSize = args2->Count() + 2; // 1 for the exe path + 1 for NULL
        this->xArgs = new char*[xArgSize];
        xArgs[0] = path->ToUtf8();
        for(int i = 0; i < args2->Count(); i++) {
            xArgs[i + 1] = args2->Array()[i]->ToUtf8();
        }
        xArgs[xArgSize - 1] = nullptr; // the array must be null-terminated
    }

    ~SLaunchArray()
    {
        for(int i = 0; i < xArgSize; i++) {
            CString::FreeUtf8(xArgs[i]);
        }
        delete [] xArgs;
    }

    static CArrayList<const CString*>* splitArgs(const CString* args)
    {       
        auto r = new CArrayList<const CString*>();
        if(CString::IsNullOrEmpty(args)) {
            return r;
        }

        try {

            int lastIndex = 0;
            bool quote = false;
            for(int i = 0; i < args->Length() + 1; i++) {
                so_char16 c = (i < args->Length())? args->Chars()[i]: SKIZO_CHAR(' ');

                if(c == SKIZO_CHAR('"')) {
                    quote = !quote;
                } else if(c == SKIZO_CHAR(' ') && !quote) {
                    Auto<const CString> substr;

                    if((i > 0)
                       && (args->CharAt(lastIndex) == SKIZO_CHAR('"'))
                       && (args->CharAt(i - 1) == SKIZO_CHAR('"')))
                    {
                        substr.SetPtr(args->Substring(lastIndex + 1, i - lastIndex - 2));
                    } else {
                        substr.SetPtr(args->Substring(lastIndex, i - lastIndex));
                    }

                    r->Add(substr);
                    lastIndex = i + 1;
                }
            }

        } catch(const SException& e) {
            // Gives up on exception (who knows) and returns the string as it is.
            r->Clear();
            r->Add(args);
        }

        return r;
    }
};

// TODO SLaunchOptions::InheritConsole?
void Launch(const CString* path, const CString* args, const SLaunchOptions& options)
{
    SLaunchArray launchArray (path, args);

    // The self-pipe trick (see below).

    int pipefds[2];
    if(pipe(pipefds)) {
        SKIZO_THROW_WITH_MSG(EC_OUT_OF_RESOURCES, "Failed to create self-pipe.");
    }
    if(fcntl(pipefds[1], F_SETFD, fcntl(pipefds[1], F_GETFD) | FD_CLOEXEC)) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }

    // Forks.

    // WARNING No complex functions should be between fork() and execvp(..)
    // because it's not thread-safe when it comes to mutexes (potential deadlocks).
    int pid = fork();
    if(pid < 0) {
        close(pipefds[0]);
        close(pipefds[1]);
        SKIZO_THROW_WITH_MSG(EC_OUT_OF_RESOURCES, "Failed to fork.");
    }

    if(pid == 0) { // Child process.

        execvp(launchArray.xArgs[0], launchArray.xArgs);

        // If we're here, that means we failed to launch the subprocess (otherwise,
        // the address space would have been replaced with a different image in
        // execvp(..) above). So, we write a byte to the pipe to signal to the
        // parent there was an error.

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-result"
        write(pipefds[1], &errno, sizeof(int));
    #pragma GCC diagnostic pop

        _exit(0);

    } else { // Parent process.

        // Closes the write end, we don't need it in the parent.
        close(pipefds[1]);

        // Tries to read one byte from the pipe. If it's successful, that means
        // the child process failed to launch.
        int bytesRead = 0;
        int err;
        while((bytesRead = read(pipefds[0], &err, sizeof(errno))) == -1) {
            if (errno != EAGAIN && errno != EINTR) {
                break;
            }
        }
        close(pipefds[0]);
        if(bytesRead) {
            SKIZO_THROW_WITH_MSG(EC_PATH_NOT_FOUND, "Failed to launch subprocess.");
        }

        if(options.WaitForExit) {
            int status;
            while(waitpid(pid, &status, 0) == -1) {
                if(errno != EINTR) {
                    SKIZO_THROW(EC_PLATFORM_DEPENDENT);
                }
            }
        }
        
    }
}

void Launch(EApplication app, const CString* args)
{
    switch(app) {
        case E_APPLICATION_THIS:
            {
                Auto<const CString> exeFileName (GetExeFileName());
                SLaunchOptions launchOptions;
                launchOptions.WaitForExit = false; // makes it explicit
                Launch(exeFileName, args, launchOptions);
            }
            break;

        default:
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
            break;
    }
}

void Launch(const CString* path, const CString* args)
{
    Launch(path, args, SLaunchOptions());
}

   // *******************
   //   GetExeFileName
   // *******************

/*
 * getexename - Get the filename of the currently running executable
 *
 * The getexename() function copies an absolute filename of the currently
 * running executable to the array pointed to by buf, which is of length size.
 *
 * If the filename would require a buffer longer than size elements, NULL is
 * returned, and errno is set to ERANGE; an application should check for this
 * error, and allocate a larger buffer if necessary.
 *
 * Return value:
 * NULL on failure, with errno set accordingly, and buf on success. The
 * contents of the array pointed to by buf is undefined on error.
 *
 * Notes:
 * This function is tested on Linux only. It relies on information supplied by
 * the /proc file system.
 * The returned filename points to the final executable loaded by the execve()
 * system call. In the case of scripts, the filename points to the script
 * handler, not to the script.
 * The filename returned points to the actual exectuable and not a symlink.
 *
 */
static char* getexename(char* buf, int size)
{
    char linkname[64]; /* /proc/<pid>/exe */
    pid_t pid;
    int ret;

    // Gets our PID and build the name of the link in /proc */
    pid = getpid();

    snprintf(linkname, sizeof(linkname), "/proc/%i/exe", pid);

    // Now reads the symbolic link.
    ret = readlink(linkname, buf, size);

    // In case of an error, leaves the handling up to the caller.
    if (ret == -1) {
        return nullptr;
    }

    // Reports insufficient buffer size.
    if (ret >= size) {
        return nullptr;
    }

    // Ensures proper NUL termination.
    buf[ret] = 0;

    return buf;
}

const CString* GetExeFileName()
{
    char buf[MAX_PATH] = { 0 };
    if(!getexename(buf, MAX_PATH)) {
        SKIZO_THROW(EC_PLATFORM_DEPENDENT);
    }

    return CString::FromUtf8(buf);
}

   // *******************

const CString* GetOSVersion()
{
    Auto<CStringBuilder> sb (new CStringBuilder());

    struct utsname osName;
    if(uname(&osName) == -1) {
        // If for some reason could not open it -- just print that it's a Unix.
        sb->Append("Unix");
    } else {
        sb->AppendFormat("%s %s %s %s", osName.sysname,
                                        osName.release,
                                        osName.version,
                                        osName.machine);
    }

    if(sizeof(void*) == 8) {
        sb->Append(" (64 bit)");
    } else if(sizeof(void*) == 4) {
        sb->Append(" (32 bit)");
    }

    return sb->ToString();
}

   // *******************
   //   GetMemoryUsage
   // *******************

static int GetMemoryUsage_parseLine(char* line)
{
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while(*p <'0' || *p > '9') p++;
    line[i - 3] = '\0';
    i = atoi(p);
    return i;
}

so_long GetMemoryUsage()
{
    FILE* file = fopen("/proc/self/status", "r");
    if(!file) {
        return 0;
    }

    int result = -1;
    char line[128];

    while(fgets(line, 128, file) != NULL) {
        if(strncmp(line, "VmSize:", 7) == 0) {
            result = GetMemoryUsage_parseLine(line) * 1024; // the value is in Kb
            break;
        }
    }

    fclose(file);
    return result;
}

int GetProcessorCount()
{
    // Technically non-standard, but will simply return 1 processor if no such
    // name is found. Or it just won't compile on such a system.
    long r = sysconf(_SC_NPROCESSORS_ONLN);
    if(r < 1) {
        return 1; // Assume 1 processor.
    }
    return r;
}

} } }

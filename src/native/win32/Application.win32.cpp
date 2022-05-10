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
#include "../../CoreUtils.h"
#include "../../Exception.h"
#include "../../Path.h"
#include "../../String.h"
#include "../../StringBuilder.h"

#include <mmsystem.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

namespace skizo { namespace core { namespace Application {
using namespace skizo::io;

static bool isWindows8OrGreater()
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0 };
    const DWORDLONG dwlConditionMask = VerSetConditionMask(
        VerSetConditionMask(
        VerSetConditionMask(
            0, VER_MAJORVERSION, VER_GREATER_EQUAL),
               VER_MINORVERSION, VER_GREATER_EQUAL),
               VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_WIN8);
    osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_WIN8);
    osvi.wServicePackMajor = 0;

    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

const CString* GetExeFileName()
{
    so_char16 exepath[MAX_PATH];
    if(!GetModuleFileName(NULL, reinterpret_cast<WCHAR*>(exepath), MAX_PATH)) {
        CoreUtils::ThrowWin32Error();
    }

    Auto<const CString> preR (CString::FromUtf16(exepath));
    return Path::Normalize(preR);
}

so_long GetMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS ppsmemCounters;
    GetProcessMemoryInfo(GetCurrentProcess(), &ppsmemCounters, sizeof(ppsmemCounters));
    return ppsmemCounters.WorkingSetSize;
}

int GetProcessorCount()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors;
}

so_long TickCount(void)
{
    return (so_long)timeGetTime();
}

void Launch(EApplication app, const CString* args)
{
    switch(app) {
        case E_APPLICATION_THIS:
        {
            Auto<const CString> exeName (GetExeFileName());
            HINSTANCE r = ShellExecute(NULL,
                                     L"open",
                                      (LPCTSTR)exeName->Chars(),
                                       args? (LPCTSTR)args->Chars(): NULL,
                                       NULL,
                                       SW_SHOWNORMAL);
            if((uintptr_t)r < 32) {
                CoreUtils::ThrowWin32Error();
            }
        }
        break;

        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
    }
}

void Launch(const CString* path, const CString* args, const SLaunchOptions& options)
{
    if(options.WaitForExit) {

        STARTUPINFO si = { 0 };
        si.cb = sizeof(si);

        if(options.InheritConsole) {
			if (!isWindows8OrGreater()) {
				// This fails under Windows 10 for some reason. With these flags enabled, "cmd.exe" is never really run
				// (even under administrator) although it is reported as successfully launched and WaitForSingleObject doesn't
				// complain either.
				// According to https://github.com/rprichard/win32-console-docs "Starting with Windows 8, console
				// handles are true NT kernel handles that reference NT kernel objects". This may be the reason for
				// the breaking change.
				// It seems to work OK without these flags anyway, so we leave it as that.

				si.dwFlags |= STARTF_USESTDHANDLES;

				si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
				si.hStdOutput =  GetStdHandle(STD_OUTPUT_HANDLE);
				si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			}
        } else {
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;
        }

        Auto<const CString> cmdLine (path->Clone()); // WARNING modified in place by CreateProcess(..)
        if(args) {
            cmdLine.SetPtr(CString::Format("%o %o",
                                           static_cast<const CObject*>(path),
                                           static_cast<const CObject*>(args)));
        }

        PROCESS_INFORMATION pi;
        BOOL b = CreateProcess(NULL,
                               (LPTSTR)cmdLine->Chars(),
                               NULL,
                               NULL,
                               FALSE, 
                               0,
                               NULL,
                               NULL,
                               &si,
                               &pi);
        if(!b) {
            CoreUtils::ThrowWin32Error();
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);

    } else {

        HINSTANCE r = ShellExecute(NULL,
                                 L"open",
                                  (LPCTSTR)path->Chars(),
                                   args? (LPCTSTR)args->Chars(): nullptr,
                                   NULL,
                                   options.InheritConsole? SW_HIDE: SW_SHOWNORMAL);
        if((uintptr_t)r < 32) {
            CoreUtils::ThrowWin32Error();
        }

    }
}

void Launch(const CString* path, const CString* args)
{
    Launch(path, args, SLaunchOptions());
}

const CString* GetSpecialFolder(ESpecialFolder specialFolder)
{
    Auto<const CString> r;

    switch(specialFolder) {
        case E_SPECIALFOLDER_APPDATA:
        {
            WCHAR wcs[260];
            int e = SHGetFolderPath(0,             // some hwnd
                                    CSIDL_APPDATA, // system folder
                                    0,             // token
                                    0,             // dwFlags
                                    wcs);
            if(e < 0) {
                SKIZO_THROW(EC_PLATFORM_DEPENDENT);
            }

            r.SetPtr(CString::FromUtf16((so_char16*)wcs));
        }
        break;

        case E_SPECIALFOLDER_HOME:
        {
            WCHAR wcs[260];
            int e = SHGetFolderPath(0,             // some hwnd
                                    CSIDL_PERSONAL, // system folder
                                    0,             // token
                                    0,             // dwFlags
                                    wcs);
            if(e < 0) {
                SKIZO_THROW(EC_PLATFORM_DEPENDENT);
            }

            r.SetPtr(CString::FromUtf16((so_char16*)wcs));
        }
        break;

        default:
        {
            SKIZO_THROW(EC_NOT_IMPLEMENTED);
        }
    }

    return Path::Normalize(r);
}

const CString* GetCommandLineArgs()
{
    Auto<const CString> line (CString::FromUtf16((so_char16*)::GetCommandLine()));

    // Finds the first element which is the name of the executable
    // taking quotes into consideration.
    int offset = 0;
    bool quoteEnabled = false;
    for(int i = 0; i < line->Length(); i++) {
        const so_char16 c = line->Chars()[i];
        if(c == SKIZO_CHAR('\"')) {
            quoteEnabled = !quoteEnabled;
        } else if(!quoteEnabled && CoreUtils::IsWhiteSpace(c)) {
            offset = i;
            break;
        }
    }

    if(offset == 0) {
        return CString::CreateEmptyString();
    } else {
        line.SetPtr(line->Substring(offset + 1));
        line.SetPtr(line->Trim()); // Windows inserts 1 random space for some reason.

        return Path::Normalize(line);
    }
}

const CString* GetOSVersion()
{
    Auto<CStringBuilder> sb (new CStringBuilder());

    LPWSTR pszSubkey = (LPWSTR)(L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion");
    HKEY hKey = NULL;
    bool isRegistrySuccess = false;

    if(RegOpenKey(HKEY_LOCAL_MACHINE, pszSubkey, &hKey) == ERROR_SUCCESS) {
        TCHAR szValue[1024];
        DWORD cbValueLength = sizeof(szValue);
        DWORD dwType = REG_SZ;

        if(RegQueryValueEx(hKey,
                           L"ProductName",
                           NULL,
                           &dwType,
                           reinterpret_cast<LPBYTE>(&szValue),
                           &cbValueLength) == ERROR_SUCCESS)
        {
            sb->Append((so_char16*)&szValue[0]);
            isRegistrySuccess = true;
        }

        RegCloseKey(hKey);
    }

    if(!isRegistrySuccess) {
        // If for some reason could not open it (a supernew Windows)
        // -- just print that it's a Windows.
        sb->Append("Microsoft Windows");
    }

    if(sizeof(void*) == 8) {
        sb->Append(" (64 bit)");
    } else if(sizeof(void*) == 4) {
        sb->Append(" (32 bit)");
    }

    return sb->ToString();
}

} } }

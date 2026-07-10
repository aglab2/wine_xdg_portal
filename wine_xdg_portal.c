#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "heavens.h"
#include "log.h"
#include "recon.h"

#include <dbus/dbus.h>

#include <windows.h>

LPSTR (*CDECL wine_get_unix_file_name_ptr)(LPCWSTR) = NULL;
LPWSTR (*CDECL wine_get_dos_file_name_ptr)(LPCSTR) = NULL;

static FILE* unixOpen(const char* _path, const wchar_t* flags)
{
    wchar_t* path = wine_get_dos_file_name_ptr(_path);
    if (!path)
    {
        return NULL;
    }

    FILE* f = _wfopen(path, flags);
    HeapFree( GetProcessHeap(), 0, path );

    return f;
}

void* heavensGate = NULL;

int getpidx (void)
{
  int pid;

  __asm__ volatile (
      "call *_heavensGate"
      : "=a" (pid)
      : "0" (39 /* __NR_getpid */)
      : "memory");

  return pid;
}

int __cdecl wine_portal_init()
{
    // Are we even running under Wine?
    {
        wine_get_unix_file_name_ptr = (void*) GetProcAddress(GetModuleHandleA("KERNEL32"), "wine_get_unix_file_name");
        wine_get_dos_file_name_ptr  = (void*) GetProcAddress(GetModuleHandleA("KERNEL32"), "wine_get_dos_file_name");

        if (!wine_get_unix_file_name_ptr || !wine_get_dos_file_name_ptr)
        {
            return -1;
        }
    }

    // Prepare logging + test we can use /tmp + unix convs
    {
        gLog = unixOpen("/tmp/test.log", L"w+");
        if (!gLog)
        {
            return -1;
        }
    }

    // Make canary + find pid of our process
    int pid;
    {
        char line[40];
        sprintf(line, "/tmp/wine_xdg_canary_%lu", GetProcessId(GetCurrentProcess()));

        HANDLE hFile = CreateFileA(line, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        HANDLE hMapFile = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, 4096, NULL);
        char* pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 4096);

        pid = find_pid(line);

        UnmapViewOfFile(pBuf);
        CloseHandle(hMapFile);
        CloseHandle(hFile);
        unlink(line);
    }

    if (!pid)
    {
        log("Failed to find pid\n");
        return -1;
    }

    struct LibcFunctions libc = find_libc(pid);
    if (!libc.syscall || !libc.errno_location)
    {
        log("Failed to find libc\n");
        return -1;
    }

    heavensGate = hg_setup(libc);
    if (!heavensGate)
    {
        log("Failed to setup heavens\n");
        return -1;
    }

    log("heavensGate: %p\n", heavensGate);

    int pidx = getpidx();
    if (pidx != pid)
    {
        log("getpidx() = %d, expected %d\n", pidx, pid);
        return -1;
    }

    return 0;
}

// MARK: Implementation

typedef struct WideFilter
{
    const wchar_t* name;
    const wchar_t* pattern;
} WideFilter;

struct Files
{
    wchar_t** paths;
    int count;
};

typedef struct Utf8Filter
{
    const char* name;
    const char* pattern;
} Utf8Filter;

struct Utf8Files
{
    char** paths;
    int count;
};


void __cdecl wine_portal_wide_open_native_for(const wchar_t* path)
{
    log("wine_portal_wide_open_native_for: %ls\n", path);

    char* unix_name = wine_get_unix_file_name_ptr(path);
    if (!unix_name)
    {
        return;
    }

    if (unix_name)
    {
        HeapFree( GetProcessHeap(), 0, unix_name );
    }

    return;
}

void __cdecl wine_portal_free(void* ptr)
{
    log("wine_portal_free: %p\n", ptr);
    free(ptr);
}

void __cdecl wine_portal_wide_open_files_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* initialDir, struct Files* outFiles)
{
    log("wine_portal_wide_open_files_dialog: %p, %p, %d, %ls, %p\n", hwndOwner, filters, filtersCount, initialDir, outFiles);
}

wchar_t* __cdecl wine_portal_wide_save_file_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* defaultName, const wchar_t* initialDir)
{
    log("wine_portal_wide_save_file_dialog: %p, %p, %d, %ls, %ls\n", hwndOwner, filters, filtersCount, defaultName, initialDir);
    return NULL;
}

wchar_t* __cdecl wine_portal_wide_choose_directory(void* hwndOwner, const wchar_t* title, const wchar_t* initialDir)
{
    log("wine_portal_wide_choose_directory: %p, %ls, %ls\n", hwndOwner, title, initialDir);
    return NULL;
}

void __cdecl wine_portal_utf8_open_native_for(const char* path)
{
    DBusError err;
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn)
    {
        log("Failed: %s\n", err.message);
    }
}

char* __cdecl wine_portal_utf8_open_file_dialog(void* hwndOwner, bool fileMustExist, Utf8Filter* filters, int filtersCount, const char* initialDir)
{
    return NULL;
}

void __cdecl wine_portal_utf8_open_files_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* initialDir, struct Utf8Files* outFiles)
{
}

char* __cdecl wine_portal_utf8_save_file_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* defaultName, const char* initialDir)
{
    return NULL;
}

char* __cdecl wine_portal_utf8_choose_directory(void* hwndOwner, const wchar_t* title, const char* initialDir)
{
    return NULL;
}

#include "wine_xdg_portal.h"

#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "errno_conv.h"
#include "heavens.h"
#include "log.h"
#include "pattern_parser_sp.h"
#include "portal.h"
#include "recon.h"
#include "string_conv.h"

static FILE* unixOpen(const char* _path, const wchar_t* flags)
{
    wchar_t* path = str_Unix_to_WinW(_path);
    if (!path)
    {
        return NULL;
    }

    FILE* f = _wfopen(path, flags);
    str_free(path);

    return f;
}

// MARK: Syscalls

static int getpidx (void)
{
    return syscall0(39);
}

static int enosys(void)
{
    return syscall0(9999);
}

int WP_DECL wine_portal_init()
{
    // Are we even running under Wine?
    if (str_conv_init() != 0)
    {
        return -1;
    }

    // Prepare logging + test we can use /tmp + unix convs
    {
        gLog = unixOpen("/tmp/wine_xdg_portal.log", L"w+");
        if (!gLog)
        {
            return -1;
        }
    }

    bool hasFsgsBase = *(bool*) 0x7ffe028a;
    log("hasFsgsBase: %d\n", hasFsgsBase);
    if (!hasFsgsBase)
    {
        log("CPU is too old, need fsgsbase support\n");
        return -1;
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
    if (!libc.syscall || !libc.errno_location || !libc.errno_fs_offset)
    {
        log("Failed to find libc\n");
        return -1;
    }

    if (hg_setup(libc) != 0)
    {
        log("Failed to setup heavens\n");
        return -1;
    }

    int pidx = getpidx();
    if (pidx != pid)
    {
        log("getpidx() = %d, expected %d\n", pidx, pid);
        return -1;
    }

    int errx = enosys();
    if (errx != -1 && errno != ENOSYS)
    {
        log("enosys() = %d, error=%d, expected %d\n", errx, errno, ENOSYS);
        return -1;
    }

    log("wine_portal_init() succeeded\n");
    return 0;
}

// MARK: Implementation

void WP_DECL wine_portal_wide_open_native_for(const wchar_t* _)
{
    // not implemented currently
}

void WP_DECL wine_portal_free(void* ptr)
{
    log("wine_portal_free: %p\n", ptr);
    free(ptr);
}

void WP_DECL wine_portal_utf8_open_native_for(const char* smth)
{
    return (void) portal_open_native_for(smth);
}

static struct PortalFilter make_utf8_pattern_parser(void* ctx, int index)
{
    Utf8Filter* filters = (Utf8Filter*)ctx;
    struct PortalFilter filter;
    filter.name    = str_WinA_to_Unix(filters[index].name);
    filter.pattern = pattern_parser_init_a(filters[index].pattern);

    return filter;
}

static struct PortalFilter make_wide_pattern_parser(void* ctx, int index)
{
    WideFilter* filters = (WideFilter*)ctx;
    struct PortalFilter filter;
    filter.name    = str_WinW_to_Unix(filters[index].name);
    filter.pattern = pattern_parser_init_w(filters[index].pattern);

    return filter;
}

char* WP_DECL wine_portal_utf8_open_file_dialog(void* hwndOwner, bool fileMustExist, Utf8Filter* filters, int filtersCount, const char* _initialDir)
{
    char* initialDir = str_WinA_to_Unix(_initialDir);

    char* ustr = NULL;
    int ok = portal_open_file_dialog(&ustr, hwndOwner, fileMustExist, make_utf8_pattern_parser, filters, filtersCount, initialDir);

    str_free(initialDir);

    char* astr = str_Unix_to_WinA(ustr);
    str_free(ustr);

    return astr;
}

wchar_t* WP_DECL wine_portal_wide_save_file_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* _defaultName, const wchar_t* _initialDir)
{
    char* defaultName = str_WinW_to_Unix(_defaultName);
    char* initialDir = str_WinW_to_Unix(_initialDir);

    char* selectedPath = NULL;
    portal_save_file_dialog(&selectedPath, hwndOwner, make_wide_pattern_parser, filters, filtersCount, defaultName, initialDir);

    str_free(defaultName);
    str_free(initialDir);

    wchar_t* wstr = str_Unix_to_WinW(selectedPath);
    str_free(selectedPath);

    return wstr;
}

char* WP_DECL wine_portal_utf8_save_file_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* _defaultName, const char* _initialDir)
{
    char* defaultName = str_WinA_to_Unix(_defaultName);
    char* initialDir = str_WinA_to_Unix(_initialDir);

    char* selectedPath = NULL;
    portal_save_file_dialog(&selectedPath, hwndOwner, make_utf8_pattern_parser, filters, filtersCount, defaultName, initialDir);

    str_free(defaultName);
    str_free(initialDir);

    char* astr = str_Unix_to_WinA(selectedPath);
    str_free(selectedPath);

    return astr;
}

wchar_t* WP_DECL wine_portal_wide_choose_directory(void* hwndOwner, const wchar_t* _title, const wchar_t* _initialDir)
{
    char* title = str_WinW_to_Unix(_title);
    char* initialDir = str_WinW_to_Unix(_initialDir);

    char* selectedPath = NULL;
    portal_choose_directory(&selectedPath, hwndOwner, title, initialDir);

    str_free(title);
    str_free(initialDir);

    wchar_t* wstr = str_Unix_to_WinW(selectedPath);
    str_free(selectedPath);

    return wstr;
}

char* WP_DECL wine_portal_utf8_choose_directory(void* hwndOwner, const wchar_t* _title, const char* _initialDir)
{
    char* title = str_WinW_to_Unix(_title);
    char* initialDir = str_WinA_to_Unix(_initialDir);

    char* selectedPath = NULL;
    portal_choose_directory(&selectedPath, hwndOwner, title, initialDir);

    str_free(title);
    str_free(initialDir);

    char* wstr = str_Unix_to_WinA(selectedPath);
    str_free(selectedPath);

    return wstr;
}

void WP_DECL wine_portal_utf8_open_files_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* _initialDir, struct Utf8Files* _outFiles)
{
    char* initialDir = str_WinA_to_Unix(_initialDir);

    struct PortalFiles outFiles = {0, NULL};
    portal_open_files_dialog(&outFiles, hwndOwner, make_utf8_pattern_parser, filters, filtersCount, initialDir);

    str_free(initialDir);

    _outFiles->count = outFiles.count;
    _outFiles->paths = outFiles.count ? HeapAlloc(GetProcessHeap(), 0, sizeof(char*) * outFiles.count) : NULL;
    for (int i = 0; i < outFiles.count; ++i)
    {
        _outFiles->paths[i] = str_Unix_to_WinA(outFiles.paths[i]);
        str_free(outFiles.paths[i]);
    }

    free(outFiles.paths);
}

void WP_DECL wine_portal_wide_open_files_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* _initialDir, struct Files* _outFiles)
{
    char* initialDir = str_WinW_to_Unix(_initialDir);

    struct PortalFiles outFiles = {0, NULL};
    portal_open_files_dialog(&outFiles, hwndOwner, make_wide_pattern_parser, filters, filtersCount, initialDir);

    str_free(initialDir);

    _outFiles->count = outFiles.count;
    _outFiles->paths = outFiles.count ? HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t*) * outFiles.count) : NULL;
    for (int i = 0; i < outFiles.count; ++i)
    {
        _outFiles->paths[i] = str_Unix_to_WinW(outFiles.paths[i]);
        str_free(outFiles.paths[i]);
    }

    free(outFiles.paths);
}

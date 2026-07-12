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
    wchar_t* path = path_Unix_to_WinW(_path);
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
        log("CPU or Wine is too old, need fsgsbase support\n");
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

int WP_DECL wine_portal_wide_open_native_for(const wchar_t* _)
{
    // not implemented currently
    return -1;
}

int WP_DECL wine_portal_free(void* ptr)
{
    log("wine_portal_free: %p\n", ptr);
    free(ptr);
    return 0;
}

int WP_DECL wine_portal_utf8_open_native_for(const char* smth)
{
    return portal_open_native_for(smth);
}

static struct PortalFilter make_utf8_pattern_parser(void* ctx, int index)
{
    Utf8Filter* filters = (Utf8Filter*)ctx;
    struct PortalFilter filter;
    filter.name    = rstr_dup(filters[index].name);
    filter.pattern = pattern_parser_init_a(filters[index].pattern);

    return filter;
}

static struct PortalFilter make_wide_pattern_parser(void* ctx, int index)
{
    WideFilter* filters = (WideFilter*)ctx;
    struct PortalFilter filter;
    filter.name    = rstr_W2A(filters[index].name);
    filter.pattern = pattern_parser_init_w(filters[index].pattern);

    return filter;
}

int WP_DECL wine_portal_utf8_open_file_dialog(void* hwndOwner, bool fileMustExist, Utf8Filter* filters, int filtersCount, const char* _initialDir, char** out)
{
    char* initialDir = path_WinA_to_Unix(_initialDir);

    char* ustr = NULL;
    int err = portal_open_file_dialog(&ustr, hwndOwner, fileMustExist, make_utf8_pattern_parser, filters, filtersCount, initialDir);

    str_free(initialDir);

    if (err)
    {
        str_free(ustr);
        *out = NULL;
        return err;
    }

    char* astr = path_Unix_to_WinA(ustr);
    str_free(ustr);

    *out = astr;
    return 0;
}

int WP_DECL wine_portal_wide_save_file_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* _defaultName, const wchar_t* _initialDir, wchar_t** out)
{
    char* defaultName = rstr_W2A(_defaultName);
    char* initialDir = path_WinW_to_Unix(_initialDir);

    char* selectedPath = NULL;
    int err = portal_save_file_dialog(&selectedPath, hwndOwner, make_wide_pattern_parser, filters, filtersCount, defaultName, initialDir);

    str_free(defaultName);
    str_free(initialDir);

    if (err)
    {
        str_free(selectedPath);
        *out = NULL;
        return err;
    }

    wchar_t* wstr = path_Unix_to_WinW(selectedPath);
    str_free(selectedPath);

    *out = wstr;
    return 0;
}

int WP_DECL wine_portal_utf8_save_file_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* defaultName, const char* _initialDir, char** out)
{
    char* initialDir = path_WinA_to_Unix(_initialDir);

    char* selectedPath = NULL;
    int err = portal_save_file_dialog(&selectedPath, hwndOwner, make_utf8_pattern_parser, filters, filtersCount, defaultName, initialDir);

    str_free(initialDir);

    if (err)
    {
        str_free(selectedPath);
        *out = NULL;
        return err;
    }

    char* astr = path_Unix_to_WinA(selectedPath);
    str_free(selectedPath);

    *out = astr;
    return 0;
}

int WP_DECL wine_portal_wide_choose_directory(void* hwndOwner, const wchar_t* _title, const wchar_t* _initialDir, wchar_t** out)
{
    char* title = path_WinW_to_Unix(_title);
    char* initialDir = path_WinW_to_Unix(_initialDir);

    char* selectedPath = NULL;
    int err = portal_choose_directory(&selectedPath, hwndOwner, title, initialDir);

    str_free(title);
    str_free(initialDir);

    if (err)
    {
        str_free(selectedPath);
        *out = NULL;
        return err;
    }

    wchar_t* wstr = path_Unix_to_WinW(selectedPath);
    str_free(selectedPath);

    *out = wstr;
    return 0;
}

int WP_DECL wine_portal_utf8_choose_directory(void* hwndOwner, const wchar_t* _title, const char* _initialDir, char** out)
{
    char* title = path_WinW_to_Unix(_title);
    char* initialDir = path_WinA_to_Unix(_initialDir);

    char* selectedPath = NULL;
    int err = portal_choose_directory(&selectedPath, hwndOwner, title, initialDir);

    str_free(title);
    str_free(initialDir);

    if (err)
    {
        str_free(selectedPath);
        *out = NULL;
        return err;
    }

    char* wstr = path_Unix_to_WinA(selectedPath);
    str_free(selectedPath);

    *out = wstr;
    return 0;
}

int WP_DECL wine_portal_utf8_open_files_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* _initialDir, struct Utf8Files* _outFiles)
{
    char* initialDir = path_WinA_to_Unix(_initialDir);

    struct PortalFiles outFiles = {0, NULL};
    int err = portal_open_files_dialog(&outFiles, hwndOwner, make_utf8_pattern_parser, filters, filtersCount, initialDir);

    str_free(initialDir);

    if (err)
    {
        for (int i = 0; i < outFiles.count; ++i)
        {
            str_free(outFiles.paths[i]);
        }
        free(outFiles.paths);

        _outFiles->count = 0;
        _outFiles->paths = NULL;
        return err;
    }

    _outFiles->count = outFiles.count;
    _outFiles->paths = outFiles.count ? HeapAlloc(GetProcessHeap(), 0, sizeof(char*) * outFiles.count) : NULL;
    for (int i = 0; i < outFiles.count; ++i)
    {
        _outFiles->paths[i] = path_Unix_to_WinA(outFiles.paths[i]);
        str_free(outFiles.paths[i]);
    }

    free(outFiles.paths);
    return 0;
}

int WP_DECL wine_portal_wide_open_files_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* _initialDir, struct Files* _outFiles)
{
    char* initialDir = path_WinW_to_Unix(_initialDir);

    struct PortalFiles outFiles = {0, NULL};
    int err = portal_open_files_dialog(&outFiles, hwndOwner, make_wide_pattern_parser, filters, filtersCount, initialDir);

    str_free(initialDir);

    if (err)
    {
        for (int i = 0; i < outFiles.count; ++i)
        {
            str_free(outFiles.paths[i]);
        }
        free(outFiles.paths);

        _outFiles->count = 0;
        _outFiles->paths = NULL;
        return err;
    }

    _outFiles->count = outFiles.count;
    _outFiles->paths = outFiles.count ? HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t*) * outFiles.count) : NULL;
    for (int i = 0; i < outFiles.count; ++i)
    {
        _outFiles->paths[i] = path_Unix_to_WinW(outFiles.paths[i]);
        str_free(outFiles.paths[i]);
    }

    free(outFiles.paths);
    return 0;
}

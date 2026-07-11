#include "wine_xdg_portal.h"

#include <intrin.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "errno_conv.h"
#include "heavens.h"
#include "log.h"
#include "string_conv.h"
#include "recon.h"

#include <dbus/dbus.h>

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

void* heavensGate = NULL;

static int syscall(int n, int a0, int a1, int a2, int a3, int a4)
{
    int ret;

    __asm__ volatile(
        "pushl %[a4]\n\t"
        "pushl %[a3]\n\t"
        "pushl %[a2]\n\t"
        "pushl %[a1]\n\t"
        "pushl %[a0]\n\t"
        "pushl %[n]\n\t"
        "call *_heavensGate\n\t"
        "addl $24, %%esp\n\t"
        : "=a"(ret)
        : [n]  "a" (n),
          [a0] "r"(a0),
          [a1] "r"(a1),
          [a2] "r"(a2),
          [a3] "r"(a3),
          [a4] "r"(a4)
        : "memory");

    return errnoConv(ret);
}

int syscall0(int n)
{
    return syscall(n, 0, 0, 0, 0, 0);
}

int syscall1(int n, int a0)
{
    return syscall(n, a0, 0, 0, 0, 0);
}

int syscall2(int n, int a0, int a1)
{
    return syscall(n, a0, a1, 0, 0, 0);
}

int syscall3(int n, int a0, int a1, int a2)
{
    return syscall(n, a0, a1, a2, 0, 0);
}

int syscall4(int n, int a0, int a1, int a2, int a3)
{
    return syscall(n, a0, a1, a2, a3, 0);
}

int syscall5(int n, int a0, int a1, int a2, int a3, int a4)
{
    return syscall(n, a0, a1, a2, a3, a4);
}

int getpidx (void)
{
    return syscall0(39);
}

int enosys(void)
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

void WP_DECL wine_portal_wide_open_files_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* initialDir, struct Files* outFiles)
{
    log("wine_portal_wide_open_files_dialog: %p, %p, %d, %ls, %p\n", hwndOwner, filters, filtersCount, initialDir, outFiles);
}

wchar_t* WP_DECL wine_portal_wide_save_file_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* defaultName, const wchar_t* initialDir)
{
    log("wine_portal_wide_save_file_dialog: %p, %p, %d, %ls, %ls\n", hwndOwner, filters, filtersCount, defaultName, initialDir);
    return NULL;
}

wchar_t* WP_DECL wine_portal_wide_choose_directory(void* hwndOwner, const wchar_t* title, const wchar_t* initialDir)
{
    log("wine_portal_wide_choose_directory: %p, %ls, %ls\n", hwndOwner, title, initialDir);
    return NULL;
}

void WP_DECL wine_portal_utf8_open_native_for(const char* _smth)
{
    bool uri = false;
    const char* smth;
    char* path_to_free = NULL;

    if (0 == strncmp(_smth, "http", 4))
    {
        smth = _smth;
        uri = true;
    }
    else
    {
        path_to_free = str_WinA_to_Unix(_smth);
        if (!path_to_free)
        {
            log("Failed to convert path to unix\n");
            return;
        }

        smth = path_to_free;
        log("Converted path to unix: %s\n", smth);
        uri = false;
    }

    DBusError err;
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn)
    {
        log("Failed: %s\n", err.message);
        str_free(path_to_free);
        return;
    }

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.OpenURI",
        uri ? "OpenURI" : "OpenFile");

    const char *parent = "";

    DBusMessageIter iter, dict;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(
        &iter,
        DBUS_TYPE_STRING,
        &parent);

    dbus_message_iter_append_basic(
        &iter,
        DBUS_TYPE_STRING,
        &smth);

    dbus_message_iter_open_container(
        &iter,
        DBUS_TYPE_ARRAY,
        "{sv}",
        &dict);

    dbus_message_iter_close_container(
        &iter,
        &dict);

    DBusMessage *reply =
        dbus_connection_send_with_reply_and_block(
            conn,
            msg,
            -1,
            &err);

    str_free(path_to_free);

    if (!reply) {
        fprintf(gLog, "%s\n", err.message);
        return;
    }

    DBusMessageIter riter;
    dbus_message_iter_init(reply, &riter);

    if (dbus_message_iter_get_arg_type(&riter) == DBUS_TYPE_OBJECT_PATH) {
        const char *handle;
        dbus_message_iter_get_basic(&riter, &handle);
        printf("Request handle: %s\n", handle);
    }

    dbus_message_unref(reply);
    dbus_message_unref(msg);

    dbus_connection_unref(conn);
}

char* WP_DECL wine_portal_utf8_open_file_dialog(void* hwndOwner, bool fileMustExist, Utf8Filter* filters, int filtersCount, const char* initialDir)
{
    return NULL;
}

void WP_DECL wine_portal_utf8_open_files_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* initialDir, struct Utf8Files* outFiles)
{
}

char* WP_DECL wine_portal_utf8_save_file_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* defaultName, const char* initialDir)
{
    return NULL;
}

char* WP_DECL wine_portal_utf8_choose_directory(void* hwndOwner, const wchar_t* title, const char* initialDir)
{
    return NULL;
}

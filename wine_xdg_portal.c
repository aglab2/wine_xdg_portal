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
#include </usr/include/asm/unistd_64.h>

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

// MARK: Syscalls

static int getpidx (void)
{
    return syscall0(39);
}

static int enosys(void)
{
    return syscall0(9999);
}

static int open(const char *pathname, int flags)
{
  return syscall3(__NR_open, (int)pathname, flags, 0);
}

static int close(int fd)
{
  return syscall1(__NR_close, fd);
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

// MARK: DBus
static DBusConnection *dbusConnection = NULL;

static void dbus_init()
{
    if (dbusConnection)
    {
        return;
    }

    DBusError err;
    dbus_error_init(&err);

    dbusConnection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!dbusConnection)
    {
        log("Failed to connect to DBus: %s\n", err.message);
        dbus_error_free(&err);
        return;
    }
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
    dbus_init(); if (!dbusConnection) { return; }

    log("wine_portal_wide_open_files_dialog: %p, %p, %d, %ls, %p\n", hwndOwner, filters, filtersCount, initialDir, outFiles);
}

wchar_t* WP_DECL wine_portal_wide_save_file_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* defaultName, const wchar_t* initialDir)
{
    dbus_init(); if (!dbusConnection) { return NULL; }
    return NULL;
}

void WP_DECL wine_portal_utf8_open_native_for(const char* _smth)
{
    dbus_init(); if (!dbusConnection) { return; }

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

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.OpenURI",
        uri ? "OpenURI" : "OpenFile");

    const char* parent = "";

    DBusMessageIter iter, dict;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent);
    if (uri)
    {
        dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &smth);
    }
    else
    {
        int fd = open(smth, 0200000);
        bool is_dir = (fd >= 0);
        if (fd < 0)
        {
            fd = open(smth, 0);
        }

        if (fd < 0)
        {
            log("Failed to open file: %s\n", smth);
            str_free(path_to_free);
            return;
        }

        dbus_bool_t ok = dbus_message_iter_append_basic(&iter, DBUS_TYPE_UNIX_FD, &fd);

        log("dbus_message_iter_append_basic: %d, ok=%d\n", fd, ok);

        close(fd);
    }

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&iter, &dict);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbusConnection, msg, -1, &err);

    str_free(path_to_free);

    if (!reply) {
        fprintf(gLog, "%s\n", err.message);
        dbus_error_free(&err);
        return;
    }

    DBusMessageIter riter;
    dbus_message_iter_init(reply, &riter);

    if (dbus_message_iter_get_arg_type(&riter) == DBUS_TYPE_OBJECT_PATH) {
        const char *handle;
        dbus_message_iter_get_basic(&riter, &handle);
        log("Request handle: %s\n", handle);
    }

    dbus_message_unref(reply);
    dbus_message_unref(msg);
}

struct PatternParser
{
    const char* pattern;
    char buf[64];
};

static void pattern_parser_init(struct PatternParser* parser, const char* pattern)
{
    parser->pattern = pattern;
}

static const char* pattern_parser_next(struct PatternParser* parser)
{
    int i = 0;
    if (*parser->pattern == '\0')
    {
        return NULL;
    }

    while (*parser->pattern != '\0')
    {
        char c = *parser->pattern++;
        if (c == ';')
        {
            parser->buf[i] = '\0';
            return parser->buf;
        }
        else
        {
            if (i > sizeof(parser->buf) - 2)
            {
                log("Pattern too long: %s?\n", parser->pattern);
                return NULL;
            }

            parser->buf[i++] = c;
        }
    }

    if (i)
    {
        parser->buf[i] = '\0';
        return parser->buf;
    }

    return NULL;
}

static DBusHandlerResult filter_func(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessageIter args, sub_iter, dict_iter, entry_iter, array_iter;
    dbus_uint32_t response_code;

    if (!dbus_message_iter_init(msg, &args))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    wchar_t** result = (wchar_t**)user_data;
    if (*result)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_iter_get_basic(&args, &response_code);
    dbus_message_iter_next(&args);

    wchar_t* path = NULL;

    if (response_code == 0)
    {
        dbus_message_iter_recurse(&args, &dict_iter);
        while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY)
        {
            dbus_message_iter_recurse(&dict_iter, &entry_iter);
            const char *key;
            dbus_message_iter_get_basic(&entry_iter, &key);

            if (0 == strcmp(key, "uris"))
            {
                dbus_message_iter_next(&entry_iter);
                dbus_message_iter_recurse(&entry_iter, &sub_iter);
                dbus_message_iter_recurse(&sub_iter, &array_iter);

                while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRING)
                {
                    char* uri;
                    dbus_message_iter_get_basic(&array_iter, &uri);
                    if (uri)
                    {
                        char* uriPathA = str_URI_to_Unix(uri);
                        if (uriPathA)
                        {
                            path = str_Unix_to_WinW(uriPathA);
                            str_free(uriPathA);
                        }
                    }

                    dbus_message_iter_next(&array_iter);
                }
            }
            dbus_message_iter_next(&dict_iter);
        }
    }
    else
    {
        log("File selection cancelled or failed. Code: %u\n", response_code);
    }

    *result = path;
    return DBUS_HANDLER_RESULT_HANDLED;
}

char* WP_DECL wine_portal_utf8_open_file_dialog(void* hwndOwner, bool fileMustExist, Utf8Filter* filters, int filtersCount, const char* _initialDir)
{
    (void) hwndOwner; (void) fileMustExist;

    dbus_init(); if (!dbusConnection) { return NULL; }

    char* initialDir = str_WinA_to_Unix(_initialDir);

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        "OpenFile");

    wchar_t* selectedPath = NULL;
    dbus_connection_add_filter(dbusConnection, filter_func, &selectedPath, NULL);

    const char* parent = "";
    const char* openTitle = "Open File";

    DBusMessageIter iter, dict;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &openTitle);

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
    {
        DBusMessageIter entry, variant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_mult = "multiple";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_mult);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
        dbus_bool_t multiple = FALSE;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &multiple);
        dbus_message_iter_close_container(&entry, &variant);

        dbus_message_iter_close_container(&dict, &entry);
    }

    if (initialDir)
    {
        DBusMessageIter entry, variant, array;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_token = "current_folder";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_token);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "ay", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "y", &array);
        dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE, &initialDir, strlen(initialDir) + 1);
        dbus_message_iter_close_container(&variant, &array);
        dbus_message_iter_close_container(&entry, &variant);

        dbus_message_iter_close_container(&dict, &entry);
    }

    if (filters && filtersCount > 0)
    {
        DBusMessageIter entry, variant, array, filterArray, filterEntry, filterVariant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_filters = "filters";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_filters);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &filterArray);

        for (int i = 0; i < filtersCount; ++i)
        {
            Utf8Filter* filter = &filters[i];

            dbus_message_iter_open_container(&filterArray, DBUS_TYPE_STRUCT, NULL, &filterEntry);

            dbus_message_iter_append_basic(&filterEntry, DBUS_TYPE_STRING, &filter->name);
            DBusMessageIter pattern_array_iter;
            dbus_message_iter_open_container(&filterEntry, DBUS_TYPE_ARRAY, "(us)", &pattern_array_iter);

            struct PatternParser parser; 
            pattern_parser_init(&parser, filter->pattern);
            const char* pattern;

            while ((pattern = pattern_parser_next(&parser)))
            {
                DBusMessageIter pattern_struct_iter;
                dbus_message_iter_open_container(&pattern_array_iter, DBUS_TYPE_STRUCT, NULL, &pattern_struct_iter);
                dbus_uint32_t type_wildcard = 0;
                dbus_message_iter_append_basic(&pattern_struct_iter, DBUS_TYPE_UINT32, &type_wildcard);
                dbus_message_iter_append_basic(&pattern_struct_iter, DBUS_TYPE_STRING, &pattern);
                dbus_message_iter_close_container(&pattern_array_iter, &pattern_struct_iter);
            }
                
            dbus_message_iter_close_container(&filterEntry, &pattern_array_iter);
            dbus_message_iter_close_container(&filterArray, &filterEntry);
        }

        dbus_message_iter_close_container(&variant, &filterArray);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }

    dbus_message_iter_close_container(&iter, &dict);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbusConnection, msg, -1, &err);

    str_free(initialDir);

    if (!reply)
    {
        log("%s\n", err.message);
        dbus_error_free(&err);
        return NULL;
    }

    DBusMessageIter riter;
    dbus_message_iter_init(reply, &riter);

    if (dbus_message_iter_get_arg_type(&riter) == DBUS_TYPE_OBJECT_PATH) {
        const char *handle;
        dbus_message_iter_get_basic(&riter, &handle);
        log("Request handle: %s\n", handle);
    }

    dbus_message_unref(reply);
    dbus_message_unref(msg);

    log("Waiting for response...\n");
    while (!selectedPath && dbus_connection_read_write_dispatch(dbusConnection, -1))
    {
        // ...
    }

    dbus_connection_remove_filter(dbusConnection, filter_func, &selectedPath);
    log("File selection completed\n");

    char* selectedPathA = str_WinW_to_WinA(selectedPath);
    str_free(selectedPath);
    log("Selected path: %s\n", selectedPathA);
    return selectedPathA;
}

void WP_DECL wine_portal_utf8_open_files_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* initialDir, struct Utf8Files* outFiles)
{
    dbus_init(); if (!dbusConnection) { return; }

}

char* WP_DECL wine_portal_utf8_save_file_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* defaultName, const char* initialDir)
{
    dbus_init(); if (!dbusConnection) { return NULL; }

    return NULL;
}

wchar_t* WP_DECL wine_portal_wide_choose_directory(void* hwndOwner, const wchar_t* title, const wchar_t* initialDir)
{
    dbus_init(); if (!dbusConnection) { return NULL; }

    return NULL;
}

char* WP_DECL wine_portal_utf8_choose_directory(void* hwndOwner, const wchar_t* title, const char* initialDir)
{
    dbus_init(); if (!dbusConnection) { return NULL; }

    return NULL;
}

#include "portal.h"

#include "dbus/dbus.h"
#include "log.h"
#include "heavens.h"
#include "string_conv_a.h"

#include <stdlib.h>
#include <string.h>

#include </usr/include/asm/unistd_64.h>

static DBusConnection *dbusConnection = NULL;

static int open(const char *pathname, int flags)
{
  return syscall3(__NR_open, (int)pathname, flags, 0);
}

static int close(int fd)
{
  return syscall1(__NR_close, fd);
}

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

int portal_open_native_for(const char* _smth)
{
    dbus_init(); if (!dbusConnection) { return -1; }

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
        path_to_free = path_WinA_to_Unix(_smth);
        if (!path_to_free)
        {
            log("Failed to convert path to unix\n");
            return -1;
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

        str_free(path_to_free);
        if (fd < 0)
        {
            return -1;
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

    if (reply)
        dbus_message_unref(reply);

    dbus_message_unref(msg);
    return 0;
}

struct FilterFuncContext
{
    bool completed;
    char* path;
};

static DBusHandlerResult filter_func(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessageIter args, sub_iter, dict_iter, entry_iter, array_iter;
    dbus_uint32_t response_code;

    if (!dbus_message_iter_init(msg, &args))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    struct FilterFuncContext* context = (struct FilterFuncContext*)user_data;
    if (context->completed)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_iter_get_basic(&args, &response_code);
    dbus_message_iter_next(&args);

    char* path = NULL;

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
                        path = path_URI_to_Unix(uri);
                        if (path)
                            break;
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

    context->path = path;
    context->completed = true;
    return DBUS_HANDLER_RESULT_HANDLED;
}

int portal_open_file_dialog(char** selectedPath, void* hwndOwner, bool fileMustExist, MakePatternParser parser, void* ctx, int filtersCount, const char* initialDir)
{
    (void) hwndOwner; (void) fileMustExist;

    dbus_init(); if (!dbusConnection) { return -1; }

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        "OpenFile");

    struct FilterFuncContext filterContext = {0};
    dbus_connection_add_filter(dbusConnection, filter_func, &filterContext, NULL);

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

    if (filtersCount > 0)
    {
        DBusMessageIter entry, variant, array, filterArray, filterEntry, filterVariant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_filters = "filters";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_filters);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &filterArray);

        for (int i = 0; i < filtersCount; ++i)
        {
            const char* pattern;
            struct PortalFilter filter = parser(ctx, i);
            if (!filter.name || !filter.pattern)
            {
                goto cont_free;
            }

            dbus_message_iter_open_container(&filterArray, DBUS_TYPE_STRUCT, NULL, &filterEntry);

            dbus_message_iter_append_basic(&filterEntry, DBUS_TYPE_STRING, &filter.name);
            DBusMessageIter pattern_array_iter;
            dbus_message_iter_open_container(&filterEntry, DBUS_TYPE_ARRAY, "(us)", &pattern_array_iter);

            while ((pattern = pattern_parser_next(filter.pattern)))
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

cont_free:
            str_free(filter.name);
            free(filter.pattern);
        }

        dbus_message_iter_close_container(&variant, &filterArray);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }

    dbus_message_iter_close_container(&iter, &dict);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbusConnection, msg, -1, &err);

    dbus_message_unref(msg);
    if (!reply)
    {
        log("%s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    else
    {
        dbus_message_unref(reply);
    }

    log("Waiting for response...\n");
    while (!filterContext.completed && dbus_connection_read_write_dispatch(dbusConnection, -1))
    {
        // ...
    }

    dbus_connection_remove_filter(dbusConnection, filter_func, &filterContext);
    *selectedPath = filterContext.path;
    log("File selection completed\n");

    return 0;
}

int portal_save_file_dialog(char** selectedPath, void* hwndOwner, MakePatternParser parser, void* ctx, int filtersCount, const char* defaultName, const char* initialDir)
{
    (void) hwndOwner;

    dbus_init(); if (!dbusConnection) { return -1; }

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        "SaveFile");

    struct FilterFuncContext filterContext = {0};
    dbus_connection_add_filter(dbusConnection, filter_func, &filterContext, NULL);

    const char* parent = "";
    const char* saveTitle = "Save File";

    DBusMessageIter iter, dict;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &saveTitle);

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
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

    if (defaultName)
    {
        DBusMessageIter entry, variant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_token = "current_name";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_token);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &defaultName);
        dbus_message_iter_close_container(&entry, &variant);

        dbus_message_iter_close_container(&dict, &entry);
    }

    if (filtersCount > 0)
    {
        DBusMessageIter entry, variant, array, filterArray, filterEntry, filterVariant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_filters = "filters";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_filters);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &filterArray);

        for (int i = 0; i < filtersCount; ++i)
        {
            const char* pattern;
            struct PortalFilter filter = parser(ctx, i);
            if (!filter.name || !filter.pattern)
            {
                goto cont_free;
            }

            dbus_message_iter_open_container(&filterArray, DBUS_TYPE_STRUCT, NULL, &filterEntry);

            dbus_message_iter_append_basic(&filterEntry, DBUS_TYPE_STRING, &filter.name);
            DBusMessageIter pattern_array_iter;
            dbus_message_iter_open_container(&filterEntry, DBUS_TYPE_ARRAY, "(us)", &pattern_array_iter);

            while ((pattern = pattern_parser_next(filter.pattern)))
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

cont_free:
            str_free(filter.name);
            free(filter.pattern);
        }

        dbus_message_iter_close_container(&variant, &filterArray);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }

    dbus_message_iter_close_container(&iter, &dict);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbusConnection, msg, -1, &err);

    dbus_message_unref(msg);
    if (!reply)
    {
        log("%s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    else
    {
        dbus_message_unref(reply);
    }

    log("Waiting for response...\n");
    while (!filterContext.completed && dbus_connection_read_write_dispatch(dbusConnection, -1))
    {
        // ...
    }

    dbus_connection_remove_filter(dbusConnection, filter_func, &filterContext);
    *selectedPath = filterContext.path;
    log("File selection completed\n");

    return 0;
}

int portal_choose_directory(char** selectedPath, void* hwndOwner, const char* title, const char* initialDir)
{
    (void) hwndOwner;

    dbus_init(); if (!dbusConnection) { return -1; }

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        "OpenFile");

    struct FilterFuncContext filterContext = {0};
    dbus_connection_add_filter(dbusConnection, filter_func, &filterContext, NULL);

    const char* parent = "";
    title = title ? title : "Select Directory";

    DBusMessageIter iter, dict;

    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &parent);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &title);

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

    {
        DBusMessageIter entry, variant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_mult = "directory";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_mult);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &variant);
        dbus_bool_t directory = TRUE;
        dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &directory);
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

    dbus_message_iter_close_container(&iter, &dict);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbusConnection, msg, -1, &err);

    if (!reply)
    {
        log("%s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    else
    {
        dbus_message_unref(reply);
    }

    dbus_message_unref(msg);

    log("Waiting for response...\n");
    while (!filterContext.completed && dbus_connection_read_write_dispatch(dbusConnection, -1))
    {
        // ...
    }

    dbus_connection_remove_filter(dbusConnection, filter_func, &filterContext);
    *selectedPath = filterContext.path;
    log("File selection completed\n");

    return 0;
}

struct FilterFuncContextMulti
{
    bool completed;
    struct PortalFiles outFiles;
};

static DBusHandlerResult filter_func_multi(DBusConnection *conn, DBusMessage *msg, void *user_data)
{
    if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessageIter args, sub_iter, dict_iter, entry_iter, array_iter;
    dbus_uint32_t response_code;

    if (!dbus_message_iter_init(msg, &args))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    struct FilterFuncContextMulti* context = (struct FilterFuncContextMulti*)user_data;
    if (context->completed)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    dbus_message_iter_get_basic(&args, &response_code);
    dbus_message_iter_next(&args);

    size_t path_capacity = 8;
    size_t path_count = 0;
    char** paths = malloc(sizeof(char*) * path_capacity);

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
                        char* path = path_URI_to_Unix(uri);
                        if (path)
                        {
                            if (path_count >= path_capacity)
                            {
                                path_capacity *= 2;
                                paths = realloc(paths, sizeof(char*) * path_capacity);
                            }
                            paths[path_count++] = path;
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

    if (0 == path_count)
    {
        free(paths);
        paths = NULL;
    }

    context->completed = true;
    context->outFiles.count = path_count;
    context->outFiles.paths = paths;
    return DBUS_HANDLER_RESULT_HANDLED;
}

int portal_open_files_dialog(struct PortalFiles* outFiles, void* hwndOwner, MakePatternParser parser, void* ctx, int filtersCount, const char* initialDir)
{
    (void) hwndOwner;

    dbus_init(); if (!dbusConnection) { return -1; }

    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.FileChooser",
        "OpenFile");

    struct FilterFuncContextMulti filterContext = {0};

    dbus_connection_add_filter(dbusConnection, filter_func_multi, &filterContext, NULL);

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
        dbus_bool_t multiple = TRUE;
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

    // TODO: factor some common pieces out
    if (filtersCount > 0)
    {
        DBusMessageIter entry, variant, array, filterArray, filterEntry, filterVariant;
        dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
        const char *key_filters = "filters";
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key_filters);

        dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "a(sa(us))", &variant);
        dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "(sa(us))", &filterArray);

        for (int i = 0; i < filtersCount; ++i)
        {
            const char* pattern;
            struct PortalFilter filter = parser(ctx, i);
            if (!filter.name || !filter.pattern)
            {
                goto cont_free;
            }

            dbus_message_iter_open_container(&filterArray, DBUS_TYPE_STRUCT, NULL, &filterEntry);

            dbus_message_iter_append_basic(&filterEntry, DBUS_TYPE_STRING, &filter.name);
            DBusMessageIter pattern_array_iter;
            dbus_message_iter_open_container(&filterEntry, DBUS_TYPE_ARRAY, "(us)", &pattern_array_iter);

            while ((pattern = pattern_parser_next(filter.pattern)))
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

cont_free:
            str_free(filter.name);
            free(filter.pattern);
        }

        dbus_message_iter_close_container(&variant, &filterArray);
        dbus_message_iter_close_container(&entry, &variant);
        dbus_message_iter_close_container(&dict, &entry);
    }

    dbus_message_iter_close_container(&iter, &dict);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(dbusConnection, msg, -1, &err);

    dbus_message_unref(msg);
    if (!reply)
    {
        log("%s\n", err.message);
        dbus_error_free(&err);
        return -1;
    }
    else
    {
        dbus_message_unref(reply);
    }

    log("Waiting for response...\n");
    while (!filterContext.completed && dbus_connection_read_write_dispatch(dbusConnection, -1))
    {
        // ...
    }

    dbus_connection_remove_filter(dbusConnection, filter_func_multi, &filterContext);
    *outFiles = filterContext.outFiles;
    log("File selection completed\n");

    return 0;
}

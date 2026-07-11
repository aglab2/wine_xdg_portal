#include "string_conv.h"

#include <windows.h>

static LPSTR (*CDECL wine_get_unix_file_name_ptr)(LPCWSTR) = NULL;
static LPWSTR (*CDECL wine_get_dos_file_name_ptr)(LPCSTR) = NULL;

int str_conv_init()
{
    wine_get_unix_file_name_ptr = (void*) GetProcAddress(GetModuleHandleA("KERNEL32"), "wine_get_unix_file_name");
    wine_get_dos_file_name_ptr  = (void*) GetProcAddress(GetModuleHandleA("KERNEL32"), "wine_get_dos_file_name");

    if (!wine_get_unix_file_name_ptr || !wine_get_dos_file_name_ptr)
    {
        return -1;
    }

    return 0;
}

char* str_WinA_to_Unix(const char* path)
{
    if (!path)
    {
        return NULL;
    }

    // convert to wide string first
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    if (len == 0)
    {
        return NULL;
    }

    wchar_t* wpath = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, len * sizeof(wchar_t));
    if (!wpath)
    {
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, len);
    char* result = wine_get_unix_file_name_ptr(wpath);
    HeapFree(GetProcessHeap(), 0, wpath);
    return result;
}

char* str_WinW_to_Unix(const wchar_t* path)
{
    if (!path)
    {
        return NULL;
    }

    return wine_get_unix_file_name_ptr(path);
}

wchar_t* str_Unix_to_WinW(const char* path)
{
    if (!path)
    {
        return NULL;
    }

    return wine_get_dos_file_name_ptr(path);
}

char* str_Unix_to_WinA(const char* path)
{
    if (!path)
    {
        return NULL;
    }

    wchar_t* wpath = wine_get_dos_file_name_ptr(path);
    if (!wpath)
    {
        return NULL;
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL);
    if (len == 0)
    {
        HeapFree(GetProcessHeap(), 0, wpath);
        return NULL;
    }

    char* result = (char*)HeapAlloc(GetProcessHeap(), 0, len * sizeof(char));
    if (!result)
    {
        HeapFree(GetProcessHeap(), 0, wpath);
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, result, len, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, wpath);
    return result;
}

char* str_WinW_to_WinA(const wchar_t* path)
{
    if (!path)
    {
        return NULL;
    }

    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, NULL, 0, NULL, NULL);
    if (len == 0)
    {
        return NULL;
    }

    char* result = (char*)HeapAlloc(GetProcessHeap(), 0, len * sizeof(char));
    if (!result)
    {
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, path, -1, result, len, NULL, NULL);
    return result;
}

char* str_URI_to_Unix(const char* uri)
{
    if (strncmp(uri, "file://", 7) != 0)
    {
        return NULL;
    }

    size_t len = strlen(uri);
    char* path = (char*)HeapAlloc(GetProcessHeap(), 0, len * sizeof(char));
    if (!path)
    {
        return NULL;
    }

    size_t needed = 0;
    char* dst = path;
    for (const char* src = uri + 7; *src; src++)
    {
        char next;
        if (*src == '%' && isxdigit(*(src + 1)) && isxdigit(*(src + 2)))
        {
            char buf[3];
            memcpy(buf, src + 1, 2 * sizeof(char));
            buf[2] = 0;

            int ih = strtol(buf, NULL, 16);

            src += 2; /* Advance to end of escape */
            next = (char) ih;
        }
        else
        {
            next = *src;
        }

        *dst++ = next;
    }

    *dst = '\0';

    return path;
}

void str_free(void* path)
{
    if (!path)
    {
        return;
    }

    HeapFree(GetProcessHeap(), 0, (LPVOID)path);
}

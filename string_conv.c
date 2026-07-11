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
    return wine_get_unix_file_name_ptr(path);
}

wchar_t* str_Unix_to_WinW(const char* path)
{
    return wine_get_dos_file_name_ptr(path);
}

void str_free(void* path)
{
    if (!path)
    {
        return;
    }

    HeapFree(GetProcessHeap(), 0, (LPVOID)path);
}

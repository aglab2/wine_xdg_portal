#pragma once

#include <stdbool.h>
#include <windows.h>

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

#ifdef WP_DLL
    #define WP_API __attribute__((dllexport))
#else
    #define WP_API __attribute__((dllimport))
#endif

#define WP_DECL __cdecl

WP_API int WP_DECL wine_portal_init(void);
WP_API int WP_DECL wine_portal_wide_open_native_for(const wchar_t* path);
WP_API int WP_DECL wine_portal_free(void* ptr);
WP_API int WP_DECL wine_portal_wide_open_files_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* initialDir, struct Files* outFiles);
WP_API int WP_DECL wine_portal_wide_save_file_dialog(void* hwndOwner, WideFilter* filters, int filtersCount, const wchar_t* defaultName, const wchar_t* initialDir, wchar_t** out);
WP_API int WP_DECL wine_portal_wide_choose_directory(void* hwndOwner, const wchar_t* title, const wchar_t* initialDir, wchar_t** out);
WP_API int WP_DECL wine_portal_utf8_open_native_for(const char* path);
WP_API int WP_DECL wine_portal_utf8_open_file_dialog(void* hwndOwner, bool fileMustExist, Utf8Filter* filters, int filtersCount, const char* initialDir, char** out);
WP_API int WP_DECL wine_portal_utf8_open_files_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* initialDir, struct Utf8Files* outFiles);
WP_API int WP_DECL wine_portal_utf8_save_file_dialog(void* hwndOwner, Utf8Filter* filters, int filtersCount, const char* defaultName, const char* initialDir, char** out);
WP_API int WP_DECL wine_portal_utf8_choose_directory(void* hwndOwner, const wchar_t* title, const char* initialDir, char** out);

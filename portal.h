#pragma once

#include "pattern_parser.h"

#include <stdbool.h>

struct PortalFilter
{
    char* name;
    struct PatternParser* pattern;
};

struct PortalFiles
{
    int count;
    char** paths;
};

typedef struct PortalFilter (*MakePatternParser)(void* ctx, int index);

// In Win domain, accepts only Win UTF8 for now
int portal_open_native_for(const char* something);

// Functions in Unix path domain
int portal_open_file_dialog(char** out, void* hwndOwner, bool fileMustExist, MakePatternParser, void* ctx, int filtersCount, const char* initialDir);
int portal_save_file_dialog(char** out, void* hwndOwner, MakePatternParser, void* ctx, int filtersCount, const char* defaultName, const char* initialDir);
int portal_choose_directory(char** out, void* hwndOwner, const char* title, const char* initialDir);
int portal_open_files_dialog(struct PortalFiles* out, void* hwndOwner, MakePatternParser, void* ctx, int filtersCount, const char* initialDir);

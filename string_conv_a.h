#pragma once

char* path_WinA_to_Unix(const char*);
char* path_Unix_to_WinA(const char*);
char* path_URI_to_Unix(const char* uri);

char* rstr_dup(const char* str);

void str_free(void* path);

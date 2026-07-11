#pragma once

char* str_WinA_to_Unix(const char*);
char* str_Unix_to_WinA(const char*);
char* str_URI_to_Unix(const char* uri);

void str_free(void* path);

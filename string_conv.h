#pragma once

#include <windows.h>

int str_conv_init();

char* str_WinA_to_Unix(const char*);
char* str_WinW_to_Unix(const wchar_t*);
wchar_t* str_Unix_to_WinW(const char*);

void str_free(void* path);

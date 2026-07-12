#pragma once

#include "string_conv_a.h"

#include <windows.h>

int str_conv_init();

char* path_WinW_to_Unix(const wchar_t*);
char* path_WinW_to_WinA(const wchar_t*);
wchar_t* path_Unix_to_WinW(const char*);

char* rstr_W2A(const wchar_t*);

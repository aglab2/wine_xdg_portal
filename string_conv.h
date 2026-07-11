#pragma once

#include "string_conv_a.h"

#include <windows.h>

int str_conv_init();

char* str_WinW_to_Unix(const wchar_t*);
char* str_WinW_to_WinA(const wchar_t*);
wchar_t* str_Unix_to_WinW(const char*);

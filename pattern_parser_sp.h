#pragma once

#include <windows.h>

#include "pattern_parser.h"

struct PatternParser* pattern_parser_init_a(const char* pattern);
struct PatternParser* pattern_parser_init_w(const wchar_t* pattern);

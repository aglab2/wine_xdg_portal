#pragma once

#include <stdint.h>

struct LibcFunctions
{
    uint64_t syscall;
    uint64_t errno_location;
};

int find_pid(const char* canary);
struct LibcFunctions find_libc(int pid);
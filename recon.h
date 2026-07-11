#pragma once

#include <stdint.h>

struct LibcFunctions
{
    uint64_t syscall;
    // TODO: technically not needed but let's keep it like this for now
    uint64_t errno_location;
    int errno_fs_offset;
};

int find_pid(const char* canary);
struct LibcFunctions find_libc(int pid);

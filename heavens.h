#pragma once

#include "recon.h"

int hg_setup(struct LibcFunctions libc);

int syscall0(int n);
int syscall1(int n, int a0);
int syscall2(int n, int a0, int a1);
int syscall3(int n, int a0, int a1, int a2);
int syscall4(int n, int a0, int a1, int a2, int a3);
int syscall5(int n, int a0, int a1, int a2, int a3, int a4);

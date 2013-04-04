// GIGO Copyright (C) 2012 Robert Kooima
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITH-
// OUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.

#ifndef GIGO_ETC_H
#define GIGO_ETC_H

#include <complex.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//------------------------------------------------------------------------------

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

//------------------------------------------------------------------------------

// Compute the integer log base 2.

static inline int log2i(unsigned int n)
{
    unsigned int r, s;

    r = (n > 0xFFFFUL) << 4; n >>= r;
    s = (n > 0xFFUL  ) << 3; n >>= s; r |= s;
    s = (n > 0xFUL   ) << 2; n >>= s; r |= s;
    s = (n > 0x3UL   ) << 1; n >>= s; r |= s;

    return (int) (r | (n >> 1));
}

// Determine whether the argument is a power of 2.

static inline bool ispow2(unsigned int n)
{
    return n && !(n & (n - 1));
}

// Compute the unit norm complex value with argument theta.

static inline float complex cisf(float theta)
{
    return cosf(theta) + I * sinf(theta);
}

// Determine whether arg ends with ext.

static inline bool isext(const char *arg, const char *ext)
{
    size_t arglen = strlen(arg);
    size_t extlen = strlen(ext);

    if (arglen < extlen)
        return false;
    else
        return (strcmp(arg + arglen - extlen, ext) == 0);
}

//------------------------------------------------------------------------------

static inline long long min(long long a, long long b)
{
    return (a < b) ? a : b;
}

static inline long long max(long long a, long long b)
{
    return (a > b) ? a : b;
}

//------------------------------------------------------------------------------

#include <sys/time.h>
#include <sys/resource.h>

// Print time as hours, minutes, and seconds.

static inline void printhms(double s)
{
    int h = (int) s / 3600; s -= h * 3600;
    int m = (int) s /   60; s -= m *   60;

    if (h) printf("%dh", h);
    if (m) printf("%dm", m);

    printf("%.3fs", s);
}

// Print the real time elapsed and the process' user and system times.

static inline void printtime(struct timeval *t0, struct timeval *t1)
{
    struct rusage ru;

    double s = (t1->tv_sec  - t0->tv_sec) +
               (t1->tv_usec - t0->tv_usec) / 1000000.0;

    printf("\treal: ");
    printhms(s);

    if (getrusage(RUSAGE_SELF, &ru) == 0)
    {
        printf("  user: ");
        printhms(ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1000000.0);
        printf("  sys: ");
        printhms(ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1000000.0);
    }
    printf("\n");
}

//------------------------------------------------------------------------------

#endif

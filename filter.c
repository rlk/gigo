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

#include <complex.h>
#include <getopt.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#include "img.h"
#include "err.h"
#include "etc.h"
#include "fft.h"

//------------------------------------------------------------------------------

static inline int range_rect(float r, float w, int m)
{
    return (int) ceil(r);
}

static inline int range_tri(float r, float w, int m)
{
    return (int) ceil(r + w / 2);
}

static inline int range_hann(float r, float w, int m)
{
    return (int) ceil(r + w / 2);
}

static inline int range_norm(float r, float w, int m)
{
    return m;
}

static inline int range_gauss(float r, float w, int m)
{
    return m;
}

static inline int range_butter(float r, float w, int m)
{
    return m;
}

static inline int range(int op, float r, float w, int m)
{
    switch (op)
    {
        case 'R': return range_rect  (r, w, m);
        case 'T': return range_tri   (r, w, m);
        case 'H': return range_hann  (r, w, m);
        case 'g': return range_norm  (r, w, m);
        case 'G': return range_gauss (r, w, m);
        case 'B': return range_butter(r, w, m);
    }
    return (int) r;
}

//------------------------------------------------------------------------------

static inline float filter_rect(float k, float r, float w)
{
    return (k > r) ? 0.f : 1.f;
}

static inline float filter_tri(float k, float r, float w)
{
    if (k > r + w / 2) return 0.f;
    if (k < r - w / 2) return 1.f;
    return 0.5f - k / w + r / w;
}

static inline float filter_hann(float k, float r, float w)
{
    return 0.5f - cosf(M_PI * filter_tri(k, r, w)) / 2.f;
}

static inline float filter_norm(float k, float r, float w)
{
    const float a =  1.f / (2.f * M_PI * r * r);
    const float b = -1.f / (2.f        * r * r);
    return a * expf(b * k * k);
}

static inline float filter_gauss(float k, float r, float w)
{
    return expf(-0.5f * (k * k) / (r * r));
}

static inline float filter_butter(float k, float r, float w)
{
    return 1.f / (1.f + powf(k / r, 2 * w));
}

static inline float filter(int op, float k, float r, float w)
{
    switch (op)
    {
        case 'R': return filter_rect  (k, r, w);
        case 'T': return filter_tri   (k, r, w);
        case 'H': return filter_hann  (k, r, w);
        case 'g': return filter_norm  (k, r, w);
        case 'G': return filter_gauss (k, r, w);
        case 'B': return filter_butter(k, r, w);
    }
    return 1.f;
}

//------------------------------------------------------------------------------

static inline float dist(int i, int j, int y, int x, float a)
{
    const float di = (i - y);
    const float dj = (j - x);

    return sqrtf(di * di + dj * dj * a * a);
}

// Apply the inverted filter. Assume anything outside the bounding box of the
// window will remain one.

static void inverse(img *d, int op, int x, int y, float r, float w)
{
    const int N = 1 << d->n;
    const int M = 1 << d->m;
    const int c =      d->p;

    // Compute the size and aspect of the bounding box.

    const int n = range(op, r, w, N);
    int m;

    if (op == 'g')
        m = n;
    else
        m = (d->m > d->n) ? n << (d->m - d->n)
                          : n >> (d->n - d->m);

    const float a = (float) n / (float) m;

    // Iterate over all pixels in the box, computing the filter value for each.

    int   i;
    int   j;
    int   k;
    float t;

    #pragma omp parallel for private(j, k, t)
    for     (i = max(0, y - n); i <= min(N - 1, y + n); i++)
        for (j = max(0, x - m); j <= min(M - 1, x + m); j++)
        {
            t = 1.f - filter(op, dist(i, j, y, x, a), r, w);

            for (k = 0; k < c; k++)
                imgz(d, i, j)[k] *= t;
        }
}

// Apply the filter. Assume that anything outside the bounding box of the window
// will be zeroed.

static void forward(img *d, int op, int x, int y, float r, float w)
{
    const int N = 1 << d->n;
    const int M = 1 << d->m;
    const int c =      d->p;

    // Compute the size and aspect of the bounding box.

    const int n = range(op, r, w, N);
    int m;

    if (op == 'g')
        m = n;
    else
        m = (d->m > d->n) ? n << (d->m - d->n)
                          : n >> (d->n - d->m);

    const float a = (float) n / (float) m;

    // Iterate over all pixels in the box, computing the filter value for each.

    int i;
    int j;
    int k;

    #pragma omp parallel for private(j, k)
    for         (i = 0; i < N; i++)
        for     (j = 0; j < M; j++)
            for (k = 0; k < c; k++)

                if (abs(i - y) > n || abs(j - x) > m)
                    imgz(d, i, j)[k]  = 0.0f;
                else
                    imgz(d, i, j)[k] *= filter(op, dist(i, j, y, x, a), r, w);
}

//------------------------------------------------------------------------------

static bool proc(const char *dst, int l, int n, int m, int p,
                 int op, bool i, int x, int y, float r, float w)
{
    bool ok = false;
    img  *d;

    if ((n && m && p) || imgargs(dst, &n, &m, &p))
    {
        if (x == INT_MAX) x = 1 << (m - 1);
        if (y == INT_MAX) y = 1 << (n - 1);

        if ((d = imgopen(dst, l, n, m, p)))
        {
            if (i)
                inverse(d, op, x, y, r, w);
            else
                forward(d, op, x, y, r, w);

            ok = true;
            imgclose(d);
        }
    }
    else apperr("Failed to guess image parameters");
    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-tRTHgGBi] "
                               "[-x X] "
                               "[-y Y] "
                               "[-r radius] "
                               "[-w width] "
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] image\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool  ok = false;
    bool  t  = false;
    bool  i  = false;
    int   op = 0;
    int   l  = 0;
    int   n  = 0;
    int   m  = 0;
    int   p  = 0;
    int   x  = INT_MAX;
    int   y  = INT_MAX;
    float r  = 0.f;
    float w  = 0.f;
    int   o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "l:n:m:p:x:y:r:w:RTHgGBIt")) != -1)
        switch (o)
        {
            case 'l': l = strtol(optarg, 0, 0); break;
            case 'n': n = strtol(optarg, 0, 0); break;
            case 'm': m = strtol(optarg, 0, 0); break;
            case 'p': p = strtol(optarg, 0, 0); break;
            case 'x': x = strtol(optarg, 0, 0); break;
            case 'y': y = strtol(optarg, 0, 0); break;
            case 'r': r = strtof(optarg, 0);    break;
            case 'w': w = strtof(optarg, 0);    break;

            case 'R': op = o; break;
            case 'T': op = o; break;
            case 'H': op = o; break;
            case 'g': op = o; break;
            case 'G': op = o; break;
            case 'B': op = o; break;

            case 'I': i = true; break;
            case 't': t = true; break;
            case '?':
            default : return usage(argv[0]);
        }

    setexe(argv[0]);

    struct timeval t0;
    struct timeval t1;

    gettimeofday(&t0, 0);
    {
        if (optind + 1 == argc)
        {
            ok = proc(argv[optind], l, n, m, p, op, i, x, y, r, w);
        }
        else return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

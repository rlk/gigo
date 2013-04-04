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
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>

#include "img.h"
#include "err.h"
#include "etc.h"
#include "fft.h"

//------------------------------------------------------------------------------

static float circle(float dd, float rr)
{
//  return dd < rr ? 1.f / (M_PI * rr) : 0.f;
    return dd < rr ? 1.f : 0.f;
}

static float gauss(float dd, float rr)
{
    const float a =  1.f / (2.f * M_PI * rr);
    const float b = -1.f / (2.f        * rr);

    return a * expf(dd * b);
}

//------------------------------------------------------------------------------

static inline float dd(img *d, int r, int c, int i, int j)
{
    const int n = 1 << (d->n - 1);
    const int m = 1 << (d->m - 1);

    const int y = (((r * d->s + i) ^ n) - n);
    const int x = (((c * d->s + j) ^ m) - m);

    return y * y + x * x;
}

static void calc(img *d, float rr, int op)
{
    int   r;
    int   c;
    int   i;
    int   j;
    int   k;
    float t;
    float T = 0.f;

    #pragma omp parallel for private(c, i, j, k, t) reduction(+:T)
    for             (r = 0; r < d->h; r++)
        for         (c = 0; c < d->w; c++)
            for     (i = 0; i < d->s; i++)
                for (j = 0; j < d->s; j++)
                {
                    switch (op)
                    {
                    case 'c': t = circle(dd(d, r, c, i, j), rr); break;
                    case 'g': t = gauss (dd(d, r, c, i, j), rr); break;
                    }

                    T += t;

                    for (k = 0; k < d->p; ++k)
                        imgbuf(d, r, c, i, j)[k] = t;
                }

    #pragma omp parallel for private(c, i, j, k)
    for                 (r = 0; r < d->h; r++)
        for             (c = 0; c < d->w; c++)
            for         (i = 0; i < d->s; i++)
                for     (j = 0; j < d->s; j++)
                    for (k = 0; k < d->p; ++k)
                        imgbuf(d, r, c, i, j)[k] /= T;
}

static bool proc(const char *dst, int l, int n, int m, int p, float r, int op)
{
    bool ok = false;
    img *d;

    if ((n && m && p) || imgargs(dst, &n, &m, &p))
    {
        if ((d = imgopen(dst, l, n, m, p)))
        {
            calc(d, r * r, op);
            imgclose(d);
            ok = true;
        }
    }
    else apperr("Failed to guess image parameters");
    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-tgc] "
                               "[-r radius] "
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] dst\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool  t  = false;
    bool  ok = false;
    int   op = 0;
    int   l  = 0;
    int   n  = 0;
    int   m  = 0;
    int   p  = 0;
    float r  = 0;
    int   o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "l:n:m:p:r:gct")) != -1)
        switch (o)
        {
            case 'l': l = (int) strtol(optarg, 0, 0); break;
            case 'n': n = (int) strtol(optarg, 0, 0); break;
            case 'm': m = (int) strtol(optarg, 0, 0); break;
            case 'p': p = (int) strtol(optarg, 0, 0); break;
            case 'r': r =       strtof(optarg, 0);    break;

            case 'c': op = o; break;
            case 'g': op = o; break;

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
            ok = proc(argv[optind], l, n, m, p, r, op);
        else
            return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

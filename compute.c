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

static float complex cmin(float complex a, float complex b)
{
    return (cabs(a) < cabs(b)) ? a : b;
}

static float complex cmax(float complex a, float complex b)
{
    return (cabs(a) > cabs(b)) ? a : b;
}

//------------------------------------------------------------------------------

static float scalar =  1.f;
static float interp =  0.f;
static float wiener =  0.f;
static float range0 = -FLT_MAX;
static float range1 =  FLT_MAX;

static void op_add(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] += s[i];
}

static void op_sub(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] -= s[i];
}

static void op_mul(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] *= s[i];
}

static void op_div(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] /= s[i];
}

static void op_pow(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = cpow(d[i], s[i]);
}

static void op_min(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = cmin(d[i], s[i]);
}

static void op_max(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = cmax(d[i], s[i]);
}

static void op_interp(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        const float m = cabs(d[i]) * (1.f - interp) + cabs(s[i]) * interp;
        const float p = carg(d[i]) * (1.f - interp) + carg(s[i]) * interp;

        d[i] = p * cisf(m);
    }
}

static void op_wiener(float complex *d, float complex *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        const float m = cabs(s[i]);

        d[i] = (d[i] * (m * m))
             / (s[i] * (m * m + wiener));
     }
}

static void op_scale(float complex *d, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] *= scalar;
}

static void op_range(float complex *d, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = ((cabs(d[i]) >= range0) &&
                (cabs(d[i]) <= range1)) ? 1.f : 0.f;
}

static void op_inv(float complex *d, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = 1.f - cabs(d[i]);
}

static void op_exp(float complex *d, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = cexp(d[i]);
}

static void op_log(float complex *d, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = clog(d[i]);
}

static void op_test(float complex *d, size_t n)
{
    for (size_t i = 0; i < n; i++)
        d[i] = cabs(d[i]) > 0.f ? 1.f : 0.f;
}

//------------------------------------------------------------------------------

static bool calc1(img *d, int op)
{
    const size_t n = (size_t) d->s * d->s * d->p;

    float complex *D;

    int y;
    int x;

    #pragma omp parallel for private(x, D)
    for     (y = 0; y < d->h; y++)
        for (x = 0; x < d->w; x++)
        {
            D = imgz(d, y * d->s, x * d->s);

            switch (op)
            {
                case 's': op_scale(D, n); break;
                case 'r': op_range(D, n); break;
                case 'R': op_range(D, n); break;
                case 'I': op_inv  (D, n); break;
                case 'E': op_exp  (D, n); break;
                case 'L': op_log  (D, n); break;
                case 'N': op_test (D, n); break;
            }
        }
    return true;
}

static bool calc2(img *d, img *s, int op)
{
    const size_t n = (size_t) d->s * d->s * d->p;

    float complex *D;
    float complex *S;

    int y;
    int x;

    #pragma omp parallel for private(x, D, S)
    for     (y = 0; y < d->h; y++)
        for (x = 0; x < d->w; x++)
        {
            D = imgz(d, y * d->s, x * d->s);
            S = imgz(s, y * d->s, x * d->s);

            switch (op)
            {
                case 'A': op_add   (D, S, n); break;
                case 'S': op_sub   (D, S, n); break;
                case 'M': op_mul   (D, S, n); break;
                case 'D': op_div   (D, S, n); break;
                case 'P': op_pow   (D, S, n); break;
                case 'x': op_min   (D, S, n); break;
                case 'X': op_max   (D, S, n); break;
                case 'i': op_interp(D, S, n); break;
                case 'w': op_wiener(D, S, n); break;
            }
        }
    return true;
}

//------------------------------------------------------------------------------

static bool proc1(const char *dst, int l, int n, int m, int p, int op)
{
    bool ok = false;
    img  *d;

    if ((n && m && p) || imgargs(dst, &n, &m, &p))
    {
        if ((d = imgopen(dst, l, n, m, p)))
        {
            ok = calc1(d, op);
            imgclose(d);
        }
    }
    else apperr("Failed to guess image parameters");
    return ok;
}

static bool proc2(const char *dst,
                  const char *src, int l, int n, int m, int p, int op)
{
    bool ok = false;
    img  *s;
    img  *d;

    if ((n && m && p) || imgargs(dst, &n, &m, &p))
    {
        if ((d = imgopen(dst, l, n, m, p)))
        {
            if ((s = imgopen(src, l, n, m, p)))
            {
                ok = calc2(d, s, op);
                imgclose(s);
            }
            imgclose(d);
        }
    }
    else apperr("Failed to guess image parameters");
    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-t]"
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] op [arg] dst [src]\n"
                     "\t        add:  -A       dst src\n"
                     "\t    subract:  -S       dst src\n"
                     "\t   multiply:  -M       dst src\n"
                     "\t     divide:  -D       dst src\n"
                     "\t      power:  -P       dst src\n"
                     "\t        min:  -x       dst src\n"
                     "\t        max:  -X       dst src\n"
                     "\t     invert:  -I       dst\n"
                     "\t  logarithm:  -L       dst\n"
                     "\texponential:  -E       dst\n"
                     "\t   non-zero:  -N       dst\n"
                     "\tinterpolate:  -i coeff dst src\n"
                     "\t     wiener:  -w coeff dst src\n"
                     "\t      scale:  -s coeff dst\n"
                     "\tthreshold >:  -r min   dst\n"
                     "\tthreshold <:  -R max   dst\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool ok = false;
    bool t  = false;
    int  op = 0;
    int  c  = 0;
    int  l  = 5;
    int  n  = 0;
    int  m  = 0;
    int  p  = 0;
    int  o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "l:n:m:p:ASMDPILENxXi:w:s:r:R:t")) != -1)
        switch (o)
        {
            case 'l': l = (int) strtol(optarg, 0, 0); break;
            case 'n': n = (int) strtol(optarg, 0, 0); break;
            case 'm': m = (int) strtol(optarg, 0, 0); break;
            case 'p': p = (int) strtol(optarg, 0, 0); break;

            case 'A': op = o; c = 2; break;
            case 'S': op = o; c = 2; break;
            case 'M': op = o; c = 2; break;
            case 'D': op = o; c = 2; break;
            case 'P': op = o; c = 2; break;
            case 'x': op = o; c = 2; break;
            case 'X': op = o; c = 2; break;
            case 'I': op = o; c = 1; break;
            case 'L': op = o; c = 1; break;
            case 'E': op = o; c = 1; break;
            case 'N': op = o; c = 1; break;

            case 'i': op = o; c = 2; interp = strtof(optarg, 0); break;
            case 'w': op = o; c = 2; wiener = strtof(optarg, 0); break;
            case 's': op = o; c = 1; scalar = strtof(optarg, 0); break;
            case 'r': op = o; c = 1; range0 = strtof(optarg, 0); break;
            case 'R': op = o; c = 1; range1 = strtof(optarg, 0); break;

            case 't': t = true; break;

            case '?':
            default : return usage(argv[0]);
        }

    setexe(argv[0]);

    struct timeval t0;
    struct timeval t1;

    gettimeofday(&t0, 0);
    {
        // Launch a binary operation...

        if      (c == 2 && optind + 2 == argc)
            ok = proc2(argv[optind], argv[optind + 1], l, n, m, p, op);

        // ... or a unary operation...

        else if (c == 1 && optind + 1 == argc)
            ok = proc1(argv[optind],                   l, n, m, p, op);

        // ... or punt.

        else return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

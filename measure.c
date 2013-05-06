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

//------------------------------------------------------------------------------

static float op_sum(img *s, int k)
{
    int   r;
    int   c;
    int   i;
    int   j;
    float v = 0.0f;

   #pragma omp parallel for private(c, i, j, k) reduction(+:v)
    for             (r = 0; r < s->h; r++)
        for         (c = 0; c < s->w; c++)
            for     (i = 0; i < s->s; i++)
                for (j = 0; j < s->s; j++)
                    v += cabs(imgbuf(s, r, c, i, j)[k]);
    return v;
}

static float op_cmin(img *s, int k)
{
    int   r;
    int   c;
    int   i;
    int   j;
    float v = FLT_MAX;

//  #pragma omp parallel for private(c, i, j, k) reduction(min:v)
    for             (r = 0; r < s->h; r++)
        for         (c = 0; c < s->w; c++)
            for     (i = 0; i < s->s; i++)
                for (j = 0; j < s->s; j++)
                    if (v > cabs(imgbuf(s, r, c, i, j)[k]))
                        v = cabs(imgbuf(s, r, c, i, j)[k]);
    return v;
}

static float op_cmax(img *s, int k)
{
    int   r;
    int   c;
    int   i;
    int   j;
    float v = -FLT_MAX;

//  #pragma omp parallel for private(c, i, j, k) reduction(min:v)
    for             (r = 0; r < s->h; r++)
        for         (c = 0; c < s->w; c++)
            for     (i = 0; i < s->s; i++)
                for (j = 0; j < s->s; j++)
                    if (v < cabs(imgbuf(s, r, c, i, j)[k]))
                        v = cabs(imgbuf(s, r, c, i, j)[k]);
    return v;
}

static float op_rmin(img *s, int k)
{
    int   r;
    int   c;
    int   i;
    int   j;
    float v = FLT_MAX;

//  #pragma omp parallel for private(c, i, j, k) reduction(min:v)
    for             (r = 0; r < s->h; r++)
        for         (c = 0; c < s->w; c++)
            for     (i = 0; i < s->s; i++)
                for (j = 0; j < s->s; j++)
                    if (v > creal(imgbuf(s, r, c, i, j)[k]))
                        v = creal(imgbuf(s, r, c, i, j)[k]);
    return v;
}

static float op_rmax(img *s, int k)
{
    int   r;
    int   c;
    int   i;
    int   j;
    float v = -FLT_MAX;

//  #pragma omp parallel for private(c, i, j, k) reduction(min:v)
    for             (r = 0; r < s->h; r++)
        for         (c = 0; c < s->w; c++)
            for     (i = 0; i < s->s; i++)
                for (j = 0; j < s->s; j++)
                    if (v < creal(imgbuf(s, r, c, i, j)[k]))
                        v = creal(imgbuf(s, r, c, i, j)[k]);
    return v;
}

static bool proc(const char *dst, int l, int n, int m, int p, int op)
{
    bool ok = false;
    img *s;
    int  k;

    if ((n && m && p) || imgargs(dst, &n, &m, &p))
    {
        if ((s = imgopen(dst, l, n, m, p)))
        {
            for (k = 0; k < s->p; k++)
            {
                float v = 0.0f;

                switch (op)
                {
                    case 's': v = op_sum(s, k); break;
                    case 'x': v = op_cmin(s, k); break;
                    case 'X': v = op_cmax(s, k); break;
                    case '0': v = op_rmin(s, k); break;
                    case '1': v = op_rmax(s, k); break;
                }

                printf("%e\n", v);
            }
            imgclose(s);
            ok = true;
        }
    }
    else apperr("Failed to guess image parameters");
    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-t] "
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] op src\n"
                     "\tsum:  -s\n"
                     "\tmin:  -x\n"
                     "\tmax:  -X\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool ok = false;
    bool T  = false;
    int  op = 0;
    int  l  = 0;
    int  n  = 0;
    int  m  = 0;
    int  p  = 0;
    int  o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "l:n:m:p:01sxXt")) != -1)
        switch (o)
        {
            case 'l': l = (int) strtol(optarg, 0, 0); break;
            case 'n': n = (int) strtol(optarg, 0, 0); break;
            case 'm': m = (int) strtol(optarg, 0, 0); break;
            case 'p': p = (int) strtol(optarg, 0, 0); break;

            case 's': op = o; break;
            case 'x': op = o; break;
            case 'X': op = o; break;
            case '0': op = o; break;
            case '1': op = o; break;

            case 't': T = true; break;

            case '?':
            default : return usage(argv[0]);
        }

    setexe(argv[0]);

    struct timeval t0;
    struct timeval t1;

    gettimeofday(&t0, 0);
    {
        ok = proc(argv[optind], l, n, m, p, op);
    }
    gettimeofday(&t1, 0);

    if (T) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

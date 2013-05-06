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

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>

#include "img.h"
#include "err.h"
#include "etc.h"

//------------------------------------------------------------------------------

static void blit(img *d, int x, int y, img *s, int X, int Y, int W, int H)
{
    const int c = (s->p < d->p) ? s->p : d->p;
    int i;
    int j;
    int k;

    #pragma omp parallel for private(j)
    for         (i = 0; i < H; i++)
        for     (j = 0; j < W; j++)
            for (k = 0; k < c; k++)
                imgz(d, y + i, x + j)[k] = imgz(s, Y + i, X + j)[k];
}

static bool proc(const char *dst,  // destination image file name
                         int l,    // destination log2 tile size
                         int n,    // destination log2 image height
                         int m,    // destination log2 image width
                         int p,    // destination pixel size
                         int x,    // destination region x
                         int y,    // destination region y
                 const char *src,  // source image file name
                         int L,    // source log2 tile size
                         int N,    // source log2 image height
                         int M,    // source log2 image width
                         int P,    // source pixel size
                         int X,    // source region x
                         int Y,    // source region y
                         int W,    // source region width
                         int H)    // source region height
{
    bool ok = false;

    img *d;
    img *s;

    if ((n && m && p) || imgargs(dst, &n, &m, &p))
    {
        if ((N && M && P) || imgargs(src, &N, &M, &P))
        {
            if ((d = imgopen(dst, l, n, m, p)))
            {
                if ((s = imgopen(src, L, N, M, P)))
                {
                    if (H == 0) H = 1 << N;
                    if (W == 0) W = 1 << M;

                    blit(d, x, y, s, X, Y, W, H);

                    ok = true;
                    imgclose(d);
                }
                imgclose(s);
            }
        }
        else apperr("Failed to guess '%s' image parameters", dst);
    }
    else apperr("Failed to guess '%s' image parameters", src);

    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-t] "
                               "[-l size] [-n height] [-m width] [-p samples] "
                               "[-L size] [-N height] [-M width] [-P samples] "
                               "-x x -y y -X X -Y Y -W W -H H dst src\n"
                               "\tl ... destination log2 tile size\n"
                               "\tn ... destination log2 image height\n"
                               "\tm ... destination log2 image width\n"
                               "\tp ... destination pixel size\n"
                               "\tx ... destination X (in pixels)\n"
                               "\ty ... destination Y (in pixels)\n"
                               "\tL ... source log2 tile size\n"
                               "\tN ... source log2 image height\n"
                               "\tM ... source log2 image width\n"
                               "\tP ... source pixel size\n"
                               "\tX ... source X      (in pixels)\n"
                               "\tY ... source Y      (in pixels)\n"
                               "\tW ... source width  (in pixels)\n"
                               "\tH ... source height (in pixels)\n"
                               , exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool ok = false;
    bool t  = false;
    int  l  = 5;
    int  n  = 0;
    int  m  = 0;
    int  p  = 0;
    int  x  = 0;
    int  y  = 0;
    int  L  = 5;
    int  N  = 0;
    int  M  = 0;
    int  P  = 0;
    int  W  = 0;
    int  H  = 0;
    int  X  = 0;
    int  Y  = 0;
    int  o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "l:n:m:p:x:y:L:N:M:P:X:Y:W:H:t")) != -1)
        switch (o)
        {
            case 'l': l = (int) strtol(optarg, 0, 0); break;
            case 'n': n = (int) strtol(optarg, 0, 0); break;
            case 'm': m = (int) strtol(optarg, 0, 0); break;
            case 'p': p = (int) strtol(optarg, 0, 0); break;
            case 'x': x = (int) strtol(optarg, 0, 0); break;
            case 'y': y = (int) strtol(optarg, 0, 0); break;
            case 'L': L = (int) strtol(optarg, 0, 0); break;
            case 'N': N = (int) strtol(optarg, 0, 0); break;
            case 'M': M = (int) strtol(optarg, 0, 0); break;
            case 'P': P = (int) strtol(optarg, 0, 0); break;
            case 'X': X = (int) strtol(optarg, 0, 0); break;
            case 'Y': Y = (int) strtol(optarg, 0, 0); break;
            case 'W': W = (int) strtol(optarg, 0, 0); break;
            case 'H': H = (int) strtol(optarg, 0, 0); break;

            case 't': t = true; break;
            case '?':
            default : return usage(argv[0]);
        }

    // Confirm the arguments and run the process.

    setexe(argv[0]);

    struct timeval t0;
    struct timeval t1;

    gettimeofday(&t0, 0);
    {
        if (optind + 2 == argc)
        {
             ok = proc(argv[optind + 0], l, n, m, p, x, y,
                       argv[optind + 1], L, N, M, P, X, Y, W, H);
        }
        else return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

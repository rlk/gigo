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
#include "fft.h"

enum options
{
    INVERSE   = 1,
    TRANSPOSE = 2,
};

//------------------------------------------------------------------------------

static inline int offset(int x, int m, int s)
{
    if (s > 0)
        return x;
    else
    {
        const int n =  1 << (m - 1);
        const int o = (1 << m) - 1;

        return (x + n) & o;
    }
}

// Copy one row of tiles from the image to a raster. De-interleave the channels
// and apply the offset and index bit reversal in preparation for FFT. Use a
// tile-wise ordering for best input cache coherence.

static void getrow(img *d, int r, int s, const int *v, float complex *z)
{
    for         (int c = 0; c < d->w; c++)
        for     (int i = 0; i < d->s; i++)
            for (int j = 0; j < d->s; j++)
            {
                const int x = v[offset((c << d->l) + j, d->m, +s)];

                imgget(d, r, c, i, j, z + (i << d->m) + x, d->s << d->m);
            }
}

// Copy one row of tiles from a raster to the image. Re-interleave the channels
// and apply the offset following the FFT. Use a tile-wise ordering for best
// output cache coherence.

static void putrow(img *d, int r, int s, float complex *z)
{
    for         (int c = 0; c < d->w; c++)
        for     (int i = 0; i < d->s; i++)
            for (int j = 0; j < d->s; j++)
            {
                const int x = offset((c << d->l) + j, d->m, -s);

                imgput(d, r, c, i, j, z + (i << d->m) + x, d->s << d->m);
            }
}

// Transpose one column of tiles from the image to a raster. De-interleave the
// channels and apply the offset and index bit reversal in preparation for FFT.
// Use a tile-wise ordering for best input cache coherence.

static void getcol(img *d, int c, int s, const int *v, float complex *z)
{
    for         (int r = 0; r < d->h; r++)
        for     (int i = 0; i < d->s; i++)
            for (int j = 0; j < d->s; j++)
            {
                const int y = v[offset((r << d->l) + i, d->n, +s)];

                imgget(d, r, c, i, j, z + (j << d->n) + y, d->s << d->n);
            }
}

// Transpose one column of tiles from a raster to the image. Re-interleave the
// channels and apply the offset following the FFT. Use a tile-wise ordering for
// best output cache coherence.

static void putcol(img *d, int c, int s, float complex *z)
{
    for         (int r = 0; r < d->h; r++)
        for     (int i = 0; i < d->s; i++)
            for (int j = 0; j < d->s; j++)
            {
                const int y = offset((r << d->l) + i, d->n, -s);

                imgput(d, r, c, i, j, z + (j << d->n) + y, d->s << d->n);
            }
}

//------------------------------------------------------------------------------

// Transform a raster.

static void transform(int n,  // raster rows
                      int m,  // raster columns
                      int p,  // pixel size
                      int s,  // transformation sign
           float complex *z)  // raster buffer
{
    int r;
    int k;

    for     (k = 0; k < p; k++)
        for (r = 0; r < n; r++)
            fft(s, m, z + n * m * k + m * r);
}

// Orchestrate the Fourier transform of the ith row of tiles of image d. Gather
// a set of tiles from the image, transform each line, and copy the results
// back. This function forms the kernel of the OpenMP parallelization.

static void dorow(img *d,
                   int i,
                   int opt,
            const int *v,
        float complex *z)
{
    const int n = d->s;
    const int m = d->s * ((opt & TRANSPOSE) ? d->h : d->w);
    const int p = d->p;

    const int s = (opt & INVERSE) ? -1 : +1;

    if (opt & TRANSPOSE) getcol(d, i, s, v, z);
    else                 getrow(d, i, s, v, z);

    transform(n, m, p, s, z);

    if (opt & TRANSPOSE) putcol(d, i, s, z);
    else                 putrow(d, i, s, z);
}

//------------------------------------------------------------------------------

#ifdef _OPENMP
#include <omp.h>
#else
static inline int omp_get_max_threads() { return 1; }
static inline int omp_get_thread_num()  { return 0; }
#endif

static void fourier(img *d, int opt)
{
    int w = (opt & TRANSPOSE) ? d->h : d->w;
    int h = (opt & TRANSPOSE) ? d->w : d->h;

    int *v;

    if ((v = revalloc(w * d->s)))
    {
        size_t N = omp_get_max_threads();
        size_t M = d->p * d->s * d->s * w;

        float complex *z;

        if ((z = (float complex *) calloc(N * M, sizeof (float complex))))
        {
            int i;

            #pragma omp parallel for schedule(static, max(1, h / N))
            for (i = 0; i < h; i++)
                dorow(d, i, opt, v, z + M * omp_get_thread_num());

            free(z);
        }
        free(v);
    }
}

// Confirm that the input conforms to spec and that the output can be created,
// open the input and output images, and then do the job.

static bool proc(const char *name, // image file name
                         int l,    // log2 tile size
                         int n,    // log2 image height
                         int m,    // log2 image width
                         int p,    // pixel size
                         int opt)  // option flags
{
    bool ok = false;

    img *d;

    if ((n && m && p) || imgargs(name, &n, &m, &p))
    {
        if ((d = imgopen(name, l, n, m, p)))
        {
            fourier(d, opt);
            ok = true;
            imgclose(d);
        }
    }
    else apperr("Failed to guess '%s' image parameters", name);

    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-tIT] "
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] image\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool  ok  = false;
    bool  t   = false;
    int   opt = 0;
    int   l   = 0;
    int   n   = 0;
    int   m   = 0;
    int   p   = 0;
    int   o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "N:TIl:n:m:p:t")) != -1)
        switch (o)
        {
            case 'l': l = (int) strtol(optarg, 0, 0); break;
            case 'n': n = (int) strtol(optarg, 0, 0); break;
            case 'm': m = (int) strtol(optarg, 0, 0); break;
            case 'p': p = (int) strtol(optarg, 0, 0); break;

            case 'I': opt |= INVERSE;   break;
            case 'T': opt |= TRANSPOSE; break;

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
        if (optind + 1 == argc)
        {
            ok = proc(argv[optind], l, n, m, p, opt);
        }
        else return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

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
#include <tiffio.h>
#include <stdlib.h>
#include <stdio.h>

#include "img.h"
#include "err.h"
#include "etc.h"
#include "icc.h"

//------------------------------------------------------------------------------

// Read and return the first scanline of the named tif. Return width in w and
// samples-per-pixel in p.

static float *readmap(const char *tif, int *w, int *p)
{
    void *V = NULL;
    TIFF *T;

    if ((T = TIFFOpen(tif, "r")))
    {
        uint32 W = 0;
        uint32 L = 0;
        uint16 P = 0;
        uint16 B = 0;
        uint16 G = 0;
        uint16 F = 0;

        TIFFGetField(T, TIFFTAG_IMAGEWIDTH,      &W);
        TIFFGetField(T, TIFFTAG_IMAGELENGTH,     &L);
        TIFFGetField(T, TIFFTAG_SAMPLESPERPIXEL, &P);
        TIFFGetField(T, TIFFTAG_BITSPERSAMPLE,   &B);
        TIFFGetField(T, TIFFTAG_PLANARCONFIG,    &G);
        TIFFGetField(T, TIFFTAG_SAMPLEFORMAT,    &F);

        if (G == PLANARCONFIG_CONTIG)
        {
            if (F == SAMPLEFORMAT_IEEEFP)
            {
                if (B == 32 && L > 0)
                {
                    if ((V = malloc(TIFFScanlineSize(T))))
                    {
                        TIFFReadScanline(T, (uint8 *) V, 0, 0);
                        if (w) *w = (int) W;
                        if (p) *p = (int) P;
                    }
                }
                else apperr("TIFF must be non-empty with 32 bits per sample");
            }
            else apperr("TIFF must have floating point sample format");
        }
        else apperr("TIFF must have contiguous planar configuration");

        TIFFClose(T);
    }
    return V;
}

// Map one pixel of the source onto one pixel of the destination by linearly
// interpolation of the given gradient. Clamp values outside (g0, g1).

static inline void pixel(float complex *d,
                   const float complex *s,
                   const float         *g, int w, int q, float g0, float g1)
{
    const float t = (creal(s[0]) - g0) / (g1 - g0);

    if      (t <= 0.0)
    {
        for (int k = 0; k < q; k++)
            d[k] = g[k];
    }
    else if (t >= 1.0)
    {
        for (int k = 0; k < q; k++)
            d[k] = g[w * q - q + k];
    }
    else
    {
        const float i  = t * (w - 1);
        const int   i0 = (int) floorf(i) + 0;
        const int   i1 = (int) floorf(i) + 1;

        for (int k = 0; k < q; k++)
            d[k] = g[i0 * q + k] * (i1 - i)
                 + g[i1 * q + k] * (i - i0);
    }
}

// Map the source onto the destination, applying the gradient map in g, with 
// width w and sample count q. g0 and g1 give the source values to map onto
// the beginning and end of the gradient.

static void map(img *d, img *s,
                const float *g, int w, int q, float g0, float g1)
{
    int r;
    int c;
    int i;
    int j;

    #pragma omp parallel for private(c, i, j)
    for             (r = 0; r < d->h; r++)
        for         (c = 0; c < d->w; c++)
            for     (i = 0; i < d->s; i++)
                for (j = 0; j < d->s; j++)

                    pixel(imgbuf(d, r, c, i, j),
                          imgbuf(s, r, c, i, j), g, w, q, g0, g1);
}

// Load the gradient image and initialize the source and destination.

static bool proc(const char *dst,
                 const char *src,
                 const char *tif,
                 int l, int n, int m, int p, float g0, float g1)
{
    bool  ok = false;
    img   *s;
    img   *d;
    float *g;
    int    w;
    int    q;

    if ((g = readmap(tif, &w, &q)))
    {
        if ((n && m && p) || imgargs(src, &n, &m, &p))
        {
            if ((s = imgopen(src, l, n, m, p)))
            {
                if ((d = imgopen(dst, l, n, m, q)))
                {
                    map(d, s, g, w, q, g0, g1);
                    imgclose(d);
                }
                imgclose(s);
            }
        }
        else apperr("Failed to guess image parameters");
    }
    else apperr("Failed to load gradient map");

    return ok;
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-t] "
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] "
                               "[-0 min] "
                               "[-1 max] "
                               "[-g map] dst src\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    const char *g = NULL;

    bool ok  = false;
    bool t   = false;
    int  l   = 5;
    int  n   = 0;
    int  m   = 0;
    int  p   = 0;
    float g0 = 0.f;
    float g1 = 1.f;

    int o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "0:1:g:l:n:m:p:t")) != -1)
        switch (o)
        {
            case 'l': l  =   (int) strtol(optarg, 0, 0); break;
            case 'n': n  =   (int) strtol(optarg, 0, 0); break;
            case 'm': m  =   (int) strtol(optarg, 0, 0); break;
            case 'p': p  =   (int) strtol(optarg, 0, 0); break;
            case '0': g0 = (float) strtod(optarg, 0);    break;
            case '1': g1 = (float) strtod(optarg, 0);    break;
            case 'g': g  = optarg; break;
            case 't': t  = true;   break;
            case '?':
            default : return usage(argv[0]);            
        }

    setexe(argv[0]);

    // Do the work.

    struct timeval t0;
    struct timeval t1;

    gettimeofday(&t0, 0);
    {
        if (optind + 2 == argc)
            ok = proc(argv[optind], argv[optind + 1], g, l, n, m, p, g0, g1);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

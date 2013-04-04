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

// Test the file extension of the given argument to determine if it's a TIFF.

static bool istif(const char *arg)
{
    return isext(arg, ".tif")
        || isext(arg, ".TIF")
        || isext(arg, ".tiff")
        || isext(arg, ".TIFF");
}

// Open a new source TIFF file. Check for a compatible format.

static TIFF *tifopenr(const char *tif,  // TIFF file name
                            bool *c,    // return is complex?
                             int *l,    // return log2 tile size
                             int *n,    // return log2 height
                             int *m,    // return log2 width
                             int *p)    // return pixel size
{
    TIFF *T;

    if ((T = TIFFOpen(tif, "r")))
    {
        uint32 W = 0;
        uint32 L = 0;
        uint32 S = 0;
        uint16 P = 0;
        uint16 B = 0;
        uint16 G = 0;
        uint16 F = 0;

        TIFFGetField(T, TIFFTAG_IMAGEWIDTH,      &W);
        TIFFGetField(T, TIFFTAG_IMAGELENGTH,     &L);
        TIFFGetField(T, TIFFTAG_TILEWIDTH,       &S);
        TIFFGetField(T, TIFFTAG_SAMPLESPERPIXEL, &P);
        TIFFGetField(T, TIFFTAG_BITSPERSAMPLE,   &B);
        TIFFGetField(T, TIFFTAG_PLANARCONFIG,    &G);
        TIFFGetField(T, TIFFTAG_SAMPLEFORMAT,    &F);

        if (G == PLANARCONFIG_CONTIG)
        {
            if (F == SAMPLEFORMAT_IEEEFP)
            {
                if (B == 32)
                {
                    if (ispow2(W) && ispow2(L))
                    {
                        *c = (P % 2) ? false : true;
                        *p = (P % 2) ? P     : P / 2;
                        *l = log2i(S);
                        *n = log2i(L);
                        *m = log2i(W);

                        return T;
                    }
                    else apperr("TIFF image size must be power of 2");
                }
                else apperr("TIFF must have 32 bits per sample");
            }
            else apperr("TIFF must have floating point sample format");
        }
        else apperr("TIFF must have contiguous planar configuration");
        TIFFClose(T);
    }
    return 0;
}

// Open a new destination TIFF file.

static TIFF *tifopenw(const char *tif,  // TIFF file name
                             bool c,    // is complex?
                              int n,    // log2 height
                              int m,    // log2 width
                              int p)    // pixel size
{
    TIFF *T;

    if ((T = TIFFOpen(tif, "w")))
    {
        TIFFSetField(T, TIFFTAG_IMAGEWIDTH,      1 << m);
        TIFFSetField(T, TIFFTAG_IMAGELENGTH,     1 << n);
        TIFFSetField(T, TIFFTAG_SAMPLESPERPIXEL, c ? 2 * p : p);
        TIFFSetField(T, TIFFTAG_BITSPERSAMPLE,   32);
        TIFFSetField(T, TIFFTAG_SAMPLEFORMAT,    SAMPLEFORMAT_IEEEFP);
        TIFFSetField(T, TIFFTAG_PLANARCONFIG,    PLANARCONFIG_CONTIG);
        TIFFSetField(T, TIFFTAG_COMPRESSION,     COMPRESSION_ADOBE_DEFLATE);

        if (p == 1)
        {
            TIFFSetField(T, TIFFTAG_ICCPROFILE, sizeof (grayicc), grayicc);
            TIFFSetField(T, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
        }
        if (p == 3)
        {
            TIFFSetField(T, TIFFTAG_ICCPROFILE, sizeof (srgbicc), srgbicc);
            TIFFSetField(T, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        }
    }
    return T;
}

//------------------------------------------------------------------------------

static inline void ctor(float *dst, const float complex *src, int c)
{
    for (int k = 0; k < c; k++)
        dst[k    ] = cabsf(src[k]);
}

static inline void ctop(float *dst, const float complex *src, int c)
{
    for (int k = 0; k < c; k++)
    {
        dst[k    ] = cabsf(src[k]);
        dst[k + c] = cargf(src[k]);
    }
}

static inline void rtoc(float complex *dst, const float *src, int c)
{
    for (int k = 0; k < c; k++)
        dst[k] = src[k];
}

static inline void ptoc(float complex *dst, const float *src, int c)
{
    for (int k = 0; k < c; k++)
        dst[k] = src[k] * cisf(src[k + c]);
}

//------------------------------------------------------------------------------

// Copy one tile of real TIFF data into a complex image cache.

static void tiletoimgr(int y,  // destination row
                       int x,  // destination column
                      img *d,  // destination image
                       int s,  // source tile size
              const float *p)  // source buffer
{
    int i = 0;
    for     (int r = y; r < y + s; r++)
        for (int c = x; c < x + s; c++, i++)
            rtoc(imgz(d, r, c), p + d->p * i, d->p);
}

// Copy one scanline of real TIFF data into a complex image cache.

static void linetoimgr(int r,  // destination row
                      img *d,  // destination image
              const float *p)  // source buffer
{
    for (int c = 0; c < 1 << d->m; c++)
        rtoc(imgz(d, r, c), p + d->p * c, d->p);
}

// Copy one scanline of real TIFF data from a complex image cache.

static void imgtoliner(int r,  // source row
                      img *d,  // source image
                    float *p)  // destination buffer
{
    for (int c = 0; c < 1 << d->m; c++)
        ctor(p + d->p * c, imgz(d, r, c), d->p);
}

//------------------------------------------------------------------------------

// Copy one tile of complex TIFF data into a complex image cache.

static void tiletoimgz(int y,  // destination row
                       int x,  // destination column
                      img *d,  // destination image
                       int s,  // source tile size
              const float *p)  // source buffer
{
    int i = 0;
    for     (int r = y; r < y + s; r++)
        for (int c = x; c < x + s; c++, i++)
            ptoc(imgz(d, r, c), p + 2 * d->p * i, d->p);
}

// Copy one scanline of complex TIFF data into a complex image cache.

static void linetoimgz(int r,  // destination row
                      img *d,  // destination image
              const float *p)  // source buffer
{
    for (int c = 0; c < 1 << d->m; c++)
        ptoc(imgz(d, r, c), p + 2 * d->p * c, d->p);
}

// Copy one scanline of complex TIFF data from a complex image cache.

static void imgtolinez(int r,  // source row
                      img *d,  // source image
                    float *p)  // destination buffer
{
    for (int c = 0; c < 1 << d->m; c++)
        ctop(p + 2 * d->p * c, imgz(d, r, c), d->p);
}

//------------------------------------------------------------------------------

static void reverse(float *p, int c, int n)
{
    int j = n - 1;

    for     (int i = 0; i < j; i++, j--)
        for (int k = 0; k < c; k++)
        {
            float temp   = p[i * c + k];
            p[i * c + k] = p[j * c + k];
            p[j * c + k] = temp;
        }
}

// Copy a scanline-based TIFF to an image cache.

static bool scantoimg(img *d, TIFF *T, bool c, bool e)
{
    int n = 1 << (d->n - e);
    int m = 1 <<  d->m;
    int r = 0;
    float *p;

    if ((p = (float *) malloc(TIFFScanlineSize(T))))
    {
        for (r = 0; r < n; r++)
        {
            if (TIFFReadScanline(T, p, r, 0) == -1)
                return false;

            if (c) linetoimgz(r, d, p);
            else   linetoimgr(r, d, p);

            if (e)
            {
                reverse(p, c ? d->p * 2 : d->p, m);

                if (c) linetoimgz(2 * n - r - 1, d, p);
                else   linetoimgr(2 * n - r - 1, d, p);
            }
        }
        free(p);
    }
    return (r == n);
}

// Copy a tile-based TIFF to an image cache.

static bool tiletoimg(img *d, TIFF *T, bool c, int e, int k)
{
    int n = 1 << (d->n - e);
    int m = 1 <<  d->m;
    int s = 1 <<  k;
    int y = 0;
    int x = 0;
    float *p;

    if ((p = (float *) malloc(TIFFTileSize(T))))
    {
        for     (y = 0; y < n; y += s)
            for (x = 0; x < m; x += s)
            {
                if (TIFFReadTile(T, p, x, y, 0, 0) == -1)
                    return false;

                if (c) tiletoimgz(y, x, d, s, p);
                else   tiletoimgr(y, x, d, s, p);

                if (e)
                {
                    reverse(p, c ? d->p * 2 : d->p, s * s);

                    if (c) tiletoimgz(2 * n - y - s, m - x - s, d, s, p);
                    else   tiletoimgr(2 * n - y - s, m - x - s, d, s, p);
                }
            }
        free(p);
    }
    return (y == n && x == m);
}

//------------------------------------------------------------------------------

// Convert a TIFF to an image cache file.

static bool tiftoimg(bool v,    // verbose?
                      int e,    // destination is extended?
                      int l,    // log2 tile size
              const char *tif,  // source TIFF image file name
              const char *bin)  // destination image cache file name
{
    bool ok = false;
    TIFF *T;
    img  *d;

    bool c;
    int  k = 0;
    int  n = 0;
    int  m = 0;
    int  p = 0;

    if ((T = tifopenr(tif, &c, &k, &n, &m, &p)))
    {
        if (imginit(bin, l, n + e, m, p, 0))
        {
            if ((d = imgopen(bin, l, n + e, m, p)))
            {
                if (k)
                    ok = tiletoimg(d, T, c, e, k);
                else
                    ok = scantoimg(d, T, c, e);

                imgclose(d);
            }
        }
        TIFFClose(T);
    }
    if (v) printf("%s %d %d %d %d\n", bin, l, n + e, m, p);

    return ok;
}

// Convert an image cache file to a TIFF.

static bool imgtotif(bool c,    // destination is complex?
                      int e,    // source is extended?
                      int l,    // source log2 tile size
                      int n,    // source log2 height
                      int m,    // source log2 width
                      int p,    // source pixel size
              const char *bin,  // source image cache file name
              const char *tif)  // destination TIFF image file name
{
    img  *d;
    TIFF *T;
    void *buf;

    int r = 0;

    if ((n && m && p) || imgargs(bin, &n, &m, &p))
    {
        if ((d = imgopen(bin, l, n, m, p)))
        {
            if ((T = tifopenw(tif, c, n - e, m, p)))
            {
                if ((buf = malloc(TIFFScanlineSize(T))))
                {
                    for (r = 0; r < 1 << (n - e); r++)
                    {
                        if (c)
                            imgtolinez(r, d, (float *) buf);
                        else
                            imgtoliner(r, d, (float *) buf);

                        if (TIFFWriteScanline(T, buf, r, 0) == -1)
                            break;
                    }
                    free(buf);
                }
                TIFFClose(T);
            }
            imgclose(d);
        }
    }
    else apperr("Failed to guess image parameters", bin);

    return (r == 1 << (n - e));
}

//------------------------------------------------------------------------------

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-tve] input.tif output.bin\n", exe);
    fprintf(stderr, "\t%s [-tr] "
                         "[-l size] "
                         "[-n height] "
                         "[-m width] "
                         "[-p samples] input.bin output.tif\n", exe);

    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool ok = false;
    bool t  = false;
    bool c  = true;
    bool v  = false;
    int  l  = 0;
    int  n  = 0;
    int  m  = 0;
    int  p  = 0;
    int  e  = 0;
    int  o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "l:n:m:p:terv")) != -1)
        switch (o)
        {
            case 't': t = true;                 break;
            case 'r': c = false;                break;
            case 'v': v = true;                 break;
            case 'l': l = strtol(optarg, 0, 0); break;
            case 'n': n = strtol(optarg, 0, 0); break;
            case 'm': m = strtol(optarg, 0, 0); break;
            case 'p': p = strtol(optarg, 0, 0); break;
            case 'e': e = 1;                    break;
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
            if (istif(argv[optind]))
                ok = tiftoimg(v, e, l,          argv[optind], argv[optind + 1]);
            else
                ok = imgtotif(c, e, l, n, m, p, argv[optind], argv[optind + 1]);
        }
        else return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

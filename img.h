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

#ifndef GIGO_IMG_H
#define GIGO_IMG_H

#include <complex.h>
#include <stdbool.h>

//------------------------------------------------------------------------------

struct img
{
    int   f;  // file descriptor
    void *a;  // data pointer
    int   l;  // log2 tile size
    int   n;  // log2 height
    int   m;  // log2 width
    int   p;  // pixel size (in samples)
    int   t;  // tile  size (in samples)
    int   s;  // tile size 2^l
    int   h;  // tile array height
    int   w;  // tile array width
};

typedef struct img img;

//------------------------------------------------------------------------------

bool imgargs(const char *name, int *n, int *m, int *p);

bool imginit(const char *name, int l, int n, int m, int p, float complex v);
img *imgopen(const char *name, int l, int n, int m, int p);

void imgclose(img *d);

//------------------------------------------------------------------------------

static inline float complex *imgz(img *d, int y, int x)
{
    const int c = x >> d->l, j = x & (d->s - 1);
    const int r = y >> d->l, i = y & (d->s - 1);

    return (float complex *) d->a + ((size_t) d->w * r + c) * d->t
                                  + ((size_t) d->s * i + j) * d->p;
}

static inline float complex *imgbuf(img *d, int r, int c, int i, int j)
{
    return (float complex *) d->a + ((size_t) d->w * r + c) * d->t
                                  + ((size_t) d->s * i + j) * d->p;
}

static inline void imgget(img *d, int r, int c,
                                  int i, int j, float complex *z, int s)
{
    float complex *t = imgbuf(d, r, c, i, j);

    if (d->p > 1)
    {
        z[s + s] = t[2];
        z[s    ] = t[1];
    }
    z[0] = t[0];
}

static inline void imgput(img *d, int r, int c,
                                  int i, int j, float complex *z, int s)
{
    float complex *t = imgbuf(d, r, c, i, j);

    if (d->p > 1)
    {
        t[2] = z[s + s];
        t[1] = z[s    ];
    }
    t[0] = z[0];
}

//------------------------------------------------------------------------------

#endif

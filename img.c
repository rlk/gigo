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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "err.h"
#include "etc.h"
#include "img.h"

//------------------------------------------------------------------------------

// Use the size of the named image cache file to guess at its parameters. Use
// the assumption that the pixel size is 1 or 3, and that the image has power of
// two size and either a 2:1 or 1:1 aspect ratio. Tile size cannot be guessed.

bool imgargs(const char *name,  // file name
                    int *n,     // log2 height
                    int *m,     // log2 width
                    int *p)     // pixel size
{
    struct stat buf;

    if (stat(name, &buf) != -1)
    {
        size_t whp = buf.st_size / sizeof (float complex);

        *p = whp % 3 ? 1 : 3;

        if (ispow2(whp / (*p)))
        {
            size_t nm = log2i(whp / (*p));

            if (nm % 2)
            {
                *n = (nm - 1) / 2;
                *m = (nm + 1) / 2;
            }
            else
            {
                *n = nm / 2;
                *m = nm / 2;
            }
            return true;
        }
        else apperr("File %s size is not a power of two", name);
    }
    else syserr("Failed to stat image %s", name);

    return false;
}

// Clear space for an image cache file.

bool imginit(const char *name,  // file name
                    int  l,     // log2 tile size
                    int  n,     // log2 height
                    int  m,     // log2 width
                    int  p,     // pixel size
           float complex v)     // value
{
    // Allocate and initialize a temporary buffer of complex values.

    const size_t N = 1024;
    float complex a[N];

    for (size_t i = 0; i < N; i++)
        a[i] = v;

    // Write a value for each sample of each pixel using this buffer.

    size_t M = sizeof (float complex) * p << (n + m);
    size_t O = sizeof (float complex) * N;
    int    fd;

    if ((fd = open(name, O_CREAT | O_TRUNC | O_WRONLY, 0644)) != -1)
    {
        while (M > 0)
        {
            size_t o = min(M, O);

            if (write(fd, a, o) == (ssize_t) o)
                M -= o;
            else
            {
                syserr("Failed to write image %s", name);
                break;
            }
        }
        close(fd);
    }
    else syserr("Failed to open image %s", name);

    return (M == 0);
}

// Open an image cache file and return a new img structure.

img *imgopen(const char *name,  // file name
                    int  l,     // log2 tile size
                    int  n,     // log2 height
                    int  m,     // log2 width
                    int  p)     // pixel size
{
    size_t len = (sizeof (float complex) * p) << (n + m);
    struct stat buf;

    int prot = PROT_READ | PROT_WRITE;
    int    f;
    img   *d;
    void  *a;

    if (stat(name, &buf) != -1 && buf.st_size == len)
    {
        if ((d = (img *) malloc(sizeof (img))))
        {
            if ((f = open(name, O_RDWR, 0644)) != -1)
            {
                if ((a = mmap(0, len, prot, MAP_SHARED, f, 0)) != MAP_FAILED)
                {
                    d->f = f;
                    d->a = a;
                    d->l = l;
                    d->n = n;
                    d->m = m;
                    d->p = p;
                    d->t = p << (l + l);
                    d->s = 1 << (    l);
                    d->h = 1 << (n - l);
                    d->w = 1 << (m - l);

                    return d;
                }
                else syserr("Failed to map image %s", name);
                close(f);
            }
            else syserr("Failed to open image %s", name);
            free(d);
        }
        else syserr("Failed to allocate image  %s", name);
    }
    else syserr("Size of %s does not match arguments", name);

    return 0;
}

// Close an image cache file.

void imgclose(img *d)
{
    size_t len = (sizeof (float complex) * d->p) << (d->n + d->m);

    if (munmap(d->a, len) != -1)
    {
        close(d->f);
        free(d);
    }
    else syserr("Failed to unmap image");
}

//------------------------------------------------------------------------------

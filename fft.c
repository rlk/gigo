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

#include <math.h>
#include <stdlib.h>
#include <complex.h>

#include "etc.h"

//------------------------------------------------------------------------------

// Initialize x[0..n] so that x[i] gives the bit-reversal of i. Invoke with
// revinit(0, 1, n, x).

static void revinit(int a, int d, int n, int *x)
{
    if (n == 1)
        *x = a;
    else
    {
        revinit(a,     d * 2, n / 2, x        );
        revinit(a + d, d * 2, n / 2, x + n / 2);
    }
}

// Allocate and initialize a bit-reversal lookup table of length n.

int *revalloc(int n)
{
    int *x;

    if ((x = (int *) malloc(n * sizeof (int))))
        revinit(0, 1, n, x);

    return x;
}

// Apply the Fast Fourier Transform in place in v[0..n]. If s is -1 then
// apply the inverse.

void fft(int s, int n, float complex *v)
{
    for (int M = 1; M < n; M *= 2)
    {
        float         a = sinf(0.5f * s * M_PI / M);
        float         b = sinf(       s * M_PI / M);
        float complex k = -2.0f * a * a + b * I;
        float complex w =  1.0f;

        for (int m = 0; m < M; ++m)
        {
            for (int i = m; i < n; i += M * 2)
            {
                float complex t = w * v[i + M];
                v[i + M] = v[i] - t;
                v[i    ] = v[i] + t;
            }
            w = w + w * k;
        }
    }

    if (s < 0)
        for (int i = 0; i < n; ++i)
            v[i] = v[i] / n;
}

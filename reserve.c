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

static int usage(const char *exe)
{
    fprintf(stderr, "Usage:\t%s [-t1] "
                               "[-l size] "
                               "[-n height] "
                               "[-m width] "
                               "[-p samples] image\n", exe);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    float complex v = 0.f;

    bool ok = false;
    bool t  = false;
    int  l  = 5;
    int  n  = 0;
    int  m  = 0;
    int  p  = 0;
    int  o;

    // Parse the command line options.

    while ((o = getopt(argc, argv, "t1l:n:m:p:")) != -1)
        switch (o)
        {
            case 'l': l = (int) strtol(optarg, 0, 0); break;
            case 'n': n = (int) strtol(optarg, 0, 0); break;
            case 'm': m = (int) strtol(optarg, 0, 0); break;
            case 'p': p = (int) strtol(optarg, 0, 0); break;

            case 't': t = true; break;
            case '1': v = 1.f;  break;
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
             ok = imginit(argv[optind], l, n, m, p, v);
        }
        else return usage(argv[0]);
    }
    gettimeofday(&t1, 0);

    if (t) printtime(&t0, &t1);

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

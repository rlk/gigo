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

#ifndef GIGO_ERR_H
#define GIGO_ERR_H

#include <errno.h>

//------------------------------------------------------------------------------

void printerr(const char *, int, int, const char *, ...);

//------------------------------------------------------------------------------

void setexe(const char *);

#define apperr(...) printerr(__FILE__, __LINE__, 0,     __VA_ARGS__)
#define syserr(...) printerr(__FILE__, __LINE__, errno, __VA_ARGS__)

//------------------------------------------------------------------------------

#endif

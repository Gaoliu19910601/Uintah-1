/*
 * The MIT License
 *
 * Copyright (c) 1997-2015 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef SCI_MPI_DEFS_H
#define SCI_MPI_DEFS_H

/* MPI++ (MPIPP_H) on SGI has a bad interaction with <iostream>.  On some linuxes, mpicxx also
   causes problems, so we turn it off too. This prevents it from being included (from mpi.h).
   We don't use it anyway. */
#define MPIPP_H
#define MPICH_SKIP_MPICXX 1

#define HAVE_MPI 1

#include <mpi.h>

// Error handling wrapper for MPI calls
#  define MPI_SAFE_CALL( call )                                                           \
    do {                                                                                  \
      int error = call;                                                                   \
      if(error != MPI_SUCCESS) {                                                          \
          fprintf(stderr, "MPI error  detected by call %i in file '%s' on line %i.\n\n", \
                  error, __FILE__, __LINE__ );                                            \
         throw;                                                                           \
      }                                                                                   \
    } while (false)

#endif


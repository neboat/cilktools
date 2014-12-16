// -*- Mode: C++ -*-
// Copyright (c) 2010, Tao B. Schardl
/*
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "graph.h"

using namespace std;

const bool UTIL_DEBUG = false;

// List of valid BFS algorithms to choose from.
enum ALG_SELECT {
  BFS = 0,
  PBFS = 1,
  PBFS_WLS = 2,
  NULL_ALG = 3
};

static const char *ALG_ABBR[] =
  {
    "b",
    "p",
    "w",
    "NULL"
  };

static const char *ALG_NAMES[] =
  {
    "Serial BFS",
    "PBFS",
    "PBFS_WLS",
    "NULL"
  };

const ALG_SELECT DEFAULT_ALG_SELECT = PBFS;

// Structure defining command line argument values
typedef struct {
  string filename;
  ALG_SELECT alg_select;
  bool check_correctness;
} BFSArgs;

// Print the usage for this program
static void
print_usage(char* argv0) {
  fprintf(stderr, "Usage: %s [-f <filename>] [-a <algorithm>] [-c]\n", argv0);
  fprintf(stderr, "Flags are:\n");
  fprintf(stderr, "\t-f <filename>\t: Specify the name of the test file to use.\n");
  fprintf(stderr, "\t-a <algorithm>\t: Specify the BFS algorithm to use.\n");
  fprintf(stderr, "\tValid values for <algorithm> are:\n");
  for (int i = 0; i < NULL_ALG; ++i) {
    fprintf(stderr, "\t\t%s for %s", ALG_ABBR[i], ALG_NAMES[i]);
    if (i == DEFAULT_ALG_SELECT)
      fprintf(stderr, " (default)");
    fprintf(stderr, "\n");
  }
  fprintf(stderr, "\t-c\t\t: Check result for correctness.\n");

  exit(1);
}

// Parse command line arguments
BFSArgs
parse_args(int argc, char* argv[])
{
  BFSArgs theArgs;
  bool found_filename = false;

  theArgs.alg_select = DEFAULT_ALG_SELECT;
  theArgs.filename = "";
  theArgs.check_correctness = false;

  for (int arg_i = 1; arg_i < argc; ++arg_i) {
    char *arg = argv[arg_i];

    if (strcmp(arg, "-c") == 0) {
      theArgs.check_correctness = true;

    } else if (strcmp(arg, "-f") == 0) {
      if (++arg_i >= argc) {
	print_usage(argv[0]);
      } else {
	theArgs.filename = argv[arg_i];
	found_filename = true;
      }

    } else if (strcmp(arg, "-a") == 0) {
      if (++arg_i >= argc) {
	print_usage(argv[0]);
      } else {
	int i;
	for (i = 0; i < NULL_ALG; ++i) {
	  if (strcmp(argv[arg_i], ALG_ABBR[i]) == 0) {
	    theArgs.alg_select = (ALG_SELECT)i;
	    break;
	  }
	}
	if (i == NULL_ALG) {
	  fprintf(stderr, "Invalid algorithm selection.\n");
	  print_usage(argv[0]);
	}

      }
    } else {
      print_usage(argv[0]);
    }
  }

  if (!found_filename)
    print_usage(argv[0]);

  return theArgs;
}

static int
CumulativeSum(int* arr, int size)
{
  int prev;
  int tempnz = 0;
  for (int i = 0; i < size; ++i) {
    prev = arr[i];
    arr[i] = tempnz;
    tempnz += prev;
  }
  return tempnz;
}

int
parseBinaryFile(const string filename, Graph **graph)
{
  int64_t m, n, nnz;

  // Read binary CSB matrix input
  // Code and matrices adapted from oskitest.cpp by Aydin Buluc
  if (UTIL_DEBUG)
    printf("Reading input file %s\n", filename.c_str());

  FILE *f = fopen(filename.c_str(), "r");
  if (!f) {
    fprintf(stderr, "Problem reading binary input file %s\n", filename.c_str());
    return -1;
  }

  fread(&m, sizeof(int), 1, f);
  fread(&n, sizeof(int), 1, f);
  fread(&nnz, sizeof(int), 1, f);

  if (m <= 0 || n <= 0 || nnz <= 0) {
    fprintf(stderr,
            "Problem with matrix size in binary input file %s\n", filename.c_str());
    return -1;
  }

  if (m != n) {
    fprintf(stderr,
            "Input file %s does not describe a graph\n", filename.c_str());
    return -1;
  }

  if (UTIL_DEBUG)
    printf("Reading %ld-by-%ld matrix having %ld nonzeros\n",
           m, n, nnz);

  int *rowindices = new int[nnz];
  int *colindices = new int[nnz];
  double *vals = new double[nnz];

  size_t rows = fread(rowindices, sizeof(int), nnz, f);
  size_t cols = fread(colindices, sizeof(int), nnz, f);
  size_t nums = fread(vals, sizeof(double), nnz, f);
  fclose(f);

  if ((int)rows != nnz || (int)cols != nnz || (int)nums != nnz) {
    fprintf(stderr, "Problem with FREAD. Aborting.\n");
    return -1;
  }
  //Csc<double, int> csc(rowindices, colindices, vals, nnz, m, n);
  double *num = new double[nnz];
  int *ir = new int[nnz];
  int *jc = new int[n+1];
  int *w = new int[n];

  for (int k = 0; k < n; ++k)
    w[k] = 0;

  for (int k = 0; k < nnz; ++k)
    w[colindices[k]]++;

  jc[n] = CumulativeSum(w,n);
  for (int k = 0; k < n; ++k)
    jc[k] = w[k];

  int last;
  for (int k = 0; k < nnz; ++k) {
    ir[last = w[colindices[k]]++] = rowindices[k];
    num[last] = vals[k];
  }

  delete[] w;
  delete[] rowindices;
  delete[] colindices;
  delete[] vals;

  if (UTIL_DEBUG)
    printf("Making graph\n");

  *graph = new Graph(ir, jc, m, n, nnz);

  delete[] ir;
  delete[] jc;
  delete[] num;

  return 0;
}

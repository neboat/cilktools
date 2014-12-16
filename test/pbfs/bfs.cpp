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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/time.h>
#include <iostream>

#include <cilk/cilk.h>
// #include <cilktools/cilkview.h>

#include "util.h"
#include "graph.h"

using namespace std;

// cilk::cilkview cv;

const bool DEBUG = false;

// Helper function for checking correctness of result
static bool
check(unsigned int distances[],
      unsigned int distverf[],
      int nodes)
{
  for (int i = 0; i < nodes; i++) {
    if (distances[i] != distverf[i]) {
      printf("distances[%d] = %d; distverf[%d] = %d\n", i, distances[i], i, distverf[i]);
      return false;
    }
  }

  return true;
}

unsigned long long todval (struct timeval *tp) {
    return tp->tv_sec * 1000 * 1000 + tp->tv_usec;
}

int
main (int argc, char** argv)
{
  Graph *graph;
  int s;
  unsigned long long runtime_ms;

  BFSArgs bfsArgs = parse_args(argc, argv);

  if (DEBUG)
    printf("algorithm = %s\n", ALG_NAMES[bfsArgs.alg_select]);

  if (parseBinaryFile(bfsArgs.filename, &graph) != 0)
    return -1;

  // Initialize extra data structures
  int numNodes = graph->numNodes();
  unsigned int *distances = new unsigned int[numNodes];

  // Pick a starting node
  s = 0;

  // Format test name for cilkview printing
  char *testname;
  size_t found2 = bfsArgs.filename.find_last_of(".");  
  size_t found1 = bfsArgs.filename.find_last_of("/\\");
  testname = new char [found2 - found1 + 32];
  std::strcpy(testname, (bfsArgs.filename.substr(found1+1, found2-found1-1)).c_str());
  switch (bfsArgs.alg_select) {
  case BFS:
    std::strncat(testname, "-BFS", 4);
    break;
  case PBFS:
    std::strncat(testname, "-PBFS", 5);
    break;
  case PBFS_WLS:
    std::strncat(testname, "-PBFS_WLS", 9);
    break;
  default: break;
  }

  // Execute BFS
  struct timeval t1, t2;
  switch (bfsArgs.alg_select) {
  case BFS:
    // cv.start();
    gettimeofday(&t1,0);
    graph->bfs(s, distances);
    gettimeofday(&t2,0);
    // cv.stop();
    //runtime_ms = cv.accumulated_milliseconds();
    //cv.dump(testname);
    break;
  case PBFS:
    //cv.start();
    gettimeofday(&t1,0);
    graph->pbfs(s, distances);      
    gettimeofday(&t2,0);
    //cv.stop();
    //runtime_ms = cv.accumulated_milliseconds();
    //cv.dump(testname);
    break;
  case PBFS_WLS:
    //cv.start();
    gettimeofday(&t1,0);
    graph->pbfs_wls(s, distances);      
    gettimeofday(&t2,0);
    //cv.stop();
    //runtime_ms = cv.accumulated_milliseconds();
    //cv.dump(testname);
    break;
  default: break;
  }

  runtime_ms = (todval(&t2)-todval(&t1))/1000;

  // Verify correctness
  if (bfsArgs.check_correctness) {

    unsigned int *distverf = new unsigned int[numNodes];
    // cilk_for (int i = 0; i < numNodes; ++i) {
    //   distverf[i] = UINT_MAX;
    // }
    graph->bfs(s, distverf);
    if (!check(distances, distverf, numNodes))
      fprintf(stderr, "Error found in %s result.\n", ALG_NAMES[bfsArgs.alg_select]);
  }

  // Print results if debugging
  if (DEBUG) {
    for (int i = 0; i < numNodes; i++)
      printf("Distance to node %d: %d\n", i+1, distances[i]);
  }

  // Print runtime result
  switch(bfsArgs.alg_select) {
  case BFS:
    printf("BFS on %s: %f seconds\n", bfsArgs.filename.c_str(), runtime_ms/1000.0);
    break;
  case PBFS:
    printf("PBFS on %s: %f seconds\n", bfsArgs.filename.c_str(), runtime_ms/1000.0);
    break;
  case PBFS_WLS:
    printf("PBFS_WLS on %s: %f seconds\n", bfsArgs.filename.c_str(), runtime_ms/1000.0);
    break;
  default: break;
  }

  return 0;
}

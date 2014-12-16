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

#ifndef GRAPH_H
#define GRAPH_H

#include <vector>
#include <sys/types.h>
#include <time.h>

#include <cilk/cilk.h>

#include "bag.h"

typedef struct wl_stack wl_stack;

struct wl_stack {
  int top[2];
  unsigned int queue[0];
};

class Graph
{

 private:
  // Number of nodes
  int nNodes;
  // Number of edges
  int nEdges;
  
  int * nodes;
  int * edges;

  void pbfs_walk_Bag(Bag<int>*, Bag_reducer<int>*, unsigned int, unsigned int[]) const;
  void pbfs_walk_Pennant(Pennant<int>*, Bag_reducer<int>*, unsigned int,
			unsigned int[]) const;

 public:
  // Constructor/Destructor
  Graph(int *ir, int *jc, int m, int n, int nnz);
  ~Graph();

  // Accessors for basic graph data
  inline u_int numNodes() const { return nNodes; }
  inline u_int numEdges() const { return nEdges; }

  // Various BFS versions
  int bfs(const int s, unsigned int distances[]) const;
  int pbfs(const int s, unsigned int distances[]) const;
  int pbfs_wls(const int s, unsigned int distances[]) const;

};

#include "graph.cpp"

#endif

/*
 * Cilk program to solve the 0-1 knapsack problem using a branch-and-bound
 * technique. 
 *
 * Author: Matteo Frigo
 */
/*
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Matteo Frigo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <cilk/cilk.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "getoptions.h"

#if CILKSAN
#include "cilksan.h"
#endif

/* every item in the knapsack has a weight and a value */
#define MAX_ITEMS 256

struct item {
  int value;
  int weight;
};

int best_so_far = INT_MIN;

int foo() {
  return 1;
}

int compare(struct item *a, struct item *b) {

  double c = ((double) a->value / a->weight) -
    ((double) b->value / b->weight);

  if (c > 0)
    return -1;
  if (c < 0)
    return 1;
  return 0;
}

int read_input(const char *filename, 
               struct item *items, int *capacity, int *n) { 
  int i;
  FILE *f;

  if (filename == NULL)
    filename = "\0";
  f = fopen(filename, "r");
  if (f == NULL) {
    fprintf(stderr, "open_input(\"%s\") failed\n", filename);
    return -1;
  }
  /* format of the input: #items capacity value1 weight1 ... */
  fscanf(f, "%d", n);
  fscanf(f, "%d", capacity);

  for (i = 0; i < *n; ++i) {
    fscanf(f, "%d %d", &items[i].value, &items[i].weight);
  }

  fclose(f);

  /* sort the items on decreasing order of value/weight */
  qsort(items, *n, sizeof(struct item), 
        (int (*)(const void *, const void *)) compare);

  return 0;
}

/* 
 * return the optimal solution for n items (first is e) and
 * capacity c. Value so far is v.
 */
int knapsack(struct item *e, int c, int n, int v) {

  int with = 0, without = 0, best;
  double ub;

  /* base case: full knapsack or no items */
  if (c < 0) {
    return INT_MIN;
  }

  if (n == 0 || c == 0) {
    return v;		/* feasible solution, with value v */
  }

  ub = (double) v + c * e->value / e->weight;

  if (ub < best_so_far) {
    // prune !
    return INT_MIN;
  }

  without = cilk_spawn knapsack(e + 1, c, n - 1, v);
  with = knapsack(e + 1, c - e->weight, n - 1, v + e->value);
  cilk_sync; 

  best = with > without ? with : without;

  /* 
   * notice the race condition here. The program is still
   * correct, in the sense that the best solution so far
   * is at least best_so_far. Moreover best_so_far gets updated
   * when returning, so eventually it should get the right
   * value. The program is highly non-deterministic.
   */
  if (best > best_so_far) {
    // __cilkscreen_disable_checking();
    best_so_far = best;
    // __cilkscreen_enable_checking();
  }

  return best;
}

int usage(void) {

  fprintf(stderr, "\nUsage: "
      "knapsack [<cilk-options>] [-f filename] [-benchmark] [-h]\n\n");
  fprintf(stderr, "The 0-1-Knapsack is a standard combinatorial "
      "optimization problem: ``A\n");
  fprintf(stderr, "thief robbing a store finds n items; the ith item "
      "is worth v_i dollars\n");
  fprintf(stderr, "and weighs w_i pounds, where v_i and w_i are integers."
      "He wants to take\n");
  fprintf(stderr, "as valuable a load as possible, but he can carry "
      "at most W pounds in\n");
  fprintf(stderr, "his knapsack for some integer W. What items "
      "should he take?''\n\n");
  return -1;
}

const char *specifiers[] = {"-f", "-benchmark", "-h", 0};
int opt_types[] = {STRINGARG, BENCHMARK, BOOLARG, 0};

int main(int argc, char *argv[]) {

  struct item items[MAX_ITEMS];	/* array of items */
  int n, capacity, sol, benchmark, help;
  char filename[100];

  /* standard benchmark options */
  strcpy(filename, "knapsack-example2.input");

  get_options(argc, argv, specifiers, opt_types, filename, &benchmark, &help);

  if (help)
    return usage();

  if (benchmark) {
    switch (benchmark) {
      case 1:		/* short benchmark options -- a little work */
        strcpy(filename, "knapsack-example1.input");
        break;
      case 2:		/* standard benchmark options */
        strcpy(filename, "knapsack-example2.input");
        break;
      case 3:		/* long benchmark options -- a lot of work */
        strcpy(filename, "knapsack-example3.input");
        break;
    }
  }
  if (read_input(filename, items, &capacity, &n))
    return 1;

  best_so_far = INT_MIN;
  sol = knapsack(items, capacity, n, 0);

  printf("\nCilk Example: knapsack\n");
  printf("options: problem-file = %s\n\n", filename);
  printf("Best value is %d\n\n", sol);

  return 0;
}

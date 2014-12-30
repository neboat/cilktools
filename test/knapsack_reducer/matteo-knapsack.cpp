#include <stdio.h>
#include <algorithm>
#include <vector>
#include <string.h>
#include <math.h>
// #include <cilk/cilk_mutex.h>
#include <sys/time.h>
#include <cilk/cilk.h>
#include <cilk/reducer.h>
// #include <cilksan.h>
#include <reducertool.h>

struct item {
    /* weight of this item */
    double weight;

    /* value of this item */
    double val;

    /* name of this item */
    const char *name;

    /* multiplicity of this item, stored as a double
       to avoid int->double conversions */
    double multiplicity;

    /* minimum weight of all the successors of this item in sorted
       order, inclusive of this */
    double min_weight_scan;

    /* next item in a solution */
    item *cdr;

    double sum_weight_scan;
    double sum_val_scan;

    item(double w, double v, const char *nam, double m)
        : weight(w), val(v), name(nam), multiplicity(m)
          /* MIN_WEIGHT_SCAN intentionally left unspecified */
    { }

    item(const item &other, item *cdr)
        : weight(other.weight)
          , val(other.val)
          , name(other.name)
          , multiplicity(other.multiplicity)
          , cdr(cdr)
          /* MIN_WEIGHT_SCAN intentionally left unspecified */
    { }

    item(const item &other, double m, item *cdr)
        : weight(other.weight)
          , val(other.val)
          , name(other.name)
          , multiplicity(m)
          , cdr(cdr)
          /* MIN_WEIGHT_SCAN intentionally left unspecified */
    { }

};

/* note that this is really operator>().  The stupid std::sort
   wants a function called operator<(), and I don't feel like
   writing the comparator functor */
inline bool operator<(const item &a, const item &b) {
    return (a.val * b.weight) > (b.val * a.weight);
}

typedef std::vector<item>::iterator item_iterator;

struct solution {
    double capacity;
    double val;
    item *items;

    solution() : capacity(0), val(0), items(0) {}

    ~solution() {
        delete_items();
    }

    void update(double newcap, double newval, item *newitems) {
        if (newval > val) {
            delete_items();
            add_items(newitems);
            val = newval;
            capacity = newcap;
        }
    }

    void delete_items() {
        item *p;
        while ((p = items)) {
            items = p->cdr;
            delete p;
        }
    }

    void move(solution *other) {
        delete_items();
        items = other->items; other->items = 0;
        val = other->val;
        capacity = other->capacity;
    }

    void add_items(item *newitems) {
        for ( ; newitems; newitems = newitems->cdr)
            items = new item(*newitems, items);
    }
};

struct solution_monoid : cilk::monoid_base<solution> {

    void reduce(solution *left, solution *right) const {
        BEGIN_REDUCE_STRAND {
            if (right->val > left->val)
                left->move(right);
        } END_REDUCE_STRAND;
    }
    void identity(solution *p) const {
        BEGIN_UPDATE_STRAND {
            new (p) solution();
        } END_UPDATE_STRAND;
    }
};

cilk::reducer<solution_monoid> the_solution;
static volatile double best_so_far;
// cilk::mutex best_so_far_lock;


/* compute the greey bound to the solution.  If LOWERP, compute a
   lower bound, else an upper bound */
static inline double either_bound(double capacity,
				  item_iterator begin,
				  item_iterator end,
				  bool lowerp)
{
    double val = 0;
    for (item_iterator i = begin; i != end; ++i) {
	if (i->multiplicity * i->weight < capacity) {
	    /* this item fits into the knapsack.
	       Add the value and continue with the next item */
	    val += i->multiplicity * i->val;
	    capacity -= i->multiplicity * i->weight;
	} else {
	    if (lowerp) {
		/* add an integral item and stop */
		val += i->val * floor(capacity / i->weight);
	    } else {
		/* add a fractional item and stop */
		val += i->val * (capacity / i->weight);
	    }
	    break;
	}
    }
    return val;
}

static inline double lower_bound(double capacity,
				 item_iterator begin,
				 item_iterator end)
{
    return either_bound(capacity, begin, end, true);
}


static inline double upper_bound(double capacity,
				 item_iterator begin,
				 item_iterator end)
{
    return either_bound(capacity, begin, end, false);
}

static void init_lp(item_iterator begin, item_iterator end)
{
    double val = 0;
    double capacity = 0;
    for (item_iterator i = begin; i != end; ++i) {
	i->sum_val_scan = val;
	i->sum_weight_scan = capacity;
	val += i->multiplicity * i->val;
	capacity += i->multiplicity * i->weight;
    }
}

/* compute the solution to the relaxed non-integer linear program,
   which is an upper bound to the value of a range (BEGIN, END) of
   items for given CAPACITY */
static inline double lp_relaxation(double capacity,
				   item_iterator begin,
				   item_iterator end)
{
    item_iterator lo = begin, hi = end;

    /* binary search for the largest item LO that does not exceed the
       capacity */
    while (hi - lo > 1) {
	item_iterator mid = lo + (hi - lo) / 2;
	if (capacity + begin->sum_weight_scan < mid->sum_weight_scan)
	    hi = mid;
	else
	    lo = mid;
    }

    /* value of all items in the range [BEGIN, LO) */
    double base_val = (lo->sum_val_scan - begin->sum_val_scan); 

    /* fractional multiplicity of item LO */
    double f = ((capacity - (lo->sum_weight_scan - begin->sum_weight_scan)) /
		lo->weight);

    /* don't let F exceed the multiplicity of LO */
    if (f > lo->multiplicity)
	f = lo->multiplicity;

    base_val += lo->val * f;

    if (0) {
	/* debugging support */
	if (base_val != upper_bound(capacity, begin, end))
	    printf("BUG %g %g\n",
		   base_val, upper_bound(capacity, begin, end));
    }

    return base_val;
}

/* see any item in the DOMINATORS list dominates the current item
   IT. */
static inline bool is_dominated(item_iterator it, item *dominators)
{
    for (item *p = dominators; p; p = p->cdr) {
	/* If these conditions hold, then we can insert *P instead of
	 *IT for a net gain.  Consequently, any solution containing
	 IT but not P is suboptimal and should be discarded. */
	if (it->weight >= p->weight && it->val <= p->val)
	    return true;
    }
    return false;
}

static void knapsack(double capacity,
		     item *current,
		     item *dominators,
		     double val,
		     item_iterator b,
		     item_iterator e,
		     int spawn_depth);

static void place(double capacity,
		  item *current,
		  item *dominators,
		  double val,
		  item_iterator b,
		  item_iterator e,
		  int spawn_depth,
		  double multiplicity)
{
    item newcurr(*b, multiplicity, current);
    item newdom(*b, multiplicity, dominators);
    knapsack(capacity - multiplicity * newcurr.weight,
	     multiplicity > 0 ? &newcurr : current, 
	     multiplicity < b->multiplicity ? &newdom : dominators, 
	     val + multiplicity * newcurr.val,
	     b + 1, e, spawn_depth);
}

static void knapsack(double capacity,     // remaining capacity
		     item *current,       // items currently in the knapsack
		     item *dominators,
		     double val,          // value of the current solution
		     item_iterator b,
		     item_iterator e,
		     int spawn_depth)
{
    {/* Recursive base cases: */ 
	
	/* definitely infeasible: */
	if (capacity < 0) {
	    return;
	}

	/* cases where the problem is feasible and we are done */
	if (0
	    /* no more items: */
	    || (b == e)

	    /* the capacity is less than the minimum of all remaining
	       weights.  There is no point in searching further, since no
	       more items fit.*/
	    || (capacity < b->min_weight_scan)
	    )
	{
	    /* update the global pruning hint.  

	       Note the race condition between the first check and the
	       update.  The race is benign because BEST_SO_FAR is
	       monotonically increasing and doubles are atomic on x86. 
            */
	    
            /*
	    if (val > best_so_far) {
		// best_so_far_lock.lock();
		__cilksan_disable_checking();
		if (val > best_so_far)
		    best_so_far = val;
		__cilksan_enable_checking();
		// best_so_far_lock.unlock();
	    } */

            BEGIN_UPDATE_STRAND {
	        the_solution().update(capacity, val, current);
            } END_UPDATE_STRAND;

	    return;
	}
    }

    /* Pruning based on the linear-programming relaxation.  Note
       the benign race on BEST_SO_FAR 
    {
	if (val + lp_relaxation(capacity, b, e) < best_so_far)
	    return;
    }
    */

    /* recursive step: */ {
	/* try all possible multiplicities of the current item,
	   largest first. */
	for (double m = b->multiplicity; m >= 0; --m) {
	    /* avoid the spawn overhead if the item does not fit (the
	       callee would return immediately in this case) */
	    if (capacity < m * b->weight)
		continue;

	    /* if we are actually trying to insert an item into the
	       solution (i.e., m>0) and the item is dominated by some
	       other previous item, don't search */
	    if (m > 0 && is_dominated(b, dominators))
		continue;

	    /* annoying cilk misfeature: avoid overflowing the spawn
	       stack */
	    if (spawn_depth < 500)
		cilk_spawn place(capacity, current, dominators, val, b, e,
				 spawn_depth + 1, m);
	    else
		place(capacity, current, dominators, val, b, e,
		      spawn_depth + 1, m);
	}
    }
}

int untimed_cilk_main(int argc, char *argv[])
{
    std::vector<item> items;
    FILE *input, *output;

    /* under the assumption that the input has 2 decimal digits,
       multiply the input by SCALE so that we work with integers,
       which are represented exactly in double precision. */
    const double SCALE = 100.0;

    if (argc < 2) {
	fprintf(stderr, "usage: knapsack INPUT-FILE\n");
	return 1;
    }

    double capacity;
    int nitems;
    int multiplicity;

    input = fopen(argv[1], "r");
    if (!input) {
	fprintf(stderr, "cannot open %s\n", argv[1]);
	return 1;
    }

    if (fscanf(input, "%lg %d %d\n", &capacity, &multiplicity, &nitems) != 3)
	goto parse_error;

    capacity = rint(capacity * SCALE);

    for (int id = 0; id < nitems; ++id) {
	double weight, val;
	char name[14];
	if (fscanf(input, "%13c%lg%lg\n", name, &weight, &val) != 3) {
	    goto parse_error;
        }
	val = rint(val * SCALE);
	weight = rint(weight * SCALE);
	name[13] = 0;
	items.push_back(item(weight, val, strdup(name), multiplicity));
    }

    fclose(input);


    std::sort(items.begin(), items.end());

    the_solution().capacity = capacity;
    the_solution().val = 0;

    init_lp(items.begin(), items.end());

    /* start with the greedy integer solution as BEST_SO_FAR */
    best_so_far = lower_bound(capacity, items.begin(), items.end());

    /* perform a reverse scan to compute min_weight_scan */
    for (item_iterator i = items.end(); i != items.begin(); --i) {
	i[-1].min_weight_scan = i[-1].weight;
	if (i != items.end()) {
	    if (i[0].min_weight_scan < i[-1].min_weight_scan)
		i[-1].min_weight_scan = i[0].min_weight_scan;
	}
    }

    knapsack(capacity, 0, 0, 0, items.begin(), items.end(), 0);

    output = fopen("packinglist.out", "w");
    if (!output) {
	fprintf(stderr, "cannot open packinglist.out\n");
	return 1;
    }

    fprintf(output,
	    "The items to fit into the %.2f knapsack are:\n\n", 
	    capacity / SCALE);
    cilk_read_reducer(&the_solution, __builtin_return_address(0), __FUNCTION__, __LINE__);
    for (item *p = the_solution().items; p; p = p->cdr) {
	fprintf(output, "%2d %13s\n", (int)p->multiplicity, p->name);
    }
    cilk_read_reducer(&the_solution, __builtin_return_address(0), __FUNCTION__, __LINE__);
    fprintf(output, "\nTotal capacity used: %.2f\n",
	    (capacity - the_solution().capacity) / SCALE);
    cilk_read_reducer(&the_solution, __builtin_return_address(0), __FUNCTION__, __LINE__);
    fprintf(output, "Total value: %.2f\n",  the_solution().val / SCALE);

    fclose(output);
    return 0;

parse_error:
    fprintf(stderr, "error reading input\n");
    return 1;
}

int main(int argc, char *argv[]) {

/*
    timeval start, stop;
    gettimeofday(&start, 0);

    int retval = untimed_cilk_main(argc, argv);
    gettimeofday(&stop, 0);

    fprintf(stderr,
	    "execution time (including everything): %g s\n",
	    (stop.tv_sec - start.tv_sec) +
	    1.0e-6 * (stop.tv_usec - start.tv_usec));
*/
    // argc = __cilksan_parse_input(argc, argv);
  cilk_set_reducer(&the_solution, __builtin_return_address(0), __FUNCTION__, __LINE__);
    int retval = untimed_cilk_main(argc, argv);

    return retval;
}

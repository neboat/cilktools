#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#define COARSENING 32

///////////////////////////////////////////////////////////////////////////
// Stolen from cilktest_timing.h
#ifdef _WIN32
#   ifndef _WINBASE_
__CILKRTS_BEGIN_EXTERN_C
unsigned long __stdcall GetTickCount();
__CILKRTS_END_EXTERN_C
#   endif
#endif  // _WIN32

#if defined __unix__ || defined __APPLE__
#   include <sys/time.h>
#endif  // defined __unix__ || defined __APPLE__

/// @brief Return the system clock with millisecond resolution
///
/// This function returns a long integer representing the number of
/// milliseconds since an arbitrary starting point, e.g., since the system was
/// started or since the Unix Epoch.  The result is meaningless by itself, but
/// the difference between two sequential calls to CILKTEST_GETTICKS()
/// represents the time interval that elapsed between them (in ms).
static inline unsigned long long CILKTEST_GETTICKS()
{
    unsigned long long ans;
    // When inlined, prevent code motion around this call
#if __INTEL_COMPILER > 1200
    __notify_zc_intrinsic((void*) "CILKTEST_GETTICKS_START", 0);
#endif

#ifdef _WIN32
    // Return milliseconds elapsed since the system started
    ans = GetTickCount();
#elif defined(__unix__) || defined(__APPLE__)
    // Return milliseconds elapsed since the Unix Epoch
    // (1-Jan-1970 00:00:00.000 UTC)
    struct timeval t;
    gettimeofday(&t, 0);
    ans = t.tv_sec * 1000ULL + t.tv_usec / 1000;
#else
#   error CILKTEST_GETTICKS() not implemented for this OS
#endif
    /* UNREACHABLE */

    // When inlined, prevent code motion around this call
#if __INTEL_COMPILER > 1200
    // Isn't that the point of having the annotations?
    __notify_zc_intrinsic((void*) "CILKTEST_GETTICKS_END", 0);
#endif
    return ans;
}

///////////////////////////////////////////////////////////////////////////

size_t partition(int64_t array[], size_t n, size_t l, size_t h)
{
  // TB: I'm cheating here, because I know the array is initially
  // randomized
  int64_t pivot = array[l];

  size_t left = l-1;
  size_t right = h;
  while (true) {
    do { ++left; } while (array[left] < pivot);
    do { --right; } while (array[right] > pivot);
    if (left < right) {
      int64_t tmp = array[left];
      array[left] = array[right];
      array[right] = tmp;
    } else {
      return (left == l ? left + 1 : left);
    }
  }
}

void quicksort(int64_t array[], size_t n, size_t l, size_t h)
{
  if (l >= h || l >= n || h > n) return;

  if (h - l < COARSENING) {
    while (l < h) {
      int64_t min = array[l];
      int64_t argmin = l;
      for (size_t i = l+1; i < h; ++i) {
        if (array[i] < min) {
          min = array[i];
          argmin = i;
        }
      }
      array[argmin] = array[l];
      array[l++] = min;
    }
    return;
  }

  size_t part = partition(array, n, l, h);
  _Cilk_spawn quicksort(array, n, l, part);
  quicksort(array, n, part, h);
}

int main(int argc, char* argv[])
{
  size_t n = 1 << 10;

  uint64_t start, end;

  bool verify = false;

  for (size_t i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-h") == 0) {
      fprintf(stderr, "Usage:\t%s [-verify] [-n <input array size>]\n", argv[0]);
      exit(1);
    }
    if (strcmp(argv[i], "-verify") == 0) {
      verify = true;
      continue;
    }
    if (strcmp(argv[i], "-n") == 0) {
      n = std::atoi(argv[++i]);
      continue;
    }
  }
  

  srand(733);

  int64_t* array = new int64_t[n];

  // Initialize input array
  for (size_t i = 0; i < n; ++i) {
    array[i] = (rand() % 16) - 8;
  }

  start = CILKTEST_GETTICKS();
  quicksort(array, n, 0, n);
  end = CILKTEST_GETTICKS();


  if (verify) {
    for (size_t i = 1; i < n; ++i) {
      if (array[i] < array[i-1]) {
        fprintf(stderr, "ERROR: array[%lu] = %ld is less than array[%lu] = %ld\n",
                i, array[i], i-1, array[i-1]);
      }
    }
  }

  printf("quicksort(%lu): %0.3f\n", n, (end-start) * 1.0e-3);

  return 0;
}

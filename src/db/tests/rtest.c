// g++ -fsanitize=address -Wall -ggdb3 -g test.cc -o test -lpthread -lm -fsanitize=address && ./test
// clang++ -Wall -march=native -O3 test.cc -o rtest -lpthread -lm && ./rtest
#include "sort.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

int main()
{
  threads_global_init();
  const int N = 1000000;
  uint64_t *arr = malloc(sizeof(uint64_t)*N);
  for(int k=0;k<N;k++) arr[k] = ((uint64_t)lrand48() << 32) | lrand48();
  clock_t t = clock();
  parallel_sort(arr, N);
  t = clock() - t;
  fprintf(stderr, "time to sort %d entries %g s\n", N, t/(double)CLOCKS_PER_SEC);
  for(int k=1;k<N;k++)
  {
    assert(arr[k] > arr[k-1]);
    // fprintf(stderr, "%lu ", arr[k]);
  }
  // fprintf(stderr, "\n");
  free(arr);
  threads_global_cleanup();
  return 0;
}

#include "threads.h"
#include "qsort.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

int main()
{
  threads_global_init(); // init thread pool
  const int N = 1000000;
  uint64_t *arr = malloc(sizeof(uint64_t)*N);
  for(int k=0;k<N;k++) arr[k] = ((uint64_t)lrand48() << 32) | lrand48();
  clock_t t = clock();
  pqsort(arr, N);
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
  exit(0);
}

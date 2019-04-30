// https://github.com/vvnguyen/parallel-in-place-radix-sort
// see dr.dobbs journal on the issue
// TODO: we need pthread pool parallelism
#pragma once

#include <stdint.h>
#include <assert.h>
#include <time.h>
// #include <atomic>
#include <thread>
#include <iostream>
// #include <mutex>
#include <functional>
// #include <cassert>
// #include <omp.h>

static inline void
insertion_sort(uint64_t* list, const int size)
{
	uint64_t temp, j;

	for (int i = 1; i < size; i++)
	{
		temp = list[i];

		for (j = i - 1; j >= 0 && list[j] > temp; j--)
			list[j + 1] = list[j];

		list[j + 1] = temp;
	}
}

void srt(int* histogram, uint64_t **marker, int shift)
{
  const int mask = 255;
  for (uint64_t k = 0; k < 256; ++k)
  {
    int left = k ? (histogram[k]-(marker[k] - marker[k - 1])) : histogram[0];
    while (left-- > 0)
    {
      uint64_t index = (*marker[k] >> shift) & mask;
      // swap elements to beginning of bin
      while (index != k)
      {
        uint64_t tmp = *marker[index];
        *marker[index]++ = *marker[k];
        *marker[k] = tmp;
        index = (*marker[k] >> shift) & mask;
      }
      ++marker[k];
    }
  }
}

// stable; in place; linear
static inline void
sort_block(uint64_t *list, const int size, const int shift)
{
	const int mask = 255;
	int histogram[256] = {0};

	for (int i = 0; i < size; ++i)
		++histogram[(list[i] >> shift) & mask];

	// markers
	uint64_t *marker[256];
	marker[0] = list;
	for (int j = 0; j < 255; ++j)
		marker[j + 1] = marker[j] + histogram[j];

  srt(histogram, marker, shift);

  if(shift)
  { // recurse
    // initialy all markers where in the beggining of sections, after sorting
    // each is in the end of section it was pointing too
    for (int l = 255; l >= 0; --l)
    {
      // TODO: push these jobs back into a parallel queue, too:
      if (histogram[l] > 32)
        //radix sort
        sort_block(marker[l] - histogram[l], histogram[l], shift-8);
      else if(histogram[l] > 1)
        //insertion sort
        insertion_sort(marker[l] - histogram[l], histogram[l]);
    }
  }
}

// TODO: this stuff needs to be parallel, too:
static inline
void count(uint64_t *list, int size, int *histogram) {
  const int shift = 56;//24;
	for (int i = 0; i < size; ++i) {
		++histogram[list[i]>>shift];
	}
}


void go_srt(int* histogram, uint64_t **marker, int &nr, const int max_nr)
{
  // atomic!
	int index;
	// m.lock();
  index = __sync_fetch_and_add(&nr, 1);
	// index = nr;
	// ++nr;
	// m.unlock();
	// int size = 0;
	while (index < max_nr)
  {
		if (histogram[index] > 64)
			sort_block((marker[index] - histogram[index]), histogram[index], 48);//16);
		else if (histogram[index] > 1)
			insertion_sort((marker[index] - histogram[index]), histogram[index]);
		// m.lock();
		// index = nr;
		// ++nr;
		// m.unlock();
    index = __sync_fetch_and_add(&nr, 1);
	}
}


void parallel_sort(uint64_t *arr, int size)
{
  // build histogram
  int histogram[256] = { 0 };

  clock_t t = clock();
  // compute prefix sum
  count(arr, size, histogram);

  uint64_t *marker[256];
  uint64_t *previous = arr;
  for ( int i = 0; i < 256; ++i ) 
  {
    marker[i] = previous;
    previous += histogram[i];
  }
  assert(previous == arr + size);

  srt(histogram, marker, 56);
  t = clock() - t;
  fprintf(stderr, "serial block: %g s\n", t/(double)CLOCKS_PER_SEC);
  // mutex M;
  int nr = 0;
  const int max_nr = 256;
		
  // TODO: this does not scale at all:
  // TODO: also, the serial variant is 25% faster.
		// thread t1(go_srt, histogram, marker, std::ref(nr), max_nr);
		// thread t2(go_srt, histogram, marker, std::ref(nr), max_nr);
		// thread t3(go_srt, histogram, marker, std::ref(nr), max_nr);
		go_srt(histogram, marker, std::ref(nr), max_nr);
		// t1.join();
		// t2.join();
		// t3.join();
}

#pragma once
#include "sort_r.h"
// portable sort
typedef int sort_compare_t(const void *, const void *, void *);
typedef struct sort_wrapper_t
{
  sort_compare_t *f;
  void           *d;
}
sort_wrapper_t;
#if defined(__APPLE__) || defined(_WIN64)
static int sort_wrap_compare(void *w, const void *a, const void *b)
{
  sort_wrapper_t *wrap = (sort_wrapper_t *)w;
  return (*wrap->f)(a, b, wrap->d);
}
#endif
static inline void sort(void *base, size_t nmemb, size_t size, sort_compare_t *compare, void *data)
{
#if defined(__APPLE__) || defined(_WIN64)
  sort_wrapper_t w = {.f = compare, .d = data};
#endif

#if defined(__APPLE__)
  qsort_r(base, nmemb, size, &w, sort_wrap_compare);
#elif defined(_WIN64)
  qsort_s(base, nmemb, size, sort_wrap_compare, &w);
#elif defined(__linux__) && defined(_GNU_SOURCE) && !defined(__ANDROID_NDK__)
  qsort_r(base, nmemb, size, compare, data);
#else
  sort_r(base, nmemb, size, compare, data);
#endif
}

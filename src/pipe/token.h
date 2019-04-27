#pragma once
#include <assert.h>
#include <string.h>
#include <stdint.h>

// for efficient storage and comparisons and sort etc,
// a token is limited to 8 chars, and stored as int64:
typedef uint64_t dt_token_t;

// create a token from a string:
static inline dt_token_t
dt_token(const char *str)
{
  // remember to switch off assertions for release:
  assert(strnlen(str, 9) <= 8);
  // this loop unfortunately needs to check whether the string
  // pointer is aligned or terminates early. gcc 8.3 carries every
  // byte through individually, clang-8.0 does it in four steps
  uint64_t t = 0ul;
  for(int i=0;i<8;i++)
    if(str[i] != 0)
      t |= ((uint64_t)str[i])<<(8*i);
  return t;
}

// reinterpret a token as a string
#define dt_token_str(A) ((const char *const)&(A))

// use this literal to print it, it's not necessarily 0-terminated
#define PRItkn ".8s"

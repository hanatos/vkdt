#pragma once
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN64
#define VKDT_API __declspec(dllexport)
#else
#define VKDT_API
#endif

// for efficient storage and comparisons and sort etc,
// a token is limited to 8 chars, and stored as int64:
typedef uint64_t dt_token_t;

// create a token from a string:
// static inline dt_token_t
// dt_token(const char *str)
// {
//   // remember to switch off assertions for release:
//   assert(strnlen(str, 8) <= 8);
//   // this loop unfortunately needs to check whether the string
//   // pointer is aligned or terminates early. gcc 8.3 carries every
//   // byte through individually, clang-8.0 does it in four steps
//   uint64_t t = 0ul;
//   for(int i=0;i<8;i++)
//     if(str[i] != 0)
//       t |= ((uint64_t)str[i])<<(8*i);
//     else break;
//   return t;
// }

// reinterpret a token as a string
#define dt_token_str(A) ((char *const)&(A))

// reinterpret string as a token. this is butt-ugly, does the same as the
// dt_token() constructor above, but can be used in const expressions.
#define dt_token(A) \
  ((A)[0] ? (\
   (A)[1] ? (\
   (A)[2] ? (\
   (A)[3] ? (\
   (A)[4] ? (\
   (A)[5] ? (\
   (A)[6] ? (\
   (A)[7] ? (\
   (A)[0] | ((uint64_t)(A)[1]<<8) | ((uint64_t)(A)[2]<<16) | ((uint64_t)(A)[3]<<24) | ((uint64_t)(A)[4] << 32) | ((uint64_t)(A)[5]<<40) | ((uint64_t)(A)[6]<<48) | ((uint64_t)(A)[7]<<56)) : \
   (A)[0] | ((uint64_t)(A)[1]<<8) | ((uint64_t)(A)[2]<<16) | ((uint64_t)(A)[3]<<24) | ((uint64_t)(A)[4] << 32) | ((uint64_t)(A)[5]<<40) | ((uint64_t)(A)[6]<<48)) : \
   (A)[0] | ((uint64_t)(A)[1]<<8) | ((uint64_t)(A)[2]<<16) | ((uint64_t)(A)[3]<<24) | ((uint64_t)(A)[4] << 32) | ((uint64_t)(A)[5]<<40)) : \
   (A)[0] | ((uint64_t)(A)[1]<<8) | ((uint64_t)(A)[2]<<16) | ((uint64_t)(A)[3]<<24) | ((uint64_t)(A)[4] << 32)) : \
   (A)[0] | ((uint64_t)(A)[1]<<8) | ((uint64_t)(A)[2]<<16) | ((uint64_t)(A)[3]<<24)) : \
   (A)[0] | ((uint64_t)(A)[1]<<8) | ((uint64_t)(A)[2]<<16)) : \
   (A)[0] | ((uint64_t)(A)[1]<<8)) : \
   (A)[0]) \
   : 0)

// use this literal to print it, it's not necessarily 0-terminated
#define PRItkn ".8s"


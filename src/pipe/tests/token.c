#include "../token.h"

#include <stdlib.h>
#include <stdio.h>

int main()
{
  // assembly check says these are inlined compile time, cool:
  // (both clang-8.0 and gcc-8.3 do it)
  const dt_token_t test0 = dt_token("exposure");
  const dt_token_t test1 = dt_token("1234");
  const dt_token_t test2 = -1ul; // something like "ÿÿÿÿÿÿÿÿ" only you can't type it
  const dt_token_t test3 = dt_token("none"); // 0x656E6F6E
  const dt_token_t test4 = 0ul; // 


  fprintf(stderr, "token '%" PRItkn "' and '%" PRItkn
      "' -1 token '%" PRItkn"' or %lX and '%"PRItkn"'\n",
      dt_token_str(test0),
      dt_token_str(test1),
      dt_token_str(test2),
      test3,
      dt_token_str(test4));


  dt_token_t a = dt_token("ui32");
  dt_token_t b = dt_token("f32");
  dt_token_t c = dt_token("ui16");
  dt_token_t d = dt_token("ui8");
  fprintf(stderr, "%lX %lX %lX %lX\n",
      a, b, c, d);

  exit(0);
}


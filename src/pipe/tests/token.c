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


  fprintf(stderr, "token '%" PRItkn "' and '%" PRItkn
      "' -1 token '%" PRItkn"' or %lX\n",
      dt_token_str(test0),
      dt_token_str(test1),
      dt_token_str(test2),
      test3);

  exit(0);
}


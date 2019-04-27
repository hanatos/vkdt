#include "../token.h"

#include <stdlib.h>
#include <stdio.h>

int main()
{
  // assembly check says these are inlined compile time, cool:
  // (both clang-8.0 and gcc-8.3 do it)
  dt_token_t test0 = dt_token("exposure");
  dt_token_t test1 = dt_token("1234");

  fprintf(stderr, "token '%" PRItkn "' and '%" PRItkn "'\n",
      dt_token_str(test0),
      dt_token_str(test1));

  exit(0);
}


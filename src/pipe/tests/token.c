#include "../token.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  if(argc < 2) exit(1);
  // assembly check says these are inlined compile time, cool:
  // (both clang-8.0 and gcc-8.3 do it)
  const dt_token_t test = dt_token(argv[1]);
  fprintf(stdout, "%lX %"PRItkn"\n", test, dt_token_str(test));
  exit(0);
}


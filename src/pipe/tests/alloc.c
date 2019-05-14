#include "../alloc.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *arg[])
{
  dt_vkalloc_t a;
  dt_vkalloc_init(&a);
  // alloc a few test things with known outcome

  dt_vkmem_t *test[70] = {0};

  int err;
  err = dt_vkalloc_check(&a);
  assert(!err);

  for(int i=0;i<70;i++)
  {
    uint64_t size = 1337 + 10*i;
    test[i] = dt_vkalloc(&a, size);
    err = dt_vkalloc_check(&a);
    assert(!err);
  }
  // TODO: random order!
  for(int i=0;i<70;i+=2)
  {
    dt_vkfree(&a, test[i]);
    err = dt_vkalloc_check(&a);
    assert(!err);
  }
  for(int i=70-1;i>=0;i-=2)
  {
    dt_vkfree(&a, test[i]);
    err = dt_vkalloc_check(&a);
    assert(!err);
  }

  dt_vkalloc_cleanup(&a);
  exit(0);
}

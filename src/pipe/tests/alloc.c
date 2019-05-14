#include "../alloc.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *arg[])
{
  dt_vkalloc_t a;
  dt_vkalloc_init(&a);
  // TODO: alloc a few test things with know outcome

  // TODO: alloc a bunch of random sizes, make sure rss etc matches
  // TODO: implement and run consistency checks on internal state of vkalloc struct
  dt_vkalloc_cleanup(&a);
  exit(0);
}

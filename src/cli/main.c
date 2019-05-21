#include "qvk/qvk.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
  if(qvk_init()) exit(1);

  // TODO: create module graph from pipeline config
  // TODO: create nodes from graph

  qvk_cleanup();
  exit(0);
}

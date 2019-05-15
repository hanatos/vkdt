#include "pipe/global.h"

#include <stdlib.h>
#include <stdio.h>

int main(int cnt, char *argv[])
{
  dt_pipe_global_init();
  for(int i=0;i<dt_pipe.num_modules;i++)
  {
    dt_module_so_t *m = dt_pipe.module + i;
    fprintf(stderr, "module %"PRItkn":\n", dt_token_str(m->name));
    for(int p=0;p<m->num_params;p++)
    {
      fprintf(stderr, "param %d: %"PRItkn" %"PRItkn" %d\n",
          p, dt_token_str(m->param[p]->name), dt_token_str(m->param[p]->type), m->param[p]->cnt);
    }
    for(int p=0;p<m->num_connectors;p++)
    {
      fprintf(stderr, "connector %d: %"PRItkn" %"PRItkn" %"PRItkn" %"PRItkn"\n",
          p,
          dt_token_str(m->connector[p].name),
          dt_token_str(m->connector[p].type),
          dt_token_str(m->connector[p].chan),
          dt_token_str(m->connector[p].format));
    }
  }
  exit(0);
}

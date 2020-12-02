#include "rc.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
  dt_rc_t rc;
  dt_rc_init(&rc);

  dt_rc_set_int(&rc, "test/integer", 1337);
  dt_rc_set_float(&rc, "test/float", 1.0027f);
  dt_rc_set(&rc, "test/string", "boom!");

  dt_rc_write(&rc, "test.rc");
  dt_rc_read (&rc, "test.rc");

  dt_rc_cleanup(&rc);
  exit(0);
}

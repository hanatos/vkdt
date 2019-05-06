/*
 * Copyright (C) 2013 John Graham <johngavingraham@googlemail.com>.
 *
 * This file is part of Pthread Pool.
 *
 * Pthread Pool is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * Pthread Pool is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Pthread Pool.  If not, see
 * <http://www.gnu.org/licenses/>.
 */


#ifndef FATAL_H
#define FATAL_H



#include <errno.h>
#include <stdio.h>
#include <stdlib.h>



/*
 * Not for use (normally); use fatal() instead.
 */
#define _fatal(file, line, msg) \
  do {                                       \
    fprintf(stderr, "%s:%d: %s\n",           \
            file, line, (msg));              \
    abort();                                 \
  } while (0)

/*
 * For when there's no choice other than to ditch. Use like:
 *
 *   fatal("What went wrong");
 */
#define fatal(msg) \
  do { _fatal(__FILE__, __LINE__, (msg)); } while (0)

/*
 * For when a return value of EINVAL indicates a programming error on
 * our part.
 */
#define fatal_inval(call) \
  do { if (EINVAL == (call)) fatal(#call " => EINVAL"); } while (0)



#endif /* FATAL_H */
























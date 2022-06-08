#pragma once
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static inline int // returns zero on success
dt_file_copy(
    const char *dst,
    const char *src)
{
  ssize_t ret;
  struct stat sb;
  loff_t len = -1;
  int fd0 = open(src, O_RDONLY), fd1 = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if(fd0 == -1 || fd1 == -1)  goto copy_error;
  if(fstat(fd0, &sb) == -1)   goto copy_error;
  len = sb.st_size;
  do {
    ret = copy_file_range(fd0, 0, fd1, 0, len, 0);
  } while((len-=ret) > 0 && ret > 0);
copy_error:
  if(fd0 >= 0) close(fd0);
  if(fd0 >= 0) close(fd1);
  return len;
}

static inline int // returns zero on success
dt_file_delete(
    const char *fn)
{
  return unlink(fn);
}

static inline int // returns zero on success
dt_mkdir(
    const char *pathname,
    mode_t      mode)
{
  return mkdir(pathname, mode);
}

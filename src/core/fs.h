#pragma once
#ifdef __linux__
  #ifdef LIBVKDT
    #include <dlfcn.h>
    #include <link.h>
  #endif
#endif
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <errno.h>

static inline int // returns zero on success
fs_copy(
    const char *dst,
    const char *src)
{
  ssize_t ret;
  struct stat sb;
  loff_t len = -1;
  int fd0 = open(src, O_RDONLY), fd1 = -1;
  if(fd0 == -1 || fstat(fd0, &sb) == -1) goto copy_error;
  len = sb.st_size;
  if(sb.st_mode & S_IFDIR) { len = 0; goto copy_error; } // don't copy directories
  if(-1 == (fd1 = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0644))) goto copy_error;
  do {
    ret = sendfile(fd1, fd0, 0, len); // works on linux >= 2.6.33, else fd1 would need to be a socket
  } while((len-=ret) > 0 && ret > 0);
copy_error:
  if(len != 0) fprintf(stderr, "[fs_copy] %s\n", strerror(errno));
  if(fd0 >= 0) close(fd0);
  if(fd0 >= 0) close(fd1);
  return len;
}

static inline int // returns zero on success
fs_delete(
    const char *fn)
{
  return unlink(fn);
}

static inline int // returns zero on success
fs_mkdir(
    const char *pathname,
    mode_t      mode)
{
  return mkdir(pathname, mode);
}

static inline int // return zero if argument contains no '/', else alter str
fs_dirname(char *str)
{ // don't use: dirname(3) since it may or may not alter the argument, implementation dependent.
  char *c = 0;
  for(int i=0;str[i]!=0;i++) if(str[i] == '/') c = str+i;
  if(c)
  {
    *c = 0;  // get dirname, i.e. strip off file name
    return 1;
  }
  return 0;
}

// pretty much the POSIX version. never modifies str but returns no const
// to not force a const cast onto you.
static inline char* // returns pointer to empty string (end of str) if str ends in '/'
fs_basename(char *str)
{ // don't use basename(3) because there are at least two versions of it which sometimes modify str
  char *c = str; // return str if it contains no '/'
  for(int i=0;str[i]!=0;i++) if(str[i] == '/') c = str+i;
  return c+1;
}

static inline void  // ${HOME}/.config/vkdt
fs_homedir(
    char *basedir,  // output will be copied here
    size_t maxlen)  // allocation size
{
  snprintf(basedir, maxlen, "%s/.config/vkdt", getenv("HOME"));
}

static inline int  // returns the directory where the actual binary (not the symlink) resides
fs_basedir(
    char *basedir,  // output will be copied here
    size_t maxlen)  // allocation size
{
#ifdef __linux__
#ifdef LIBVKDT
  void *handle;
  char mod[PATH_MAX];
  handle = dlopen("libvkdt.so", RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "%s\n", dlerror());
	return(1);
  }
  dlinfo(handle, RTLD_DI_ORIGIN, &mod);
  snprintf(basedir, maxlen, "%s/", mod);
  dlclose(handle);
#else
  // stupid allocation dance because passing basedir directly
  // may or may not require PATH_MAX bytes instead of maxlen
  char *bd = realpath("/proc/self/exe", 0);
  snprintf(basedir, maxlen, "%s", bd);
  free(bd);
  #endif
#elif defined(__FreeBSD__)
  int mib_procpath[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  size_t len_procpath = maxlen;
  sysctl(mib_procpath, 4, basedir, &len_procpath, NULL, 0);
#else
#error port me
#endif
  fs_dirname(basedir);
  return(0);
}

static inline int // return the number of devices found
fs_find_usb_block_devices(
    char devname[20][20],
    char mountpoint[20][50])
{ // pretty much ls /sys/class/scsi_disk/*/device/block/{sda,sdb,sdd,..}/{..sdd1..} and then grep for it in /proc/mounts
  int cnt = 0;
  char block[1000];
  struct dirent **ent, **ent2;
  int ent_cnt = scandir("/sys/class/scsi_disk/", &ent, 0, alphasort);
  if(ent_cnt < 0) return 0;
  for(int i=0;i<ent_cnt&&cnt<20;i++)
  {
    snprintf(block, sizeof(block), "/sys/class/scsi_disk/%s/device/block", ent[i]->d_name);
    int ent2_cnt = scandir(block, &ent2, 0, alphasort);
    if(ent2_cnt < 0) goto next;
    for(int j=0;j<ent2_cnt&&cnt<20;j++)
    {
      for(int k=1;k<10&&cnt<20;k++)
      {
        snprintf(block, sizeof(block), "/sys/class/scsi_disk/%s/device/block/%s/%.13s%d", ent[i]->d_name,
            ent2[j]->d_name, ent2[j]->d_name, k);
        struct stat sb;
        if(stat(block, &sb)) break;
        if(sb.st_mode & S_IFDIR)
          snprintf(devname[cnt++], sizeof(devname[0]), "/dev/%.13s%d", ent2[j]->d_name, k%10u);
      }
      free(ent2[j]);
    }
    free(ent2);
next:
    free(ent[i]);
  }
  free(ent);
  for(int i=0;i<cnt;i++) mountpoint[i][0] = 0;
  FILE *f = fopen("/proc/mounts", "r");
  if(f) while(!feof(f))
  {
    char mp[50];
    if(fscanf(f, "%999s %49s %*[^\n]", block, mp) >= 2)
    for(int i=0;i<cnt;i++)
    {
      if(!strcmp(block, devname[i]))
      {
        strncpy(mountpoint[i], mp, sizeof(mountpoint[0]));
        break;
      }
    }
  }
  if(f) fclose(f);
  return cnt;
}

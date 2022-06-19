#pragma once
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

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
    ret = copy_file_range(fd0, 0, fd1, 0, len, 0);
  } while((len-=ret) > 0 && ret > 0);
copy_error:
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

static inline void  // returns the directory where the actual binary (not the symlink) resides
fs_basedir(
    char *basedir,  // output will be copied here
    size_t maxlen)  // allocation size
{
#ifdef __linux__
  realpath("/proc/self/exe", basedir);
#elif defined(__FreeBSD__)
  int mib_procpath[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  size_t len_procpath = maxlen;
  sysctl(mib_procpath, 4, basedir, &len_procpath, NULL, 0);
#else
#error port me
#endif
  char *c = 0;
  for(int i=0;basedir[i]!=0;i++) if(basedir[i] == '/') c = basedir+i;
  if(c) *c = 0; // get dirname, i.e. strip off executable name
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
    fscanf(f, "%999s %49s %*[^\n]", block, mp);
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

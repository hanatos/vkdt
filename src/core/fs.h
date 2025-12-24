#pragma once
#include "strexpand.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#if defined(__APPLE__)
#include <libproc.h>
#endif
#if defined(_WIN64)
#include <shlobj.h>
#include <direct.h>
#include <io.h>
#endif
#if !defined(_WIN64) && !defined(__APPLE__)
#include <sys/sendfile.h>
#else
#include <stdbool.h>
#endif
#include <errno.h>

static inline int
fs_copy_file(
    const char *dst,
    FILE *fsrc)  // copy from file descriptor, enables zlib/apk stuff
{
  FILE *fdst = fopen(dst, "wb");
  if(!fdst) return 1;
  if(!fsrc) return 2;
  char buf[BUFSIZ];
  size_t n;
  while((n = fread(buf, sizeof(char), sizeof(buf), fsrc)) > 0)
    if(fwrite(buf, sizeof(char), n, fdst) != n)
    { fclose(fdst); fclose(fsrc); return 3; }
  fclose(fdst);
  return 0;
}

static inline int // returns zero on success
fs_copy(
    const char *dst,
    const char *src)
{
#if defined(_WIN64) || defined(__APPLE__)
  FILE *fdst = fopen(dst, "wb");
  if(!fdst) return 1;
  FILE *fsrc = fopen(src, "rb");
  if(!fsrc) return 2;
  char buf[BUFSIZ];
  size_t n;
  while((n = fread(buf, sizeof(char), sizeof(buf), fsrc)) > 0)
    if(fwrite(buf, sizeof(char), n, fdst) != n)
    { fclose(fdst); fclose(fsrc); return 3; }
  fclose(fdst); fclose(fsrc);
  return 0;
#else
  ssize_t ret;
  struct stat sb = {0};
  int64_t len = -1;
  int fd0 = open(src, O_RDONLY), fd1 = -1;
  if(fd0 == -1 || fstat(fd0, &sb) == -1) goto copy_error;
  len = sb.st_size;
  if(sb.st_mode & S_IFDIR) { len = 0; goto copy_error; } // don't copy directories
  if(-1 == (fd1 = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0644))) goto copy_error;
  do {
    ret = sendfile(fd1, fd0, 0, len); // works on linux >= 2.6.33, else fd1 would need to be a socket
  } while((len-=ret) > 0 && ret > 0);
copy_error:
  // if(len != 0) fprintf(stderr, "[fs_copy] %s\n", strerror(errno));
  if(fd0 >= 0) close(fd0);
  if(fd0 >= 0) close(fd1);
  return len;
#endif
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
#ifdef _WIN64
  return _mkdir(pathname);
#else
  return mkdir(pathname, mode);
#endif
}

static inline int // recursive variant, i.e. `mkdir -p`
fs_mkdir_p(
    const char *pathname,
    mode_t      mode)
{
  char tmp[2048];
  char *p = NULL;
  size_t len;

  if(snprintf(tmp, sizeof(tmp), "%s", pathname) >= (int)sizeof(tmp)) return 1;
  len = strlen(tmp);
  if (tmp[len-1] == '/' || tmp[len-1] == '\\') tmp[len - 1] = 0;
  for(p = tmp + 1; *p; p++) if (*p == '/' || *p == '\\')
  {
    *p = 0;
    fs_mkdir(tmp, mode); // ignore error (if it exists etc)
    *p = '/';
  }
  return fs_mkdir(tmp, mode);
}

static inline int // return zero if argument contains no '/', else alter str
fs_dirname(char *str)
{ // don't use: dirname(3) since it may or may not alter the argument, implementation dependent.
  char *c = 0;
  for(int i=0;str[i]!=0;i++) if(str[i] == '/' || str[i] == '\\') c = str+i;
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
  if(!str) return 0;
  char *c = str; // return str if it contains no '/'
  for(int i=0;str[i]!=0;i++) if(str[i] == '/' || str[i] == '\\') c = str+i;
  return c == str ? str : c+1;
}

static inline void // ${HOME}/Pictures
fs_picturesdir(
  char  *picdir,
  size_t maxlen)
{
#ifndef _WIN64
  const char *home = getenv("HOME");
#else
  char home[MAX_PATH];
  SHGetFolderPath(0, CSIDL_PROFILE, 0, 0, home);
#endif
  snprintf(picdir, maxlen, "%s/Pictures", home);
}

static inline void // ${HOME}/.cache/vkdt for thumbnails
fs_cachedir(
  char  *cachedir,
  size_t maxlen)
{
#if defined(__ANDROID__)
  cachedir[0] = 0; // can't access dt_pipe.app->activity-> here
#elif defined(_WIN64)
  char home[MAX_PATH];
  SHGetFolderPath(0, CSIDL_PROFILE, 0, 0, home);
  snprintf(cachedir, maxlen, "%s/vkdt/cache", home);
#else
  // TODO: getenv(XDG_CACHE_HOME)
  const char *home = getenv("HOME");
  snprintf(cachedir, maxlen, "%s/.cache/vkdt", home);
#endif
}

static inline void  // ${HOME}/.config/vkdt
fs_homedir(
    char  *homedir, // output will be copied here
    size_t maxlen)  // allocation size
{
#if defined(__ANDROID__)
  homedir[0] = 0;
#elif defined(_WIN64)
  char home[MAX_PATH];
  SHGetFolderPath(0, CSIDL_PROFILE, 0, 0, home);
  snprintf(homedir, maxlen, "%s/vkdt/config", home);
#else
  snprintf(homedir, maxlen, "%s/.config/vkdt", getenv("HOME"));
#endif
}

static inline void  // returns the directory where the actual binary (not the symlink) resides
fs_basedir(
    char  *basedir, // output will be copied here
    size_t maxlen)  // allocation size
{
  basedir[0] = 0;
#if defined(__ANDROID__)
  // have to use android_fopen etc instead, i.e. go through the asset manager thing.
  snprintf(basedir, maxlen, "%s", "nosuch");
#elif defined(__linux__)
  // stupid allocation dance because passing basedir directly
  // may or may not require PATH_MAX bytes instead of maxlen
  char *bd = realpath("/proc/self/exe", 0);
  snprintf(basedir, maxlen, "%s", bd);
  free(bd);
  fs_dirname(basedir);
  char mod[PATH_MAX];
  snprintf(mod, sizeof(mod), "%s/darkroom.ui", basedir);
#ifdef __GLIBC__
  if(access(mod, F_OK))
  { // no darkroom.ui file and probably also lacking the rest. try dlopen/dso path:
    void *handle = dlopen("libvkdt.so", RTLD_LAZY);
    if(handle)
    {
      dlinfo(handle, RTLD_DI_ORIGIN, &mod);
      size_t r = snprintf(basedir, maxlen, "%s/vkdt", mod);
      if(r >= maxlen) { basedir[0] = 0; return; } // got truncated
      dlclose(handle);
    }
  }
#endif
#elif defined(__FreeBSD__)
  int mib_procpath[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
  size_t len_procpath = maxlen;
  sysctl(mib_procpath, 4, basedir, &len_procpath, NULL, 0);
  fs_dirname(basedir);
#elif defined(_WIN64)
  char *path;
  _get_pgmptr(&path);
  snprintf(basedir, maxlen, "%s", path);
  fs_dirname(basedir);
#elif defined(__APPLE__)
  pid_t pid = getpid();
  proc_pidpath (pid, basedir, maxlen);
  fs_dirname(basedir);
#else
#warning "port me! (basedir)"
#endif
}

static inline int // return the number of devices found
fs_find_usb_block_devices(
    char devname[20][20],
    char mountpoint[20][50])
{ // pretty much ls /sys/class/scsi_disk/*/device/block/{sda,sdb,sdd,..}/{..sdd1..} and then grep for it in /proc/mounts
#ifdef _WIN64
  int cnt = 0;
  DWORD mask = GetLogicalDrives();
  if(mask == 0) return 0;
  for(int i=0; i<26 && cnt<20; i++)
  {
    if(!(mask & (1u << i))) continue;
    char root[4] = "A:\\"; // e.g. "C:\\"
    root[0] = 'A' + i;
    UINT dtype = GetDriveTypeA(root);
    if(dtype == DRIVE_REMOVABLE || dtype == DRIVE_FIXED || dtype == DRIVE_REMOTE)
    {
      snprintf(devname[cnt], sizeof(devname[0]), "%c:", 'A' + i);
      snprintf(mountpoint[cnt], sizeof(mountpoint[0]), "%s", root);
      cnt++;
    }
  }
  return cnt;
#else
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
#endif
}

static inline int fs_islnk_file(const char *filename)
{
#ifdef _WIN64
  DWORD attr = GetFileAttributesA(filename);
  if(attr == INVALID_FILE_ATTRIBUTES) return 0;
  return (attr & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0;
#else
  struct stat buf = {0};
  lstat(filename, &buf);
  return (buf.st_mode & S_IFMT) == S_IFLNK;
#endif
}

static inline int fs_islnk(const char *dirname, const struct dirent *e)
{
#ifdef _WIN64
  char buf[PATH_MAX];
  if((int)sizeof(buf) <= snprintf(buf, sizeof(buf), "%s/%s", dirname, e->d_name)) return 0;
  return fs_islnk_file(buf);
#else
  if (e->d_type == DT_LNK) return 1;
  else if (e->d_type == DT_UNKNOWN)
  {
    char buf[PATH_MAX];
    if((int)sizeof(buf) <= snprintf(buf, sizeof(buf), "%s/%s", dirname, e->d_name)) return 0;
    return fs_islnk_file(buf);
  }
  return 0;
#endif
}

static inline int fs_isreg_file(const char *filename)
{
#ifdef _WIN64
  struct __stat64 buf = {0};
  _stat64(filename, &buf);
  return (buf.st_mode & _S_IFREG) != 0;
#else
  struct stat buf = {0};
  lstat(filename, &buf);
  return (buf.st_mode & S_IFMT) == S_IFREG;
#endif
}

static inline int fs_isreg(const char *dirname, const struct dirent *e)
{
#ifdef _WIN64
  char filename[PATH_MAX];
  struct __stat64 buf;
  snprintf(filename, sizeof(filename), "%s/%s", dirname, e->d_name);
  _stat64(filename, &buf);
  return (buf.st_mode & _S_IFREG) != 0;
#else
  if (e->d_type == DT_REG) return 1;
  else if (e->d_type == DT_UNKNOWN)
  {
    char buf[PATH_MAX];
    if((int)sizeof(buf) <= snprintf(buf, sizeof(buf), "%s/%s", dirname, e->d_name)) return 0;
    return fs_isreg_file(buf);
  }
  return 0;
#endif
}

static inline char*
fs_realpath(const char *path, char *resolved_path)
{
#ifdef _WIN64
  // GetFinalPathNameByHandleA() would resolve symlinks but requires us to open the file first.
  // this means we first need to call GetFullPathNameA to reconstruct the path
  // and return this as fallback if the second step fails.
  char *ret = resolved_path;
  if(!ret) ret = (char*)malloc(sizeof(char)*PATH_MAX);
  GetFullPathNameA(path, PATH_MAX, ret, 0);
  HANDLE hf = CreateFile(ret, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
		  FILE_ATTRIBUTE_NORMAL, 0);
  if(hf == INVALID_HANDLE_VALUE)
    return ret;
  // DWORD sz = // and then check < PATH_MAX
  GetFinalPathNameByHandleA(hf, ret, PATH_MAX, 0);
  CloseHandle(hf);
  if(!strncmp(ret, "\\\\?\\", 4))
  { // extra sanitisation required
    char *ret2 = (char*)malloc(sizeof(char)*PATH_MAX);
    snprintf(ret2, PATH_MAX, "%s", ret+4);
    snprintf(ret,  PATH_MAX, "%s", ret2);
    free(ret2);
  }
  return ret;
#elif defined(__GLIBC__)
  // For a nonexistent file, a glibc GNU extension will fill in resolved_path
  return realpath(path, resolved_path);
#else
  // If we only have POSIX realpath available we need to emulate the GNU extension behavior,
  // but only for the case where the file may or may not exist but the parent does exist.
  char *ret = realpath(path, resolved_path);
  if(ret != NULL)
    return ret;
  if(resolved_path != NULL)
  {
    // Caller wants resolved_path set even if the file doesn't exist. Output empty string
    // unless the parent does exist
    resolved_path[0] = 0;
    if(errno == ENOENT)
    {
      // The file doesn't exist but maybe the parent does
      char parent[PATH_MAX];
      snprintf(parent, PATH_MAX, "%s", path);
      if(!fs_dirname(parent))
        strcpy(parent, ".");
      char *realparent = realpath(parent, NULL);
      if(realparent != NULL)
        snprintf(resolved_path, PATH_MAX, "%s/%s", realparent, fs_basename((char *)path));
      free(realparent);
    }
  }
  return NULL;
#endif
}

static inline int fs_isdir_file(const char *filename)
{
#ifdef _WIN64
  struct __stat64 buf = {0};
  _stat64(filename, &buf);
  return (buf.st_mode & _S_IFDIR) != 0;
#else
  struct stat buf = {0};
  lstat(filename, &buf);
  return (buf.st_mode & S_IFMT) == S_IFDIR;
#endif
}

// this calls `realpath` on symlinks before classification.
static inline int fs_isdir(const char *dirname, const struct dirent *e)
{
#ifdef _WIN64
  char filename[PATH_MAX];
  struct __stat64 buf;
  snprintf(filename, sizeof(filename), "%s/%s", dirname, e->d_name);
  _stat64(filename, &buf);
  // fprintf(stderr, "isdir %s => %d\n", filename, (buf.st_mode & _S_IFDIR) != 0);
  return (buf.st_mode & _S_IFDIR) != 0;
#else
  char resolved[PATH_MAX], filename[PATH_MAX];
  if(e->d_type == DT_UNKNOWN)
  { // lame filesystem
    snprintf(filename, sizeof(filename), "%s/%s", dirname, e->d_name);
    struct stat buf = {0};
    lstat(filename, &buf);

    if((buf.st_mode & S_IFMT) == S_IFLNK)
    {
      fs_realpath(filename, resolved);
      return fs_isdir_file(resolved);
    }
    return (buf.st_mode & S_IFMT) == S_IFDIR;
  }
  if(e->d_type == DT_LNK) 
  {
    snprintf(filename, sizeof(filename), "%s/%s", dirname, e->d_name);
    fs_realpath(filename, resolved);
    return fs_isdir_file(resolved);
  }
  return e->d_type == DT_DIR;
#endif
}

static inline int // return 0 on success
fs_symlink(const char *target, const char *linkpath)
{
#ifdef _WIN64
  return !CreateSymbolicLinkA(linkpath, target, 2);
#else
  return symlink(target, linkpath);
#endif
}

static inline void
fs_expand_import_filename(
    const char *pattern, size_t pattern_size, // input pattern with ${home} ${date} ${yyyy} ${dest}
    char       *dst,     size_t dst_size,     // return expanded string here
    const char *dest)                         // ${dest} replacement string
{
  char date[10] = {0}, yyyy[5] = {0};
#ifdef _WIN64
  char home[MAX_PATH];
  SHGetFolderPath(0, CSIDL_PROFILE, 0, 0, home);
  time_t t = time(0);
  struct tm result;
  localtime_s(&result, &t);
  strftime(date, sizeof(date), "%Y%m%d", &result);
  strftime(yyyy, sizeof(yyyy), "%Y", &result);
#else
  char *home = getenv("HOME");
  time_t t = time(0);
  struct tm result;
  struct tm *tm = localtime_r(&t, &result);
  strftime(date, sizeof(date), "%Y%m%d", tm);
  strftime(yyyy, sizeof(yyyy), "%Y", tm);
#endif
  const char *key[] = { "home", "yyyy", "date", "dest", 0};
  const char *val[] = { home, yyyy, date, dest, 0};
  dt_strexpand(pattern, pattern_size, dst, dst_size, key, val);
}

static inline void
fs_expand_export_filename(
    const char *pattern, size_t pattern_size, // input pattern with ${home} ${yyyy} ${date} ${seq} ${fdir} ${fbase}
    char       *dst,     size_t dst_size,     // return expanded string here
    char       *filename,                     // source image filename
    int         seq)                          // sequence number for ${seq}
{
  char *filebase = fs_basename(filename);
  size_t len = strlen(filebase);
  if(len > 4) filebase[len-4] = 0; // cut away .cfg
  if(len > 8 && filebase[len- 8] == '.') filebase[len- 8] = 0; // cut .xxx
  if(len > 9 && filebase[len- 9] == '.') filebase[len- 9] = 0; // cut .xxxx
  if(len >10 && filebase[len-10] == '.') filebase[len-10] = 0; // cut .xxxxx
  fs_dirname(filename);
  char date[10] = {0}, yyyy[5] = {0}, istr[5] = {0};
#ifdef _WIN64
  char home[MAX_PATH];
  SHGetFolderPath(0, CSIDL_PROFILE, 0, 0, home);
  time_t t = time(0);
  struct tm result;
  localtime_s(&result, &t);
  strftime(date, sizeof(date), "%Y%m%d", &result);
  strftime(yyyy, sizeof(yyyy), "%Y", &result);
#else
  char *home = getenv("HOME");
  time_t t = time(0);
  struct tm result;
  struct tm *tm = localtime_r(&t, &result);
  strftime(date, sizeof(date), "%Y%m%d", tm);
  strftime(yyyy, sizeof(yyyy), "%Y", tm);
#endif
  snprintf(istr, sizeof(istr), "%04d", seq);
  const char *key[] = { "home", "yyyy", "date", "seq", "fdir", "fbase", 0};
  const char *val[] = { home, yyyy, date, istr, filename, filebase, 0};
  dt_strexpand(pattern, pattern_size, dst, dst_size, key, val);
}

static inline uint64_t
fs_createtime(
    const char *filename) // filename to stat
{
#ifdef _WIN64
  struct stat statbuf = {0};
  stat(filename, &statbuf);
  return statbuf.st_ctime;
#elif defined(__APPLE__)
  struct stat statbuf = {0};
  stat(filename, &statbuf);
  return statbuf.st_birthtimespec.tv_sec;
#else
  struct stat statbuf = {0};
  stat(filename, &statbuf);
  return statbuf.st_mtim.tv_sec;
#endif
}
static inline void
fs_createdate(
    const char *filename,  // filename to stat
    char       *datetime)  // at least [20]
{
#ifdef _WIN64
  struct stat statbuf = {0};
  stat(filename, &statbuf);
  struct tm result;
  localtime_s(&result, &statbuf.st_ctime);
  strftime(datetime, 20, "%Y:%m:%d %H:%M:%S", &result);
#else
  struct stat statbuf = {0};
  if(!stat(filename, &statbuf))
  {
    struct tm result;
    strftime(datetime, 20, "%Y:%m:%d %H:%M:%S", localtime_r(&statbuf.st_mtime, &result));
  }
#endif
}

static inline int // return 0 on success
fs_link(const char *oldpath, const char *newpath)
{
#ifdef _WIN64
  return !CreateHardLinkA(newpath, oldpath, 0);
#else
  return link(oldpath, newpath);
#endif
}

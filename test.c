
#define _XOPEN_SOURCE  700
#define DEV "/dev/pgcachectl"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <x86intrin.h>
#include <assert.h>

#include "upgcachectl.h"

#define WARM 0
#define DROP 1
#define POPULATE (1 << 1)

int open_dev() {
  int fd = open(DEV, O_RDWR, 0);

  if(fd < 0) {
    perror("failed to open device");
    exit(-1);
  }
}

struct timespec diff(struct timespec start, struct timespec end) {
  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}


int add_pages(int pgcache_fd, int fd, void *buf, size_t size) {
  struct pgcachectl_req req;
  req.fd = fd;
  req.start_addr = buf;
  req.len = size;
  req.offset = 0;

  int ret = ioctl(pgcache_fd, PGCACHECTL_INSERT, &req);

  if (ret < 0) {
    perror("ioctl failed");
    return -1;
  }

  return 0;
}


int drop_caches() {
  
  int f = open("/proc/sys/vm/drop_caches", O_WRONLY, 0);
  if(f < 0) {
    perror("failed to open proc");
    return f;
  }

  char *buf = "1";

  int w = write(f, buf, strlen(buf));
  if (w != strlen(buf)) {
    printf("error writing to proc\n");
    return -1;
  }

  fsync(f);
  close(f);
}

__attribute__((optimize("-O0"))) void test(void *dst, void *src, size_t len)  {
  int n = 0;
  char tmp;
  while (n < len) {
    tmp = *((char *)src + n);
    n += 4096;
  }
  //memcpy(dst, src, len);
}

long measure_read(char *file, size_t size, int pgcache_fd, int flags, char *decomp_buf) {
  int fd = open(file, O_RDONLY, 0); 
  if (fd <= 0) {
    return -1;
  }
  
  if (flags & DROP) {
    int ret = posix_fadvise(fd, 0, size, POSIX_FADV_DONTNEED);
    if (ret < 0){
      perror("fadvise error");
      return -1;
    }
  }

  fsync(fd);
  
  if (flags & POPULATE) {
    if(add_pages(pgcache_fd, fd, decomp_buf, size) == -1) {
      printf("error adding pages to the page cache\n");
      return -1;
    }
  }

  struct timespec start, end;

  void *base = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (base == MAP_FAILED) {
    perror("mmap error");
    return -1;
  }

  char *buf = malloc(size);
  if (buf == NULL) {
    perror("malloc error");
    return -1;
  }
  
  clock_gettime(CLOCK_MONOTONIC, &start);
  test(buf, base, size);
  clock_gettime(CLOCK_MONOTONIC, &end);

  free(buf);
  munmap(base, size);

  struct timespec tmp = diff(start, end);
  assert(tmp.tv_sec == 0);
  close(fd);
  
  return tmp.tv_nsec;
}

size_t decompress(char *filename, char **buf) {
  gzFile file = gzopen(filename, "rb");
  if (!file) {
    perror("error opening compressed file");
    return 0;
  }

  size_t len;
  size_t total = 0;

  char *tmp = (char *)malloc(4096);
  if (tmp == NULL) {
    perror("malloc error");
  }

  while ((len = gzread(file, tmp, 4096)) > 0) {
    *buf = realloc(*buf, total + len);
    memcpy(*buf + total, tmp, len);
    total += len;

    if (*buf == NULL) {
      perror("error resizing buf");
      return 0;
    }
  }

  gzclose(file);
  return total;
}

int main(int argc, char **argv) {
  printf("testing pgcachectl\n");

  int num_tests = 100;

  if(argc > 1) {
    if(!atoi(argv[1])) {
      printf("usage: %s <num-tests>\n", argv[0]);
      return -1;
    }
    num_tests = atoi(argv[1]);
  }

  int pgcache_fd = open_dev();

  char *decomp_buf = (char *)malloc(4096);
  if (decomp_buf == NULL) {
    perror("malloc error");
    return -1;
  }

  size_t size = decompress("./file.gz", &decomp_buf);

  // warm up run
  long total = 0;
  for (int i = 0; i < num_tests; i++) {
    total += measure_read("file.txt", size, pgcache_fd, WARM, decomp_buf);
  }

  // measure cold
  total = 0;
  for (int i = 0; i < num_tests; i++) {
    total += measure_read("file.txt", size, pgcache_fd, DROP, decomp_buf);
  }
  printf("average cold read time=%ldns\n", total/num_tests);

  // measure warm
  total = 0;
  for (int i = 0; i < num_tests; i++) {
    total += measure_read("file.txt", size, pgcache_fd, WARM, decomp_buf);
  }

  printf("average warm read time=%ldns\n", total/num_tests);

  // measure warm after we insert the pages ourselves (dropping the caches each time
  total = 0;
  for(int i = 0; i < num_tests; i++) {
    total += measure_read("file.txt", size, pgcache_fd, DROP | POPULATE, decomp_buf);
  }

  printf("average read time after inserting pages into the page cache=%ldns\n", total/num_tests);
  return 0;
}

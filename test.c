#define DEV "/dev/pgcachectl"

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#include "upgcachectl.h"

int main() {
  printf("testing pgcachectl\n");

  // try opening the device
  int pgcachectl_fd = open(DEV, O_RDWR, 0);

  if(pgcachectl_fd < 0) {
    perror("failed to open device");
    return -1;
  }

  int fd = open("./file.txt", O_RDONLY, 0);

  if(fd < 0) {
    perror("failed to open test file");
    return -1;
  }

  // test: read in two files, lets say file.txt and test.c
  // then try to replace a page of file.txt with test.c
  // by mmapping test.c and asking the pgcachectl to
  // replace the first page of file.txt with the mmapped region.

  // read all of file.txt
  char buf[4096];

  size_t n = 0;
  while(n < 4096) {
    size_t ret = read(fd, buf, 4096 - n);
    if (ret == 0) {
      printf("EOF, n=%ld\n", n);
      break;
    }

    if (ret < 0) {
      printf("error reading file");
      return -1;
    }
    
    n += ret;
  }

  // tests that we read the correct data
  buf[4094] = '\n';
  buf[4095] = '\0';
  printf("%s", buf);

  // mmap test.c
  int test_fd = open("./test.c", O_RDONLY, 0);
  if (test_fd < 0) {
    perror("error opening test.c");
    return -1;
  }
  
  void *addr = mmap(0, 4096, PROT_READ, MAP_PRIVATE, test_fd, 0);
  if(addr == MAP_FAILED) {
    perror("failed to mmap test.c");
    return -1;
  }

  // doing this memcpy so things
  // things will be brought into the page cache
  memcpy(buf, addr, 4094);

  struct pgcachectl_req req;
  req.fd = fd;
  req.start_addr = addr;
  req.len = 4096;
  
  int ret = ioctl(pgcachectl_fd, _IO(PGCACHECTL_MAGIC, 1), &req);

  printf("addr=%p\n", req.start_addr);
  if (ret < 0) {
    perror("ioctl failed");
    return -1;
  }

  lseek(fd, 0, SEEK_SET);

  n = 0;
  while(n < 4096) {
    size_t ret = read(fd, buf, 4096 - n);
    if (ret == 0) {
      printf("EOF, n=%ld\n", n);
      break;
    }

    if (ret < 0) {
      printf("error reading file");
      return -1;
    }
    
    n += ret;
  }

  // after 
  printf("%s", buf);
}

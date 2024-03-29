#pragma once

#define PGCACHECTL_MAGIC 0xF1
#define PGCACHECTL_INSERT _IO(PGCACHECTL_MAGIC, 1)

struct pgcachectl_req {
  int fd;
  void *start_addr;
  size_t len;
  unsigned long offset;
};

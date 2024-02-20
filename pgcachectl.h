#pragma once

#define pr_fmt(fmt) "%s: " fmt "\n", KBUILD_MODNAME

#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/types.h>

#define PGCACHECTL_MAGIC 0xF1
#define PGCACHECTL_IOC_MAXNR 1

#define PGCACHECTL_IOC_REPLPG _IO(PGCACHECTL_MAGIC, 1)

struct pgcachectl_req {
  int fd;
  void *start_addr;
  size_t len;
};

static long pgcachectl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static int pgcachectl_open(struct inode *, struct file *); 
static int pgcachectl_release(struct inode *, struct file *);
int replace_pages(struct pgcachectl_req *ureq);
int replace_page(pgoff_t idx, void *addr, size_t len, struct address_space *mapping);

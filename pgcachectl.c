/*
 * pgcachectl.c - interface for userspace to insert pages into the page cache for a given file
 */

#include <linux/printk.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <linux/file.h>
#include <linux/pagemap.h>

#include "pgcachectl.h"

static int major;
static struct class *cls;

static struct file_operations pgcachectl_fops = {
  .open = pgcachectl_open,
  .release = pgcachectl_release,
  .unlocked_ioctl = pgcachectl_ioctl,
};

static int __init pgcachectl_init(void) {
  pr_info("init");

  major = register_chrdev(0, KBUILD_MODNAME, &pgcachectl_fops);

  if (major < 0) {
    pr_err("error allocating device major");
    return major;
  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) 
  cls = class_create(KBUILD_MODNAME); 
#else 
  cls = class_create(THIS_MODULE, KBUILD_MODNAME); 
#endif

  device_create(cls, NULL, MKDEV(major, 0), NULL, KBUILD_MODNAME);

  pr_info("device created at /dev/%s", KBUILD_MODNAME); 
  
  return 0;
}

static long pgcachectl_ioctl(struct file *fp, unsigned int cmd, unsigned long arg) {
  if (unlikely(_IOC_TYPE(cmd) != PGCACHECTL_MAGIC))
    return -ENOTTY;
  if (unlikely(_IOC_NR(cmd) > PGCACHECTL_IOC_MAXNR))
    return -ENOTTY;

  switch (cmd) {
  case PGCACHECTL_IOC_REPLPG:
    return insert_pages((struct pgcachectl_req *)arg);
  default:
    break;
  }

  return -ENOTTY;
}

int insert_page(pgoff_t idx, void *addr, size_t len, struct address_space *mapping) {
  struct page *pg;
  void *pg_addr;

  pg = grab_cache_page(mapping, idx);
  if (!pg)
    return -ENOMEM;
  
  pg_addr = kmap_local_page(pg);

  if(unlikely(copy_from_user(pg_addr, addr, PAGE_SIZE)))
    return -EFAULT;

  kunmap_local(pg_addr);
  SetPageUptodate(pg);
  put_page(pg);
  unlock_page(pg);
  
  return 0;
}

int insert_pages(struct pgcachectl_req *ureq) {
  struct pgcachectl_req req;
  struct file *f;
  size_t n, idx;
  int ret;
  // copy request from user
  if (unlikely(copy_from_user(&req, ureq, sizeof(req))))
    return -EFAULT;

  f = fget(req.fd);
  if(f == NULL){
    pr_err("error getting file from fd");
    return -ENOENT;
  }

  // only allow regular files
  if(!S_ISREG(f->f_inode->i_mode)) {
    pr_err("user provided non-regular file");
    return -EINVAL;
  }
  
  n = req.len;
  idx = req.offset;
  while (n > 0) {
    size_t len = n > PAGE_SIZE ? PAGE_SIZE : n;
    if((ret = insert_page(idx, req.start_addr + (idx * PAGE_SIZE), len, f->f_inode->i_mapping)))
      return ret;
    n -= len;
    idx += 1;
  }

  return 0;
}

static int pgcachectl_open(struct inode *, struct file *) {
  return 0;
}

static int pgcachectl_release(struct inode *, struct file *) {
  return 0;
}

static void __exit pgcachectl_exit(void) {
  device_destroy(cls, MKDEV(major, 0));
  class_destroy(cls);

  pr_info("unregistering device: /dev/%s major=%d", KBUILD_MODNAME, major);

  unregister_chrdev(major, KBUILD_MODNAME);
}

module_init(pgcachectl_init);
module_exit(pgcachectl_exit);

MODULE_LICENSE("GPL");


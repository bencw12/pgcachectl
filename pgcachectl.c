/*
 * pgcachectl.c - interface to manipulate the page cache
 */
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

  pr_info("device major: %d", major);

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
  pr_info("Got ioctl");
  if (unlikely(_IOC_TYPE(cmd) != PGCACHECTL_MAGIC))
    return -ENOTTY;
  if (unlikely(_IOC_NR(cmd) > PGCACHECTL_IOC_MAXNR))
    return -ENOTTY;

  switch (cmd) {
  case PGCACHECTL_IOC_REPLPG:
    return replace_pages((struct pgcachectl_req *)arg);
  default:
    break;
  }

  return -ENOTTY;
}

int replace_page(pgoff_t idx, void *addr, size_t len, struct address_space *mapping) {
  struct page *pg;
  void *pg_addr;
  
  // grab_cache_page returns a locked page and idx, creating it if needed
  // TODO: can we use grab_cache_page_nowait instead?
  pg = grab_cache_page(mapping, idx);

  if (pg == NULL) {
    pr_err("out of memory");
    return -EFAULT;
  }
    
  // map page cache page we found
  pg_addr = kmap_local_page(pg);

  // TODO: make rdonly/clear dirty flag (do we even need this now?)
  if (unlikely(copy_from_user(pg_addr, addr, PAGE_SIZE)))
    return -EFAULT;
  
  kunmap_local(pg_addr);
  unlock_page(pg);

  return 0;
}

int replace_pages(struct pgcachectl_req *ureq) {
  struct pgcachectl_req req;
  struct file *f;
  size_t n, idx;
  int ret;
  // copy request from user
  if (unlikely(copy_from_user(&req, ureq, sizeof(req))))
    return -EFAULT;
    
  // get file from user-provided fd
  // this function internally calls task_lock/unlock() and rcu_read_lock/unlock()
  // TODO: there might be a better way of doing this but this works or now
  f = fget(req.fd);
  if(f == NULL){
    pr_err("error getting file from fd");
    return -ENOENT;
  }

  // check if this is a regular file
  if(!S_ISREG(f->f_inode->i_mode)) {
    pr_err("user provided non-regular file");
    return -EINVAL;
  }

  n = req.len;
  idx = 0;
  while (n > 0) {
    size_t len = n > PAGE_SIZE ? PAGE_SIZE : n;
    if(!(ret = replace_page(idx, req.start_addr + (idx * PAGE_SIZE), len, f->f_inode->i_mapping))){
      return ret;
    }
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


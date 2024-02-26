# pgcachectl
A kernel module providing an interface for userspace to insert/replace pages in the page cache for a given file

## Usage
When the module is installed it creates the char device `/dev/pgcachectl` which a user program communicates with via `ioctl`. An example usage is as follows:

```C
char buf[4096]; // buf is filled with some data

// open /dev/pgcachectl
int pgcachectl = open("/dev/pgcachectl", O_RDWR, 0);

// open the file we want to insert data for in the page cache
int fd = open("base.txt", O_RDONLY, 0);

// populate request fields
struct pgcachectl_req req;
req.fd = fd; // the file for which we are inserting pages
req.start_addr = buf; // data to insert
req.len = 4096;
req.offset = 0; // starting page offset

// do insert
ioctl(pgcachectl, PGCACHECTL_INSERT, &req);
```

Depending on whether the first page of data for `file.txt` is already present in the page cache, this inserts/replaces the first page of `file.txt` with the contents of `buf`.
The kernel maintains control of eviction/writeback so files will be overwritten with the data inserted into the page cache. 

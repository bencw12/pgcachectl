obj-m += pgcachectl.o

PWD := $(CURDIR)

all: pgcachectl.ko test

test: test.c upgcachectl.h
	gcc -O3 -o test test.c -lz
pgcachectl.ko: pgcachectl.c pgcachectl.h
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules	

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm test

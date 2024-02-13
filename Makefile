obj-m += pgcachectl.o

PWD := $(CURDIR)

all: mod test

test: test.c upgcachectl.h
	gcc -O2 -o test test.c

mod:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules	

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm test

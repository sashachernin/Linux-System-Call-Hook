obj-m += alternate_openat.o

all: module client

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f client 
client:
	gcc -o client client.c -O2

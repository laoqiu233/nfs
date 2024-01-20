obj-m += networkfs.o
networkfs-objs += main.o http.o
ccflags-y := -std=gnu11 -Wno-declaration-after-statement

PWD := $(CURDIR) 

all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

mnt:
	sudo insmod networkfs.ko
	sudo mount -t networkfs token /mnt/nfs

umnt:
	sudo umount /mnt/nfs
	sudo rmmod networkfs

reload: umnt clean all mnt
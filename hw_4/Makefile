obj-m := buddy_alloc.o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

load:
	sudo insmod buddy_alloc.ko

unload:
	sudo rmmod buddy_alloc

reload:
	sudo rmmod buddy_alloc && sudo insmod buddy_alloc.ko

remove:
	sudo rm /dev/mem_dev

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

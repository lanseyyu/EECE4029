obj-m += hidden_loopback.o

all:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
		sudo insmod hidden_loopback.ko

unload: 
		sudo rmmod hidden_loopback

reload:
		sudo rmmod hidden_loopback && sudo insmod hidden_loopback.ko

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

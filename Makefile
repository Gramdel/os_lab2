obj-m += kmod.o

PWD := $(CURDIR)

.PHONY: all clean load unload reload

all: clean
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load:
	insmod ./kmod.ko

unload:
	rmmod ./kmod.ko

reload: unload load

user: kmod.h user.c
	gcc kmod.h user.c -o user

test: test.c
	gcc test.c -o test
modname := konami
obj-m := $(modname).o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := "$$(pwd)"

ifdef DEBUG
	CFLAGS_$(obj-m)
endif

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o ghost ghost.c
	gcc -o manpac manpac.c

clean:
	$(MAKE) O=$(PWD) -C $(KDIR) M=$(PWD) clean
	-rm ghost
	-rm manpac

load:
	-rmmod $(modname)
	insmod $(modname).ko

modname := konami
obj-m := $(modname).o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := "$$(pwd)"

ifdef DEBUG
	CFLAGS_$(obj-m) := -DDEBUG
endif

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o ghost ghost.c -lrt -pthread
	gcc -o manpac manpac.c -lrt -pthread

clean:
	$(MAKE) O=$(PWD) -C $(KDIR) M=$(PWD) clean
	-rm ghost
	-rm manpac

load:
	cp -f ./ghost /tmp/
	cp -f ./manpac /tmp/
	-rmmod $(modname)
	insmod $(modname).ko

modname := konami
obj-m := $(modname).o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := "$$(pwd)"

ifdef DEBUG
	CFLAGS_$(obj-m) := -g -DDEBUG
endif

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o ghost ghost.c -lrt -lX11 -lXext -pthread
	gcc -o manpac manpac.c -lrt -lX11 -lXext -pthread

clean:
	$(MAKE) O=$(PWD) -C $(KDIR) M=$(PWD) clean
	-rm ghost || true
	-rm manpac || true

load:
	cp -f ./ghost /tmp/
	cp -f ./manpac /tmp/
	chmod 755 /tmp/ghost
	chmod 755 /tmp/manpac
	-rmmod $(modname) || true
	insmod $(modname).ko

obj-m += elevator.o

cc-flags-y := -g -Wno-error
KDIR=/lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean



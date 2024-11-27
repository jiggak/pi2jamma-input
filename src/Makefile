obj-m += pi2jamma_input.o

all: module dt

module: pi2jamma_input.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
	depmod -A

dt: pi2jamma-input.dts
	dtc -@ -I dts -O dtb -o pi2jamma-input.dtbo pi2jamma-input.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

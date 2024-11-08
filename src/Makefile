obj-m += pi2jamma_input.o

all: module dt

module: pi2jamma_input.c
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

dt: pi2jamma_input_overlay.dts
	dtc -@ -I dts -O dtb -o pi2jamma_input_overlay.dtbo pi2jamma_input_overlay.dts

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

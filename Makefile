obj-m := controllerDriver.o
controllerDriver-y := main_driver.o file_ops.o admin_ops.o circular_buffer.o gamepad_ioctl.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
obj-m += main_driver.o
obj-m += file_ops.o
obj-m += admin_ops.o
obj-m += circular_buffer.o

all: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

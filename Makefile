KERN_DIR = /home/yc/lab/BSP/100ask_imx6ull-qemu/linux-4.9.88
BUILD_DIR = build

all:
	make -C $(KERN_DIR) M=`pwd` modules 
	$(CROSS_COMPILE)gcc -o at24c02_test at24c02_test.c
	$(CROSS_COMPILE)gcc -o key_test key_test.c
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order  at24c02_test key_test

obj-m += key_input_drv.o
obj-m += at24c02_drv.o
obj-m += lcd_drv.o

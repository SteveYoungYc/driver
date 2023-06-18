KERN_DIR = /home/yc/lab/BSP/100ask_imx6ull-qemu/linux-4.9.88
BUILD_DIR = build

all:
	make -C $(KERN_DIR) M=`pwd` modules 
	$(CROSS_COMPILE)gcc -o at24c02_test at24c02_test.c
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order  at24c02_test

obj-m += sr501_drv.o at24c02_drv.o

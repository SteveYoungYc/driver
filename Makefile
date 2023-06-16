KERN_DIR = /home/yc/lab/BSP/100ask_imx6ull-qemu/linux-4.9.88
BUILD_DIR = build

all:
	rm -rf $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)
	make -C $(KERN_DIR) M=`pwd` modules 
	$(CROSS_COMPILE)gcc -o sr501_test sr501_test.c
	mv *.ko *.o *.order *.symvers sr501_test $(BUILD_DIR)
	find . -name "*.cmd" -exec mv {} $(BUILD_DIR) \;
	find . -name "*.mod.c" -exec mv {} $(BUILD_DIR) \;
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order  sr501_test

obj-m += sr501_drv.o at24c02_drv.o

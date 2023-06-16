#!/bin/bash

linux_kernel_path="/home/yc/lab/BSP/100ask_imx6ull-qemu/linux-4.9.88"
qemu_img_path="/home/yc/lab/ubuntu-18.04_imx6ul_qemu_system/imx6ull-system-image"
dtb="100ask_imx6ull_qemu.dtb"

cd "$linux_kernel_path"
make dtbs

if [ -f "$linux_kernel_path/arch/arm/boot/dts/$dtb" ]; then
  # Copy dtb file
  cp "$linux_kernel_path/arch/arm/boot/dts/$dtb" "$qemu_img_path"
  echo "Successfully copied."
else
  echo "Build dtd failed!"
fi

#scp yc@10.0.2.2://home/yc/lab/project/driver/sr501/sr501_drv.ko .
#scp yc@10.0.2.2://home/yc/lab/project/driver/sr501/sr501_test .

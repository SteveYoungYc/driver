# Driver
## Usage
in qemu:
1. scp yc@10.0.2.2:/home/yc/lab/project/driver/sr501/lcd_drv.ko .
2. scp yc@10.0.2.2:/home/yc/lab/project/driver/sr501/key_input_drv.ko .
3. scp yc@10.0.2.2:/home/yc/lab/project/driver/sr501/at24c02_drv.ko .
## LCD
### 设备树
https://blog.csdn.net/u012489236/article/details/97137007
```dts
// 在根节点下
lcd {
    compatible = "100ask,lcd";
    reg = <0x021C8000 16>;
};
```
lcd节点在根节点下，reg属性表示LCD寄存器的起始地址以及长度。
### 驱动
将LCD挂载到平台总线。在probe函数中：
1. 读取寄存器的值
2. fb_info
   1. 分配fb_info
   2. 设置可变参数：分辨率，颜色格式
   3. 设置固定参数：
      1. 设置显存长度
      2. 分配DMA显存，得到显存的虚拟地址screen_base，以及物理地址
      3. 配置物理地址和虚拟地址
      4. 配置fbops
   4. 注册fb_info
3. 操作硬件，配置起始地址寄存器的值为显存起始物理地址
## Key
### 设备树
```dts
// 在根节点下
input_dev {
    compatible = "100ask,input_dev";
    gpios = <&gpio5 1 GPIO_ACTIVE_LOW>;
};
```
### 驱动
根据设备树中的GPIO信息来注册中断回调函数，在回调函数中读取按键值，并使用input_event来上报数据。
## AT24C02
### 设备树
```dts
&i2c1 {
	clock-frequency = <100000>;
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_i2c1>;
	status = "okay";

	at24c02 {
		compatible = "100ask,at24c02";
		reg = <0x50>;
	};
};
```
设备树中，表明它使用i2c1控制器节点。
### 驱动
使用i2c_add_driver来注册驱动程序at24c02_drv，会根据at24c02_of_match来与设备树进行匹配。匹配成功就会调用at24c02_probe函数。在at24c02_probe函数中获取一个at24c02_client，并注册file_operation结构体，其中包括at24c02_ioctl。应用程序可以通过ioctl来对at24c02进行读写。

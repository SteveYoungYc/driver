#include <asm/div64.h>
#include <asm/mach/map.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>

struct lcd_regs {
	volatile unsigned int fb_base_phys;
	volatile unsigned int fb_xres;
	volatile unsigned int fb_yres;
	volatile unsigned int fb_bpp;	
};

static struct lcd_regs *mylcd_regs;
static struct fb_info *myfb_info;

static struct fb_ops myfb_ops = {
	.owner		= THIS_MODULE,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static const struct of_device_id lcd_match_table[] = {
    {.compatible = "100ask,lcd"},
    {},
};

static int lcd_probe(struct platform_device *pdev) {
    unsigned int fb_base_phys_value;
	dma_addr_t phy_addr;
	printk("lcd probed\n");

    // 0 从设备树节点的 "reg" 属性读取寄存器值
    if (of_property_read_u32(pdev->dev.of_node, "reg", &fb_base_phys_value) != 0) {
        return -1;
    }
	
	/* 1.1 分配fb_info */
	myfb_info = framebuffer_alloc(0, NULL);

	/* 1.2 设置fb_info */
	/* a. 可变参数 : LCD分辨率、颜色格式 */
	myfb_info->var.xres_virtual = myfb_info->var.xres = 500;
	myfb_info->var.yres_virtual = myfb_info->var.yres = 300;
	myfb_info->var.bits_per_pixel = 16;  /* rgb565 */
	myfb_info->var.red.offset = 11;
	myfb_info->var.red.length = 5;
	myfb_info->var.green.offset = 5;
	myfb_info->var.green.length = 6;
	myfb_info->var.blue.offset = 0;
	myfb_info->var.blue.length = 5;
	
	/* b. 固定参数 */
	myfb_info->fix.smem_len = myfb_info->var.xres * myfb_info->var.yres * myfb_info->var.bits_per_pixel / 8;

	/* fb的虚拟地址 */
	myfb_info->screen_base = dma_alloc_wc(NULL, myfb_info->fix.smem_len, &phy_addr, GFP_KERNEL);
	/* fb的物理地址 */
	myfb_info->fix.smem_start = phy_addr;
	
	myfb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	myfb_info->fix.visual = FB_VISUAL_TRUECOLOR;
	myfb_info->fix.line_length = myfb_info->var.xres * myfb_info->var.bits_per_pixel / 8;
	
	/* c. fbops */
	myfb_info->fbops = &myfb_ops;

	/* 1.3 注册fb_info */
	register_framebuffer(myfb_info);

	/* 1.4 硬件操作 */
	mylcd_regs = ioremap(fb_base_phys_value, sizeof(struct lcd_regs));
	mylcd_regs->fb_base_phys = phy_addr;
	mylcd_regs->fb_xres = myfb_info->var.xres;
	mylcd_regs->fb_yres = myfb_info->var.yres;
	mylcd_regs->fb_bpp  = myfb_info->var.bits_per_pixel;

	return 0;
}

static int lcd_remove(struct platform_device *pdev) {
	unregister_framebuffer(myfb_info);
	framebuffer_release(myfb_info);
	iounmap(mylcd_regs);
	return 0;
}

static struct platform_driver lcd_driver = {
    .probe = lcd_probe,
    .remove = lcd_remove,
    .driver =
        {
            .name = "lcd",
            .of_match_table = lcd_match_table,
        },
};

/* 1. 入口 */
int __init lcd_drv_init(void) {
	return platform_driver_register(&lcd_driver);
}

/* 2. 出口 */
static void __exit lcd_drv_exit(void) {
	platform_driver_unregister(&lcd_driver);
}

module_init(lcd_drv_init);
module_exit(lcd_drv_exit);

MODULE_AUTHOR("www.100ask.net");
MODULE_DESCRIPTION("Framebuffer driver for the linux");
MODULE_LICENSE("GPL");

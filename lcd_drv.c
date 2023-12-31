#include "linux/printk.h"
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
static dma_addr_t *buffer;

static int myfb_mmap(struct fb_info *info, struct vm_area_struct *vma) {
  unsigned long start = vma->vm_start; // 用户空间虚拟地址的起始
  /* vma->vm_end - vma->vm_start = 303104 向上对其到4096
      而info->fix.smem_len = 300000
      vm_pgoff为0
  */
  unsigned long size = vma->vm_end - vma->vm_start; // 映射区域大小
  unsigned long offset = (vma->vm_pgoff << PAGE_SHIFT) +
                         info->fix.smem_start; // 映射物理地址的起始

  // dump_stack();
  printk("size: %lx\n", size);
  printk("vma start: %lx, end: %lx\n", vma->vm_start, vma->vm_end);
  printk("smem_start: %lx\n", info->fix.smem_start);
  printk("offset: %lx\n", offset);
  // 检查映射请求的范围是否在 Framebuffer 内存区域内
  if (offset + size > info->fix.smem_start + round_up(info->fix.smem_len, PAGE_SIZE)) {
    printk(KERN_ERR "myfb_mmap: Invalid mmap request\n");
    return -EINVAL;
  }

  // 将物理地址映射到用户空间虚拟地址
  if (remap_pfn_range(vma, start, offset >> PAGE_SHIFT, size,
                      vma->vm_page_prot)) {
    printk(KERN_ERR "myfb_mmap: Failed to remap framebuffer\n");
    return -EAGAIN;
  }
  return 0;
}

static struct fb_ops myfb_ops = {
    .owner = THIS_MODULE,
    .fb_fillrect = cfb_fillrect,
    .fb_copyarea = cfb_copyarea,
    .fb_imageblit = cfb_imageblit,
    .fb_mmap = myfb_mmap,
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
  if (of_property_read_u32(pdev->dev.of_node, "reg", &fb_base_phys_value) !=
      0) {
    return -1;
  }

  /* 1.1 分配fb_info */
  myfb_info = framebuffer_alloc(0, NULL);

  /* 1.2 设置fb_info */
  /* a. 可变参数 : LCD分辨率、颜色格式 */
  myfb_info->var.xres_virtual = myfb_info->var.xres = 500;
  myfb_info->var.yres_virtual = myfb_info->var.yres = 300;
  myfb_info->var.bits_per_pixel = 16; /* rgb565 */
  myfb_info->var.red.offset = 11;
  myfb_info->var.red.length = 5;
  myfb_info->var.green.offset = 5;
  myfb_info->var.green.length = 6;
  myfb_info->var.blue.offset = 0;
  myfb_info->var.blue.length = 5;

  /* b. 固定参数 */
  myfb_info->fix.smem_len = myfb_info->var.xres * myfb_info->var.yres *
                            myfb_info->var.bits_per_pixel / 8;

  /* fb的虚拟地址 */
  buffer = kmalloc(myfb_info->fix.smem_len, GFP_KERNEL);
  myfb_info->screen_base = (char *)buffer;
  phy_addr = dma_map_single(&pdev->dev, buffer, myfb_info->fix.smem_len, DMA_TO_DEVICE);
  if (dma_mapping_error(&pdev->dev, phy_addr)) {
    framebuffer_release(myfb_info);
    kfree(buffer);
    printk("dma mapping error!\n");
    return -1;
  }
  /* fb的物理地址 */
  myfb_info->fix.smem_start = phy_addr;

  myfb_info->fix.type = FB_TYPE_PACKED_PIXELS;
  myfb_info->fix.visual = FB_VISUAL_TRUECOLOR;
  myfb_info->fix.line_length =
      myfb_info->var.xres * myfb_info->var.bits_per_pixel / 8;

  /* c. fbops */
  myfb_info->fbops = &myfb_ops;

  /* 1.3 注册fb_info */
  register_framebuffer(myfb_info);

  /* 1.4 硬件操作 */
  mylcd_regs = ioremap(fb_base_phys_value, sizeof(struct lcd_regs));
  mylcd_regs->fb_base_phys = phy_addr;
  mylcd_regs->fb_xres = myfb_info->var.xres;
  mylcd_regs->fb_yres = myfb_info->var.yres;
  mylcd_regs->fb_bpp = myfb_info->var.bits_per_pixel;

  return 0;
}

static int lcd_remove(struct platform_device *pdev) {
  unregister_framebuffer(myfb_info);
  framebuffer_release(myfb_info);
  // 传输完成后，取消内存映射
  dma_unmap_single(&pdev->dev, *buffer, myfb_info->fix.smem_len, DMA_TO_DEVICE);
  // 使用完毕后，释放内存
  kfree(buffer);
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
int __init lcd_drv_init(void) { return platform_driver_register(&lcd_driver); }

/* 2. 出口 */
static void __exit lcd_drv_exit(void) {
  platform_driver_unregister(&lcd_driver);
}

module_init(lcd_drv_init);
module_exit(lcd_drv_exit);

MODULE_AUTHOR("www.100ask.net");
MODULE_DESCRIPTION("Framebuffer driver for the linux");
MODULE_LICENSE("GPL");

/*
static inline void *dma_alloc_coherent(struct device *dev, size_t size,
		dma_addr_t *dma_handle, gfp_t flag)
{
	return dma_alloc_attrs(dev, size, dma_handle, flag, 0);
}

static inline void *dma_alloc_wc(struct device *dev, size_t size,
				 dma_addr_t *dma_addr, gfp_t gfp)
{
	return dma_alloc_attrs(dev, size, dma_addr, gfp,
			       DMA_ATTR_WRITE_COMBINE);
}

*/

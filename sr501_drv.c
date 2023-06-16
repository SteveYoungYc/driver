#include <linux/irqreturn.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/major.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/workqueue.h>

#define BUF_LEN 128
#define NEXT_POS(x) ((x + 1) % BUF_LEN)

struct gpio_sr501 {
    int gpio;
    struct gpio_desc *gpiod;
    int flag;
    int irq;
    struct tasklet_struct tasklet;
};

static struct gpio_sr501 *gpio_sr501s;
static int major = 0;
static struct class *gpio_sr501_class;
static DECLARE_WAIT_QUEUE_HEAD(sr501_queue);
static int buff[BUF_LEN];
static int r, w;
static int is_buf_empty(void) { return (r == w); }
static int is_buf_full(void) { return (r == NEXT_POS(w)); }

static void put_to_buff(int val) {
    if (!is_buf_full()) {
        buff[w] = val;
        w = NEXT_POS(w);
    }
}

static int get_from_buff(void) {
    int val = 0;
    if (!is_buf_empty()) {
        val = buff[r];
        r = NEXT_POS(r);
    }
    return val;
}

static ssize_t gpio_sr501_drv_read(struct file *file, char __user *buf,
                                   size_t size, loff_t *offset) {
    int err;
    int val;

    if (is_buf_empty() && (file->f_flags & O_NONBLOCK))
        return -EAGAIN;

    wait_event_interruptible(sr501_queue, !is_buf_empty());
    val = get_from_buff();
    err = copy_to_user(buf, &val, 4);

    return 4;
}

static unsigned int gpio_sr501_drv_poll(struct file *fp, poll_table *wait) {
    return 0;
}

static struct file_operations gpio_sr501_drv = {
    .owner = THIS_MODULE,
    .read = gpio_sr501_drv_read,
    .poll = gpio_sr501_drv_poll,
};

static irqreturn_t sr501_isr(int irq, void *dev_id) {
    int val;
    int sr501_val;
    struct gpio_sr501 *sr501 = dev_id;
    printk("sr501_isr key %d irq happened\n", sr501->gpio);

    sr501_val = gpiod_get_value(sr501->gpiod);
    val = (sr501->gpio << 8) | sr501_val;
    put_to_buff(val);
    wake_up_interruptible(&sr501_queue);

    tasklet_schedule(&sr501->tasklet);
    return IRQ_HANDLED;
}

static void sr501_tasklet_func(unsigned long data) {
    struct gpio_sr501 *sr501 = (struct gpio_sr501 *)data;
    int val;
    val = gpiod_get_value(sr501->gpiod);
    printk("sr501_tasklet_func key %d %d\n", sr501->gpio, val);
}

static int gpio_sr501_probe(struct platform_device *pdev) {
    struct device_node *node = pdev->dev.of_node;
    int i;
    int err;
    int count;
    enum of_gpio_flags flag;

    count = of_gpio_count(node);
    if (!count) {
        printk("No gpio of sr501 available\n");
        return -1;
    }
    printk("GPIO count: %d\n", count);

    gpio_sr501s = kzalloc(sizeof(struct gpio_sr501) * count, GFP_KERNEL);

    for (i = 0; i < count; i++) {
        gpio_sr501s[i].gpio = of_get_gpio_flags(node, i, &flag);
        gpio_sr501s[i].gpiod = gpio_to_desc(gpio_sr501s[i].gpio);
        gpio_sr501s[i].irq = gpio_to_irq(gpio_sr501s[i].gpio);
        tasklet_init(&gpio_sr501s[i].tasklet, sr501_tasklet_func,
                    (unsigned long) &gpio_sr501s[i]);
    }

    for (i = 0; i < count; i++) {
        err = request_irq(gpio_sr501s[i].irq, sr501_isr,
                          IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                          "sr501_irq", &gpio_sr501s[i]);
    }

    major = register_chrdev(0, "100ask_gpio_sr501", &gpio_sr501_drv);
    gpio_sr501_class = class_create(THIS_MODULE, "100ask_gpio_sr501_class");
    if (IS_ERR(gpio_sr501_class)) {
        unregister_chrdev(major, "100ask_gpio_sr501");
        return PTR_ERR(gpio_sr501_class);
    }
    device_create(gpio_sr501_class, NULL, MKDEV(major, 0), NULL,
                  "100ask_gpio_sr501");

    printk("sr501 probed.\n");
    return 0;
}

static int gpio_sr501_remove(struct platform_device *pdev) {
    int i;
    int count;

    struct device_node *node = pdev->dev.of_node;
    count = of_gpio_count(node);

    device_destroy(gpio_sr501_class, MKDEV(major, 0));
    class_destroy(gpio_sr501_class);
    unregister_chrdev(major, "100ask_gpio_sr501");
    for (i = 0; i < count; i++) {
        free_irq(gpio_sr501s[i].irq, &gpio_sr501s[i]);
    }
    kfree(gpio_sr501s);
    printk("sr501 removed.\n");
    return 0;
}

static const struct of_device_id sr501_match_table[] = {
    {.compatible = "100ask,sr501"},
    {},
};

static struct platform_driver gpio_sr501_driver = {
    .probe = gpio_sr501_probe,
    .remove = gpio_sr501_remove,
    .driver =
        {
            .name = "sr501",
            .of_match_table = sr501_match_table,
        },
};

static int __init gpio_sr501_init(void) {
    return platform_driver_register(&gpio_sr501_driver);
}

static void __exit gpio_sr501_exit(void) {
    platform_driver_unregister(&gpio_sr501_driver);
}

module_init(gpio_sr501_init);
module_exit(gpio_sr501_exit);

MODULE_LICENSE("GPL");

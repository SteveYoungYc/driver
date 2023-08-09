#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
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

struct gpio_key {
  int gpio;
  struct gpio_desc *gpiod;
  int flag;
  int irq;
  struct tasklet_struct tasklet;
};

static struct gpio_key *gpio_keys;
static int major = 0;
static struct class *gpio_key_class;
static DECLARE_WAIT_QUEUE_HEAD(key_queue);
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

static ssize_t gpio_key_drv_read(struct file *file, char __user *buf,
                                 size_t size, loff_t *offset) {
  int err;
  int val;
  // dump_stack();
  if (is_buf_empty() && (file->f_flags & O_NONBLOCK))
    return -EAGAIN;

  wait_event_interruptible(key_queue, !is_buf_empty());
  val = get_from_buff();
  err = copy_to_user(buf, &val, 4);

  return 4;
}

static unsigned int gpio_key_drv_poll(struct file *fp, poll_table *wait) {
  return 0;
}

static struct file_operations gpio_key_drv = {
    .owner = THIS_MODULE,
    .read = gpio_key_drv_read,
    .poll = gpio_key_drv_poll,
};

static irqreturn_t key_isr(int irq, void *dev_id) {
  int val;
  int key_val;
  struct gpio_key *key = dev_id;
  printk("key_isr key %d irq happened\n", key->gpio);

  key_val = gpiod_get_value(key->gpiod);
  val = (key->gpio << 8) | key_val;
  put_to_buff(val);
  wake_up_interruptible(&key_queue);

  tasklet_schedule(&key->tasklet);
  return IRQ_HANDLED;
}

static void key_tasklet_func(unsigned long data) {
  struct gpio_key *key = (struct gpio_key *)data;
  int val;
  dump_stack();
  val = gpiod_get_value(key->gpiod);
  printk("key_tasklet_func key %d %d\n", key->gpio, val);
}

static int gpio_key_probe(struct platform_device *pdev) {
  struct device_node *node = pdev->dev.of_node;
  int i;
  int err;
  int count;
  enum of_gpio_flags flag;

  count = of_gpio_count(node);
  if (!count) {
    printk("No gpio of key available\n");
    return -1;
  }
  printk("GPIO count: %d\n", count);

  gpio_keys = kzalloc(sizeof(struct gpio_key) * count, GFP_KERNEL);

  for (i = 0; i < count; i++) {
    gpio_keys[i].gpio = of_get_gpio_flags(node, i, &flag);
    gpio_keys[i].gpiod = gpio_to_desc(gpio_keys[i].gpio);
    gpio_keys[i].irq = gpio_to_irq(gpio_keys[i].gpio);
    tasklet_init(&gpio_keys[i].tasklet, key_tasklet_func,
                 (unsigned long)&gpio_keys[i]);
  }

  for (i = 0; i < count; i++) {
    err = request_irq(gpio_keys[i].irq, key_isr,
                      IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "key_irq",
                      &gpio_keys[i]);
  }

  major = register_chrdev(0, "100ask_gpio_key", &gpio_key_drv);
  gpio_key_class = class_create(THIS_MODULE, "100ask_gpio_key_class");
  if (IS_ERR(gpio_key_class)) {
    unregister_chrdev(major, "100ask_gpio_key");
    return PTR_ERR(gpio_key_class);
  }
  device_create(gpio_key_class, NULL, MKDEV(major, 0), NULL, "100ask_gpio_key");

  printk("key probed.\n");
  return 0;
}

static int gpio_key_remove(struct platform_device *pdev) {
  int i;
  int count;

  struct device_node *node = pdev->dev.of_node;
  count = of_gpio_count(node);

  device_destroy(gpio_key_class, MKDEV(major, 0));
  class_destroy(gpio_key_class);
  unregister_chrdev(major, "100ask_gpio_key");
  for (i = 0; i < count; i++) {
    free_irq(gpio_keys[i].irq, &gpio_keys[i]);
  }
  kfree(gpio_keys);
  printk("key removed.\n");
  return 0;
}

static const struct of_device_id key_match_table[] = {
    {.compatible = "100ask,sr501"},
    {},
};

static struct platform_driver gpio_key_driver = {
    .probe = gpio_key_probe,
    .remove = gpio_key_remove,
    .driver =
        {
            .name = "key",
            .of_match_table = key_match_table,
        },
};

static int __init gpio_key_init(void) {
  return platform_driver_register(&gpio_key_driver);
}

static void __exit gpio_key_exit(void) {
  platform_driver_unregister(&gpio_key_driver);
}

module_init(gpio_key_init);
module_exit(gpio_key_exit);

MODULE_LICENSE("GPL");

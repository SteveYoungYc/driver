#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio_keys.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sysctl.h>
#include <linux/workqueue.h>

struct key_info {
  int gpio;
  struct gpio_desc *gpiod;
  int flag;
  int irq;
};

static int key_num;
static struct key_info *keys;
static struct input_dev *g_input_dev;

static irqreturn_t input_dev_isr(int irq, void *dev_id) {
  /* read data */
  int key_val;
  struct key_info *key = dev_id;
  key_val = gpiod_get_value(key->gpiod);
  printk("key_isr key %d irq happened, val: %d\n", key->gpio, key_val);
  /* report data */
  input_event(g_input_dev, EV_KEY, key->gpio, key_val);
  input_sync(g_input_dev);

  return IRQ_HANDLED;
}

/* alloc/set/register platform_driver */
static int input_dev_probe(struct platform_device *pdev) {
  int error;
  int i;
  enum of_gpio_flags flag;
  struct device *dev = &pdev->dev;

  /* get hardware info from device tree */
  key_num = of_gpio_count(dev->of_node);
  if (!key_num) {
    printk("No gpio of key available\n");
    return -1;
  }
  printk("key input dev probed\n");

  keys = kzalloc(sizeof(struct key_info) * key_num, GFP_KERNEL);
  for (i = 0; i < key_num; i++) {
    keys[i].gpio = of_get_gpio_flags(dev->of_node, i, &flag);
    keys[i].gpiod = gpio_to_desc(keys[i].gpio);
    keys[i].irq = gpio_to_irq(keys[i].gpio);
  }

  /* alloc/set/register input_dev */
  g_input_dev = devm_input_allocate_device(dev);

  g_input_dev->name = "input_dev";
  g_input_dev->phys = "input_dev";
  g_input_dev->dev.parent = dev;
  g_input_dev->id.bustype = BUS_HOST;
  g_input_dev->id.product = 1;
  g_input_dev->id.version = 2;
  /* set 1: which type event ? */
  __set_bit(EV_KEY, g_input_dev->evbit);
  __set_bit(INPUT_PROP_DIRECT, g_input_dev->propbit);
  /* set 2: which event ? */
  __set_bit(BTN_TOUCH, g_input_dev->keybit);
  /* set 3: event params ? */
  error = input_register_device(g_input_dev);
  if (error) {
    printk("input_register_device failed\n");
    return -1;
  }

  /* hardware opration */
  for (i = 0; i < key_num; i++) {
    error = request_irq(keys[i].irq, input_dev_isr,
                        IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                        "input_dev_irq", &keys[i]);
    if (error) {
      printk("request_irq failed\n");
      return -1;
    }
  }
  return 0;
}

static int input_dev_remove(struct platform_device *pdev) {
  int i;
  for (i = 0; i < key_num; i++) {
    free_irq(keys[i].irq, NULL);
  }
  input_unregister_device(g_input_dev);
  kfree(keys);
  return 0;
}

static const struct of_device_id input_dev_of_match[] = {
    {
        .compatible = "100ask,input_dev",
    },
    {},
};

static struct platform_driver input_dev_driver = {
    .probe = input_dev_probe,
    .remove = input_dev_remove,
    .driver = {
        .name = "input_dev",
        .of_match_table = input_dev_of_match,
    }};

static int __init input_dev_init(void) {
  return platform_driver_register(&input_dev_driver);
}

static void __exit input_dev_exit(void) {
  platform_driver_unregister(&input_dev_driver);
}

module_init(input_dev_init);
module_exit(input_dev_exit);

MODULE_LICENSE("GPL");

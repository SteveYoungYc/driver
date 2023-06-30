#include <linux/device.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>
#include <linux/log2.h>
#include <linux/bitops.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/acpi.h>
#include <linux/i2c.h>
#include <asm/uaccess.h>

#define IOC_AT24C02_READ  100
#define IOC_AT24C02_WRITE 101

static int major;
static struct class *at24c02_class;
static struct i2c_client *at24c02_client;

static long at24c02_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	unsigned char addr;
	unsigned char data;
	unsigned int ker_buf[2];
	unsigned int *usr_buf = (unsigned int *)arg;
	unsigned char byte_buf[2];
	int ret;
	
	struct i2c_msg msgs[2];
	ret = copy_from_user(ker_buf, usr_buf, 8);
	addr = ker_buf[0];
	
	switch (cmd) {
		case IOC_AT24C02_READ: {
			msgs[0].addr  = at24c02_client->addr;
			msgs[0].flags = 0; /* 写 */
			msgs[0].len   = 1;
			msgs[0].buf   = &addr;

			msgs[1].addr  = at24c02_client->addr;
			msgs[1].flags = I2C_M_RD; /* 读 */
			msgs[1].len   = 1;
			msgs[1].buf   = &data;
		
			i2c_transfer(at24c02_client->adapter, msgs, 2);

			ker_buf[1] = data;
			ret = copy_to_user(usr_buf, ker_buf, 8);
			
			break;
		}
		case IOC_AT24C02_WRITE: {
			/* 写AT24C02 */
			byte_buf[0] = addr;
			byte_buf[1] = ker_buf[1];
			
			msgs[0].addr  = at24c02_client->addr;
			msgs[0].flags = 0; /* 写 */
			msgs[0].len   = 2;
			msgs[0].buf   = byte_buf;

			i2c_transfer(at24c02_client->adapter, msgs, 1);

			mdelay(20);
			
			break;
		}
	}

	return 0;
}

static struct file_operations at24c02_fops = {
	.owner	 = THIS_MODULE,
	.unlocked_ioctl = at24c02_ioctl,
};

static int at24c02_probe(struct i2c_client *client, const struct i2c_device_id *id) {
	at24c02_client = client;
	/* register_chrdev */
	major = register_chrdev(0, "at24c02", &at24c02_fops);  
	/* class_create */
	at24c02_class = class_create(THIS_MODULE, "at24c02_class");
	/* device_create */
	device_create(at24c02_class, NULL, MKDEV(major, 0), NULL, "myat24c02");
	return 0;
}

static int at24c02_remove(struct i2c_client *client) {
	device_destroy(at24c02_class, MKDEV(major, 0));
	unregister_chrdev(major, "at24c02");
	class_destroy(at24c02_class);
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static const struct of_device_id at24c02_of_match[] = {
	{.compatible = "100ask,at24c02"},
	{}
};

static const struct i2c_device_id at24c02_ids[] = {
	{ "",	(kernel_ulong_t)NULL },
	{ /* END OF LIST */ }
};

static struct i2c_driver at24c02_drv = {
	.driver = {
		.name = "my24c02",
		.of_match_table	 = at24c02_of_match,
	},
	.probe = at24c02_probe,
	.remove = at24c02_remove,
	.id_table = at24c02_ids,
};

static int at24c02_init(void) {
	int err;
	err = i2c_add_driver(&at24c02_drv);
	printk("%s %s %d, err = %d\n", __FILE__, __FUNCTION__, __LINE__, err);
	return err;
}

static void at24c02_exit(void) {
	i2c_del_driver(&at24c02_drv);
}

module_init(at24c02_init);
module_exit(at24c02_exit);

MODULE_LICENSE("GPL");

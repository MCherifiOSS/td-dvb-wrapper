/*
 * td-dvb-frontend.c: wrapper module that presents the Tripledragon SAT tuner
 *                    as a /dev/dvb/adapter0/frontend0 device to userspace
 *
 * Copyright (C) 2012 Stefan Seyfried, seife@tuxbox-git.slipkontur.de
 *
 * Licensed under the GPL V2
 *
 * TODO: this should be a proper dvb frontend driver which registers to dvb-core
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <asm/uaccess.h>
#include <linux/devfs_fs_kernel.h>

/* for ugly open/close hack */
//#include <linux/fs.h>
#include <asm/segment.h>
//#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#include "frontend.h"
#include "td-dvb-frontend.h"

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static unsigned int device_poll(struct file *, struct poll_table_struct *);
static long device_ioctl(struct file *, unsigned int, unsigned long);

#define DEVICE_NAME "td-fe-dvb"	/* Dev name as it appears in /proc/devices */
#define DVB_MAJOR 212		/* DVB device */

static struct file *tdfe = NULL;
static struct class_simple *my_class;
static __u16 pol = SEC_VOLTAGE_OFF;
static __u16 tone = 0;
static int usecount = 0;
static int polled = 0;

static struct dvb_frontend_info fe_info = {
	.name = "TD Sattuner",
	.type = FE_QPSK
};

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.poll = device_poll,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
};

static struct file* file_open(const char* path, int flags, int rights) {
	struct file* filp = NULL;
	mm_segment_t oldfs;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);

	if(IS_ERR(filp))
		return NULL;
	return filp;
}

#if 0
/* not yet used, but useful for debugging / testing */
static int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_read(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}
#endif

static int file_ioctl(unsigned int cmd, __u32 arg)
{
	mm_segment_t oldfs;
	int error;

	if (!tdfe)
		return -EBADF;
//	error = security_file_ioctl(file, cmd, arg);
//	if (error)
//		goto out;

	if (!tdfe->f_op || !tdfe->f_op->ioctl)
		return -ENOTTY;

	oldfs = get_fs();
	set_fs(get_ds());

	error = tdfe->f_op->ioctl(tdfe->f_dentry->d_inode, tdfe, cmd, arg);
	if (error == -ENOIOCTLCMD)
		error = -EINVAL;

	set_fs(oldfs);
	return error;
}

static unsigned int device_poll(struct file *file, struct poll_table_struct *wait)
{
	mm_segment_t oldfs;
	int error;
	if (!tdfe)
		return -EBADF;

	if (!tdfe->f_op || !tdfe->f_op->poll)
		return -ENOTTY;

	polled = 1;
	oldfs = get_fs();
	set_fs(get_ds());

	error = tdfe->f_op->poll(tdfe, wait);
	/* the NBF setup ioctl returns POLLIN after first tune, POLLPRI
	 * after finetuning finished. Mask out POLLIN? */
#ifdef USE_FINETUNE
	error &= ~POLLIN;
#endif
	if (error & POLLERR) /* neutrino userspace does not check for POLLERR :( */
		error |= POLLIN|POLLPRI;

	set_fs(oldfs);
	return error;
}
/* end ugly hack */

static struct cdev device_cdev = {
	.kobj   = {.name = "td-dvb-fe", },
	.owner  =       THIS_MODULE,
};

int init_module(void)
{
	int ret;
	dev_t dev = MKDEV(DVB_MAJOR, 0);

	if ((ret = register_chrdev_region(dev, 64, DEVICE_NAME)) != 0) {
		printk(KERN_INFO "td-dvb-fe: unable to get major %d\n", DVB_MAJOR);
		goto error;
	}

	cdev_init(&device_cdev, &fops);
	if ((ret = cdev_add(&device_cdev, dev, 64)) != 0) {
		printk(KERN_INFO "td-dvb-fe: unable to get major %d\n", DVB_MAJOR);
		goto error;
	}

	devfs_mk_dir("dvb");
	my_class = class_simple_create(THIS_MODULE, DEVICE_NAME);

	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		goto error;
	}

	devfs_mk_cdev(MKDEV(DVB_MAJOR, 3), S_IFCHR | S_IRUSR | S_IWUSR, "dvb/adapter0/frontend0");
	class_simple_device_add(my_class, MKDEV(DVB_MAJOR, 3), NULL, "dvb0.frontend0");

	printk(KERN_INFO "[td-dvb-fe] loaded\n");
	return 0;
error:
	cdev_del(&device_cdev);
	unregister_chrdev_region(dev, 64);
	return ret;
}

void cleanup_module(void)
{
	/*
	 * Unregister the device
	 */
	devfs_remove("dvb/adapter0/frontend0");
	class_simple_device_remove(MKDEV(DVB_MAJOR, 3));
	devfs_remove("dvb");
	class_simple_destroy(my_class);
	cdev_del(&device_cdev);
	unregister_chrdev_region(MKDEV(DVB_MAJOR, 0), 64);
	printk(KERN_INFO "[td-dvb-fe] unload successful\n");
}

static int device_open(struct inode *inode, struct file *file)
{
	/* usecount is probably not race-free etc, but it is good enough to
	 * avoid accidental iterference by opening the device */
	if (usecount)
		return -EBUSY;

	tdfe = file_open("/dev/stb/tdtuner0", O_RDWR|O_NONBLOCK, 0);

	if (!tdfe) {
		printk(KERN_INFO "td-dvb-fe: could not open tdtuner0\n");
		return -ENODEV;
	}
	usecount++;
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
	if (tdfe) {
		/* reset the tuner on close */
		if (pol != SEC_VOLTAGE_OFF)
			device_ioctl(NULL, FE_SET_VOLTAGE, SEC_VOLTAGE_OFF);
		filp_close(tdfe, NULL);
	}
	tdfe = NULL;
	usecount--;
	return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	__u32 ui32 = 0;
	__u16 ui16;
	struct dvb_frontend_event event;
	struct dvb_diseqc_master_cmd *sec;
	struct dtv_properties *cmdseq;
	char raw[1 + 3 + 3];
	td_fe t;

	switch(cmd)
	{
		case FE_GET_INFO:
			if (copy_to_user((struct dvb_frontend_info *)arg, &fe_info, sizeof(struct dvb_frontend_info)))
				ret = -EFAULT;
			break;
		case FE_READ_STATUS:
			file_ioctl(TD_FE_READ_STATUS_C, (__u32)&ui32);
			put_user(ui32, (__u32 *)arg);
			break;
		case FE_READ_BER:
			file_ioctl(TD_FE_SET_ERROR_SOURCE, ERR_SRC_VITERBI_BIT_ERRORS);
			file_ioctl(TD_FE_GET_ERRORS, (__u32)&ui32);
			put_user(ui32, (__u32 *)arg);
			break;
		case FE_READ_UNCORRECTED_BLOCKS:
			file_ioctl(TD_FE_SET_ERROR_SOURCE, ERR_SRC_PACKET_ERRORS);
			file_ioctl(TD_FE_GET_ERRORS, (__u32)&ui32);
			put_user(ui32, (__u32 *)arg);
			break;
		case FE_READ_SIGNAL_STRENGTH:
			file_ioctl(TD_FE_READ_SIGNAL_STRENGTH_C, (__u32)&ui16);
			put_user(ui16, (__u16 *)arg);
			break;
		case FE_READ_SNR:
			file_ioctl(TD_FE_READ_SNR_C, (__u32)&ui16);
			put_user(ui16, (__u16 *)arg);
			break;
		case FE_GET_EVENT:
			memset(&event, 0, sizeof(struct dvb_frontend_event));
			file_ioctl(TD_FE_READ_STATUS_C, (__u32)&ui32);
			event.status = ui32;
			file_ioctl(TD_FE_SETUP_GET, (__u32)&t);
			event.parameters.frequency = t.realfrq;
			if (event.status & FE_HAS_LOCK)
				file_ioctl(TD_FE_LOCKLED_ENABLE, 1);
			/* if no poll before GET_EVENT => return error
			   this is adopted to neutrino userspace behaviour */
			if (!polled)
				ret = -EWOULDBLOCK;
			if (copy_to_user((struct dvb_frontend_event *)arg, &event, sizeof(struct dvb_frontend_event)))
				ret = -EFAULT;
			polled = 0;
			break;
		case FE_SET_TONE:
			if (arg == SEC_TONE_ON)
				ui32 = 1;
			tone = ui32;
			ret = file_ioctl(TD_FE_22KHZ_ENABLE, ui32);
			break;
		case FE_SET_VOLTAGE:
			if (arg == SEC_VOLTAGE_OFF) {
				pol = SEC_VOLTAGE_OFF;
				tone = 0;
				file_ioctl(TD_FE_LOCKLED_ENABLE, 0);
				file_ioctl(TD_FE_LNB_ENABLE, 0);
				file_ioctl(TD_FE_22KHZ_ENABLE, 0);
				file_ioctl(TD_FE_SET_STANDBY, 1);
				file_ioctl(TD_FE_LNBLOOPTHROUGH, 1);
				break;
			}
			if (pol == SEC_VOLTAGE_OFF) {
				file_ioctl(TD_FE_DISEQC_MODE, DISEQC_MODE_CMD_ONLY);
				file_ioctl(TD_FE_LNBLOOPTHROUGH, 0);
				file_ioctl(TD_FE_SET_STANDBY, 0);
				file_ioctl(TD_FE_HARDRESET, 0);
				file_ioctl(TD_FE_INIT, 0);
				file_ioctl(TD_FE_LNB_ENABLE, 1);
				file_ioctl(TD_FE_LNB_SET_HORIZONTAL, 0);
			}
			if (arg == SEC_VOLTAGE_13)
				ui32 = 1;
			pol = ui32;
			ret = file_ioctl(TD_FE_LNB_SET_POLARISATION, ui32);
			break;
		case FE_DISEQC_SEND_MASTER_CMD:
			sec = (struct dvb_diseqc_master_cmd *)arg;
			memset(&raw[0], 0, 7);
			raw[0] = sec->msg_len;
			memcpy(&raw[1], sec->msg, sec->msg_len);
			ret = file_ioctl(TD_FE_DISEQC_SEND, (__u32)&raw);
			break;
		case FE_DISEQC_SEND_BURST:
			if (arg == SEC_MINI_B)
				ui32 = 1;
			ret = file_ioctl(TD_FE_DISEQC_SEND_BURST, ui32);
			break;
		case FE_SET_PROPERTY:
#define FREQUENCY 1
#define SYMBOL_RATE 4
#define DELIVERY_SYSTEM 5
			cmdseq = (struct dtv_properties *)arg;
			/* don't even try to tune to DVB-S2 */
			if (cmdseq->props[DELIVERY_SYSTEM].u.data != SYS_DVBS) {
				ret = -EINVAL;
				break;
			}
			t.fec = 0; /* always AUTO_FEC */
			t.frequency = cmdseq->props[FREQUENCY].u.data;
			t.symbolrate = cmdseq->props[SYMBOL_RATE].u.data / 1000;
			t.polarity = pol;
			t.highband = tone;
			file_ioctl(TD_FE_LOCKLED_ENABLE, 0);
			file_ioctl(TD_FE_STOPTHAT, 0);
			file_ioctl(TD_FE_SET_FRANGE, (int)(t.symbolrate / 500));
#ifdef USE_FINETUNE
			file_ioctl(TD_FE_SETUP_NBF, (__u32)&t);
#else
			file_ioctl(TD_FE_SETUP_NB, (__u32)&t);
#endif
			break;
		default:
			printk(KERN_ERR "td-dvb-fe: unhandled ioctl 0x%08x\n", cmd);
			ret = -EINVAL;
			break;
	}
	return ret;
}

MODULE_AUTHOR("Stefan Seyfried <seife@tuxbox-git.slipkontur.de>");
MODULE_LICENSE("GPL");

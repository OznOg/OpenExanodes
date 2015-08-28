#ifndef _SMD_OPS_H
#define _SMD_OPS_H
#include <linux/fs.h>

extern int smd_open(struct inode *inode, struct file *filp);
extern int smd_release(struct inode *inode, struct file *filp); 

extern struct block_device_operations smd_fops;

#endif

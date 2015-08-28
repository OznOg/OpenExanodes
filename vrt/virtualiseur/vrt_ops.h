#ifndef _VRT_OPS_H
#define _VRT_OPS_H
#include <linux/fs.h>

extern int vrt_open(struct inode *inode, struct file *filp);
extern int vrt_release(struct inode *inode, struct file *filp);

extern struct block_device_operations vrt_fops;

#endif

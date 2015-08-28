#ifndef _VRT_RDEV 
#define _VRT_RDEV

extern int get_rdev_nb(group_t *g,
		       u8 rdev_major, 
		       u8 rdev_minor, 
		       u32 *no_rdev);

extern int vrt_add_new_rdev(group_t *g,
			    char *dev_name, 
			    u64 dev_size, 
			    u8 dev_major, 
			    u8 dev_minor);

extern int vrt_add_old_rdev(group_t *,
			    u32 *uuid_rdev,
			    char *rdev_name,
			    u64 rdev_size,
			    u8 rdev_major, 
			    u8 rdev_minor);
extern int rdev_already_used(u8 rdev_major, 
			     u8 rdev_minor);
#endif

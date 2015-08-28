#ifndef _VRT_GROUP_H
#define _VRT_GROUP_H
extern u64 group_size_KB(group_t *g);
extern int vrt_create_group(char *group_name, char *layout_name);
extern int get_group_nb(char *group_name, u32 *no_group);
extern void clean_group(group_t *g);
extern int vrt_start_group(group_t *g);
extern int vrt_write_group(group_t *g);
extern int vrt_create_old_group(char *gname,
				int rdev_major, 
				int rdev_minor,
				u64 rdev_size);
extern int vrt_start_old_group(group_t *g);
extern int vrt_stop_group(group_t *g, int no_group);
extern int gname_not_used(char *group_name);
#endif

#ifndef __DATAACCESSCOMMANDS
#define __DATAACCESSCOMMANDS
#include "constantes.h"
#include <sys/stat.h>

struct sb_brique;
struct sb_zone;

extern inline int valid_no_zone(int no_zone, struct sb_brique *sbb);
extern int access_block(int cmd, char *device_name, __u64 offset,
			void* data, int data_size);
extern int access_meta_b(int cmd, char *brique_name, __u64 offset_r, 
			 void* data, int data_size);
extern int zone_existe(char *brique_name,
		 int uuid_ok, char *uuid_zone,
		 int name_ok, char *zone_name,
		struct sb_zone *sbz);
int load_sbb(char *dev_name, struct sb_brique *sbb);
extern int get_nb_kbytes(char *nom_de_device, __u64 *nb_kbytes);
extern unsigned long long calculate_offset_sbb(__u64 size_kb);
extern int raz_sbg_rdev(char *dev_name);
extern void get_major_minor(struct stat *info, int *major, int *minor);
extern int stornbd_rdevs(char *rdevs_path[], int *nb_rdevs);
extern int get_group_name(char *dev_path, char *gname);
extern int get_uuid_rdevs(char *dev_path, 
			  __u32 rdevs_uuid[NBMAX_RDEVS][4],
			  int *nb_rdevs_in_group);
extern int all_rdevs_reachable(__u32 rdevs_uuid[NBMAX_RDEVS][4],
			       int nb_rdevs_in_group,
			       char *rdevs_path[], 
			       int nb_rdevs_in_path,
			       char *rdevs_path_g[]);
extern int get_a_rdev_in_group(char *gname, 
			       char *rdevs_path[], 
			       int nb_rdevs,
			       int *no_rdev);
extern int get_all_group_rdevs(char *gname,
			       char *rdevs_path_g[], 
			       __u32 rdevs_uuid_g[NBMAX_RDEVS][4], 
			       int *nb_rdevs_g);
#endif

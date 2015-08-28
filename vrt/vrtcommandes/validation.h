#ifndef _VALIDATION
#define _VALIDATION

extern int zname_in_group(char *zname, char *gname);
extern int valid_unused_zname(char *zname, char *gname);
extern int valid_unused_gname(char *gname);
extern int gname_active(char *gname);
extern int zone_active(char *zname, char *gname);
extern int get_all_active_zones(char *gname, 
			 char *zones_names[], 
			 int *nb_az);
extern int get_all_idling_zones(char *gname, 
				char *zones_names[], 
				int *nb_iz);
extern int desactivate_all_zones(char *gname);
extern int desactivate_zone(char *g_name, char *z_name);
extern int desactivate_group(char *gname);
extern int all_group_rdevs_accessibles(char *gname);
extern int vrt_active(void);
#endif

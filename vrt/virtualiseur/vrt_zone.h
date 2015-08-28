#ifndef _vrt_zone_h
#define _vrt_zone_h
extern int vrt_new_zone(group_t *g, u64 zsize_KB, char *zname);
extern int get_zone_nb(group_t *g, char *zname, int *no_zone);
extern int vrt_activate_zone(group_t *g, storzone_t *z);
extern int vrt_desactivate_zone(group_t *g, storzone_t *z);
extern int vrt_delete_zone(group_t *g, storzone_t *z);
extern int vrt_resize_zone(group_t *g, storzone_t *z, u64 newsize);
extern int is_zone_ptr_valid(storzone_t *z);
extern int vrt_desactivate_allzones(group_t *g);
  //extern void vrt_delete_zone(long no_zone);
  //extern void vrt_resize_zone(long no_zone, long taille_ko);
#endif

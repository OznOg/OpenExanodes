#ifndef _VRT_LAYOUT
#define _VRT_LAYOUT
struct sb_zone_sstriping;

extern int vrt_init_new_brique_reflex(int no_brique, long taille);
extern int vrt_init_new_brique_sstriping(int no_brique, long taille);
extern void vrt_determiner_pos_brique_reflex(int no_zone, long no_ue_zone, int *no_brique, long *no_ue_brique);
extern void vrt_determiner_pos_brique_sstriping(int no_zone, long no_ue_zone, int *no_brique, long *no_ue_brique);
extern void vrt_placer_nvelle_ue_reflex(int no_zone, long no_ue_zone);
extern int vrt_ue_non_placee_reflex(int no_zone, long no_ue_zone);
extern void vrt_zone_new_size_reflex(int no_zone, long taille_zone);
extern void vrt_zone_new_size_sstriping(int no_zone, long taille_zone);
extern long vrt_ue_actually_used_reflex(void);
extern long vrt_ue_actually_used_sstriping(void);
extern long vrt_nb_unused_ue_in_zone_reflex(long no_zone);
extern int vrt_init_new_zone_reflex(int no_zone, long taille);
extern int vrt_delete_zone_sstriping(int no_zone);
extern int vrt_init_new_zone_sstriping(int no_zone, long taille);
extern long vrt_nb_ue_on_ssh_sstriping(void);
extern int layout_type_sstriping(void);
extern char *layout_name_sstriping(void);
extern void clean_layout_sstriping(group_t *g);
extern void clean_layout_zone_sstriping(storzone_t *z);
extern int init_layout_sstriping(group_t *g);
extern u64 min_rdev_size_sstriping(void);
extern u64 used_cs_sstriping(group_t *g);
extern u64 usable_cs_sstriping(group_t *g);
extern int create_and_write_sb_zone_sstriping(group_t *g, 
				       storzone_t *z);
extern u64 r_offset_sbz_sstriping(storzone_t *z);
extern int init_zone_layout_sstriping(group_t *g, storzone_t *z);
extern void zone2rdev_sstriping(storzone_t *z, 
			 unsigned long zsector, 
			 realdev_t **rd, 
			 unsigned long *rdsector);
extern int laytype2layname(u8 layout_type, char *layout_name);
extern int read_sbz_and_rebuild_lay_sstriping(group_t *g);
extern int rebuild_zone_sstriping(storzone_t **z, 
				  struct sb_zone_sstriping *sbz,
				  group_t *g);
extern void calculate_capa_used_sstriping(group_t *g);
extern int resize_layout_zone_sstriping(storzone_t *z, u64 newsize);
#endif

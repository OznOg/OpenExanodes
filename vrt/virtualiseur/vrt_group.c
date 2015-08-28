#include "virtualiseur.h"
#include <linux/kernel.h>
#include "vrt_superblock.h"
#include <asm/string.h>
#include "vrt_divers.h"
#include <linux/vmalloc.h>
#include <linux/random.h>
#include "vrt_layout.h"
#include <linux/proc_fs.h>
#include "vrt_proc.h"
#include "vrt_zone.h"


static struct sb_group sbg_aux;



//! init les repertoires et fichiers devfs et procfs utilisés par le groupe 'g' 
int init_dev_and_proc(group_t *g) {
  g->dev_rep = devfs_mk_dir(vs.dev_rep, g->name, NULL);
  if (g->dev_rep == NULL) {
    PERROR("can't create devfs rep for group %s\n",g->name);
    return ERROR;
  }
    
  g->proc_rep = proc_mkdir(g->name, vs.gproc_rep);
  if (g->proc_rep == NULL) {
    PERROR("can't create proc rep for group %s\n",g->name);
    return ERROR;
  }
  
  g->proc_az = create_proc_read_entry("active_zones",
				      0,
				      g->proc_rep,
				      vrt_read_active_zones,
				      (void *)g->name);
  if (g->proc_az == NULL) {
    PERROR("can't create proc file 'active_zones' for group %s\n",
	   g->name);
    return ERROR;
  }
    
  g->proc_iz = create_proc_read_entry("idling_zones",
				      0,
				      g->proc_rep,
				      vrt_read_idling_zones,
				      (void *)g->name);
  if (g->proc_iz == NULL) {
    PERROR("can't create proc file 'idling_zones' for group %s\n",
	   g->name);
    return ERROR;
  }
  
  g->proc_uz = create_proc_read_entry("used_zones",
				      0,
				      g->proc_rep,
				      vrt_read_used_zones,
				      (void *)g->name);
  if (g->proc_uz == NULL) {
    PERROR("can't create proc file 'used_zones' for group %s\n",
	   g->name);
    return ERROR;
  }
  return SUCCESS;
    
} 
 
//! libère les repertoires et fichiers devfs et procfs utilisés par le groupe 'g' 
void clean_dev_and_proc(group_t *g) {
  
  if (g->proc_az) 
    remove_proc_entry("active_zones",g->proc_rep);

  if (g->proc_iz)
    remove_proc_entry("idling_zones",g->proc_rep);

  if (g->proc_uz)
    remove_proc_entry("used_zones",g->proc_rep);

  if (g->proc_rep) 
    remove_proc_entry(g->name,vs.gproc_rep);

  if (g->dev_rep) 
    devfs_unregister(g->dev_rep);
}

//! démarre le groupe, c'est-à-dire, init sa layout et crée un rep /dev, pour pouvoir gérer des zones. renvoie ERROR/SUCCESS
int vrt_start_group(group_t *g) {
  if (g->nb_realdevs  > 0) {
    (*(g->init_layout))(g);
    
    if (init_dev_and_proc(g) == ERROR) return ERROR;
    
    g->active = TRUE;
    return SUCCESS;
  }
  else return ERROR; // pas de rdevs dans ce group
}



//! affecte les pointeurs de fonction du group avec les fonctions du layout passé en paramètre. Renvoie ERROR/SUCCESS
int setup_gfunctions(group_t *g, char *layout_name) {
  if (strcmp(layout_name, SSTRIPING_NAME) == 0) {
    g->init_layout              =   &init_layout_sstriping;
    g->clean_layout             =   &clean_layout_sstriping;
    g->clean_layout_zone        =   &clean_layout_zone_sstriping;
    g->min_rdev_size            =   &min_rdev_size_sstriping;
    g->layout_name              =   &layout_name_sstriping;
    g->layout_type              =   &layout_type_sstriping;
    g->usable_cs                =   &usable_cs_sstriping;
    g->used_cs                  =   &used_cs_sstriping;
    g->init_zone_layout         =   &init_zone_layout_sstriping;
    g->resize_layout_zone       =   &resize_layout_zone_sstriping;
    g->r_offset_sbz             =   &r_offset_sbz_sstriping;
    g->create_and_write_sb_zone =   &create_and_write_sb_zone_sstriping;
    g->zone2rdev                =   &zone2rdev_sstriping;
    g->read_sbz_and_rebuild_lay =   &read_sbz_and_rebuild_lay_sstriping;
    g->calculate_capa_used      =   &calculate_capa_used_sstriping;
    return SUCCESS;
  }
  
  return ERROR; // nom de layout inconnu
}


//! donne le numéro du group de nom 'group_name' dans le tableau vs.groups. Renvoie SUCCESS/ERROR.
int get_group_nb(char *group_name, u32 *no_group) {
  u32 i;

  for (i=0; i<NBMAX_GROUPS; i++) {
    if (vs.groups[i] != NULL) {
      if (strcmp(vs.groups[i]->name, group_name) == 0) {
	*no_group = i; 
	return SUCCESS;
      }
    }
  }

  return ERROR;
}


//! vérifie si le virtualiseur ne gère pas déjà un groupe qui a ce nom. Attention : ce groupe peut exister dans le cluster du moment qu'il n'est pas pris en charge actuellement par le virtualiseur. Renvoie TRUE/FALSE
int gname_not_used(char *group_name) {
  u32 bidon;
  if (get_group_nb(group_name, &bidon) == ERROR) 
    return TRUE;
  else return FALSE;
}



//! création et init du group dans le virtualiseur. Renvoie SUCCESS/ERROR
int vrt_create_group(char *group_name, char *layout_name) {
  group_t *g;
  int retval;
  u32 no_new_group;
  
  if (vs.nb_groups == NBMAX_GROUPS) {
    PERROR("NB_MAX groups (%d) already activated\n", NBMAX_GROUPS);
    return ERROR;
  }

  if (gname_not_used(group_name) == FALSE) {
    PERROR("this virtualizer already manages a group named %s\n",
	   group_name);
    return ERROR;
  }

  g = vmalloc(sizeof(struct group));
  if (g == NULL) {
      PERROR("can't allocate a group struct\n");
      return ERROR;
    }
   
  get_random_bytes(g->uuid, 4*4);
  g->active = FALSE;
  g->nb_realdevs = 0;
  g->nb_zones = 0;
  strcpy(g->name,group_name);
  nullify_table((void *)g->realdevs, NBMAX_RDEVS);
  nullify_table((void *)g->zones, NBMAX_ZONES);
  g->dev_rep = NULL;
  g->proc_rep = NULL;
  g->proc_az = NULL;
  g->proc_iz = NULL;
  g->proc_uz = NULL;
  g->lay.sstriping = NULL; // raz de l'union (ie. raz layout qq soit type)
  
  retval = setup_gfunctions(g, layout_name);
  if (retval == ERROR) {
    PERROR("can't setup functions for group %s\n", group_name);
    goto cleanup_g;
  }
  no_new_group = unused_elt_indice((void *)vs.groups, NBMAX_GROUPS);
  vs.groups[no_new_group] = g;
  vs.nb_groups++;
  return SUCCESS;
  
  cleanup_g:
      vfree(g);   
      return ERROR;
}

extern int read_block_on_device(kdev_t device, 
				 void *block,
				 int block_size,
				 unsigned long long offset);

extern inline u64 r_offset_sbr(void);
extern inline u64 r_offset_sbg(void);
u64 offset_end_dev(u64 size_kb);


//! création et init (par lecture de son SBG sur le rdev) d'un ancien group. Renvoie SUCCESS/ERROR
int vrt_create_old_group(char *gname,
			 int rdev_major, 
			 int rdev_minor,
			 u64 rdev_size)  {
  group_t *g;
  int retval, i;
  u32 no_group;
  char layout_name[MAXSIZE_LAYNAME];

  PDEBUG("gname=%s, rdev: maj=%d min=%d sizeKB=%lld\n",
	 gname,rdev_major,rdev_minor,rdev_size);
  
  if (vs.nb_groups == NBMAX_GROUPS) {
    PERROR("NB_MAX groups (%d) already activated\n", NBMAX_GROUPS);
    return ERROR;
  }
  
  g = vmalloc(sizeof(struct group));
  if (g == NULL) {
      PERROR("can't allocate a group struct\n");
      return ERROR;
    }
   
  retval = read_block_on_device(MKDEV(rdev_major, rdev_minor),
				&sbg_aux,
				sizeof(struct sb_group),
				offset_end_dev(rdev_size) - r_offset_sbg());
  if (retval == ERROR) {
    PERROR("can't get SBG on rdev [%d,%d]\n", rdev_major, rdev_minor);
    return ERROR;
  }
  
  for (i=0; i<4; i++) g->uuid[i] = sbg_aux.uuid[i];
  PDEBUG("guuid=0x%x:%x:%x:%x\n",
	 g->uuid[3],g->uuid[2],g->uuid[1], g->uuid[0]);
  g->nb_realdevs =  sbg_aux.nb_rdevs;
  PDEBUG("nb realdevs = %d\n",  g->nb_realdevs);
  g->nb_zones = sbg_aux.nb_zones;
  PDEBUG("nb zones = %d\n",  g->nb_zones);
  if (strcmp(gname,sbg_aux.gname) != 0) {
    PERROR("group name read on SBG ='%s' is different from "
	   "group name given to virtualizer ='%s'\n",
	   sbg_aux.gname,
	   gname);
    return ERROR;
  }
	   
  strcpy(g->name,sbg_aux.gname);
  nullify_table((void *)g->realdevs, NBMAX_RDEVS);
  nullify_table((void *)g->zones, NBMAX_ZONES);
  for (i=0; i<NBMAX_ZONES; i++)
    if (sbg_aux.zone_exist[i] == TRUE) {
      g->zones[i]=ZONE_NON_INIT;
      PDEBUG("zone %d exist\n",i);
    }

  g->active = FALSE;
  g->dev_rep = NULL;
  g->lay.sstriping = NULL; // raz de l'union (ie. raz layout qq soit type)
  
  // TODO : conversion pas géniale, il faudrait rester avec le type 
  //        de layout sans passer par les strings
  if (laytype2layname(sbg_aux.layout, layout_name)
      == ERROR) {
    PERROR("can't convert layout type %d to a layout name\n",
	   sbg_aux.layout);
    goto cleanup_g;
  }
    

  retval = setup_gfunctions(g, layout_name);
  if (retval == ERROR) {
    PERROR("can't setup functions for group %s\n", sbg_aux.gname);
    goto cleanup_g;
  }
  no_group = unused_elt_indice((void *)vs.groups, NBMAX_GROUPS);
  vs.groups[no_group] = g;
  vs.nb_groups++;
  return SUCCESS;
  
  cleanup_g:
      vfree(g);   
      return ERROR;
}



//! renvoie la capacité de stockage (en Ko) du group passé en paramètre
u64 group_size_KB(group_t *g) {
  u32 i;
  u64 total_size = 0;

  for (i=0; i<NBMAX_RDEVS; i++)
    if (g->realdevs[i] != NULL) 
      total_size += g->realdevs[i]->size;
  
  return total_size;
}

//! libère la mémoire utilisée pour stocker les métadonnées du group 
void clean_group(group_t *g) {
  PDEBUG("cleaning group '%s'\n", g->name);
  (*(g->clean_layout))(g); 
  PDEBUG("layout cleaned\n");
  vfree_table((void *)g->realdevs, NBMAX_RDEVS);
  vfree_table((void *)g->zones, NBMAX_ZONES);
  PDEBUG("tables freed\n");
  clean_dev_and_proc(g);
  PDEBUG("group dev and proc rep/file unregistered\n");  
  vfree(g);
}

//! démarre un ancien group. Création des *storzone_t et de la layout du group et des zone, et du dev_rep du group. renvoie ERROR/SUCCESS.
int vrt_start_old_group(group_t *g) {
  PDEBUG("rebuilding layout of the zones of group '%s'\n", g->name);
  if ((*(g->read_sbz_and_rebuild_lay))(g) == ERROR) {
    PERROR("can't initialize the layout of group '%s'\n",
	   g->name);
    return ERROR;
  }
  
  PDEBUG("initializing layout of group '%s'\n", g->name);
  if ((*(g->init_layout))(g) == ERROR) {
    PERROR("can't initialize the layout of group '%s'\n",
	   g->name);
    return ERROR;
  }

  PDEBUG("calculating capa used on rdevs of group '%s'\n", g->name);
  (*(g->calculate_capa_used))(g); 

  PDEBUG("creating dev and proc files/rep for group '%s'\n", g->name);
  if (init_dev_and_proc(g) == ERROR) return ERROR;
  
  g->active = TRUE;
  return SUCCESS;
}

//! désactive le groupe, c'est-à-dire, libération layout + rep /dev. renvoie ERROR/SUCCESS
int vrt_stop_group(group_t *g, int no_group) {
  PDEBUG("desactivating all active zones on group '%s'\n", g->name);
  if (vrt_desactivate_allzones(g) == ERROR) {
    PERROR("can't desactivate all zones on group '%s'\n", g->name);
    return ERROR;
  }

  PDEBUG("desactivating group '%s'\n", g->name);
  clean_group(g); 

  vs.groups[no_group] = NULL;
  vs.nb_groups --;

  return SUCCESS;
}

#include "virtualiseur.h"
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include "vrt_ops.h"
#include "vrt_layout.h"
#include "vrt_superblock.h"
#include "vrt_divers.h"
#include <linux/random.h>


//! renvoie la CS disponible en KB sur le group "g" 
u64 cs_available(group_t *g) {
  return (*(g->usable_cs))(g) - (*(g->used_cs))(g);
}

//! le pointeur pointe sur une zone ? return TRUE/FALSE
int is_zone_ptr_valid(storzone_t *z) {
  if ((z != NULL) && (z != ZONE_NON_INIT)) 
    return TRUE;
  else return FALSE;
}

// NEW
//! crée une nouvelle zone et init sa layout
/*!
 */
int vrt_new_zone(group_t *g, u64 zsize_KB, char *zname) {
  int no_zone;
  u64 capa_dispo;
  storzone_t *z;
  
  // VERIF NB ZONES
  no_zone = unused_elt_indice((void **)g->zones, NBMAX_ZONES); 
  PDEBUG("zone size =%lld KB",zsize_KB);
  if (no_zone >= NBMAX_ZONES) {
    PERROR("can't create zone, maximum nb of zones (=%d) reached "
	   "for group '%s')\n",
	   NBMAX_ZONES,
	   g->name);
    return ERROR;
  }
  
  // VERIF CAPA DISPO
  capa_dispo = cs_available(g);     
  PDEBUG("capa_dispo %lld, taille zone = %lld\n",  capa_dispo, zsize_KB); 
  if (zsize_KB > capa_dispo) {
    PERROR("not enough CS available (=%lld KB)on group to create "
	   "the zone (size = %lld KB",
	   capa_dispo,
	   zsize_KB);
    return ERROR;
  }
  
  z = vmalloc(sizeof (struct storzone));
  if (z == NULL) {
    PERROR("can't allocate memory for storzone structure !\n");
    return ERROR;
  }
  
  z->g = g;
  strcpy(z->name, zname);
  z->active = FALSE;
  get_random_bytes(z->uuid, 4*4);
  z->size = zsize_KB;
  z->lay.sstriping = NULL;
  z->dev_file = NULL;
  z->minor = -1;
  z->used = 0;
  (*(g->init_zone_layout))(g,z); // init de la layout de la zone
  
  g->zones[no_zone] = z; 
  g->nb_zones++;
  return SUCCESS;
}


// NEW
//! donne le no de la zone de nom 'zname' dans le tableau g->zones[]. Renvoie SUCCESS/ERROR.
int get_zone_nb(group_t *g, char *zname, int *no_zone) {
  u32 i;

  for (i=0; i<NBMAX_ZONES; i++) {
    if (is_zone_ptr_valid(g->zones[i])) {
      if (strcmp(g->zones[i]->name, zname) == 0) {
	*no_zone = i; 
	return SUCCESS;
      }
    }
  }

  return ERROR;
}


//! crée le /dev/.. et inscrit le block device dans le noyau pourla zone "no_zone" (minor number). renvoie SUCCESS si tout s'est bien passé et ERROR sinon
int init_zone_block_device(storzone_t *z) {
  
  z->dev_file = devfs_register(z->g->dev_rep,
			       z->name,
			       DEVFS_FL_DEFAULT, 
			       VRT_MAJOR_NUMBER,
			       z->minor,
			       S_IFBLK | S_IRUSR | S_IWUSR,
			       &vrt_fops, 
			       NULL
			       );
  

  if (z->dev_file == NULL) {
    PERROR("can't create dev_file for zone %s:%s\n",
	   z->g->name,
	   z->name);
    return ERROR;
  }

  blk_size[VRT_MAJOR_NUMBER][z->minor] = z->size;
  blksize_size[VRT_MAJOR_NUMBER][z->minor] = VRT_BLKSIZE;
  hardsect_size[VRT_MAJOR_NUMBER][z->minor] = VRT_HARDSECT;
  max_readahead[VRT_MAJOR_NUMBER][z->minor] = VRT_READAHEAD;
  
  return SUCCESS;
}


//! active la zone de façon à ce qu'on puisse utiliser son stockage
int vrt_activate_zone(group_t *g, storzone_t *z) {
  int minor;

  // TO DO : peut-être un lock autour de la ligne suivante
  //         au cas où 2 appels quasi-simultanés
  minor = unused_elt_indice((void **)vs.minor2zone, NBMAX_ZONES); 
  
  if (minor >= NBMAX_ZONES) {
    PERROR("can't get a minor number, maximum nb of zones (=%d) reached ",
	   NBMAX_ZONES);
    return ERROR;
  }
  
  z->minor = minor;
  vs.minor2zone[minor] = z;
  
  z->used = 0;

  if (init_zone_block_device(z) == ERROR) {
    PERROR("can't create block device entry for zone %s:%s\n",
	   z->g->name,
	   z->name);
    return ERROR;
  }
  z->active = TRUE;

  return SUCCESS;
}
//! desactive la zone. Flush des blocs du caches, traitement des dernières requêtes, suppression de /dev, du minor number, et enfin desallocation de la layout et de la struct storzone
int vrt_desactivate_zone(group_t *g, storzone_t *z) {
  int no_zone;

  fsync_dev(MKDEV(VRT_MAJOR_NUMBER, z->minor));
  PDEBUG("device [%d,%d] for zone '%s:%s' flushed\n",
	 VRT_MAJOR_NUMBER, z->minor,
	 g->name, z->name);

  devfs_unregister(z->dev_file);
  PDEBUG("dev file of zone '%s:%s' unregistered\n",
	 g->name, z->name);

  vs.minor2zone[z->minor] = NULL;
  
  if (get_zone_nb(g,z->name, &no_zone) == ERROR) {
    PERROR("zone named '%s' unknown on group '%s'\n",
	   z->name, g->name);
    return ERROR;
  }
  
  z->active = FALSE; 
  z->minor = -1;
  
  return SUCCESS;
}

//!désactive toutes les zones actives du group 'g'
int vrt_desactivate_allzones(group_t *g) {
  int i;
  int retval = SUCCESS;

  for (i=0; i<NBMAX_ZONES; i++) 
    if (is_zone_ptr_valid(g->zones[i])) 
      if (g->zones[i]->active == TRUE)
	if (vrt_desactivate_zone(g,g->zones[i]) == ERROR) 
	  retval = ERROR;
  return retval;
}

//! destruction complète de la zone : désallocation de la layout, de la struct storzone et suppression des tables du groupe
int vrt_delete_zone(group_t *g, storzone_t *z) {
  int no_zone;

  (*(g->clean_layout_zone))(z);

  if (get_zone_nb(g,z->name, &no_zone) == ERROR) {
    PERROR("zone named '%s' unknown on group '%s'\n",
	   z->name, g->name);
    return ERROR;
  }
  
  g->zones[no_zone] = NULL; 
  g->nb_zones --;
  vfree(z);  
  return SUCCESS;
}

//! retaillage de la zone 'gname:zname'. Nouvelle taille = 'zsize'. return ERROR/SUCCESS. 
int vrt_resize_zone(group_t *g, storzone_t *z, u64 newsize) {
  
  // VERIF CAPA DISPO 
  if ( (newsize > z->size) &&  // on doit laisser ce test car u64 (unsigned) 
       (newsize - z->size > cs_available(g))) {
    PERROR("not enough CS available (=%lld KB) on group to resize "
	   "the zone from %lld KB to %lld KB\n",
	   cs_available(g),
	   z->size,
	   newsize);
    return ERROR;
  }

 
  if ((*(g->resize_layout_zone))(z, newsize) == ERROR) return ERROR;
 
  if (z->active == TRUE) 
    blk_size[VRT_MAJOR_NUMBER][z->minor] = newsize;
  
  z->size = newsize;

  return SUCCESS;
} 

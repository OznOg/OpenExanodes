#include <linux/vmalloc.h>
#include <asm/string.h>
#include <linux/random.h>
#include "virtualiseur.h"
#include "vrt_divers.h"

//! ajoute un rdev aux structures de données DRAM du virtualiseur. renvoie SUCCESS/ERROR
int vrt_add_new_rdev(group_t *g,
		      char *rdev_name, 
		      u64 rdev_size, 
		      u8 rdev_major, 
		      u8 rdev_minor) {
  
  realdev_t *rd;
  u32 new_rdev_indice;

  PDEBUG("%s size=%lld KB [%d,%d]\n",
	 rdev_name, rdev_size, rdev_major, rdev_minor);

  if (g->nb_realdevs >= NBMAX_RDEVS) {
    PERROR("can't add a new device. group %s has already "
	   " %d devices (>=NBMAX_RDEVS)\n",
	   g->name,
	   g->nb_realdevs);
    return ERROR;
  }

  if (rdev_size < g->min_rdev_size()) {
     PERROR("can't add the new device because it's too small: " 
	    "rdev size = %lld KB < min size = %lld KB\n",
	    rdev_size,
	    g->min_rdev_size());
     return ERROR;
  }
   
  rd = vmalloc(sizeof(struct realdev));
  if (rd == NULL) {
      PERROR("can't allocate a realdev struct\n");
      return ERROR;
  }
   
  rd->g = g;
  get_random_bytes(rd->uuid, 4*4);
  sscanf(rdev_name,"%s",rd->name); // string size control déjà effectué
  rd->size = rdev_size;
  rd->major = rdev_major;
  rd->minor = rdev_minor;
  rd->capa_used = 0;

  new_rdev_indice = unused_elt_indice((void **)g->realdevs, NBMAX_RDEVS);
  g->realdevs[new_rdev_indice] = rd;
  g->nb_realdevs++;

  return SUCCESS;
}

//! donne le n° du rdev dans le tableau readdevs du group 'g', en fonction des major et minor nb passés en param. Renvoie SUCCESS/ERROR.
int get_rdev_nb(group_t *g,
		u8 rdev_major, 
		u8 rdev_minor, 
		u32 *no_rdev) {
  u32 i;
  realdev_t *rd;
  
  for (i=0; i<NBMAX_RDEVS; i++) {
    rd = g->realdevs[i];
    if (rd != NULL) {
      if ((rd->major == rdev_major) && 
	  (rd->minor == rdev_minor)) {
	*no_rdev = i; 
	return SUCCESS;
      }
    }
  }

  return ERROR;
}

//! indique si un rdev ayant le major/minor (passés en param) est déjà utilisé par un des groupes du cluster.
int rdev_already_used(u8 rdev_major, 
		      u8 rdev_minor) {
  group_t *g;
  realdev_t *rd;
  int i,j;
      
  for (i=0; i<vs.nb_groups; i++) { 
    g =  get_elt(i, (void **)vs.groups, NBMAX_GROUPS);
    for (j=0; j<g->nb_realdevs; j++) {
      rd = get_elt(j, (void **)g->realdevs, NBMAX_RDEVS);
      if ((rd->major == rdev_major) &&
	  (rd->minor == rdev_minor)) 
	return TRUE;
    }
  }
  
  return FALSE;
}
    
//! ajoute un ancien rdev au group. Renvoie ERROR/SUCCESS. 
int vrt_add_old_rdev(group_t *g,
		     u32 *uuid_rdev,
		     char *rdev_name,
		     u64 rdev_size,
		     u8 rdev_major, 
		     u8 rdev_minor) {
  realdev_t *rd;
  u32 rdev_indice, i;
  
  rd = vmalloc(sizeof(struct realdev));
  if (rd == NULL) {
      PERROR("can't allocate a realdev struct\n");
      return ERROR;
  }
  
  rd->g = g;
  for (i=0; i<4; i++) rd->uuid[i] = uuid_rdev[i];
  sscanf(rdev_name,"%s",rd->name); // string size control déjà effectué
  rd->size = rdev_size;
  rd->major = rdev_major;
  rd->minor = rdev_minor;
  rd->capa_used = 0;

  rdev_indice = unused_elt_indice((void **)g->realdevs, NBMAX_RDEVS);
  g->realdevs[rdev_indice] = rd;
  return SUCCESS;
}

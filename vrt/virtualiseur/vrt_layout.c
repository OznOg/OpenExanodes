#include "virtualiseur.h"
#include <linux/vmalloc.h>
#include <math.h>
#include <linux/time.h>
#include "vrt_superblock.h"
#include "vrt_group.h"
#include "vrt_divers.h"
#include "vrt_zone.h"



extern int vrt_write_group(group_t *g);
extern int read_block_on_device(kdev_t device, 
			 void *block,
			 int block_size,
			 unsigned long long offset);
extern u64 r_offset_sbr(void);
extern int write_block_on_device(kdev_t device, 
				 void *block,
				 int block_size,
				 unsigned long long offset);
extern u64 offset_end_rdev(realdev_t *rd);

extern int write_block_on_all_rdevs(group_t *g, 
			    void *block, 
			    int block_size, 
			     u64 r_offset);

int layout_type_sstriping(void) {
  return SSTRIPING_LAYOUT;
}

char *layout_name_sstriping(void) {
  return SSTRIPING_NAME;
}

static u32 generate_date(void) {
  struct timeval maintenant;
  do_gettimeofday(&maintenant);
  return maintenant.tv_sec;
}

//! renvoie le nb d'UE par plages
inline long nb_ue_par_plage_sstriping(void) {
  return (long) ceil((1.0*VRT_PLAGESIZE) / VRT_UESIZE); 
}

//! renvoie la taille de la partie metadata (SB + LB) stockée par un rdev, en Ko
static u64 metadata_area_size_KB_sstriping(void) {
  u64 size;
  
  size = NBMAX_ZONES * VRT_SBSIZE; // SBZ
  size += VRT_SBSIZE *2; // SBG + SBR 
  size = quotient64(size,1024);
  return size + 1;
}

//! renvoie la taille min (en Ko) que doit avec un rdev (pour stocker les metadonnées + des données)
u64 min_rdev_size_sstriping(void) { 
  return 2*metadata_area_size_KB_sstriping();
}

//! libère la mémoire pour les métadonnées de la zone 'z'
void clean_layout_zone_sstriping(storzone_t *z) {
  z_sstriping_t *zl;
  
  zl = z->lay.sstriping;
  PDEBUG("freeing layout of zone '%s'\n",z->name);
  if (zl != NULL) {
    vfree(zl->plages);
    vfree(zl);
  }
}

// NEW
//! libère la mémoire des métadonnées du group "g"
void clean_layout_sstriping(group_t *g) {
  u32 i;
  g_sstriping_t *l = g->lay.sstriping;
  
  //PDEBUG("DEBUT : l=%p\n",l);  
  if (l!=NULL) {
    //PDEBUG("l->plages=%p\n",l->plages);  
    if (l->plages !=NULL) {
      PDEBUG("freeing %lld plages\n",l->nb_plages);
      for (i=0; i<l->nb_plages; i++) 
	if (l->plages[i] != NULL) vfree(l->plages[i]);
      
      vfree(l->plages);
    }

    // libération de la layout des zones
    for (i=0; i<NBMAX_ZONES; i++) {
      if (is_zone_ptr_valid(g->zones[i])) 
	clean_layout_zone_sstriping(g->zones[i]);
    }	  
  }
  //PDEBUG("FIN\n");
}

//! initialisation des tables qui servent au placement par striping simple (round robin). "taille" en UE
int init_plages_sstriping(group_t *g, g_sstriping_t *l) {
  u32 i,j;
  u64 usable_size;
  u64 taille_max,total_ue;
  u32 ue_par_plage;
  u64 nb_plages_max;
  u64 no_stripe_base;
  int taille_stripe;
  int petit_rdev;
  u64 no_plage;
   
  PDEBUG("gname=%s\n", g->name);
  
  usable_size = group_size_KB(g) 
    - metadata_area_size_KB_sstriping() * g->nb_realdevs;

  total_ue = quotient64(usable_size, VRT_UESIZE);
  //PDEBUG("total_ue = %lld\n",total_ue);

  ue_par_plage = nb_ue_par_plage_sstriping(); 
  //PDEBUG("ue_par_plage = %d\n",ue_par_plage);

  nb_plages_max = quotient64(total_ue,ue_par_plage);
  PDEBUG("nb_plages_max=%lld\n",nb_plages_max);


  // calcul de "sorted_devs"
  
  //PDEBUG("l=%p\n", l);
  //PDEBUG("l->sorted_devs=%p\n", l->sorted_devs);
  //PDEBUG("g->realdevs=%p g->nb_realdevs=%d\n",
  //	 g->realdevs, g->nb_realdevs);
  j=0;
  for (i=0; i<NBMAX_RDEVS; i++)
    if (g->realdevs[i] != NULL) {
      l->sorted_devs[j] = i;
      j++;
    }

  

  for (i=0; i <g->nb_realdevs-1; i++) {
    u64 min = g->realdevs[l->sorted_devs[i]]->size;
    u8 pos_min = i;
    u8 aux;

    for (j=i+1; j < g->nb_realdevs; j++)
      if (g->realdevs[l->sorted_devs[j]]->size < min) {
	min = g->realdevs[l->sorted_devs[j]]->size;
	pos_min = j;
      }
    
    aux = l->sorted_devs[i];
    l->sorted_devs[i] = l->sorted_devs[pos_min];
    l->sorted_devs[pos_min] = aux;
  }

  
  //for (i=0; i < g->nb_realdevs; i++) 
  //  PDEBUG("l->sorted_devs[%d]=%d (size = %lld)\n",
  //	   i,
  //	   l->sorted_devs[i],
  //	   g->realdevs[l->sorted_devs[i]]->size);
  
  // allocation mémoires pour le tableau des pages
  PDEBUG("vmalloc(%lld bytes)\n",
	 nb_plages_max*sizeof(struct plage_sstriping *));
  l->plages = vmalloc(nb_plages_max*sizeof(struct plage_sstriping *));
  if (l->plages == NULL) {
    PERROR("can't vmalloc plages table (size = %ld bytes)\n",
	  (long)(nb_plages_max*sizeof(struct plage_sstriping *))); 
    return ERROR;
  }
  for (i=0; i<nb_plages_max; i++) l->plages[i] = NULL;
    

  // créer la table des plages
  no_stripe_base = 0;
  taille_stripe = g->nb_realdevs;
  petit_rdev = 0;
  no_plage = 0;
  
  taille_max = quotient64(g->realdevs[l->sorted_devs[taille_stripe-1]]->size, 
			  VRT_UESIZE);

  //PDEBUG("creating plages table for stripe %lld to stripe %lld\n",
  //	 no_stripe_base,
  //	 taille_max);

  while (no_stripe_base < taille_max) {
    long nb_stripes_plage = (long) ceil((1.0*ue_par_plage)/taille_stripe);
    int ne_rentre_pas = 0;
    u64 endrdev_stripe;
    //PDEBUG("no_stripe_base=%lld < taille_max=%lld\n",
    //	   no_stripe_base,
    //	   taille_max);

    // est-ce que la plage rentre dans tous les devs ?
    // TESTME : avec des RAMdisk (pour avoir un bon échantillon de taille)
    endrdev_stripe = quotient64(g->realdevs[l->sorted_devs[petit_rdev]]->size,
				VRT_UESIZE);
    if (endrdev_stripe < no_stripe_base + nb_stripes_plage) {
      petit_rdev++;
      taille_stripe--;
      ne_rentre_pas = 1;
      //PDEBUG(" plage don't fit into all briques => changing stripe "
      //         "size (i.e. plage form). new size = %d (small rdev) "
      //	       " =%d)\n",
      //	     taille_stripe,
      //	     l->sorted_devs[petit_rdev]);   
    }
    
    if (ne_rentre_pas) {      
      no_stripe_base = quotient64(g->realdevs[l->sorted_devs[petit_rdev-1]]->size,
				  VRT_UESIZE);
    }
    else {
      // remplissage de la nouvelle plage
      l->plages[no_plage] = vmalloc(sizeof(struct plage_sstriping));
      if (l->plages[no_plage] == NULL) {
	PERROR("can't vmallocate memory for plage %lld\n", no_plage);
	l->nb_plages = no_plage;
	clean_layout_sstriping(g);
	return ERROR;
      }
      l->plages[no_plage]->no_stripe_debut = no_stripe_base;
      l->plages[no_plage]->no_stripe_fin = no_stripe_base 
	                                   + nb_stripes_plage -1;
      l->plages[no_plage]->free = TRUE;
      l->plages[no_plage]->largeur = taille_stripe;
      l->plages[no_plage]->hauteur = nb_stripes_plage;

      //PDEBUG("plage %lld stripes[%lld-%lld] l=%d h=%lld\n",
      //     no_plage,
      //     l->plages[no_plage]->no_stripe_debut,
      //     l->plages[no_plage]->no_stripe_fin,
      //     l->plages[no_plage]->largeur,
      //     l->plages[no_plage]->hauteur);   
      
      no_stripe_base += nb_stripes_plage;
      no_plage ++;
    }
  }

  l->nb_plages = no_plage;
  PDEBUG("%lld plages created\n",l->nb_plages);

  return SUCCESS;
}

//! renvoie un pointeur sur un realdev du group. renvoie NULL si il y a un pb
realdev_t *select_a_valid_rdev(group_t *g) {
  return get_elt(0, (void **)g->realdevs, NBMAX_RDEVS);
}

//! met à jour la table des plages occupées du groupe 'g', et lisant la table des plages utilisées par la zone 'no_zone'. Renvoie ERROR/SUCCESS.
int get_busy_plages_zone_sstriping(group_t *g,  int no_zone) {
  z_sstriping_t *lz;
  g_sstriping_t *lg;
  u32 i;

  lg = g->lay.sstriping;
  lz = g->zones[no_zone]->lay.sstriping;
  for (i=0; i<lz->nb_plages; i++) {
    PDEBUG("gplage %d isn't free\n",  lz->plages[i]);
    lg->plages[lz->plages[i]]->free = FALSE;
  }
    
  return SUCCESS;

}

// NEW
//! lit les LB des zones pour connaître les plages qui sont utilisées par des zones. Renvoie ERROR ou SUCCESS.
int get_busy_plages_sstriping(group_t *g) {
  u32 no_zone;
    
  PDEBUG("debut (gname=%s)\n",g->name);
  for (no_zone = 0; no_zone < NBMAX_ZONES; no_zone++) {
    if (is_zone_ptr_valid(g->zones[no_zone])) {
      PDEBUG("zone %d valide => scanning the layout)\n",no_zone);
      if (get_busy_plages_zone_sstriping(g, no_zone)== ERROR) return ERROR;      
    }
  }

  return SUCCESS;
}

// NEW
//! init les métadonnées du placement 
int init_layout_sstriping(group_t *g) {
  g_sstriping_t *l;
  int retval;

  l = vmalloc(sizeof(struct g_sstriping));
  if (l == NULL) {
    PERROR("can't allocate a g_sstriping struct for group %s\n", g->name);
    return ERROR;
  }
  l->nb_plages = 0;
  
  retval = init_plages_sstriping(g,l);
  if (retval == ERROR) {
      PERROR("can't init sstriping plages for group %s\n", g->name);
      return ERROR;
  }  
  
  g->lay.sstriping = l;

  retval = get_busy_plages_sstriping(g);  
  if (retval == ERROR) {
      PERROR("can't get busy plages for group %s\n", g->name);
      return ERROR;
  }  

  return SUCCESS;
}

// NEW
//! renvoie la quantité de CS du groupe, consommée pour stocker les données des zones.
u64 used_cs_sstriping(group_t *g) {
  int no_zone;
  u64 nb_plages_occupees = 0;
  
  for (no_zone = 0; no_zone < NBMAX_ZONES; no_zone++) 
    if (is_zone_ptr_valid(g->zones[no_zone]))
      nb_plages_occupees += g->zones[no_zone]->lay.sstriping->nb_plages;

  PDEBUG("%lld plages in group, %lld used plages => %lld KB used\n",
	 g->lay.sstriping->nb_plages,
	 nb_plages_occupees,
	 nb_plages_occupees * VRT_PLAGESIZE);

  return nb_plages_occupees * VRT_PLAGESIZE;     
}
  
// NEW
//! renvoie la CS du groupe   
u64 usable_cs_sstriping(group_t *g) {
  return g->lay.sstriping->nb_plages * VRT_PLAGESIZE;
}      

//NEW
//! init la layout de la zone (plages utilisées par la zone)
int init_zone_layout_sstriping(group_t *g, storzone_t *z) {
  
  u32 ue_par_plage = nb_ue_par_plage_sstriping();
  u32 nb_plages_zone = (u32) ceil((1.0*z->size) / (ue_par_plage*VRT_UESIZE));
  long i, j, no_plage, index;
  z_sstriping_t *lz;
  g_sstriping_t *lg = g->lay.sstriping;
  
  PDEBUG("init layout zone %s, size = %lld KB (=%d plages)\n",
	 z->name,
	 z->size,
	 nb_plages_zone);

  lz = vmalloc(sizeof(struct z_sstriping));
  if (lz == NULL) {
    PERROR("can't allocate a z_sstriping struct for zone '%s'\n", z->name);
    return ERROR;
  }

  lz->plages = vmalloc(nb_plages_zone * sizeof(u32));
  if (lz->plages == NULL) {
    PERROR("can't allocate memory for zone %s plages tables (%d bytes)\n",
	   z->name,
	   nb_plages_zone * sizeof(u32));
    return ERROR;
  }
  PDEBUG("memory for plages table for zone %s allocated\n",
	 z->name);

  no_plage = 0;
  for (i=0; i < lg->nb_plages && no_plage < nb_plages_zone; i++) {
    if (lg->plages[i]->free == TRUE) {
      lz->plages[no_plage] =  i;
      PDEBUG("adding plage %ld to zone %s (no plage=%ld)",
	     i, 
	     z->name, 
	     no_plage);
      lg->plages[i]->free = FALSE;
      no_plage++;

      // maj de l'occupation des rdevs
      index = g->nb_realdevs - lg->plages[i]->largeur;
      for (j=0; j < lg->plages[i]->largeur; j++) {
	g->realdevs[lg->sorted_devs[index + j]]->capa_used 
	  +=  lg->plages[i]->hauteur * VRT_UESIZE;
	PDEBUG("i = %ld, j= %ld index + j = %ld -> "
	       "cs_used = %lld hauteur = %lld KB\n", 
	       i,j,
	       index + j,
	       g->realdevs[lg->sorted_devs[index + j]]->capa_used,
	       lg->plages[i]->hauteur * VRT_UESIZE);
      }
    }
  }
  
  lz->nb_plages = no_plage;
  z->lay.sstriping = lz;
  PDEBUG("\n");

  return SUCCESS;
}

//! tranforme la layout de la zone 'z' pour prendre en compte sa nouvelle taille
int resize_layout_zone_sstriping(storzone_t *z, u64 newsize) {
  u32 ue_par_plage = nb_ue_par_plage_sstriping();
  u32 new_nb_plages_zone = (u32) ceil((1.0*newsize) / 
				      (ue_par_plage*VRT_UESIZE));
  long i, j, no_plage, index;
  z_sstriping_t *lz = z->lay.sstriping;
  g_sstriping_t *lg = z->g->lay.sstriping;
  u32 *plages;
  u32 min,  no_pl_aux;
  group_t *g = z->g;
  
  PDEBUG("resize layout zone %s, size = %lld KB (=%d plages)\n",
	 z->name,
	 newsize,
	 new_nb_plages_zone);

  // la newsize KB change trop peu pour nécessiter une
  // modif de la layout
  if (new_nb_plages_zone == lz->nb_plages) return SUCCESS;

  plages = vmalloc(new_nb_plages_zone * sizeof(u32));
  if (plages == NULL) {
    PERROR("can't allocate memory for zone %s new plages table (%d bytes)\n",
	   z->name,
	   new_nb_plages_zone * sizeof(u32));
    return ERROR;
  }
  PDEBUG("memory for new plages table for zone %s allocated\n",
	 z->name);

  // récupération des anciennes plages
  if (new_nb_plages_zone > lz->nb_plages) 
    min = lz->nb_plages;
  else min = new_nb_plages_zone;
  
  for (i=0; i<min; i++) plages[i] = lz->plages[i];
  
  if (new_nb_plages_zone > lz->nb_plages) {
    // affectation de nouvelles plages à la zone 
    no_plage = lz->nb_plages;
    for (i=0; i < lg->nb_plages && no_plage < new_nb_plages_zone; i++) {
      if (lg->plages[i]->free == TRUE) {
	plages[no_plage] =  i;
	PDEBUG("adding plage %ld to zone %s (no plage=%ld)",
	       i, 
	       z->name, 
	       no_plage);
	lg->plages[i]->free = FALSE;
	no_plage++;
	
	// maj de l'occupation des rdevs
	index = g->nb_realdevs - lg->plages[i]->largeur;
	for (j=0; j < lg->plages[i]->largeur; j++) {
	  g->realdevs[lg->sorted_devs[index + j]]->capa_used 
	    +=  lg->plages[i]->hauteur * VRT_UESIZE;
	  PDEBUG("i = %ld, j= %ld index + j = %ld -> "
		 "cs_used = %lld hauteur = %lld KB\n", 
		 i,j,
		 index + j,
	       g->realdevs[lg->sorted_devs[index + j]]->capa_used,
		 lg->plages[i]->hauteur * VRT_UESIZE);
	}
      }
    }
    new_nb_plages_zone = no_plage;
  } else {
    // libération des plages surnuméraires
    for (i=new_nb_plages_zone; i < lz->nb_plages; i++) {
      no_pl_aux = lz->plages[i];
      lg->plages[no_pl_aux]->free = TRUE;
 	
      // maj de l'occupation des rdevs
      index = g->nb_realdevs - lg->plages[no_pl_aux]->largeur;
      for (j=0; j < lg->plages[no_pl_aux]->largeur; j++) {
	g->realdevs[lg->sorted_devs[index + j]]->capa_used 
	  -=  lg->plages[no_pl_aux]->hauteur * VRT_UESIZE;
      }
    } 
  }

  vfree(lz->plages); // libération ancienne layout

  lz->nb_plages = new_nb_plages_zone;
  lz->plages = plages;

  PDEBUG("\n");

  return SUCCESS;  
}

// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// 
// Cette 3 fonction doit être synchronisée avec le fichier 
// vrt_superblock.h du répertoire "vrtadm"
// NEW
//! renvoie l'offset du SBZ de la zone d'indice 'indice_zone'
inline u64 r_offset_z_sstriping(int indice_zone) {
  return r_offset_sbr() + VRT_SBSIZE * (indice_zone +1);
}

// NEW
//! renvoie l'offset d'un superblock de zone sur un rdev. Renvoie -1 si pb.
u64 r_offset_sbz_sstriping(storzone_t *z) {
  int indice_zone = indice_elt(z, (void **)z->g->zones,NBMAX_ZONES); 
  if (indice_zone >= NBMAX_ZONES) {
    PERROR("can't get SBZ offset for zone %s\n", 
	   z->name);
    return -1;
  }
  
  return r_offset_z_sstriping(indice_zone);
}

//! genere la layout sstriping pour le SBZ de la zone "z"
void generate_sbz_sstriping(struct sb_zone_sstriping *sbz, 
			    storzone_t *z) {
  int i, etendue, plage_courante;
  z_sstriping_t *zl = z->lay.sstriping;

  sbz->magic_number = SBZ_SSTRIPING_MAGIC;
  sscanf(z->name,"%s",sbz->name);
  for (i=0; i<4; i++) sbz->zone_uuid[i] = z->uuid[i];
  sbz->create_time = generate_date();
  sbz->update_time = sbz->create_time;
  sbz->zone_size = z->size;
  PDEBUG("sbz->create_time = %d\n",sbz->create_time); 
  
  etendue = 0; 
  sbz->pl_start[etendue]= zl->plages[0];
  plage_courante = sbz->pl_start[etendue];
  PDEBUG("nb_plages de zone %s = %d\n",
	 z->name,
	 zl->nb_plages);
  for (i=1; i<zl->nb_plages; i++) {
    PDEBUG("plage_courante = %d  zl->plages[%d]= %d\n",
	   plage_courante,
	   i,
	   zl->plages[i]);

    if (zl->plages[i]  != plage_courante +1) {
      sbz->pl_end[etendue] = plage_courante;
      PDEBUG("ETENDUE %d : start =%d end=%d\n",
	     etendue,
	     sbz->pl_start[etendue],
	     sbz->pl_end[etendue]);

      etendue++;
      sbz->pl_start[etendue] = zl->plages[i];
    }
    plage_courante = zl->plages[i];
  }
  
  sbz->pl_end[etendue] = plage_courante;
  sbz->nb_etendues = etendue+1;

  PDEBUG("ETENDUE %d : start =%d end=%d\n",
	 etendue,
	 sbz->pl_start[etendue],
	 sbz->pl_end[etendue]);
  PDEBUG("%d étendues créées\n",sbz->nb_etendues);
}

struct sb_zone_sstriping sbz_sstriping;

//! génère le SBZ pour la zone "z" et les écrit sur le disque 
int create_and_write_sb_zone_sstriping(group_t *g, 
				       storzone_t *z) {
  int retval;
  u64 offset_sbz_r;
  
  generate_sbz_sstriping(&sbz_sstriping, z);
  
  offset_sbz_r = g->r_offset_sbz(z);
  if (offset_sbz_r == -1) {
    PERROR("can't get SBZ offset for zone %s\n", 
	   z->name);
    return ERROR;
  }
  
  retval  = write_block_on_all_rdevs(g,
				     (void *)&sbz_sstriping, 
				     sizeof(struct sb_zone_sstriping),
				     offset_sbz_r); 
  if (retval == ERROR) return ERROR;
  return SUCCESS;
}

//! reconstruit le struct storzone + layout de la zone passée en param, grâce à son SBZ lu précédemment sur le disque. Renvoie SUCCESS s'il y arrive et ERROR sinon.
int rebuild_zone_sstriping(storzone_t **z, 
			   struct sb_zone_sstriping *sbz,
			   group_t *g) {
  u32 nb_plages, i, j, no_plage;
  struct z_sstriping *zl;

  *z = vmalloc(sizeof(struct storzone));
  if (*z == NULL) {
    PERROR("can't allocate memory for storzone\n");
    return ERROR;
  }

  // TODO : il y a une redondance de code avec vrt_new_zone =>
  //        rassembler ces init de var dans une fonction si
  //        on veut éviter les futurs bugs dû à une erreur d'init
  (*z)->g = g;
  (*z)->active = FALSE;
  (*z)->used = 0;
  sscanf(sbz->name,"%s",(*z)->name);
  for (i=0; i<4; i++) (*z)->uuid[i] = sbz->zone_uuid[i];
  (*z)->size = sbz->zone_size;
  (*z)->minor = -1; // pas encore affecté car la zone est inactive
  (*z)->dev_file = NULL;

  (*z)->lay.sstriping = vmalloc(sizeof(struct z_sstriping));
  if ((*z)->lay.sstriping == NULL) {
    PERROR("can't allocate memory for storzone layout\n");
    return ERROR;
  }
  
  zl = (*z)->lay.sstriping;
  nb_plages = 0;
  for (i=0; i<sbz->nb_etendues; i++) 
    nb_plages += sbz->pl_end[i] - sbz->pl_start[i] + 1;
    
  zl->nb_plages = nb_plages;
  zl->plages = vmalloc(sizeof(u32)*nb_plages);
  if (zl->plages == NULL) {
    PERROR("can't allocate memory for storzone plages table\n");
    return ERROR;
  }
  
  no_plage = 0;
  for (i=0; i<sbz->nb_etendues; i++) 
    for (j=sbz->pl_start[i]; j<=sbz->pl_end[i]; j++) {
      zl->plages[no_plage] = j;
      no_plage ++;
    }
    
  return SUCCESS;
}

struct sb_zone_sstriping sbz_sstriping_aux;

//! lit tous les SBZ des zones existantes du groupe et recpnstitue les struct *storzone et les layout de ces zones
int read_sbz_and_rebuild_lay_sstriping(group_t *g) {
  int  i;
  realdev_t *rd = NULL;
  
  PDEBUG("selecting an rd\n");
  // choix d'un rdev du group 
  // (sur lequel on va lire les sbz)
  for (i=0; i<NBMAX_RDEVS; i++) 
    if (g->realdevs[i]!= NULL) {
      rd = g->realdevs[i];
      break;
    }
  
  if (rd == NULL) {
    PERROR("no rdev found in group '%s'\n", g->name);
    return ERROR;
  }

  PDEBUG("rd %s selected\n",rd->name);

  // g->zones contient ZONE_NON_INIT si la zone existe
  // (cf. vrt_create_old_group)
  for (i=0; i<NBMAX_ZONES; i++) {
    if (g->zones[i] == ZONE_NON_INIT) { 
      PDEBUG("reading SBZ of zone %d (=%p)\n",
	     i, g->zones[i]);
      if (read_block_on_device(MKDEV(rd->major,rd->minor), 
			       (void *)&sbz_sstriping_aux,
			       sizeof(struct sb_zone_sstriping),
			       offset_end_rdev(rd) - r_offset_z_sstriping(i)) 
	  == ERROR) {
	PERROR("can't read SBZ for zone ind=%d on group '%s' "
	       "on rdev '%s'",
	       i,
	       g->name,
	       rd->name);
	return ERROR;
      }
      
      PDEBUG("rebuilding zone[%d]=%p of group %s\n",
	     i, g->zones[i], g->name);
      if (rebuild_zone_sstriping(&(g->zones[i]), 
				 &sbz_sstriping_aux,
				 g) == ERROR) {
	PERROR("can't rebuild zone int=%d on group '%s'	on rdev '%s'"
	       " with SBZ",
	       i,
	       g->name,
	       rd->name);
	return ERROR;
      }  
      PDEBUG("zone %s (=%p) rebuilt\n",g->zones[i]->name, g->zones[i]); 
    }
  }
   
  return SUCCESS;
}

//! convertit une pos sur une zone en pos sur un rdev. Comme req appartient à 1 bloc, et que le mapping zone -> rdev se fait par bloc entier, pas de risque que req zone soit sur 2 rdev.
// TODO : vérifier très sérieusement ce que je dis ci-dessus. 
void zone2rdev_sstriping(storzone_t *z, 
			 unsigned long zsector, 
			 realdev_t **rd, 
			 unsigned long *rdsector) {
  u64 ue_zone, ue_rdev;
  z_sstriping_t *lz   = z->lay.sstriping;
  g_sstriping_t *lg   = z->g->lay.sstriping;
  u32 no_plage, no_ue_ds_plage; 
  int no_rdev_provisoire, index, nb_sect_ds_ue;
  
  //PDEBUG("z=%s, zsector =%ld\n",z->name, zsector);

  nb_sect_ds_ue       = (VRT_UESIZE * 1024) / VRT_HARDSECT;
  ue_zone             = quotient64(zsector,nb_sect_ds_ue);
  no_plage            = lz->plages[quotient64(ue_zone,
					      nb_ue_par_plage_sstriping())]; 
  no_ue_ds_plage      = reste64(ue_zone, nb_ue_par_plage_sstriping());   
  no_rdev_provisoire  = no_ue_ds_plage % lg->plages[no_plage]->largeur;
  index               = z->g->nb_realdevs - lg->plages[no_plage]->largeur;

  //PDEBUG("ue_zone=%lld no_plage=%d no_ue_dans_plage=%d "
  // "no_rdev_provisoire=%d index=%d\n",
  // ue_zone,
  // no_plage,
  // no_ue_ds_plage,
  // no_rdev_provisoire,
  // index); 

  *rd          = z->g->realdevs[lg->sorted_devs[index + no_rdev_provisoire]];
  ue_rdev      = lg->plages[no_plage]->no_stripe_debut 
                    + no_ue_ds_plage / lg->plages[no_plage]->largeur;
  *rdsector    = ue_rdev * nb_sect_ds_ue + reste64(zsector,nb_sect_ds_ue);
 

  //PDEBUG("rdev=%s ue_rdev=%lld rdsector=%ld\n",
  // (*rd)->name,
  // ue_rdev,
  // *rdsector); 
}

//! convertit un type de layout dans le nom de la layout. Renvoie ERROR si le type de layout ne correspond à rien et SUCCESS sinon.
int laytype2layname(u8 layout_type, char *layout_name) {
  switch(layout_type) {
  case SSTRIPING_LAYOUT:
    strcpy(layout_name, SSTRIPING_NAME);
    return SUCCESS;
  default:
    return ERROR;
  }
}

//! calcule la CS utilisée par les zones, sur chaque rdev du group passé en paramètre.
void calculate_capa_used_sstriping(group_t *g) {
  u64 i, j, index;
  g_sstriping_t *gl = g->lay.sstriping;

  for (i=0; i<g->nb_realdevs; i++) 
    g->realdevs[i]->capa_used = 0;

  // pour chaque plage utilisée dans le groupe, 
  // on ajoute à la capa_used de chaque rdev 
  // la part de stockage qu'elle utilise
  for (i=0; i<gl->nb_plages; i++) {
    if (gl->plages[i]->free == FALSE) {
      index = g->nb_realdevs - gl->plages[i]->largeur;
      for (j=0; j < gl->plages[i]->largeur; j++) {
	g->realdevs[gl->sorted_devs[index + j]]->capa_used 
	  +=  gl->plages[i]->hauteur * VRT_UESIZE;
      }
    }
  }
}

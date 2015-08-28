/*!\file smd_proc.c
\brief définit les fonctions associées à /proc/virtualiseur/zones_enregistrees
*/
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include "virtualiseur.h"
#include "vrt_ops.h"
#include "parser.h"
#include "vrt_request.h"
#include "vrt_zone.h"
#include "vrt_layout.h"
#include "vrt_superblock.h"
#include "constantes.h"
#include "vrt_group.h"
#include "vrt_rdev.h"
#include "vrt_divers.h"

struct proc_dir_entry *vrt_proc_zones;

#ifdef PERF_STATS
extern double somme_durees_aiguillages;
extern long nb_aiguillages;
#endif

//TODO: le code d'affichage d'info est à réécrire (c'est pourquoi je les mis en commentaires) pendant l'écriture des commandes d'admin en perl

//! gère la lecture du fichier /proc/virtualiseur/data_etat
int vrt_proc_read_data_etat(char *buf, 
			    char **start, 
			    off_t offset, 
			    int count, 
			    int *eof, 
			    void *data) {
  int len=0;
/*  int no_brique; */

/*   long nb_ue_usable =  nb_ue_on_ssh(); */
  
/*   len += sprintf(buf+len,"%ld %ld ", */
/* 		 (long)((1.0*nb_ue_usable)*VRT_UESIZE)/(1024*1024), */
/* 		 (((long)((1.0*nb_ue_usable)*VRT_UESIZE))%1048576)/1024); */
  
/*   for (no_brique = 0; no_brique < nb_briques; no_brique++)  */
/*     if (v_data.taille_brique[no_brique]) */
/*       len += sprintf(buf+len,"%d ", */
/* 		     (int) ((100.0*v_data.ue_used_brique[no_brique])/v_data.taille_brique[no_brique])); */
/*     else len += sprintf(buf+len,"%d ",0); */
  
  *eof=1;
  return len;
}

//! gère la lecture du fichier /proc/virtualiseur/data_briques
int vrt_proc_read_data_zones(char *buf, char **start, off_t offset, int count, int *eof, void *data) {
  int len=0;
/*   int no_zone; */
/*   long nb_ue_used; */
/*   long nb_ue_usable; */
  
/*  for (no_zone=0; no_zone < NB_MAX_ZONES; no_zone++) { */
/*    if (vrt_zone_active[no_zone] == TRUE) */
/*      len += sprintf(buf+len,"%d %ld %ld ", */
/* 		    no_zone, */
/* 		    (long) ((1.0*v_data.taille_zone[no_zone])*VRT_UESIZE)/(1024*1024), */
/* 		    (((long)((1.0*v_data.taille_zone[no_zone])*VRT_UESIZE))%1048576)/1024); */
/*  }	 */

/*  nb_ue_usable =  nb_ue_on_ssh(); */
/*  nb_ue_used =  nb_ue_usable - ue_available(); */

/*  len += sprintf(buf+len,"%ld %ld ", */
/* 		(long)((1.0*nb_ue_usable)*VRT_UESIZE)/(1024*1024), */
/* 		(((long)((1.0*nb_ue_usable)*VRT_UESIZE))%1048576)/1024); */

/*  len += sprintf(buf+len,"%ld %ld ", */
/* 		(long)((1.0*nb_ue_used)*VRT_UESIZE)/(1024*1024), */
/* 		(((long)((1.0*nb_ue_used)*VRT_UESIZE))%1048576)/1024); */
  
 *eof=1;
 return len;
}


//! fonction appelée lorsqu'une application lit /proc/virtualiseur/zones_enregistrees.
/*!
	Renvoie la liste des zones enregistrées (sous forme de texte) dans un buffer
	géré par le noyau.
*/
int vrt_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data) {
  int len=0;
  int i,j;

  len += sprintf(buf+len,"%d device group(s)\n\n", vs.nb_groups);
  for (i=0; i<vs.nb_groups; i++) { 
    group_t *g;
    g =  get_elt(i, (void **)vs.groups, NBMAX_GROUPS);
    len += sprintf(buf+len,"Group '%s' :\tUUID=0x%x:%x:%x:%x\n",
		   g->name,
		   g->uuid[3], g->uuid[2], g->uuid[1], g->uuid[0]);

    // ZONES du groupe
    if (g->nb_zones ==0) len += sprintf(buf+len,". No logical volume\n");
    else {
      storzone_t *z;

      len += sprintf(buf+len,". %d logical volume(s) :\n", g->nb_zones);
      for (j=0; j<NBMAX_ZONES; j++) {
	if (g->zones[j] != NULL) {
	  if (g->zones[j] == ZONE_NON_INIT) 
	    len += sprintf(buf+len,"  LV ind=%d exist but no info "
			           "loaded\n", i);
	  else {
	    z = g->zones[j]; 
	    len += sprintf(buf+len,"  %s [%d,%d] %lld KB ",
			   z->name,
			   VRT_MAJOR_NUMBER, z->minor,
			   z->size);
	    if (z->active == TRUE) {
	      len += sprintf(buf+len,"(ACTIVE) ");
	      if (z->used == 0) 
		len += sprintf(buf+len,"(NOT IN USE) ");
	      else len += sprintf(buf+len,"(IN USE : %d time(s)) ",z->used);
	    }
	    else len += sprintf(buf+len,"(INACTIVE) ");
	    len += sprintf(buf+len,"\tUUID=0x%x:%x:%x:%x\n",
			   z->uuid[3], z->uuid[2], z->uuid[1], z->uuid[0]);
	  }
	}
      }
    }

    // RDEVS du groupe
    if (g->nb_realdevs ==0) len += sprintf(buf+len,". No real devices\n");
    else {
      realdev_t *rd;
      
      len += sprintf(buf+len,". %d real device(s)\n", g->nb_realdevs);
      for (j=0; j<g->nb_realdevs; j++) {
	rd = get_elt(j, (void **)g->realdevs, NBMAX_RDEVS);
	len += sprintf(buf+len,"  %s [%d,%d] %lld KB ",
		       rd->name, 
		       rd->major, 
		       rd->minor, 
		       rd->size);
	len += sprintf(buf+len,"\tUUID=0x%x:%x:%x:%x\n",
		       rd->uuid[3], rd->uuid[2], rd->uuid[1], rd->uuid[0]);
      }
    }
    len += sprintf(buf+len,"\n");
  }
  
/*   int no_zone; */
/*  int limite = count - 80; */
/*  int no_brique; */

/*  PDEBUG("VRT - vrt_read_proc : Entering\n"); */
/*  if (groupe_actif == TRUE) { */
/*    len += sprintf(buf+len,"Data layout = %s\n",vrt_lname()); */
/*    len += sprintf(buf+len,"\n%d zone(s) active(s) (%ld Go %ld Mo)\n" */
/* 		          "-------------------------------------------\n", */
/* 		  nb_activated_zones(), */
/* 		  (long) ((1.0*total_size_of_zones())*VRT_UESIZE)/(1024*1024), */
/* 		  (((long)((1.0*total_size_of_zones())*VRT_UESIZE))%1048576)/1024); */

/*    for (no_zone=0; no_zone < NB_MAX_ZONES && len <= limite; no_zone++) { */
/*      if (vrt_zone_active[no_zone] == TRUE) */
/*        // len += sprintf(buf+len,"zone %d [%c] UUID 0x%x:%x:%x:%x, %ld UE (%ld Mo)\n", */
/*  // 	 no_zone, */
/*  // 	 v_data.type_zone[no_zone], */
/*  // 	 v_data.z_uuid[no_zone][3], */
/*  // 	 v_data.z_uuid[no_zone][2], */
/*  // 	 v_data.z_uuid[no_zone][1], */
/*  // 	 v_data.z_uuid[no_zone][0], */
/*  // 	 v_data.taille_zone[no_zone], */
/*  // 	 (long int)((v_data.taille_zone[no_zone]*VRT_UESIZE)/1024.0)); */
       
/*        len += sprintf(buf+len,"zone %d [%c] %ld UE (%ld Mo)\n", */
/* 		      no_zone, */
/* 		      v_data.type_zone[no_zone], */
/* 		      v_data.taille_zone[no_zone], */
/* 		      (long int)((v_data.taille_zone[no_zone]*VRT_UESIZE)/1024.0)); */
     
/*    } */
   
   
/*    len += sprintf(buf+len,"\n%ld brique(s) enregistrée(s) (%ld Go %ld Mo)\n-------------------------------------------\n", */
/* 		  nb_briques, */
/* 		  (long) ((1.0*vrt_storage_size())*VRT_UESIZE)/(1024*1024), */
/* 		  (((long)((1.0*vrt_storage_size())*VRT_UESIZE))%1048576)/1024); */
   
/*    for (no_brique=0; no_brique < nb_briques; no_brique++) { */
/*      len += sprintf(buf+len,"brique %d : UUID 0x%x:%x:%x:%x\n", */
/* 		    no_brique, */
/* 		    v_data.br_uuid[no_brique][3], */
/* 		    v_data.br_uuid[no_brique][2], */
/* 		    v_data.br_uuid[no_brique][1], */
/* 		    v_data.br_uuid[no_brique][0]); */
     
/*      len += sprintf(buf+len,"\ttaille = %ld UE (%ld Mo). %ld UE used by zones\n", */
		    
/* 		    v_data.taille_brique[no_brique], */
/* 		    (long int)((v_data.taille_brique[no_brique]*VRT_UESIZE)/1024.0), */
/* 		    v_data.ue_used_brique[no_brique]); */
/*    } */
   
/*    // enlever les commentaires pour afficher le mapping UEs zones -> UEs briques dans le log kernel */
/*    //if (vrt_layout == REFLEX) {  */
/*    //  for (no_zone=0; no_zone< NB_ZONES_MAX && len <= limite; no_zone++) { */
/*    //  long no_ue; */
/*    //  if (vrt_zone_active[no_zone] == TRUE){ */
/*    //    PDEBUG("VRT - Affichage placement de la zone %d\n",no_zone); */
/*    //      for (no_ue=0; no_ue<v_data.taille_zone[no_zone]; no_ue++) { */
/*    //      if (v_data.zone_vers_brique[no_zone][no_ue]!= NB_MAX_BRIQUES)  */
/*    //      PDEBUG("%ld -> B%d, UE%d\n", */
/*    //      no_ue, */
/*    //     v_data.zone_vers_brique[no_zone][no_ue],  */
/*    //      v_data.zone_vers_ue[no_zone][no_ue]); */
/*    //      } */
/*    //      PDEBUG("\n\n"); */
/*    //      } */
/*    //      } */
/*    //        */
/*    if (vrt_storage_size()!=0) { */
/*      len += sprintf(buf+len,"\nBriques space: real = %ld UE, usable = %ld UE. %d%% available (%ld UE)\n", */
/* 		    vrt_storage_size(), */
/* 		    nb_ue_on_ssh(), */
/* 		    (int)((100.0*ue_available())/nb_ue_on_ssh()), */
/* 		    ue_available()); */
/*    } else len += sprintf(buf+len,"\nNo storage managed by the virtualiseur\n"); */
   
/*    if (total_size_of_zones()!=0) { */
/*      int wasted = (int) ((100.0*(nb_ue_on_ssh() - ue_available() - total_size_of_zones())) / (nb_ue_on_ssh() - ue_available())); */
/*      if (wasted < 0) wasted = 0; // car pour REFLEX qui place les UE à la volée : on a un wasted très très < à 0 */
/*      len += sprintf(buf+len,"Zone space: %ld UE. %ld UE really used (%d%% wasted)",  */
/* 		    total_size_of_zones(), */
/* 		    nb_ue_on_ssh() - ue_available(), */
/* 		    (int) ((100.0*(nb_ue_on_ssh() - ue_available() - total_size_of_zones()))  */
/* 			   / (nb_ue_on_ssh() - ue_available()))); */
/*    } */

   
/* #ifdef PERF_STATS */
/*    if (nb_aiguillages!=0) { */
/*      len += sprintf(buf+len,"\n%ld switchings from zones to briques, in %lld microsec (%ld microsec mean time)\n", */
/* 		    nb_aiguillages, */
/* 		    (long long)somme_durees_aiguillages, */
/* 		(long)somme_durees_aiguillages/nb_aiguillages); */
/*    } */
/* #endif */
/*  }  */
/*  else len += sprintf(buf+len,"\t GROUPE INACTIF\n"); */
 


 *eof=1;
 
 PDEBUG("VRT - vrt_read_proc : Completed\n");

 return len;
}

extern struct sb_brique sbb_aux;
extern struct sb_zone sbz_aux;

//! fonction appelée lorsqu'une application écrit dans /proc/virtualiseur/zones_enregistrees.
/*!
	  Par exemple : echo "10" > /proc/virtualiseur/zones_enregistrees
	  Indique qu'on crée une zone de taille 10 UE
*/
int vrt_write_proc(struct file *file, 
		   const char *buffer, 
		   unsigned long count, 
		   void *data) {
  //u64 taille_ko;
  //int no_zone;
  
  char ligne_de_configuration[TAILLE_LIGNE_CONF];
  char commande[10];
  char dev_name[MAXSIZE_DEVNAME];
  u64 dev_size;
  //int no_brique;
  //int error;
  //int brique_ok;

  //u32 uuid_zone[4];
  char gname[MAXSIZE_GROUPNAME];
  char lname[MAXSIZE_LAYNAME];
  char zname[MAXSIZE_ZONENAME];
  u32  no_group, no_zone;
  u32 rdev_major, rdev_minor;
  u32 uuid_rdev[4];
  u64 zsize;
  int retval;
  group_t *g;

  
  copy_from_user(ligne_de_configuration, buffer, count);
  ligne_de_configuration[count]=0; // pour fermer la chaine de car
  PDEBUG("ligne de configuration received = <%s>\n", 
	 ligne_de_configuration);
    
  Extraire_mot(ligne_de_configuration, commande);
  
  switch(commande[0]) {
  case 'w':
    // COMMANDES D'ECRITURE DES SUPERBLOCKS
    switch(commande[1]) {
    case 'a':
      // INIT ou MAJ DE L'ENSEMBLES DES SUPERBLOCKS SUR LES RDEVS DU GROUPE

      PDEBUG("write group metadata. ligne de conf=<%s>\n",
	     ligne_de_configuration);
      
      retval = sscanf(ligne_de_configuration,"%s",
		      gname);
      
      if (retval != 1) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group name %s isn't currently managed by ze virtualizer\n",
	       gname);
	return count;
      }
      
      if (vrt_write_group(vs.groups[no_group]) == ERROR) {
	PERROR("can't write all superblocks on rdevs for ze group %s\n",
	       gname);
	return count;
      }
      break;
    case 'g':
      // INIT ou MAJ du superblock SBG
      PDEBUG("write group metadata. ligne de conf=<%s>\n",
	     ligne_de_configuration);
      
      retval = sscanf(ligne_de_configuration,"%s",
		      gname);
      
      if (retval != 1) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group name %s isn't currently managed by ze virtualizer\n",
	       gname);
	return count;
      }
      
      if (vrt_write_SBG(vs.groups[no_group]) == ERROR) {
	PERROR("can't write SBG on rdevs for ze group %s\n",
	       gname);
	return count;
      }
      break;
      
    }
    break;
  case 'g':
    // GROUP COMMAND
    switch(commande[1]) {
    case 'c':
      // CREATION D'UN GROUPE
      
      PDEBUG("Creation device group.ligne de conf=<%s>\n",
	     ligne_de_configuration); 

      retval = sscanf(ligne_de_configuration,"%s %s",
		      gname,
		      lname);
      
      if (retval != 2) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (gname_not_used(gname)) {
	if (vrt_create_group(gname, lname) == ERROR) {
	  PERROR("can't create group %s\n", gname);
	  return count;
	}
      } else {
	PERROR("a group named '%s' already in use\n", gname);
	return count;
      }
	
   
      break;
    case 'n': // AJOUTE UN "NEW" REAL DEVICE A UN GROUPE
      PDEBUG("add new rdev. ligne de conf=<%s>\n",
	     ligne_de_configuration);
      retval =sscanf(ligne_de_configuration,"%s %s %Ld %d %d",
		     gname,
		     dev_name,
		     &dev_size,
		     &rdev_major,
		     &rdev_minor);
    
      if (retval != 5) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }


      PDEBUG("%s [%d,%d] %lld\n",
	     dev_name,
	     rdev_major,
	     rdev_minor,
	     dev_size);

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group name %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }

      if (rdev_already_used(rdev_major, rdev_minor)) {
 	PERROR("realdev with identical major/minor [%d,%d] already "
	       "managed by the virtualizer\n",
	       rdev_major,  rdev_minor);
	return count;
      }


      if (vrt_add_new_rdev(vs.groups[no_group],
			   dev_name, 
			   dev_size, 
			   rdev_major, 
			   rdev_minor) == ERROR) {
	PERROR("can't add new rdev %s to group %s\n", dev_name, gname);
	return count;
      }
	
      break;

    case 's': // ACTIVE LE GROUPE : ON PEUT Y ACTIVER DES ZONES

      PDEBUG("activate group. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,"%s",
		      gname);

      if (retval != 1) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

     if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named '%s' isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }

     if (vs.groups[no_group]->active == FALSE) {
       if (vrt_start_group(vs.groups[no_group]) == ERROR) {
	 PERROR("can't start ze group '%s'\n", gname);
	 return count;
       }
     }
     else {
 	PERROR("group named '%s' already active\n",gname);
	return count;
     }
       
     break;
    case 'k': // DESACTIVER LE GROUPE

      PDEBUG("desactivate group. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,"%s",
		      gname);

      if (retval != 1) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }
  
     if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named '%s' isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }

     if (vs.groups[no_group]->active) {
       if (vrt_stop_group(vs.groups[no_group], no_group) == ERROR) {
	 PERROR("can't desactivate ze group '%s'\n", gname);
	 return count;
       }
     }
     else {
 	PERROR("group named '%s' isn't active\n",gname);
	return count;
     }
      
     break;

    case 'l': // CREER UN ANCIEN GROUPE

      PDEBUG("create old group. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,"%s %d %d %Ld",
		      gname, &rdev_major, &rdev_minor, &dev_size);

      if (retval != 4) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }


      if (gname_not_used(gname)) {
	if (vrt_create_old_group(gname, rdev_major, rdev_minor, dev_size) 
	    == ERROR) {
	  PERROR("can't start ze group '%s'\n", gname);
	  return count;
	}
      } else {
	PERROR("a group named '%s' already in use\n", gname);
	return count;
      }
      break;
    case 'o': // INTEGRER UN ANCIEN RDEV DANS LE GROUPE
      PDEBUG("add old rdev. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,
		      "%s %u %u %u %u %s %Ld %d %d",
		      gname, 
		      &(uuid_rdev[3]),
		      &(uuid_rdev[2]),
		      &(uuid_rdev[1]),
		      &(uuid_rdev[0]),
		      dev_name,
		      &dev_size,
		      &rdev_major, 
		      &rdev_minor);

      if (retval != 9) {
	PERROR("can't parse correctly the command line : "
	       "error on arg %d\n", retval);
	return count;
      }
      
      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group name %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }

      if (vrt_add_old_rdev(vs.groups[no_group],
			   uuid_rdev,
			   dev_name,
			   dev_size,
			   rdev_major, 
			   rdev_minor) 
	  == ERROR) {
	PERROR("can't start ze group %s\n", gname);
	return count;
      }
      break;
    case 'z': // DEMARRER un ancien groupe
      PDEBUG("start old group. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration, "%s ", gname);

      if (retval != 1) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }
      
      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group name %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }
      if (vs.groups[no_group]->active == FALSE) {
	if (vrt_start_old_group(vs.groups[no_group]) 
	    == ERROR) {
	  PERROR("can't start ze old group %s\n", gname);
	  return count;
	}
      }
      else {
 	PERROR("group name %s already active\n",gname);
	return count;
      }
      break;      
    default: // COMMANDE DE GROUPE INCONNUE
 	PERROR("unknown group command : %s\n",commande);
	return count;     
    }    
    break;
  case 'z': // COMMANDE POUR LES ZONES;
    switch(commande[1]) {
    case 'c': // CREE LA ZONE
      PDEBUG("Create zone. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,
		      "%s %s %Ld",
		      gname,
		      zname,
		      &zsize);

      if (retval != 3) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named '%s' isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }

      PDEBUG("%s %d\n",gname, no_group);

      if (get_zone_nb(vs.groups[no_group],zname, &no_zone) == SUCCESS) {
 	PERROR("zone named '%s' already managed by group '%s'\n",
	       zname, gname);
	return count;
      }

      if (vrt_new_zone(vs.groups[no_group],zsize,zname)== ERROR) {
	PERROR("can't create zone %s:%s\n", gname, zname);
	return count;
      }
      break;
    case 's': // ACTIVER LA ZONE
      PDEBUG("Start zone. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,
		      "%s %s",
		      gname,
		      zname);

      if (retval != 2) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }
      PDEBUG("%s %d\n",gname, no_group);
      g = vs.groups[no_group];
      if (get_zone_nb(g,zname, &no_zone) == ERROR) {
 	PERROR("zone named '%s' unknown on group '%s'\n",
	       zname, gname);
	return count;
      }
      
      if (g->zones[no_zone]->active == FALSE) {
	if (vrt_activate_zone(g,g->zones[no_zone]) == ERROR) {
	  PERROR("can't activate zone %s:%s\n", gname, zname);
	  return count;
	}
      } else {
	PERROR("zone %s:%s already active\n", gname, zname);
	return count;
      }
      break;
    case 'k': // DESACTIVER LA ZONE
      PDEBUG("Stop zone. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,
		      "%s %s",
		      gname,
		      zname);

      if (retval != 2) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }
      PDEBUG("%s %d\n",gname, no_group);

      g = vs.groups[no_group];
      if (get_zone_nb(g,zname, &no_zone) == ERROR) {
 	PERROR("zone named %s unknown on group %s\n",
	       zname, gname);
	return count;
      }
      
      if (g->zones[no_zone]->active) {
	if (vrt_desactivate_zone(g,g->zones[no_zone]) == ERROR) {
	  PERROR("can't desactivate zone %s:%s\n", gname, zname);
	  return count;
	}
      }
      else {
	PERROR("zone %s:%s isn't active\n", gname, zname);
	return count;
      }
      break;
     
    case 'r': // RETAILLER LA ZONE
      PDEBUG("Resize zone. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,
		      "%s %s %Ld",
		      gname,
		      zname,
		      &zsize);

      if (retval != 3) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }
      PDEBUG("%s %d\n",gname, no_group);

      g = vs.groups[no_group];
      if (get_zone_nb(g,zname, &no_zone) == ERROR) {
 	PERROR("zone named %s unknown on group %s\n",
	       zname, gname);
	return count;
      }
      
      if (vrt_resize_zone(g,g->zones[no_zone], zsize) == ERROR) {
	PERROR("can't resize zone %s:%s to %lld KB\n", gname, zname, zsize);
       return count;
      }
      break;

    case 'd': // DETRUIRE LA ZONE
      PDEBUG("Delete zone. ligne de conf=<%s>\n",ligne_de_configuration);
      retval = sscanf(ligne_de_configuration,
		      "%s %s",
		      gname,
		      zname);

      if (retval != 2) {
	PERROR("can't parse correctly the command line\n");
	return count;
      }

      if (get_group_nb(gname, &no_group) == ERROR) {
	PERROR("group named %s isn't currently manage by ze virtualizer\n",
	       gname);
	return count;
      }
      PDEBUG("%s %d\n",gname, no_group);

      g = vs.groups[no_group];
      if (get_zone_nb(g,zname, &no_zone) == ERROR) {
 	PERROR("zone named %s unknown on group %s\n",
	       zname, gname);
	return count;
      }
      
      if (vrt_delete_zone(g,g->zones[no_zone]) == ERROR) {
	PERROR("can't desactivate zone %s:%s\n", gname, zname);
       return count;
      }
      break;
     
    default: // COMMANDE DE ZONE INCONNUE
 	PERROR("unknown zone command : %s\n",commande);
	return count;     
    }    
    break;

  default :
    printk(KERN_ERR "VRT - vrt_write_proc : bad command\n");
  }
 
  PDEBUG("end\n");
  return count;
}


//! affiche les zones actives du group passé dans les data
int vrt_read_active_zones(char *buf, 
			  char **start, 
			  off_t offset, 
			  int count, 
			  int *eof, 
			  void *data) {
  int len=0, i, no_group;
  char *gname;
  group_t *g;
  
  gname = data;
  if (get_group_nb(gname, &no_group) == ERROR) {
    PERROR("group named %s isn't currently manage by ze virtualizer\n",
	   gname);
    goto end;
  }

  g = vs.groups[no_group];
  
  for (i=0; i<NBMAX_ZONES; i++) 
    if (is_zone_ptr_valid(g->zones[i]))
      if (g->zones[i]->active == TRUE)
	len += sprintf(buf+len,"%s ",g->zones[i]->name);
	
  len += sprintf(buf+len,"\n");
  
 end:
  *eof=1;
  return len;
}

//! affiche les zones oisives du group passé dans les data
int vrt_read_idling_zones(char *buf, 
			  char **start, 
			  off_t offset, 
			  int count, 
			  int *eof, 
			  void *data) {
  int len=0, i, no_group;
  char *gname;
  group_t *g;
  
  gname = data;
  if (get_group_nb(gname, &no_group) == ERROR) {
    PERROR("group named %s isn't currently manage by ze virtualizer\n",
	   gname);
    goto end;
  }

  g = vs.groups[no_group];
  
  for (i=0; i<NBMAX_ZONES; i++) 
    if (is_zone_ptr_valid(g->zones[i]))
       if (g->zones[i]->active == FALSE)
	 len += sprintf(buf+len,"%s ",g->zones[i]->name);
	
  len += sprintf(buf+len,"\n");
  
 end:
  *eof=1;
  return len;
} 
//! affiche les zones en cours d'utilisation (i.e. ayant plus d'open que de close)  du group passé dans les data
int vrt_read_used_zones(char *buf, 
			char **start, 
			off_t offset, 
			int count, 
			int *eof, 
			void *data) {
  int len=0, i, no_group;
  char *gname;
  group_t *g;
  
  gname = data;
  if (get_group_nb(gname, &no_group) == ERROR) {
    PERROR("group named %s isn't currently manage by ze virtualizer\n",
	   gname);
    goto end;
  }

  g = vs.groups[no_group];
  
  for (i=0; i<NBMAX_ZONES; i++) 
    if (is_zone_ptr_valid(g->zones[i]))
       if (g->zones[i]->used > 0)
	 len += sprintf(buf+len,"%s ",g->zones[i]->name);
	
  len += sprintf(buf+len,"\n");
  
 end:
  *eof=1;
  return len;
} 

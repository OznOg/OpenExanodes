#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <asm/types.h>
#include "vrt_superblock.h"
#include "uuid.h"
//#include "constantes.h"
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>

#define LINE_SIZE 256 // taille d'une ligne de texte (pour commande, etc..)

struct sb_group sbg_aux;
struct sb_rdevs sbr_aux;

// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// 
// Ce fichier doit être synchronisé avec le fichier 
// vrt_superblock.h du répertoire "vrtadm"
//! renvoie l'offset d'un superblock de group sur un rdev
inline __u64 r_offset_sbg(void) {
  return 0;
}

//! renvoie l'offset d'un superblock de group sur un rdev
inline __u64 r_offset_sbr(void) {
  return r_offset_sbg() + VRT_SBSIZE;
}

//! renvoie l'offset de la fin "arrondie" du rdev
/*! la fin "arrondie" d'un rdev est situé à la fin 
  du dernier bloc de 32 Ko entier.
  LIMITE TAILLE brique : 2^64 octets = 16 Exaoctets
*/
inline __u64 offset_end_rdev(__u64 size_kb /*en Ko */) {
  __u64 offset_kb;  
  if (size_kb % 32 == 0) 
    offset_kb = size_kb - 32;
  else 
    offset_kb = 32 * (size_kb / 32 - 1); // (size_kb / 32) permet de faire un arrondi (car arrondi "classique" avec u64 pose des pbs à l'insmod)

  return offset_kb * 1024;
} 
 
//! renvoie l'offset du SBZ de la zone d'indice 'indice_zone'
inline __u64 r_offset_z_sstriping(int indice_zone) {
  return r_offset_sbr() + VRT_SBSIZE * (indice_zone +1);
}

/// FIN SYNCHRO





//! extrait le major number et le minor number d'une struct stat
void get_major_minor(struct stat *info, int *major, int *minor) {
  *major = info->st_rdev >> 8;
  *minor = info->st_rdev & 0xff;
}

//! renvoie le nombre de kilo octets qu'on peut stocker sur le device
int get_nb_kbytes(char *nom_de_device, __u64 *nb_kbytes) {
  
  __u64 nb_sectors=-1;
  __u32 nb_sectors32 = -1;
  int sector_size=-1;
  int retval;
  int fd;

  if ((fd = open(nom_de_device,O_RDWR)) <0) {
    perror("open");
    return EXIT_FAILURE;
  }

  // TODO : 
  // BLKGETSIZE64 n'est pas défini dans les include
  // => pourquoi ?
  /*
  retval = ioctl(fd,BLKGETSIZE64,&nb_sectors);
  if (retval <0) {
    __u32 aux;
    perror("ioctl BLKGETSIZE64 failed. Trying ioctl BLKGETSIZE");
    retval = ioctl(fd,BLKGETSIZE,&aux);
    if (retval <0) {
      perror("ioctl BLKGETSIZE");
      return EXIT_FAILURE;
    }

    nb_sectors = aux;
    }*/

  retval = ioctl(fd,BLKGETSIZE,&nb_sectors32);
  if (retval <0) {
    perror("ioctl BLKGETSIZE");
    return EXIT_FAILURE;
  }
  nb_sectors = nb_sectors32;
  
  retval = ioctl(fd,BLKSSZGET,&sector_size);
  if (retval <0) {
    perror("ioctl BLKSSZGET");
    return EXIT_FAILURE; 
  }
 
  if (close(fd) <0) {
    perror("close");
    return EXIT_FAILURE;
  }
  

  /*printf("nb sect32=%d nb_sect64 = %lld size sect=%d kb_size=%lld\n", 
	 nb_sectors32,
	 nb_sectors,
	 sector_size,
	 (__u64) (floor((nb_sectors/1024.0)*sector_size)));
  */
  *nb_kbytes = (__u64) (floor((nb_sectors/1024.0)*sector_size));
  return EXIT_SUCCESS;
}


//! lit ou écrit un bloc sur le device (renvoie EXIT_FAILURE ou SUCCESS)
int access_block(int cmd, char *device_name, __u64 offset, void* data, 
		 int data_size) {
  int fd;
  __u64 pos_io;
  int nb_bytes;
  
  if ((cmd != READ) && (cmd !=WRITE))
    return EXIT_FAILURE;

  //printf("accessing block on device %s. cmd=%d (0 = READ, 1 = WRITE) size=%d bytes, offset %lld\n",
  // device_name,
  // cmd,
  // data_size,
  // offset);

  if ((fd = open(device_name,O_RDWR | O_LARGEFILE))<0) return EXIT_FAILURE;
 
  pos_io = lseek64(fd,offset,SEEK_SET);
 
  if (cmd == READ) 
    nb_bytes = read(fd,data,data_size);
  else
    nb_bytes = write(fd,data,data_size);

  if (nb_bytes != data_size) return EXIT_FAILURE;
  
  if (close(fd) != 0) return EXIT_FAILURE;
  return EXIT_SUCCESS;
}

 
//! lit ou écrit un block sur le dev "dev_name" (l'offset est relatif au début du sbg)
int access_meta_b(int cmd, 
		  char *dev_name,
		  __u64 offset_r, 
		  void* data, 
		  int data_size) {
  int error;
  // calcul de l'offset absolu (le sbb est situé à la fin de la brique)
  __u64 dev_size;  
  __u64 offset;

  error = get_nb_kbytes(dev_name, &dev_size);  
  if (error == EXIT_FAILURE) return EXIT_FAILURE;  
 
  offset = offset_end_rdev(dev_size) - offset_r;
  
  error = access_block(cmd, dev_name, offset, data, data_size);
  return error;
}


//! raz du sbg du real dev représenté par /dev/.. "dev_name"
int raz_sbg_rdev(char *dev_name) {
  struct sb_group sbg;
  int err;
  
  memset(&sbg, 0, sizeof(struct sb_group));
  err = access_meta_b(WRITE, 
		      dev_name, 
		      r_offset_sbg(),
		      &sbg, 
		      sizeof(struct sb_group));

  return err;
}

// A REACTUALISER
//! regarde le superbloc de la brique dont le /dev/.. est passé en paramètre et indique si la zone est présente (en fonction du uuid ou du nom de la zone)
/*int zone_existe(char *brique_name,
		 int uuid_ok, char *uuid_zone,
		 int name_ok, char *zone_name,
		struct sb_zone *sbz) {

  __u32 uuid[4];
  struct sb_brique sbb;
  int error,no_zone;

  if (uuid_ok) {
    sscanf(uuid_zone,"%x:%x:%x:%x",&uuid[3], &uuid[2], &uuid[1], &uuid[0]);
     printf("uuid param = 0x%x:%x:%x:%x\n",
	   uuid[3],
	   uuid[2],
	   uuid[1],
	   uuid[0]);
  }
  
  if (name_ok) printf("zone_name = %s\n",zone_name);
  
  printf("reading sbb on brique %s\n",brique_name);
  
  error = access_meta_b(READ,
			brique_name,
			0,
			(void *) &sbb,
			sizeof(struct sb_brique));

  if (error == EXIT_FAILURE) return FALSE;
  printf("sbb on brique %s : sbb.nb_zones = %d\n",
	 brique_name,
	 sbb.nb_zones);

  for (no_zone=0; no_zone<NB_MAX_ZONES; no_zone++) {
    if (valid_no_zone(no_zone, &sbb)) {
      
      printf("reading sbz on zone %d offset_r = %lld\n",
	     no_zone,
	     sbb.pos_sbz_r[no_zone]);
      
      error = access_meta_b(READ,
			    brique_name,
			    sbb.pos_sbz_r[no_zone],
			    (void *) sbz,
			    sizeof(struct sb_zone));
      
      if (error == EXIT_FAILURE) return FALSE;
      
      printf("uuid on zone %d  = 0x%x:%x:%x:%x\n",
	   no_zone,
	     sbz->zone_uuid[3],
	     sbz->zone_uuid[2],
	     sbz->zone_uuid[1],
	     sbz->zone_uuid[0]);
      
      if (same_uuid(uuid,sbz->zone_uuid)) return TRUE;
    }
  }
        
  return FALSE;
  }*/

// A REACTUALISER
//! charge dans en mémoire le sbb d'un /dev/../brique renvoie SUCCESSsi le sbb est valide et  EXIT_FAILURE sinon (ne peut lire le /dev, ou sbb invalide) 
/*int load_sbb(char *dev_name, struct sb_brique *sbb) {
  int error;

  error = access_meta_b(READ,
			dev_name,
			0,
			(void *) sbb,
			sizeof(struct sb_brique));
  if (error == EXIT_FAILURE) return EXIT_FAILURE;
  if (sbb-> magic_number != SB_BR_MAGIC) return EXIT_FAILURE;
  
  return EXIT_SUCCESS;
}

//! renvoie le sbz de la zone d'UUID zuuid. Renvoie EXIT_FAILURE ou SUCCESS
int get_sbz(__u32 *zuuid, struct sb_zone *sbz) {
  return EXIT_SUCCESS;
}

//! renvoie un tableau de sbb et de devname pour tous les devices accessibles par le virtualiseur depuis ce noeud
void load_all_sbb(struct sb_brique **sbbs, char **devnames, int *nb_sbb) {
}*/

// NEW
//! indique si 'dev_name' est un block device valide. Renvoie TRUE/FALSE
int valid_bd(char *dev_name) {
  struct stat info;
  
  if (stat(dev_name, &info) <0) 
    return FALSE;

  
  if (S_ISBLK(info.st_mode) == FALSE)
    return FALSE;
  
  return TRUE;
}
  

//TODO: fonction récupérée des vrtcommand. A adapter aux new_scripts
#define NBD_DIRNAME "/dev/scimapdev"
#define NBD_DEVMOTIF "brique"

//! renvoie la liste de tous les devices importés sur ce noeud par nos nbd. renvoie le SUCCESS/FAILURE.
int stornbd_rdevs(char *rdevs_path[], int *nb_rdevs) {
  char *dir_name=NBD_DIRNAME;
  char *dev_motif=NBD_DEVMOTIF; 
  char dev_name[LINE_SIZE];
  regex_t motif_recherche;
  int nb_entrees;
  struct dirent **liste;
  int i;

  int is_a_dev_name(const struct dirent *entree) {
    if (regexec(&motif_recherche, entree->d_name, 0, NULL, 0) == 0)
      return TRUE;
    else return FALSE;
  }
  
  // activation des devices qui ont le motif "dev_motif" dans le rép "dir_name"
  if (regcomp(&motif_recherche, dev_motif, REG_NOSUB) !=0) {
    fprintf(stderr, "erreur dans la mise en place du motif de recherche\n");
    return EXIT_FAILURE;
  }
  
  nb_entrees = scandir(dir_name, &liste, is_a_dev_name, alphasort);
  if (nb_entrees <= 0) {
    fprintf(stderr, "erreur: pas de devices à activer\n");
    return EXIT_FAILURE;
  }
  
  for (i=0; i<nb_entrees; i++) {
    sprintf(dev_name,"%s/%s", dir_name,liste[i]->d_name);
    if (valid_bd(dev_name)) 
      sprintf(rdevs_path[i],"%s",dev_name);
    free(liste[i]);
  }
  
  *nb_rdevs = nb_entrees;
  return EXIT_SUCCESS;
}

// NEW
// lit le SBG du rdev 'dev_path'.Renvoie EXIT_FAILURE/SUCCESS 
int load_sbg(struct sb_group *sbg, char *dev_path) {
  int retval;

  retval = access_meta_b(READ, 
			 dev_path,
			 r_offset_sbg(), 
			 &sbg_aux, 
			 sizeof(struct sb_group));
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR : impossible de lire le SBG sur le dev '%s'\n",
	    dev_path);
    return EXIT_FAILURE;
  }
  if (sbg_aux.magic_number != SBG_MAGIC) {
    fprintf(stderr, "ERREUR : mauvais magic number sur le dev '%s'\n",
	    dev_path);
    return EXIT_FAILURE;
  }
  
  // TODO : contrôler la checksum (utiliser une fonction générique
  // qui fonctionne quel que soit le type de bloc)

  return EXIT_SUCCESS;
}

// NEW
// lit le SBG du rdev 'dev_path'.Renvoie EXIT_FAILURE/SUCCESS 
int load_sbr(struct sb_rdevs *sbr, char *dev_path) {
  int retval;
  retval = access_meta_b(READ, 
			 dev_path,
			 r_offset_sbr(), 
			 &sbr_aux, 
			 sizeof(struct sb_rdevs));
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR : impossible de lire le SBR sur le dev '%s'\n",
	    dev_path);
    return EXIT_FAILURE;
  }
  if (sbr_aux.magic_number != SBR_MAGIC) {
    fprintf(stderr, "ERREUR : mauvais magic number sur le dev '%s'\n",
	    dev_path);
    return EXIT_FAILURE;
  }

  // TODO : contrôler la checksum (utiliser une fonction générique
  // qui fonctionne quel que soit le type de bloc)
  
  return EXIT_SUCCESS;
}
  
// NEW
//! lit le SBG du /dev/... et renvoie le nom du group. Renvoie EXIT_FAILURE/SUCCESS
int get_group_name(char *dev_path, char *gname) {  
  if (load_sbg(&sbg_aux, dev_path) == EXIT_FAILURE)
    return EXIT_FAILURE;
  
  strcpy(gname, sbg_aux.gname);
  return EXIT_SUCCESS;
}

//! renvoie le numéro du rdev (dans le liste de path) qui appartient au groupe dans le nom est passé en param. renvoie EXIT_FAILURE ou SUCCESS
int get_a_rdev_in_group(char *gname, 
			char *rdevs_path[], 
			int nb_rdevs,
			int *no_rdev) { 
  int i;
  char name[NAME_MAX_SZ];

  for (i=0; i<nb_rdevs; i++) {
    get_group_name(rdevs_path[i],name);
    //printf("gname on rdev '%s' = %s\n",
    //	   rdevs_path[i],
    //	   name);
    if (strcmp(name, gname) == 0) {
      *no_rdev = i;
      return EXIT_SUCCESS;
    }
  }

  return EXIT_FAILURE;
}



//NEW
//! lit le SBR du group sur /dev/.. et renvoie un tableau les UUID des rdevs du group. Renvoie EXIT_FAILURE/SUCCESS
int get_uuid_rdevs(char *dev_path, 
		   __u32 rdevs_uuid[NBMAX_RDEVS][4],
		   int *nb_rdevs_in_group) {
  int  i,j;
  
  if (load_sbg(&sbg_aux, dev_path) == EXIT_FAILURE)
    return EXIT_FAILURE;
  
  *nb_rdevs_in_group = sbg_aux.nb_rdevs;
  //printf("rdevs_in_group = %d \n", *nb_rdevs_in_group);
  
  if (load_sbr(&sbr_aux, dev_path) == EXIT_FAILURE)
    return EXIT_FAILURE;

  for (i=0; i<*nb_rdevs_in_group; i++) {
    //printf("rdevs_uuid[%d]=%p\n", i, rdevs_uuid[i]);
    for (j=0; j<4; j++) 
      rdevs_uuid[i][j] = sbr_aux.uuid_rdevs[i][j];
    
    //printf("uuid dev %d = 0x%x:%x:%x:%x\n",
    //   i, rdevs_uuid[i][0], rdevs_uuid[i][1],
    //   rdevs_uuid[i][2], rdevs_uuid[i][3]);
  }
  return EXIT_SUCCESS;
}

// NEW
//! indique si tous les rdevs dont les UUID sont dans 'rdevs_uuid' sont accessibles (i.e. sont des block devices dont les chemins sont dans 'rdevs_path'). Renvoie TRUE/FALSE et la liste des chemins de rdevs du groupe
int all_rdevs_reachable(__u32 rdevs_uuid[NBMAX_RDEVS][4],
			int nb_rdevs_in_group,
			char *rdevs_path[], 
			int nb_rdevs_in_path,
			char *rdevs_path_g[]) {
  int i,j;
  __u8 trouve[NBMAX_RDEVS];

  //printf("** all_rdevs_reachable : in_g=%d in_path=%d **\n",
  //	 nb_rdevs_in_group, nb_rdevs_in_path);
  for (i=0; i<nb_rdevs_in_group; i++) 
    trouve[i] = FALSE;

  for (i=0; i<nb_rdevs_in_path; i++) {
    if (load_sbg(&sbg_aux, rdevs_path[i]) == EXIT_FAILURE)
      return EXIT_FAILURE;
    
    //printf("load_sbd %s : OK\n", rdevs_path[i]);
    for (j=0; j<nb_rdevs_in_group; j++)
      if (same_uuid(sbg_aux.rdev_uuid, rdevs_uuid[j])) {
	trouve[j] = TRUE;
	strcpy(rdevs_path_g[j],rdevs_path[i]); 
	/*printf("rdevs_uuid[%d]=0x%x:%x:%x:%x trouvé sur rdev '%s'\n",
	       j,
	       rdevs_uuid[j][3], 
	       rdevs_uuid[j][2], 
	       rdevs_uuid[j][1], 
	       rdevs_uuid[j][0],
	       rdevs_path[i]);*/
      } /*else 
	printf("rdevs_uuid[%d]=0x%x:%x:%x:%x != "
	       "sbg.dev_uuid = 0x%x:%x:%x:%x\n",
	       j,
	       rdevs_uuid[j][3], 
	       rdevs_uuid[j][2], 
	       rdevs_uuid[j][1], 
	       rdevs_uuid[j][0],
	       sbg_aux.rdev_uuid[3],
	       sbg_aux.rdev_uuid[2],
	       sbg_aux.rdev_uuid[1],
	       sbg_aux.rdev_uuid[0]);
	
	       printf("\n");*/	
  }
  
  for (i=0; i<nb_rdevs_in_group; i++) 
    if (trouve[i] == FALSE) return FALSE;

  return TRUE;
}

//! revoie le path et les uuid des rdevs du groupe de nom 'gname'. Contrôle que les rdevs renvoyés sont accessibles.
int get_all_group_rdevs(char *gname,
			char *rdevs_path_g[], 
			__u32 rdevs_uuid_g[NBMAX_RDEVS][4], 
			int *nb_rdevs_g) {
  char *rdevs_path[NBMAX_RDEVS];
  int nb_rdevs, no_rdev, i;

  for (i=0; i<NBMAX_RDEVS; i++)
    rdevs_path[i] = malloc(sizeof(char)* LINE_SIZE);
  //printf("malloc : OK\n");
            
  if (stornbd_rdevs(rdevs_path, &nb_rdevs) == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR dans la recherche de rdevs importés\n");
    goto error;
  }
  //printf("stornbd_rdevs : OK\n");

  if (nb_rdevs == 0) {
    fprintf(stderr, "ERREUR aucun rdev n'a été trouvé\n");
    goto error;
  }
  
  if (get_a_rdev_in_group(gname, rdevs_path, nb_rdevs, &no_rdev) 
      == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR aucun rdev accessible n'appartient au"
	    " groupe '%s'\n",gname);
    goto error;
  }
  //printf("get_a_rdev_in_group : OK\n");
  
  if (get_uuid_rdevs(rdevs_path[no_rdev], rdevs_uuid_g, nb_rdevs_g) 
      == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR impossible d'obtenir les UUID"
	    " des rdevs du groupe '%s'\n",gname);
    goto error;
  }
  //printf("get_uuid_rdevs : OK\n");
  

  if (all_rdevs_reachable(rdevs_uuid_g, *nb_rdevs_g, 
			  rdevs_path, nb_rdevs,
			  rdevs_path_g) 
       == FALSE) {
    fprintf(stderr, "ERREUR les rdevs du groupe '%s' ne sont "
	    "pas tous accessibles\n",gname);
    goto error;
  }
  //printf("all_rdevs_reachable : OK\n");
  

  for (i=0; i<NBMAX_RDEVS; i++) 
    free(rdevs_path[i]);

  //printf("free : OK\n");
  return EXIT_SUCCESS;

  error :
  for (i=0; i<NBMAX_RDEVS; i++)
    free(rdevs_path[i]);
  return EXIT_FAILURE;
}  

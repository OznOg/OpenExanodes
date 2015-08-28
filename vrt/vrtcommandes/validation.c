#include "constantes.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include "parser.h"
#include <asm/types.h>
#include "dataaccess.h"

//!renvoie TRUE si la chaine est dans les 'max_read' octets du fichier file_name et FALSE sinon (rmq : renvoie FALSE si on a un pb pour ouvrir, lire le fichier, réserver de la mémoire pour le buffer)
int string_in_file(char *file_name, char *str, int max_read) {
  int fd, nb_octets_lus, result; 
  char *buf;

  if ((fd = open(file_name,O_RDWR)) <0) {
    perror("open");
    return FALSE;
  }
  
  buf = malloc(max_read);
  if (buf == NULL) {
    perror("malloc");
    return FALSE;
  }

  memset(buf,0, max_read);

  nb_octets_lus = read(fd, buf, max_read);

  result = FALSE;
  if (strstr(buf,str) != NULL) result = TRUE;

  free(buf);

  if (close(fd) <0) {
    perror("close");
    return FALSE;
  }
  
  return result;
}

//! indique si tous les rdevs du groupe sont accessibles depuis le noeud courant
int all_group_rdevs_accessibles(char *gname) {
  char *rdevs_path_g[NBMAX_RDEVS];
  __u32 rdevs_uuid_g[NBMAX_RDEVS][4];
  int nb_rdevs_g, i, result;

  for (i=0; i<NBMAX_RDEVS; i++) 
    rdevs_path_g[i] = malloc(sizeof(char)* LINE_SIZE);
  
  result = get_all_group_rdevs(gname,
			       rdevs_path_g, 
			       rdevs_uuid_g, 
			       &nb_rdevs_g);
  
  for (i=0; i<NBMAX_RDEVS; i++)
      free(rdevs_path_g[i]);

  if (result == EXIT_FAILURE) 
    return FALSE;
  return TRUE;  
}



//! mets tous les mots d'un fichier dans le tableau de mots + renvoie le nb de mots. Renvoie EXIT_FAILURE/SUCCESS
int get_all_words(char *file_name, 
		  char *words[], 
		  int *nb_words, 
		  int max_size) {
  int fd, nb_octets_lus; 
  char *buf;
  char word[BASIC_SSIZE];
  
  if ((fd = open(file_name,O_RDWR)) <0) {
    perror("open");
    return EXIT_FAILURE;
  }
  
  buf = malloc(max_size);
  if (buf == NULL) {
    perror("open");
    return EXIT_FAILURE; 
  }

  nb_octets_lus = read(fd, buf, max_size);

  *nb_words =0;
  while (extraire_mot(buf,word) != FALSE) {
    strcpy(words[*nb_words],word);
    (*nb_words)++;
  }
     
  free(buf);

  if (close(fd) <0) {
    perror("close");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}


//! renvoie la liste des noms des zones actives du groupe 'gname' + le nombre de zones actives dans ce groupe
int get_all_active_zones(char *gname, 
			 char *zones_names[], 
			 int *nb_az) {
  int max_size; 
  char file_name[LINE_SIZE];

  max_size = NBMAX_ZONES * MAXSIZE_ZONENAME * sizeof(char) ;
  sprintf(file_name,"%s/%s/%s", VRT_GPROCPATH, gname, VRT_PROCAZ);
  
  return get_all_words(file_name, zones_names, nb_az, max_size); 
}

//! indique si la zone est en cours d'utilisation (+ de open que de close sur la zone)
//TODO : les param devrait être gname puis zname (pour éviter les confusions)
int zone_used(char *zname, char *gname) {
  int max_size;
  char file_name[LINE_SIZE];
  
  max_size = NBMAX_ZONES * MAXSIZE_ZONENAME * sizeof(char) ;
  sprintf(file_name,"%s/%s/%s", VRT_GPROCPATH, gname, VRT_PROCUZ);
  if (string_in_file(file_name, zname, max_size) == FALSE) return FALSE;
  return TRUE;
}

//! envoie la commande de désactivation de la zone au virtualiseur
int desactivate_zone(char *g_name, char *z_name) {
  char commande[LINE_SIZE];

  if (zone_used(z_name,g_name) == TRUE) {
     fprintf(stderr, "** ERROR **\n"
	             "zone '%s:%s' is currently used by an application or a "
	             "file system! It can't be desactivated!\n", 
	             g_name,
	             z_name);
      return EXIT_FAILURE;
    }
    
  sprintf(commande,
	  "echo \"zk %s %s\" > " PROC_VIRTUALISEUR,
	  g_name,
	  z_name);
  system(commande);
  printf("\t > %s\n",commande);
  
  return EXIT_SUCCESS;
}

//! désactive toutes les zones actives du groupe 'gname'
int desactivate_all_zones(char *gname) {
  char *zones_names[NBMAX_ZONES];
  int i, nb_az; //nb de zones actives
  int retval = EXIT_SUCCESS;

  for (i=0; i<NBMAX_ZONES; i++) {
    zones_names[i] = malloc(MAXSIZE_ZONENAME * sizeof(char));
    if (zones_names[i] == NULL) {
      fprintf(stderr, "can't allocate mem for zones_names[%d]\n", i);
      return EXIT_FAILURE;
    }
  }

  if (get_all_active_zones(gname,  zones_names, &nb_az)  == EXIT_FAILURE)
    return EXIT_FAILURE;
		       
  for (i=0; i<nb_az; i++) {
    printf("desactivating zone '%s:%s'\n", gname,zones_names[i]); 
    if (desactivate_zone(gname, zones_names[i]) == EXIT_FAILURE) 
      retval = EXIT_FAILURE;
  }
  for (i=0; i<NBMAX_ZONES; i++) free(zones_names[i]);
  return retval;
}


//! stop le groupe 'gname' : stoppe ses zones, puis demande au virtualiseur de ne plus gérer ce groupe (désallocation des tables et unregistering des rep/file procfs, devfs)
int desactivate_group(char *gname) {
  char commande[LINE_SIZE];

  if (desactivate_all_zones(gname) ==  EXIT_FAILURE) 
    return EXIT_FAILURE;
  
  sprintf(commande,
	  "echo \"gk %s\" > " PROC_VIRTUALISEUR,
	  gname);
  system(commande);
  printf("\t > %s\n",commande);
  
  return EXIT_SUCCESS;
}

//! renvoie la liste des noms des zones oisives du groupe 'gname' + le nombre de zones oisives dans ce groupe
int get_all_idling_zones(char *gname, 
			 char *zones_names[], 
			 int *nb_iz) {
  int max_size; 
  char file_name[LINE_SIZE];

  max_size = NBMAX_ZONES * MAXSIZE_ZONENAME * sizeof(char) ;
  sprintf(file_name,"%s/%s/%s", VRT_GPROCPATH, gname, VRT_PROCIZ);
  
  return get_all_words(file_name, zones_names, nb_iz, max_size); 
}



//! renvoie TRUE si le nom 'zname' est utilisée pour une zone du group 'gname'. Renvoie FALSE sinon.
int zname_in_group(char *zname, char *gname) {
  char file_name[LINE_SIZE];
  int max_size;
  
  max_size = NBMAX_ZONES * MAXSIZE_ZONENAME * sizeof(char) ;

  sprintf(file_name,"%s/%s/%s", VRT_GPROCPATH, gname, VRT_PROCAZ);
  if (string_in_file(file_name, zname, max_size) == TRUE) return TRUE;
  
  sprintf(file_name,"%s/%s/%s", VRT_GPROCPATH, gname, VRT_PROCIZ);
  if (string_in_file(file_name, zname, max_size) == TRUE) return TRUE;

  return FALSE;
}

//! indique si le nom de la zone  est valide
int valid_unused_zname(char *zname, char *gname) {
  if (strlen(zname) > MAXSIZE_ZONENAME) return FALSE;
  if (zname_in_group(zname, gname) == TRUE) return FALSE;
  return TRUE;
}

//! indique si le nom du groupe  est valide
// TODO : ajouter un controle que le gname n'a pas déjà été utilisé pour un groupe (mais comment faire ??? mettre en place un fichier qui référence tous les groupes qui ont été créés et non détruits ? utiliser des UUID pour faire la part entre les groupes ayant le même nom ? c'est une question ouverte..
int valid_unused_gname(char *gname) {
  if (strlen(gname) > MAXSIZE_ZONENAME) return FALSE;
  return TRUE;
}

//! renvoie TRUE si le group 'gname' est actuellement géré par le virtualiseur (ACTIF). 
int gname_active(char *gname) {
  char rep_name[LINE_SIZE];

  sprintf(rep_name,"%s/%s", VRT_GPROCPATH, gname);
  if (opendir(rep_name) == NULL) return FALSE;
  
  return TRUE;
}

//! renvoie TRUE si le virtualiseur est ACTIF. 
int vrt_active(void) {
  if (opendir(VRT_GPROCPATH) == NULL) return FALSE;
  
  return TRUE;
}


//! indique si la zone est activée ou désactivée
int zone_active(char *zname, char *gname) {
  int max_size;
  char file_name[LINE_SIZE];
  
  max_size = NBMAX_ZONES * MAXSIZE_ZONENAME * sizeof(char) ;
  sprintf(file_name,"%s/%s/%s", VRT_GPROCPATH, gname, VRT_PROCAZ);
  if (string_in_file(file_name, zname, max_size) == FALSE) return FALSE;
  return TRUE;
}
    


#include "vrtcommands.h" 
#include "uuid.h"
#include "dataaccess.h"
#include "validation.h"
#include <string.h>


void help(void) {
  printf(". Usage : zstart GROUPNAME:ZONENAME\n");
  printf(". Active la zone GROUPNAME:ZONENAME pour pouvoir utiliser\n"
	 "  son stockage. Le groupe auquel elle appartient doit être\n"
	 "  actuellement actif dans le virtualiseur\n");
  printf(". Exemple :\n"
	 "\t> zstart video:mpeg\n\n"
	 "\t> Active la zone 'mpeg' du group 'video'\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
}  

//! envoie la commande d'activation de la zone au virtualiseur
int activate_zone(char *g_name, char *z_name) {
  char commande[LINE_SIZE];
  sprintf(commande,
	  "echo \"zs %s %s\" > " PROC_VIRTUALISEUR,
	  g_name,
	  z_name);
  system(commande);
  printf("\t > %s\n",commande);
  
  return EXIT_SUCCESS;
}

//! active toutes les zones oisives du groupe 'gname'
int activate_all_zones(char *gname) {
  char *zones_names[NBMAX_ZONES];
  int i, nb_az; //nb de zones actives

  for (i=0; i<NBMAX_ZONES; i++) {
    zones_names[i] = malloc(MAXSIZE_ZONENAME * sizeof(char));
    if (zones_names[i] == NULL) {
      fprintf(stderr, "can't allocate mem for zones_names[%d]\n", i);
      return EXIT_FAILURE;
    }
  }

  if (get_all_idling_zones(gname,  zones_names, &nb_az)  == EXIT_FAILURE)
    return EXIT_FAILURE;
		       
  for (i=0; i<nb_az; i++) {
    printf("activating zone '%s:%s'\n", gname,zones_names[i]); 
    if (activate_zone(gname, zones_names[i]) == EXIT_FAILURE)
      return EXIT_FAILURE;
  }
  for (i=0; i<NBMAX_ZONES; i++) free(zones_names[i]);
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  char g_name[BASIC_SSIZE], z_name[BASIC_SSIZE];
 
  int retval;
  char option;

  while ((option = getopt(argc, argv, "h")) != -1) {
    printf("option = %c\n",option);
    if (optarg) printf("optarg = %s\n",optarg);
    switch (option) {
    case 'h':
      help();
      return EXIT_SUCCESS;
      break;
    case '?':
      fprintf(stderr,"Option inconnue\n");
      help();
      return EXIT_FAILURE;
    } 
  }   

  if (argc-optind != 1) {
    help();
    return EXIT_FAILURE;
  }
  
  if (vrt_active() == FALSE) {
    fprintf(stderr, "échec : le virtualiseur n'est pas présent "
	            "en mémoire\n");
    return EXIT_FAILURE;
  }

  retval = sscanf(argv[optind],"%16[A-Za-z0-9]:%16[A-Za-z0-9*]", 
		  g_name, z_name);  
  if (retval != 2) {
    fprintf(stderr,"impossible de parser le nom de la zone "
	    "et le nom du groupe (taille limitée à 16 caractères"
	    " chacun)\n");
    return EXIT_FAILURE;
  }
  
  
  if (gname_active(g_name) == FALSE) {
    fprintf(stderr,"Le group '%s' n'est pas actif\n",
	    g_name);
    return EXIT_FAILURE;
  }

  if (strcmp(z_name,"*") != 0) {
    if (zname_in_group(z_name, g_name) == FALSE) {
      fprintf(stderr,"La zone '%s' n'est pas dans le groupe '%s'\n",
	      z_name, g_name);
      return EXIT_FAILURE;
    }
    
    if (zone_active(z_name, g_name) == TRUE) {
      fprintf(stderr,"La zone '%s:%s' est déjà active!\n",
	    g_name,z_name);
      return EXIT_FAILURE;
    }

    return activate_zone(g_name, z_name);
  }
  else return activate_all_zones(g_name);
} 

#include "vrtcommands.h"
#include "uuid.h"
#include "validation.h"



void help(void) {
  printf(". Usage : zresize GROUPNAME:ZONENAME ZONESIZEMB\n");
  printf(". Crée une zone de stockage de nom ZONENAME et de taille\n"
	 "  ZONESIZEMB (en Mo) sur le groupe désigné par son nom\n"
	 "  GROUPNAME accolé au nom de la zone\n");
  printf(". Exemples :\n"
	 "\t> zresize cluster1:userfile 20000\n\n"
	 "\tRetaille à 20000 Mo la zone 'userfile' du  groupe\n"
	 "\t'cluster1'\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
}  
 
//! retaille la zone 'z_name' du groupe 'g_name' à la taille 'size_MD'. Renvoie EXIT_SUCCESS si tout est ok, et EXIT_FAILURE sinon
int resize_zone(char *g_name, char *z_name,  __u64 size_MB) {
  char commande[LINE_SIZE];
    
  printf("Retaillage de la zone %s:%s à %lld Mo\n",
	 g_name, 
	 z_name,
	 size_MB);

  sprintf(commande,
	  "echo \"zr %s %s %lld\" > " PROC_VIRTUALISEUR,
	  g_name,
	  z_name,
	  size_MB*1024);
  system(commande);
  printf("\t > %s\n",commande); 
 
  // TODO 
  // à la place du sleep, faire une boucle qui 
  // teste le /proc pour voir si la zone a pu être créée
  // et qui attend qu'elle soit créée avant de maj
  // les SB et de l'activer
  sleep(1);

  sprintf(commande,
	  "echo \"wa %s\" > " PROC_VIRTUALISEUR,
	  g_name);
  system(commande);
  printf("\t > %s\n",commande);  
  
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  char g_name[BASIC_SSIZE], z_name[BASIC_SSIZE];
  __u64 size_MB;
 
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

  if (argc-optind != 2) {
    help();
    return EXIT_FAILURE;
  }
  
  if (vrt_active() == FALSE) {
    fprintf(stderr, "échec : le virtualiseur n'est pas présent "
	            "en mémoire\n");
    return EXIT_FAILURE;
  }

  retval = sscanf(argv[optind],"%16[A-Za-z0-9]:%16[A-Za-z0-9]", 
		  g_name, z_name);  
  if (retval != 2) {
    fprintf(stderr,"impossible de parser le nom de la zone"
	    " et le nom du groupe (taille limitée à 16 caractères"
	    " chacun)\n");
    return EXIT_FAILURE;
  }
  
  retval = sscanf(argv[optind+1],"%Ld",&size_MB); 
  if (retval != 1) {
    fprintf(stderr,"impossible de parser la nouvelle taille de la zone\n");
    return EXIT_FAILURE;
  }  

  if (gname_active(g_name) == FALSE) {
    fprintf(stderr,"Le groupe '%s' n'est pas actif\n",
	    g_name);
    return EXIT_FAILURE;
  }

  if (zname_in_group(z_name, g_name) == FALSE) {
    fprintf(stderr,"La zone '%s:%s' n'existe pas\n",
 	    z_name, g_name);
    return EXIT_FAILURE;
  }

  /*if (zone_active(z_name, g_name) == FALSE) {
    fprintf(stderr,"La zone '%s:%s' n'est active. Impossible de la "
	           "retailler\n",
 	            z_name, g_name);
    return EXIT_FAILURE;
    }*/
 
  return resize_zone(g_name, z_name, size_MB);
}

#include "vrtcommands.h" 
#include "uuid.h"
#include "dataaccess.h"
#include "validation.h"
#include <string.h>


void help(void) {
  printf(". Usage : zstop GROUPNAME:ZONENAME\n");
  printf(". D�sactive la zone GROUPNAME:ZONENAME. Elle n'est plus\n"
	 "  accessible (plus de minor number).\n");
  printf(". Exemple :\n"
	 "\t> zstop video:mpeg\n\n"
	 "\t> D�sactive la zone 'mpeg' du group 'video'\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
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
    fprintf(stderr, "�chec : le virtualiseur n'est pas pr�sent "
	            "en m�moire\n");
    return EXIT_FAILURE;
  }

  retval = sscanf(argv[optind],"%16[A-Za-z0-9]:%16[A-Za-z0-9*]", 
		  g_name, z_name);  
  if (retval != 2) {
    fprintf(stderr,"impossible de parser le nom de la zone "
	    "et le nom du groupe (taille limit�e � 16 caract�res"
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
    
    if (zone_active(z_name, g_name) == FALSE) {
      fprintf(stderr,"La zone '%s:%s' est d�j� d�sactiv�e!\n",
	      g_name,z_name);
      return EXIT_FAILURE;
    }

    return desactivate_zone(g_name, z_name);
  }
  else return desactivate_all_zones(g_name);

} 

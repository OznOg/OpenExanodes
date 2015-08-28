#include "vrtcommands.h" 
#include "uuid.h"
#include "dataaccess.h"
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include "validation.h"

void help(void) {
  printf(". Usage : dgstop GROUPNAME\n");
  printf(". D�sactive le device group dont le nom est pass� en \n"
	 "  param�tre. Le group n'est plus g�r� par le virtualiseur");
  printf(". Exemple :\n"
	 "\t> dgstop userdata\n\n"
	 "\tD�sactive le groupe 'userdata'\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
} 

int main(int argc, char **argv) {
  char option;
  char g_name[BASIC_SSIZE];
  int retval;

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

  if (argc-optind !=1) {
    help();
    return EXIT_FAILURE;
  }
  
  if (vrt_active() == FALSE) {
    fprintf(stderr, "�chec : le virtualiseur n'est pas pr�sent "
	            "en m�moire\n");
    return EXIT_FAILURE;
  }

  retval = sscanf(argv[optind],"%16[A-Za-z0-9]", g_name);  

  if (retval != 1) {
    fprintf(stderr,"impossible de parser le nom du groupe "
	    "(taille limit�e � 16 caract�res alphanum)\n");
    return EXIT_FAILURE;
  }

  if (gname_active(g_name) == FALSE) {
    fprintf(stderr,"Le group '%s' n'est pas actif\n",
	    g_name);
    return EXIT_FAILURE;
  }

  return desactivate_group(g_name);
}

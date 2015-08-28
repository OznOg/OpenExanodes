#include "vrtcommands.h" 
#include "uuid.h"
#include "dataaccess.h"
#include "validation.h"
#include <string.h>


void help(void) {
  printf(". Usage : vstatus\n");
  printf(". Affiche l'état du virtualiseur (zones, groupes, rdevs...)\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
}  

int main(int argc, char **argv) {
  char option;
  char commande[LINE_SIZE];

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

  if (argc-optind != 0) {
    help();
    return EXIT_FAILURE;
  }
  
  if (vrt_active() == FALSE) {
    fprintf(stderr, "échec : le virtualiseur n'est pas présent "
	            "en mémoire\n");
    return EXIT_FAILURE;
  }

  printf("** ETAT du virtualiseur **\n\n");
  sprintf(commande,
	  "cat " PROC_VIRTUALISEUR);
  system(commande);

  return EXIT_SUCCESS;
} 

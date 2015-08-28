#include "vrtcommands.h" 
#include "uuid.h"
#include "dataaccess.h"
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include "validation.h"

void help(void) {
  printf(". Usage : dgdelete GROUPNAME\n");
  printf(". D�truit le device group dont le nom est pass� en \n"
	 "  param�tre. Ses m�tadonn�es sont effac�es.");
  printf(". Exemple :\n"
	 "\t> dgdelete userdata\n\n"
	 "\tD�truit le groupe 'userdata'\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
} 


//! demande � l'utilisateur de confirmer la cr�ation du groupe sur les devices dont la liste est affich�e
int user_confirm(char *gname,
		 char *rdevs_path[], 
		 int nb_rdevs) {
  char c;
  int i;

  printf("Le syst�me s'appr�te � d�truire le groupe '%s'\n"
	 "constitu� des %d devices suivants :\n",
	 gname,
	 nb_rdevs);

  for (i=0; i<nb_rdevs; i++)
    printf("\t%s\n",rdevs_path[i]); 

  printf("\n** LES DONNEES DE CES DEVICES SERONT PERDUES **\n");
  printf("\nDestruction du groupe ? o/n : ");
  scanf("%c",&c);
  
  if (c == 'o') 
    return TRUE;
  else return FALSE; 
}

//! d�truit le groupe toutes ses donn�es sont perdues
int delete_group(char *gname) {
  char *rdevs_path_g[NBMAX_RDEVS];
  __u32 rdevs_uuid_g[NBMAX_RDEVS][4];
  int nb_rdevs_g, i, result;

  for (i=0; i<NBMAX_RDEVS; i++) {
    rdevs_path_g[i] = malloc(sizeof(char)* LINE_SIZE);
    if (rdevs_path_g[i] == NULL) {
      fprintf(stderr, "can't allocate memory for rdevs_path_g\n");
      return EXIT_FAILURE;
    }
  }
  
  result = get_all_group_rdevs(gname,
			       rdevs_path_g, 
			       rdevs_uuid_g, 
			       &nb_rdevs_g);

  if (user_confirm(gname, rdevs_path_g, nb_rdevs_g) == FALSE) {
    printf("\n\tCOMMANDE ANNULEE\n");
    goto error;
  }

  for (i=0; i<nb_rdevs_g; i++) 
    raz_sbg_rdev(rdevs_path_g[i]);
  
  for (i=0; i<NBMAX_RDEVS; i++)
      free(rdevs_path_g[i]);
  
  return EXIT_SUCCESS;

  error :
    for (i=0; i<NBMAX_RDEVS; i++)
      free(rdevs_path_g[i]);
  
  return EXIT_FAILURE;
    
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

  if (gname_active(g_name) == TRUE) {
    printf("Le groupe '%s' est actif -> d�sactivation\n",g_name);
    
    if (desactivate_group(g_name) == EXIT_FAILURE) {
    fprintf(stderr,"Impossible de d�sactiver le groupe '%s'\n",
	    g_name);
    return EXIT_FAILURE;
    }
  }
  return delete_group(g_name);
}

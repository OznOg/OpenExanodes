#include "vrtcommands.h"
#include "uuid.h"
#include "validation.h"



void help(void) {
  printf(". Usage : zcreate GROUPNAME:ZONENAME ZONESIZEMB\n");
  printf(". Cr�e une zone de stockage de nom ZONENAME et de taille\n"
	 "  ZONESIZEMB (en Mo) sur le groupe d�sign� par son nom\n"
	 "  GROUPNAME accol� au nom de la zone\n");
  printf(". Exemples :\n"
	 "\t> zcreate multimedia:divx 50000\n\n"
	 "\tCr�e la zone \"divx\" de taille 50000 Mo sur le groupe\n"
	 "\tde nom \"multimedia\"\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
}  
 
//! cr�e la zone z_name de taille "size_MD" sur le groupe de nom "g_name". Renvoie EXIT_SUCCESS si tout est ok, et EXIT_FAILURE sinon
int create_zone(char *g_name, char *z_name,  __u64 size_MB) {
  char commande[LINE_SIZE];
    
  printf("Cr�ation de la zone %s:%s (%lld Mo)\n",
	 g_name, 
	 z_name,
	 size_MB);

  sprintf(commande,
	  "echo \"zc %s %s %lld\" > " PROC_VIRTUALISEUR,
	  g_name,
	  z_name,
	  size_MB*1024);
  system(commande);
  printf("\t > %s\n",commande);
 
  // TODO 
  // � la place du sleep, faire une boucle qui 
  // teste le /proc pour voir si la zone a pu �tre cr��e
  // et qui attend qu'elle soit cr��e avant de maj
  // les SB et de l'activer
  sleep(1);

  sprintf(commande,
	  "echo \"wa %s\" > " PROC_VIRTUALISEUR,
	  g_name);
  system(commande);
  printf("\t > %s\n",commande);

  sleep(1);
  
  sprintf(commande,
	  "echo \"zs %s %s\" > " PROC_VIRTUALISEUR,
	  g_name,
	  z_name);
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
    fprintf(stderr, "�chec : le virtualiseur n'est pas pr�sent "
	            "en m�moire\n");
    return EXIT_FAILURE;
  }

  retval = sscanf(argv[optind],"%16[A-Za-z0-9]:%16[A-Za-z0-9]", 
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

  if (zname_in_group(z_name, g_name) == TRUE) {
    fprintf(stderr,"La zone '%s:%s' existe d�j�\n",
 	    z_name, g_name);
    return EXIT_FAILURE;
  }


  retval = sscanf(argv[optind+1],"%Ld",&size_MB); 
  if (retval != 1) {
    fprintf(stderr,"impossible de parser la taille de la zone\n");
    return EXIT_FAILURE;
  }  
 
  return create_zone(g_name, z_name, size_MB);
}

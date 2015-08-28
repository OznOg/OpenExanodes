#include "vrtcommands.h" 
#include "uuid.h"
#include "dataaccess.h"
#include <dirent.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include "validation.h"

void help(void) {
  printf(". Usage : dgstart GROUPNAME\n");
  printf(". Active le device group dont le nom est passé en \n"
	 "  paramètre. L'activation d'un groupe signifie que le\n" 
	 "  virtualiseur prend en compte son espace de stockage :\n"
	 "  accès aux zones précédemment créés, ajout/retrait de\n"
	 "  zones, resize de zones, etc... sur ce groupe\n");
  printf(". Exemple :\n"
	 "\t> dgstart userdata\n\n"
	 "\tActive le groupe 'userdata'\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
} 


//! indique au virtualiseur qu'on veut pouvoir utiliser le groupe 'gname'. Il faut que les briques du groupe aient été activées avant d'appeler cette fonction. Renvoie EXIT_FAILURE ou SUCCESS.
int activate_group(char *gname) {
  char commande[LINE_SIZE];

  printf("Activation du groupe '%s'\n",gname);
  sprintf(commande,
	    "echo \"g \" > " PROC_VIRTUALISEUR);
  system(commande);
  printf("\t > %s\n",commande);
  return EXIT_SUCCESS;
}

//! envoie une commande de recréation d'un vieux group
int send_group_load(char *rdev_path, char *gname) {
  struct stat info;
  char commande[LINE_SIZE];
  int major, minor;  
  __u64 nb_KB; 
  int retval;
  
  if (stat(rdev_path, &info) <0) {
    perror("stat");
    return EXIT_FAILURE;
  }
  
  retval = get_nb_kbytes(rdev_path,&nb_KB);
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "échec activation du dev %s (échec open, getsize, "
	    "major ou minor number)\n",
	    rdev_path);
    return EXIT_FAILURE;
  }
  
  get_major_minor(&info,&major,&minor);

  printf("Recréation du groupe '%s'\n",gname);
  sprintf(commande,
	  "echo \"gl %s %d %d %lld\" > " PROC_VIRTUALISEUR,
	  gname,
	  major,
	  minor,
	  nb_KB);
  system(commande);
  printf("\t > %s\n",commande);
  return EXIT_SUCCESS;
}  

//! envoie une commande pour réinscrire le device dans le groupe 'gname'
int send_add_old_rdevs(char *gname, 
		       char *rdev_path, 
		       __u32 *rdev_uuid) {
  struct stat info;
  char commande[LINE_SIZE];
  int retval;
  __u64 nb_KB; 
  int major, minor;  
  
  if (stat(rdev_path, &info) <0) {
    perror("stat");
    return EXIT_FAILURE;
  }
  
  retval = get_nb_kbytes(rdev_path,&nb_KB);
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "échec activation du dev %s (échec open, getsize, "
	    "major ou minor number)\n",
	    rdev_path);
    return EXIT_FAILURE;
  }
  get_major_minor(&info,&major,&minor);

  printf("ajout du rdev '%s'\n",rdev_path);
  sprintf(commande,
	  "echo \"go %s %u %u %u %u %s %lld %d %d\" > " 
	  PROC_VIRTUALISEUR,
	  gname,
	  rdev_uuid[3], rdev_uuid[2], rdev_uuid[1], rdev_uuid[0], 
	  rdev_path,
	  nb_KB,
	  major,
	  minor);
  system(commande);
  printf("\t > %s\n",commande); 
  return EXIT_SUCCESS;
}

//! démarre une ancien group. recréation des zones et de l'ancienne layout. Renvoie EXIT_FAILURE/SUCCESS
int send_start_old_group(char *gname) {
  char commande[LINE_SIZE];
  printf("démarrage du groupe '%s'\n",gname);
  sprintf(commande,
	  "echo \"gz %s\" > " PROC_VIRTUALISEUR,
	  gname);
  system(commande);
  printf("\t > %s\n",commande); 
  return EXIT_SUCCESS;
}

//! active le groupe de devices. 
int start_group(char *gname) {
  char *rdevs_path_g[NBMAX_RDEVS];
  __u32 rdevs_uuid_g[NBMAX_RDEVS][4];
  int nb_rdevs_g,  i;

  
  for (i=0; i<NBMAX_RDEVS; i++) {
    rdevs_path_g[i] = malloc(sizeof(char)* LINE_SIZE);
    if (rdevs_path_g[i] == NULL) {
      fprintf(stderr, "can't allocate memory for rdevs_path_g\n");
      return EXIT_FAILURE;
    }
  }

  if (get_all_group_rdevs(gname,
			  rdevs_path_g, 
			  rdevs_uuid_g, 
			  &nb_rdevs_g) 
      == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR impossible de déterminer tous les"
	            "rdevs du group '%s'\n",gname);
    goto error;
  }   
   

  /*for (i=0; i<nb_rdevs_g; i++) 
    printf("rdev %d = %s uuid = 0x%x:%x:%x:%x\n", 
	   i, 
	   rdevs_path_g[i], 
	   rdevs_uuid_g[i][3], rdevs_uuid_g[i][2], 
	   rdevs_uuid_g[i][1], rdevs_uuid_g[i][0]);*/
    
  if (send_group_load(rdevs_path_g[0],gname) == EXIT_FAILURE)
      goto error;
  for (i=0; i<nb_rdevs_g; i++) 
    if (send_add_old_rdevs(gname, rdevs_path_g[i], rdevs_uuid_g[i])
	== EXIT_FAILURE) 
      goto error;

  return send_start_old_group(gname);

  error :
    for (i=0; i<NBMAX_RDEVS; i++)
      free(rdevs_path_g[i]);
  
  return EXIT_FAILURE;
}

int main(int argc, char **argv) {
  char option;
  char *group_name;

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
    fprintf(stderr, "échec : le virtualiseur n'est pas présent "
	            "en mémoire\n");
    return EXIT_FAILURE;
  }

  group_name = argv[optind];
  if (gname_active(group_name) == TRUE) {
    fprintf(stderr,"Le group '%s' est déjà actif\n",
	    group_name);
    return EXIT_FAILURE;
  }
  //printf("test activité groupe : OK\n");
    
  return start_group(group_name);
}

#include "vrtcommands.h"
#include "dataaccess.h"
#include "conffile.h"
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include "i18n.h"
#include "validation.h"

#define NB_MAX_RETRIES 5

//! demande � l'utilisateur de confirmer la cr�ation du groupe sur les devices dont la liste est affich�e
int user_confirm(int first_dev, 
		 int last_dev, 
		 char *group_name,
		 char *layout_type,
		 char **argv) {
  char c;
  int i;

  printf(_("Le syst�me s'appr�te � cr�er le groupe \"%s\" avec les %d devices\nsuivants (placement = %s):\n\n"),
	 group_name,
	 last_dev - first_dev +1,
	 layout_type);

  for (i=first_dev; i<=last_dev; i++)
    printf("\t%s\n",argv[i]); 

  printf("\n** LES DONNEES DE CES DEVICES SERONT PERDUES **\n");
  printf("\nCr�ation du groupe ? o/n : ");
  scanf("%c",&c);
  
  if (c == 'o') 
    return TRUE;
  else return FALSE; 
}



//! renvoie TRUE si le device n'est pas d�j� utilis� dans un autre groupe. Renvoie FALSE et maj le param out other_devgroup si le device est d�j� utilis�
//TODO : programmer cette fonction en lisant les meta donn�es sur le device.
//       doit permettre �galement d'emp�cher qu'une brique soit ajout�e 
//       deux fois au m�me groupe
int device_free(char *dev_name, char *other_devgroup /*OUT*/) {
  return TRUE;
}

//! envoie au virtualiseur une cmd d'ajout d'un device r�el dans un groupe. Renvoie EXIT_SUCCESS/FAILURE 
int send_add_rdev(char *dev_name, char *group_name) {
  int retval;
  char commande[LINE_SIZE];
  __u64 nb_KB; 
  int major, minor;  
  char other_devgroup[BASIC_SSIZE];
  struct stat info;
  
  if (stat(dev_name, &info) <0) {
    perror("stat");
    return EXIT_FAILURE;
  }
  
  if (S_ISBLK(info.st_mode) == FALSE) {
    fprintf(stderr, "�chec ajout du dev %s (not a BLOCK DEVICE)\n",
	    dev_name );
    return EXIT_FAILURE;
  }      
  
  retval = get_nb_kbytes(dev_name,&nb_KB);
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "�chec ajout du dev %s (�chec open, getsize, "
	    "major ou minor number)\n",
	    dev_name);
    return EXIT_FAILURE;
  }
      
  if (device_free(dev_name, other_devgroup) == FALSE) {
    fprintf(stderr, "dev %s deja utilis� par le groupe %s\n",
	    dev_name,
	    other_devgroup);
    return EXIT_FAILURE;
  }
      
  get_major_minor(&info,&major,&minor);
  
  printf("Ajout du device \"%s\" (%.2f GB, dev[%d,%d]) " 
	 "dans le groupe \"%s\"\n", 
	 dev_name,
	 nb_KB/(1024.0*1024),
	 major,
	 minor,
	 group_name);
  sprintf(commande,
	  "echo \"gn %s %s %lld %d %d\" > " PROC_VIRTUALISEUR,
	  group_name,
	  dev_name,
	  nb_KB,
	  major,
	  minor);
  system(commande);
  printf("\t > %s\n",commande);
  
  return EXIT_SUCCESS;
}

//! envoie la commande "cr�er groupe" au virtualiseur
void send_create_group(char *group_name, char *layout_type) {
  char commande[LINE_SIZE];
  printf("Cr�ation groupe %s (layout=%s)\n", 
	 group_name,
	 layout_type);
  sprintf(commande,
	  "echo \"gc %s %s \" > " PROC_VIRTUALISEUR,
	  group_name,
	  layout_type);
  
  system(commande);
  printf("\t > %s\n",commande);  
}

//! �crit les m�ta donn�es du "group_name" sur le "rdev_name". Renvoie EXIT_SUCCESS/FAILURE
int send_write_group(char *group_name) {
  char commande[LINE_SIZE];
  printf("Ecriture du SBG sur les rdev sur group\n");
  sprintf(commande,
	  "echo \"wa %s\" > " PROC_VIRTUALISEUR,
	  group_name);
  system(commande);
  printf("\t > %s\n",commande);  
  return EXIT_SUCCESS;
}

//! envoie la commande "d�truit groupe" au virtualiseur
void send_delete_group(char *group_name) {
  char commande[LINE_SIZE];
  printf("Destruction groupe %s\n", 
	 group_name);
  sprintf(commande,
	  "echo \"gd %s \" > " PROC_VIRTUALISEUR,
	  group_name);
  
  system(commande);
  printf("\t > %s\n",commande);  
}  

int create_group(char *layout_type,
		 int first_dev, 
		 int last_dev, 
		 char *group_name,
		 char **argv) {
  int i;
  int nb_good_devs=0;
  
  send_create_group(group_name,layout_type);
  for (i=first_dev; i<= last_dev; i++)
    if (send_add_rdev(argv[i],group_name) == EXIT_SUCCESS) {
      nb_good_devs++;
    }
  
  if (nb_good_devs == 0) {
    fprintf(stderr, "�chec : aucun device n'est valide =>\n"
	    "groupe %s n'est pas cr��\n",
	    group_name);
    goto cleanup;
  }

  if (send_write_group(group_name) == EXIT_FAILURE) {
    fprintf(stderr, 
	    "�chec : impossible d'�crire les m�ta donn�es "
	    "du groupe %s sur ses devices\n", 
	    group_name);
    goto cleanup;
  }
    
  
  printf("groupe %s cr�� avec %d devices\n",
	 group_name,
	 nb_good_devs);
  return EXIT_SUCCESS;
  
  cleanup :
    send_delete_group(group_name);
    return EXIT_FAILURE;  
}
 
//! envoie la commande d'activation du group "group_name" au virtualiseur 
void send_start_group(char *group_name) {
  char commande[LINE_SIZE];
  printf("D�marrage du group %s\n", group_name); 
  sprintf(commande,
	  "echo \"gs %s\" > " PROC_VIRTUALISEUR,
	  group_name);
  system(commande);
  printf("\t > %s\n",commande);
}  

//! contr�le que le nom de groupe est valide
//        1) contr�ler la taille du nom (<= � 16 caract�res)
// TODO:  2) contr�ler qu'il est unique 
int valid_gname(char *group_name) {
  if (strlen(group_name) > MAXSIZE_GROUPNAME) return FALSE;
  return TRUE;
}

//! contr�le que le type du placement est correct
int valid_laytype(char *layout_type) {
  if (strcmp(layout_type, "sstriping") == 0) return TRUE;
  return FALSE;
}

void help(void) {
  printf(". Usage : dgcreate GROUPNAME LAYOUTTYPE DEVICE1 DEVICE2...\n");
  printf(". Cr�e un nouveau groupe de nom GROUPNAME sur les devices\n"
	 "  donn�s en param�tre. Le nom doit �tre unique (ne pas avoir\n"
	 "  �t� donn� � un autre groupe) et les devices doivent �tre\n"
	 "  libres (n'appartiennent � aucun groupe jusqu'� pr�sent).\n" 
	 "  Un seul type de layout pour l'instant : sstriping\n");
  printf(". Exemple :\n\t"
	 "> dgcreate video sstriping /dev/brique[0-4]\n\n"
	 "\tCr�e le groupe \"video\" avec les 5 devices\n"
	 "\t/dev/brique0,.. /dev/brique4. La politique de\n"
	 "\tplacement est le \"sstriping\"\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,VRTCMD_VERSION,__DATE__);
}  

int main(int argc, char **argv) {
  int first_dev, last_dev, err, i, nb_retries;
  char *group_name, *layout_type;
  char option;
  int group_added = FALSE;

  while ((option = getopt(argc, argv, "h")) != -1) {
    printf("option = %c\n",option);
    if (optarg) printf("optarg = %s\n",optarg);
    switch (option) {
    case 'h':
      help();
      break;
      return EXIT_SUCCESS;
    case '?':
      fprintf(stderr,"Option inconnue\n");
      help();
      return EXIT_FAILURE;
    } 
  }   

  if (argc-optind < 3) {
    help();
    return EXIT_FAILURE;
  }

  if (vrt_active() == FALSE) {
    fprintf(stderr, "�chec : le virtualiseur n'est pas pr�sent "
	            "en m�moire\n");
    return EXIT_FAILURE;
  }

  group_name = argv[optind];
  if (valid_gname(group_name) == FALSE) {
    fprintf(stderr, "�chec : nom de groupe (%s) invalide" 
	    "(existe d�j� ou nb carac > %d)\n",
	    group_name, MAXSIZE_GROUPNAME);
    return EXIT_FAILURE;
  }
  
  layout_type = argv[optind+1];
  if (valid_laytype(layout_type) == FALSE) {
    fprintf(stderr, "�chec : type de layout (%s) invalide\n",
	    layout_type);
    return EXIT_FAILURE;
  }
  
  first_dev = optind + 2;
  last_dev = argc - 1;
  
  if (user_confirm(first_dev, last_dev, group_name, layout_type,
		   argv) == FALSE) {
    printf("\n\tCOMMANDE ANNULEE\n");
    return EXIT_FAILURE;
  }

  for (i = first_dev; i<=last_dev; i++) {
    printf("RAZ du superblock group du device %s...",argv[i]);
    err = raz_sbg_rdev(argv[i]);
    if (err == EXIT_SUCCESS) 
      printf(" termin�\n"); 
    else {
      printf(" ERREUR\n");
      return EXIT_FAILURE;
    }
  }
  
  if (create_group(layout_type, first_dev, last_dev, group_name, argv) == EXIT_FAILURE) 
    return EXIT_FAILURE;

  send_start_group(group_name);
  
  // le group est cr��! on l'ajoute au fichier de conf du virtualiseur
  nb_retries = 0;
  while (group_added == FALSE && nb_retries < NB_MAX_RETRIES) {
    sleep(1);   
    group_added = add_group_conf_file(group_name);

    if (group_added == FALSE) {
      nb_retries++;
      fprintf(stderr, "maj du fichier de config a �chou�. Essai %d/%d\n",
	      nb_retries,
	      NB_MAX_RETRIES);
    }
  }
  
  return EXIT_SUCCESS;
}

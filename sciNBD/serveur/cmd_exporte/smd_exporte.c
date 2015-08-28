#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "../include/constantes.h"
#include <sys/ioctl.h>
#include <linux/fs.h>

int fichier_proc; // fichier proc qui référence les devices exportés par cette brique

void Raz_buffer(char *buffer) {
  int i;
  for (i=0;i<MAXSIZE_BUFFER+1;i++) buffer[i]=0;
}
 	
int exported_devices(void) {
  int to_smdd_pipe;
  int from_smdd_pipe;
  char buffer_emission[MAXSIZE_BUFFER+1];
  char buffer_reception[MAXSIZE_BUFFER+1];
  
  Raz_buffer(buffer_emission);
  Raz_buffer(buffer_reception);		
  
  // on envoie la commande au smdd
  to_smdd_pipe=open(EXPORTE2SMDD,O_RDWR);
  strcpy(buffer_emission,"L ");
  write(to_smdd_pipe,buffer_emission,strlen(buffer_emission));
  close(to_smdd_pipe);
  
  // on attend sa réponse
  from_smdd_pipe=open(SMDD2EXPORTE,O_RDONLY);
  read(from_smdd_pipe,buffer_reception,MAXSIZE_BUFFER);
  
  printf("%s",buffer_reception);

  return SUCCESS;
}

int get_nb_sectors(char *nom_de_device) {
  
  int nb_sectors=-1;
  int retval;
  int fd;

  if ((fd = open(nom_de_device,O_RDWR)) <0) return -1;
   
  retval = ioctl(fd,BLKGETSIZE,&nb_sectors);
  if (retval !=0) return -1;
  
 
  if (close(fd) !=0) return -1;

  return nb_sectors;
}


int add_device(char *path_device, char  *nom_de_device) {
  int to_smdd_pipe;
  int from_smdd_pipe;
  char buffer[MAXSIZE_BUFFER+1];
  int nb_sectors;

  // demande la taille du device qu'on veut exporter
  nb_sectors = get_nb_sectors(path_device);
  
  if (nb_sectors > 0) {
    // on envoie la commande au smdd
    to_smdd_pipe=open(EXPORTE2SMDD,O_RDWR);
    sprintf(buffer,"A %s %s %d", path_device, nom_de_device, nb_sectors);
    printf("commande envoyée vers SMDD : <%s>\n",buffer);
    write(to_smdd_pipe,buffer,strlen(buffer));
    close(to_smdd_pipe);
			
    // on attend sa réponse
    Raz_buffer(buffer);
    from_smdd_pipe=open(SMDD2EXPORTE,O_RDONLY);
    read(from_smdd_pipe,buffer,2);
			
    if (strcmp(buffer,"OK")==0) {
      printf("device '%s' path %s ajouté\n",nom_de_device, path_device);
      return SUCCESS;
    }

    if (strcmp(buffer,"ED")==0) { // ERROR DEVICE
      printf("ERREUR : smd_transfertd n'a pas pu ouvrir "
	     "le device %s\n",nom_de_device);
      return ERROR;
    }
		  
    if (strcmp(buffer,"NU")==0) { // NAME NON UNIQUE
      printf("ERREUR : ce noeud exporte déjà un "
	     "device de nom '%s'\n",nom_de_device);
      return ERROR;
    }

    if (strcmp(buffer,"PN")==0) { // PATH NON UNIQUE
      printf("ERREUR : ce noeud exporte déjà un device "
	     "dont le path est '%s'\n",path_device);
      return ERROR;
    }

    printf("add_device : ERREUR - SMDD ne peut pas ajouter le device %s\n",
	   nom_de_device);
    printf("add_device : code retourné = %.2s\n", buffer);
    printf("add_device : (vérifiez que le démon smdd tourne correctement)\n");
    return ERROR;
  }
  else 
    printf("add_device : ERREUR - le device %s est invalide "
	   "(taille en secteurs = %d)\n",
	   nom_de_device, nb_sectors);
  return SUCCESS;
}

int del_device( char *nom_du_device) {
  int to_smdd_pipe;
  int from_smdd_pipe;
  char buffer_emission[MAXSIZE_BUFFER+1];
  char buffer_reception[MAXSIZE_BUFFER+1];
  
  printf("Demande de suppression du dev %s\n", nom_du_device);
		
  Raz_buffer(buffer_emission);
  Raz_buffer(buffer_reception);
				
  // on envoie la commande au smdd
  to_smdd_pipe=open(EXPORTE2SMDD,O_RDWR);
  sprintf(buffer_emission,"D %s",nom_du_device);
  printf("commande envoyée = <%s>\n", buffer_emission);

  write(to_smdd_pipe,buffer_emission,strlen(buffer_emission));
  close(to_smdd_pipe);

  // on attend sa réponse
  from_smdd_pipe=open(SMDD2EXPORTE,O_RDONLY);
  read(from_smdd_pipe,buffer_reception,MAXSIZE_BUFFER);
		
  if (strcmp(buffer_reception,"OK")==0) {
    printf("Device %s supprimé\n",nom_du_device);
    return SUCCESS;
  }
  else
    if (strcmp(buffer_reception,"ERROR")==0) {
      printf("del_device : ERREUR - Le device %s est inconnu de smdd (erreur de frappe ?)\n",nom_du_device);
      return ERROR;
    }
    else {
      printf("del_device : ERREUR - Vérifiez que le démon smdd tourne correctement\n");
      return ERROR;
    }
		
  return SUCCESS;
}

void usage(void) {
  printf("USAGE :\n\n"
	 ". smd_exporte\n"
	 "  Affiche la liste des devices exportés par ce noeud\n\n"
	 ". smd_exporte -a DEVPATH DEVNAME\n"
	 "  Ajoute le device de chemin DEVPATH à la liste des devices\n"
	 "  exportés par ce noeuds. L'utilisateur donne un nom (DEVNAME)\n"
	 "  au device. C'est sous ce nom qu'apparaîtra le device sur le\n"
	 "  noeud distant (le noeud client). Ce nom doit être unique sur\n"
	 "  le noeud local (le noeud serveur)\n\n" 
	 ". smd_exporte -d DEVNAME\n"
	 "  Enlève le device de nom DEVNAME de la liste des devices\n"
	 "  exporté par ce noeud\n\n");
  printf(". Exemples :\n"
	 "\t> ./smd_exporte -a /dev/hda6 disqueATA\n"
	 "\texporte le device '/dev/hda6' sous le nom de 'disqueATA'\n"
	 "\t> ./smd_exporte -d disqueATA\n"
	 "\tenlève le device 'disqueATA' de la liste des dev exportés\n");
  printf("\n\n** %s \n** v%s (%s)\n",__FILE__,SMDEXPORT_VER,__DATE__);
}

int main(int argc, char **argv) {
  char c;

  // commande qui donne la liste des devices exportés
  if (argc==1) 
    return exported_devices();

  c=getopt(argc,argv,"adh");

  switch (c) {
  case 'a':
    // commmande d'exportation d'un nouveau device
    if (argc - optind != 2) {
      usage();
      return ERROR;
    }
    return add_device(argv[optind], argv[optind+1]);
  case 'd':
    // commande de retrait d'un device
    if (argc - optind != 1) {
      usage();
      return ERROR;
    }
    return del_device(argv[optind]);
  case 'h':
    // demande l'aide en ligne
    usage();
    break;
  default:
    // erreur dans la ligne de commande : on écrit comment l'utiliser
    usage();
  }

  return SUCCESS;
}

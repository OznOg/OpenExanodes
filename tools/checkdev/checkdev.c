#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

// on accède directement à un /dev/... mais ça n'empêche pas que les I/O sont tamponnées par le buffer-cache de Linux
#define taille_bloc 512

int main(int argc, char *argv[]) {
  int fd,pos_io;
  char s[taille_bloc+1]; // +1 pour pouvoir ajouter le caractère '\0' de fin de chaine
  long nb_char=0;
  long taille_nombre;
  long nb_char_ecrits_total=0;
  long nb_char_lus_total=0;
  int i;
  long nb_nombre;
  int nb_max=100;
  char *device_name;
  char str_nombre[100];
  int retval;
  unsigned int taille;
  char rep='n';
  long nb_car_a_lire;
  int bad=0, nb_ecrits;

  

  if (argc != 2) {
    fprintf(stderr, "usage : %s <nom du dev>\n", argv[0]);
    return 1;
  }
  device_name = argv[1];
  printf("device name =%s\n", device_name);

  if ((fd = open(device_name,O_RDWR)) <0) {
    printf("Erreur à l'ouverture du device %s",device_name);
    exit(-1);
  } 
  printf("Ouverture de %s terminée\n",device_name);
  
  
  printf("Test taille\n");
  retval = ioctl(fd,BLKGETSIZE,&taille);
  if (retval !=0) 
    printf("Erreur dans le ioctl BLKGETSIZE, taille =%d\n",taille);
  else printf("Ioctl BLKGETSIZE, taille =%d\n",taille);
  
  
  printf("Test d'écriture et de lecture\nEntrez le nombre d'entier à écrire : ");
  scanf("%ld",&nb_nombre);
  printf("Voulez-vous un affichage de ce qui est écrit et de ce qui est relu ? (o/n) : ");
  scanf("\n%c",&rep);

  printf("\n");

  printf("Ecriture de la chaine 1 2 3 4 ... %ld sur %s\n",nb_nombre,device_name);

  pos_io=lseek(fd,0,SEEK_SET);
  
  for (i=1;i<=nb_nombre;i++) {
    taille_nombre=sprintf(str_nombre,"%ld ",i);
    nb_ecrits = write(fd,str_nombre,taille_nombre);
    if (nb_ecrits != taille_nombre) {
      fprintf(stderr,"erreur à l'écriture de '%s', return = %d\n", 
	      str_nombre,
	      nb_ecrits);
    }
    nb_char_ecrits_total+=taille_nombre;
    str_nombre[taille_nombre]='\0';
    if (rep=='o') printf("%s",str_nombre);
  }
  if (rep=='o') printf(".\n");

  printf("Ecriture terminée : %ld caractères écrits\n",nb_char_ecrits_total);

  printf("\n");
  printf("Test de lecture : par blocs de %ld caractères\n",taille_bloc);
  lseek(fd,0,SEEK_SET);
  for (i=0;i<nb_char_ecrits_total;i+=taille_bloc) {

    if (i+taille_bloc>nb_char_ecrits_total) 
      nb_car_a_lire=nb_char_ecrits_total-i;
    else nb_car_a_lire=taille_bloc;

    nb_char=read(fd,s,nb_car_a_lire); 
    nb_char_lus_total+=nb_char;
    s[nb_char]='\0';
    if (rep=='o') printf("%s",s);
  }
  if (rep=='o') printf(".\n");
  printf("Lecture terminée : %ld caractères lus\n",nb_char_lus_total);
 
  printf("\n");
  printf("fsync du fichier\n");
  retval = fsync(fd);
  if (retval == 0) 
    printf("fsync ok\n");
  else 
    fprintf(stderr, "erreur fsync\n");

  printf("\n");
  printf("Test de lecture : lecture des %ld entiers et vérif que chaque entier est correct\n",nb_nombre);
  lseek(fd,0,SEEK_SET);
  for (i=1;i<nb_nombre;i++) {
    taille_nombre=sprintf(str_nombre,"%ld ",i);
    nb_char=read(fd,s,taille_nombre);
    if (nb_char!=taille_nombre) {
      printf("Erreur dans la lecture du nombre %ld : "
	     "taille voulue = %ld, taille lue = %ld\n",
	     i,taille_nombre,nb_char);
      bad=1;
    }
    else {
      s[nb_char]='\0';
      str_nombre[nb_char]='\0';
      if (strcmp(s,str_nombre)!=0) {
	printf("Erreur dans la lecture du nombre %ld : "
	       "str voulue = <%s>, str obtenue = <%s>\n",
	       i,str_nombre,s);
	bad=1;
      }
    }
    if (bad) break;
  }
  
  printf("Lecture de contrôle terminée : ");
  if (bad == 0) printf("** SUCCES ! **\n");  else printf("** ECHEC ! **\n");

  printf("\n");

  printf("Fermeture du fichier\n");
  if (close(fd) !=0) {
    printf("Erreur détectée à la fermeture");
  }

  printf("Fin du programme\n");
  return(0);
}


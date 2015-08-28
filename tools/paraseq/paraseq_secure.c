#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <malloc.h>
#include <time.h>

// on accède directement à un /dev/... mais ça n'empêche pas que les I/O sont tamponnées par le buffer-cache de Linux
#define taille_bloc 512

int main(int argc, char *argv[]) {
  int fd,pos_io;
  //char s[taille_bloc+1]; // +1 pour pouvoir ajouter le caractère '\0' de fin de chaine
  long nb_char=0;
  long taille_nombre;
  long nb_char_ecrits_total=0;
  long nb_char_lus_total=0;
  int i;
  long nb_nombre;
  int nb_max=100;
  char str_nombre[100];
  char strtype[20];
  int retval;
  unsigned int taille;
  char rep='n';
  long nb_car_a_lire;
  int bad=0;
  char type, type_io;
  long nb_io, io_size, incraff;
  long long offset;
  char *s;
  struct timeval tdeb, tfin;
  int nb_retries;

  printf("Début\n");
  /*if ((fd = open(argv[1],O_RDWR)) <0) {
   * printf("Erreur à l'ouverture du device %s",argv[1]);
   *exit(-1);
   *} 
   *printf("Ouverture de %s terminée\n",argv[1]);
   */
  
  printf("Test taille\n");
  retval = ioctl(fd,BLKGETSIZE,&taille);
  if (retval !=0) 
    printf("Erreur dans le ioctl BLKGETSIZE, taille =%d\n",taille);
  else printf("Ioctl BLKGETSIZE, taille =%d\n",taille);
  
  printf("lecture ou écriture ? (l/e): ");
  scanf("%c",&type_io);
  printf("Entrer le nombre d'E/S : "); 
  scanf("%ld",&nb_io);
  printf("Entrer la tailles des E/S (en Ko) : ");
  scanf("%ld",&io_size);
  printf("E/S aléatoires ou séquentielles ? (a/s): ");
  scanf("\n%c",&type);
  printf("Incrément d'affichage ? (1000 ?): ");
  scanf("%ld",&incraff);
  
  
  printf("\n");
 
  switch(type_io) {
  case 'l' :
    sprintf(strtype,"lectures");
    break;
  case 'e' :
    sprintf(strtype,"ecritures");
    break;
  default :
    printf("ERREUR : mauvais type d'E/S !\n");
    exit(-1);
  }
   
  
 
  switch(type) {
  case 'a' :
    printf("lancement de %ld %s aléatoires de taille = %ld Ko\n",
	   nb_io,strtype, io_size);
    break;
  case 's' :
    printf("lancement de %ld %s séquentielles de taille = %ld Ko\n",
	   nb_io, strtype, io_size);
    break;
  default :
    printf("ERREUR : mauvais type de pattern d'accès !\n");
    exit(-1);
  }
   
  s = malloc(io_size * 1024);

  offset = 0;
  gettimeofday(&tdeb, NULL);
  for (i=0; i<nb_io; i++) {
    if (type == 'a') offset = (rand()%200000) * 8 * 1024;
    //printf("lecture %ld, offset =%lld, taille= %ld o\n",i, offset,io_size*1024 );
    if ((fd = open(argv[1],O_RDWR)) >=0) {
      pos_io=lseek(fd,offset,SEEK_SET);
      if (type_io == 'l') {
	//nb_retries=0;
	nb_char=read(fd,s,io_size*1024);
	//while (nb_char != io_size*1024) {
	if (nb_char != io_size*1024)
	  fprintf(stderr, "erreur de lecture\n");
	//fprintf(stderr, "retry %d\n",++nb_retries);
	//nb_char=read(fd,s,io_size*1024);
      }
      else {
	nb_char=write(fd,s,io_size*1024);
	if (nb_char != io_size*1024) {
	  fprintf(stderr, "erreur d'écriture\n");
	  break;
	}
      }
      if (type == 's') offset += nb_char;
      if (i%incraff == 0 && i!=0) {
	double temps_ecoule;
	static int oldnb=0;
	int nbio;
	
	gettimeofday(&tfin,NULL);
	temps_ecoule =  (tfin.tv_sec - tdeb.tv_sec)*1000.0 
	  + (tfin.tv_usec - tdeb.tv_usec)/1000.0;
	nbio = i - oldnb;
	
	printf("%ld %s. %.2f IO/s et %.2f Mo/s\n",
	       i,
	       strtype,
	       nbio/(temps_ecoule/1000),
	       ((nbio*1.0)*(io_size/1000.0))/(temps_ecoule/1000.0),
	       temps_ecoule);
	gettimeofday(&tdeb, NULL);
	oldnb = i;
      }
      //printf("%lld ",offset);
      retval = close(fd);
      if (retval != 0) 
	fprintf(stderr, "erreur dans le close !\n");
    }
    //printf("\n");
  }
  free(s);
  printf("%d I/O terminées\n", i);

  //printf("Fermeture du fichier\n");
  //if (close(fd) !=0) {
  //  printf("Erreur détectée à la fermeture");
  //}

  printf("Fin du programme\n");
  return(0);
}


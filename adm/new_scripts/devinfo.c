#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <math.h>
#include "dataaccess.h"
#include "uuid.h"
#include "vrt_superblock.h"

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define FALSE 0
#define TRUE 1

struct sb_group sbg_aux;

//! retrouve le uuid du dev dont le path est 'dev_path'. Renvoie EXIT_SUCCESS/FAILURE.
int getuuid(__u32 dev_uuid[], char *dev_path) {
  if (load_sbg(&sbg_aux, dev_path) == EXIT_FAILURE)
    return EXIT_FAILURE;
  
  cpy_uuid(dev_uuid, sbg_aux.rdev_uuid);

  return EXIT_SUCCESS;
}
  

int main(int argc, char **argv) {
  int retval;
  __u64 nb_KB; 
  int major, minor;  
  struct stat info;
  char *dev_path;
  int needuuid = FALSE;
  char option;
  __u32 dev_uuid[4];

  while ((option = getopt(argc, argv, "u")) != -1) {
    switch (option) {
    case 'u':
      needuuid = TRUE;
      break;
    case '?':
      printf("Option inconnue\n");
      return EXIT_FAILURE;
    } 
  }   


  dev_path = argv[optind];
  
  if (stat(dev_path, &info) <0) {
    perror("stat");
    return EXIT_FAILURE;
  }
  
  if (S_ISBLK(info.st_mode) == FALSE) {
    fprintf(stderr, "ERREUR : dev '%s' n'est pas un BLOCK DEVICE\n",
	    dev_path );
    return EXIT_FAILURE;
  }      
  
  retval = get_nb_kbytes(dev_path,&nb_KB);
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "ERREUR : échec open, getsize, "
	    "major ou minor number sur 'dev' %s\n",
	    dev_path);
    return EXIT_FAILURE;
  }
      
      
  get_major_minor(&info,&major,&minor);

  printf("%lld %d %d ", nb_KB, major, minor);
  
  if (needuuid == TRUE) {
    if (getuuid(dev_uuid,dev_path) == EXIT_FAILURE) {
      fprintf(stderr, "ERREUR : impossible de retrouve l'UUID du dev '%s'\n",
	      dev_path);
      return EXIT_FAILURE;
    }
    printf("%u %u %u %u\n", dev_uuid[3], dev_uuid[2], dev_uuid[1], dev_uuid[0]);
  }
  else printf("\n");

  return EXIT_SUCCESS;
}

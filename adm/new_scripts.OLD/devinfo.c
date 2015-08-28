#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <math.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

#define FALSE 0


//! extrait le major number et le minor number d'une struct stat
void get_major_minor(struct stat *info, int *major, int *minor) {
  *major = info->st_rdev >> 8;
  *minor = info->st_rdev & 0xff;
}

//! renvoie le nombre de kilo octets qu'on peut stocker sur le device
int get_nb_kbytes(char *nom_de_device, __u64 *nb_kbytes) {
  
  __u64 nb_sectors=-1;
  __u32 nb_sectors32 = -1;
  int sector_size=-1;
  int retval;
  int fd;

  if ((fd = open(nom_de_device,O_RDWR)) <0) {
    perror("open");
    return EXIT_FAILURE;
  }

  // TODO : 
  // BLKGETSIZE64 n'est pas défini dans les include
  // => pourquoi ?
  /*
  retval = ioctl(fd,BLKGETSIZE64,&nb_sectors);
  if (retval <0) {
    __u32 aux;
    perror("ioctl BLKGETSIZE64 failed. Trying ioctl BLKGETSIZE");
    retval = ioctl(fd,BLKGETSIZE,&aux);
    if (retval <0) {
      perror("ioctl BLKGETSIZE");
      return EXIT_FAILURE;
    }

    nb_sectors = aux;
    }*/

  retval = ioctl(fd,BLKGETSIZE,&nb_sectors32);
  if (retval <0) {
    perror("ioctl BLKGETSIZE");
    return EXIT_FAILURE;
  }
  nb_sectors = nb_sectors32;
  
  retval = ioctl(fd,BLKSSZGET,&sector_size);
  if (retval <0) {
    perror("ioctl BLKSSZGET");
    return EXIT_FAILURE; 
  }
 
  if (close(fd) <0) {
    perror("close");
    return EXIT_FAILURE;
  }
  

  /*printf("nb sect32=%d nb_sect64 = %lld size sect=%d kb_size=%lld\n", 
	 nb_sectors32,
	 nb_sectors,
	 sector_size,
	 (__u64) (floor((nb_sectors/1024.0)*sector_size)));
  */
  *nb_kbytes = (__u64) (floor((nb_sectors/1024.0)*sector_size));
  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  int retval;
  __u64 nb_KB; 
  int major, minor;  
  struct stat info;
  char *dev_name;

  dev_name = argv[1];
  
  if (stat(dev_name, &info) <0) {
    perror("stat");
    return EXIT_FAILURE;
  }
  
  if (S_ISBLK(info.st_mode) == FALSE) {
    fprintf(stderr, "dev '%s' n'est pas un BLOCK DEVICE\n",
	    dev_name );
    return EXIT_FAILURE;
  }      
  
  retval = get_nb_kbytes(dev_name,&nb_KB);
  if (retval == EXIT_FAILURE) {
    fprintf(stderr, "échec open, getsize, "
	    "major ou minor number sur 'dev' %s\n",
	    dev_name);
    return EXIT_FAILURE;
  }
      
      
  get_major_minor(&info,&major,&minor);

  printf("%lld %d %d\n", nb_KB, major, minor);
  
  return EXIT_SUCCESS;
}

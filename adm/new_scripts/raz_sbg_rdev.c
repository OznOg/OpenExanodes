#include <dirent.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <math.h>
#include <string.h>
#include <asm/types.h>

#include "dataaccess.h"

//! raz du SBG du rdev dont le path est passé en paramètre. ATTENTION à ne pas se tromper dans l'argument car risque de bousiller une partition.
int main(int argc, char **argv) {  
  int retval;

  retval = raz_sbg_rdev(argv[1]);
  if (retval == EXIT_FAILURE) {
    fprintf(stderr,"can't raz SBG of rdev '%s'\n",argv[1]); 
  }
  exit(retval);
  
}

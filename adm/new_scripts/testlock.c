#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
  int lfp;

  lfp=open(argv[1],O_RDWR|O_CREAT,0640);
  if (lfp<0) exit(1); 
  if (lockf(lfp,F_TLOCK,0)<0)
    printf("O");
  else printf("N");
 
  close(lfp);

  return 0;
}

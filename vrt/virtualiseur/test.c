#include <linux/kernel.h>



int toto(const char *s, int a, int b) {
  int i;
  i = sscanf(s,"%d %d", &a, &b);
  printk(KERN_ERR "%d %d",a,b);

  return a+b;
}

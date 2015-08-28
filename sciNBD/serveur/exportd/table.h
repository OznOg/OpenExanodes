#ifndef _SMD_TABLE
#define _SMD_TABLE

#include <asm/types.h>

extern __u32 unused_elt_indice(void *table[], __u32 size);
extern void *get_elt(__u32 n, void *table[], __u32 size);
extern void nullify_table(void *table[], __u32 size);
extern void free_table(void *table[], __u32 size);

#endif

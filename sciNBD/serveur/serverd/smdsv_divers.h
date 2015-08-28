#ifndef _SMDSV_DIVERS
#define _SMDSV_DIVERS
#include <asm/types.h>

extern void ecrire_stats(void);
extern void afficher_donnees(unsigned long offset, char *donnees, long taille);
extern __u32 indice_elt(void *elt, void *table[], __u32 size);
extern __u32 unused_elt_indice(void *table[], __u32 size);
extern void *get_elt(__u32 n, void *table[], __u32 size);
extern void nullify_table(void *table[], __u32 size);
extern void vfree_table(void *table[], __u32 size);

#endif

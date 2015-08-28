
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <asm/div64.h>

//! renvoie le n-ième élément non nul de la table (attention : le 1er élément a l'indice 0). Renvoie NULL si cet élément n'existe pas 
void *get_elt(u32 n, void *table[], u32 size) {
  u32 i;
  u32 count=n;

  for (i=0; i<size; i++) 
    if (table[i] != NULL) {
      if (count == 0) return table[i];
      count--;
    }
  
  return table[i];
}

//! renvoie l'indice d'un élt libre du tableau vs.groups. renvoie max_size si le tableau est entièrement rempli 
u32 unused_elt_indice(void *table[], u32 size) {
  u32 i;
  for (i=0; i<size; i++)
    if (table[i] == NULL) return i;
  
  return i;
}

//! met tous les pointeurs de la table à NULL
void nullify_table(void *table[], u32 size) {
  u32 i;

  for (i=0; i<size; i++) 
    table[i] = NULL;
}

//! libère la mémoire des objets pointés par les elts de la table.
void vfree_table(void *table[], u32 size) {
  u32 i;
  if (table != NULL) {
    for (i=0; i<size; i++) 
      if(table[i] != NULL) vfree(table[i]); 
  }
}

//!renvoie le quotient a/q arrondi à l'entier inférieur
/*! do_div est une macro et met a/q dans a
 */
u64 quotient64(u64 a, u32 q) {
  do_div(a,q);
  return a;
}

//! renvoie le quotient a/q arrondi à l'entier supérieur
/*! do_div est une macro et met a/q dans a
 */
u64 quotient_ceil64(u64 a, u32 q) {
  if (do_div(a,q) == 0) 
    return a; 
  else return a+1;
}
  
//! renvoie le reste de a/q
u32 reste64(u64 a, u32 q) {
  return do_div(a,q);
}



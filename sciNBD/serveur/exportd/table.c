#include <asm/types.h>
#include <malloc.h>

//! renvoie le n-i�me �l�ment non nul de la table (attention : le 1er �l�ment a l'indice 0). Renvoie NULL si cet �l�ment n'existe pas 
void *get_elt(__u32 n, void *table[], __u32 size) {
  __u32 i;
  __u32 count=n;

  for (i=0; i<size; i++) 
    if (table[i] != NULL) {
      if (count == 0) return table[i];
      count--;
    }
  
  return table[i];
}

//! renvoie l'indice d'un �lt libre du tableau vs.groups. renvoie max_size si le tableau est enti�rement rempli 
__u32 unused_elt_indice(void *table[], __u32 size) {
  __u32 i;
  for (i=0; i<size; i++)
    if (table[i] == NULL) return i;
  
  return i;
}

//! met tous les pointeurs de la table � NULL
void nullify_table(void *table[], __u32 size) {
  __u32 i;

  for (i=0; i<size; i++) 
    table[i] = NULL;
}

//! lib�re la m�moire des objets point�s par les elts de la table.
void free_table(void *table[], __u32 size) {
  __u32 i;
  if (table != NULL) {
    for (i=0; i<size; i++) 
      if(table[i] != NULL) free(table[i]); 
  }
}

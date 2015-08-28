#include "smd_server.h"
#include "constantes.h"
#include <stdio.h>
#include <asm/types.h>
#include <malloc.h>
#include <time.h>


#ifdef PERF_STATS
void ecrire_stats(void) {
  FILE *statfile=NULL;
  time_t t;

 statfile=fopen(STAT_FILE,"a"); 
 time(&t);
 fprintf(statfile,"\nFlushing stats at ");
 fprintf(statfile,ctime(&t));
 
 fprintf(statfile,"E/S (temps de service) : debit moyen = %.2f Mo/s "
	 "(%.2f Mo en %.2f s).\n",
	 (smdsv.somme_transferts)/(smdsv.somme_durees * 1000.0),
	 smdsv.somme_transferts/(1024*1024),
	 smdsv.somme_durees/1000);

 fprintf(statfile,"E/S (temps de transfert disque) : %ld "
	 "LECTURES = %.2f Mo/s, %ld ECRITURES = %.2f Mo/s \n",
	 smdsv.nb_transferts_lecture,
	 smdsv.somme_debit_lecture/smdsv.nb_transferts_lecture,
	 smdsv.nb_transferts_ecriture,
	 smdsv.somme_debit_ecriture/smdsv.nb_transferts_ecriture);

 if (smdsv.nb_transferts_sci != 0) 
 fprintf(statfile,"E/S (temps de transfert DMA sci) : %ld "
	 "LECTURES = %.2f Mo/s (ECRITURES non prises en compte) \n",
	 smdsv.nb_transferts_sci,
	 smdsv.somme_debit_sci/smdsv.nb_transferts_sci);
 else fprintf(statfile,"E/S (temps de transfert DMA sci) : pas "
	      "de lectures => pas de stat\n");
 
 fprintf(statfile,"Taille moy (bytes): LECTURES = %.2f, ECRITURES = %.2f, "
	 "global = %.2f\n",
	 smdsv.somme_tailles_lecture/smdsv.nb_transferts_lecture,
	 smdsv.somme_tailles_ecriture/smdsv.nb_transferts_ecriture,
	 smdsv.somme_transferts/smdsv.nb_io);
 
 fprintf(statfile, "\n"); 
 fclose(statfile);
}
#endif

//! affiche les données transférer dans le log file (bonjour les Mo de traces!)
void afficher_donnees(unsigned long offset, char *donnees, long taille) {
  long no_octet, i;
  int taille_ligne=32;

  
  logfile=fopen(LOG_FILE,"a"); 
  for (no_octet=0; no_octet<taille; no_octet+=taille_ligne) {
    fprintf(logfile,"0x%08lx :", offset+no_octet);
    for (i=0; i<taille_ligne && no_octet+i<taille; i++) 
      fprintf(logfile," %02x",(unsigned char) *(donnees+no_octet+i));
    fprintf(logfile," | ");
    for (i=0; i<taille_ligne && no_octet+i<taille; i++) 
      if (*(donnees+no_octet+i)<32 || *(donnees+no_octet+i)==0x7f) fprintf(logfile," ");
      else fprintf(logfile,"%c", (unsigned char) *(donnees+no_octet+i));
    fprintf(logfile,"\n");
  }   
  fclose(logfile);
}


//! renvoie le n-ième élément non nul de la table (attention : le 1er élément a l'indice 0). Renvoie NULL si cet élément n'existe pas 
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

//! renvoie l'indice d'un élt libre du tableau vs.groups. renvoie max_size si le tableau est entièrement rempli 
__u32 unused_elt_indice(void *table[], __u32 size) {
  __u32 i;
  for (i=0; i<size; i++)
    if (table[i] == NULL) return i;
  
  return i;
}

//! met tous les pointeurs de la table à NULL
void nullify_table(void *table[], __u32 size) {
  __u32 i;

  for (i=0; i<size; i++) 
    table[i] = NULL;
}

//! libère la mémoire des objets pointés par les elts de la table.
void free_table(void *table[], __u32 size) {
  __u32 i;
  if (table != NULL) {
    for (i=0; i<size; i++) 
      if(table[i] != NULL) free(table[i]); 
  }
}

//! renvoie la position qui contient l'elt "elt" (comparaison de pointeur). renvoie size s'il ne le trouve pas
__u32 indice_elt(void *elt, void *table[], __u32 size) {
  __u32 i;
  for (i=0; i<size; i++)
    if (table[i] == elt) return i;
  return size;
}

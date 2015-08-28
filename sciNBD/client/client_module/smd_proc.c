/*!\file smd_proc.c
  \brief définit les fonctions associées à /proc/scimapdev/ndevs_enregistres
*/
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include "scimapdev.h"
#include "smd_ops.h"
#include "parser.h"
#include "smd_request.h"
#include "smd_divers.h"



extern void (*smd_new_brique)(int no_brique, long taille);
extern int smd_add_snode(char *nom_node, 
			 long no_sci);

extern int smd_add_ndev(servernode_t *sn, 
			char *nom_device, 
			char *path_device,
			int devnb,
			u64 nb_sectors);
extern int new_snode(char *name);
extern int smd_add_snode_sci(servernode_t *sn, long no_sci);
extern int cleanup_sci_sn(servernode_t *sn, int csession_flag);

#ifdef PERF_STATS
extern void synchro_busy_stats(void);
#endif


//! gère la lecture du fichier /proc/scimapdev/data_etat
int smd_proc_read_data_etat(char *buf, 
			    char **start, 
			    off_t offset, 
			    int count, 
			    int *eof, 
			    void *data) {
  int len=0;
  int i;
  u32 nb_reads_writes = 0;
  u64 bytes_transfered = 0;
  u32 b_nb_reads[NBMAX_SNODES],b_nb_writes[NBMAX_SNODES];
  u64 b_bytes_transfered[NBMAX_SNODES];

#ifdef PERF_STATS
  u64 g_busy_time, b_busy_time[NBMAX_SNODES];
#endif  
  servernode_t *sn;

  // SNAPSHOT DES STATS avant les sprintf pour éviter les distortions
  // dans l'état si par exemple il y a des modifs de l'état pendant que 
  // cette procedure /proc s'exécute (à cause de l'arrivée/fin 
  // de requêtes I/O)
#ifdef PERF_STATS
  // maj des stats
  synchro_busy_stats();
#endif  
  for (i=0; i < NBMAX_SNODES; i++) {
    if (smd.snodes[i] != NULL) {
      sn = smd.snodes[i];
      b_nb_reads[i] = sn->nb_reads;
      b_nb_writes[i] = sn->nb_writes;
      b_bytes_transfered[i] = sn->request_bytes_transfered;
#ifdef PERF_STATS
      b_busy_time[i] = sn->busy_time;
#endif  
    }
  }

#ifdef PERF_STATS
  g_busy_time = smd.busy_time;
#endif  
    

  // création de la string
  len += sprintf(buf+len,"%d ",smd.nb_snodes);
  for (i=0; i < NBMAX_SNODES; i++) {
    if (smd.snodes[i] != NULL) {
    nb_reads_writes += b_nb_reads[i] + b_nb_writes[i];
    bytes_transfered += b_bytes_transfered[i];	
    

#ifdef PERF_STATS
    len += sprintf(buf+len,"%lld ", 
		   quotient64(b_busy_time[i],1000));
#endif  
    }
  }
#ifdef PERF_STATS
  len += sprintf(buf+len,"%lld ",
		 quotient64(g_busy_time,1000));
#endif  
  
  len += sprintf(buf+len,"%lld %d ",
		 bytes_transfered,
		 nb_reads_writes);
  *eof=1;
  return len;
}

//! gère la lecture du fichier /proc/scimapdev/data_snodes
int smd_proc_read_data_ndevs(char *buf, 
			     char **start, 
			     off_t offset, 
			     int count, 
			     int *eof, 
			     void *data) {
  int len=0;
  int i;
  servernode_t *sn;

  //infos + précises pour chaque snodes
  len += sprintf(buf+len,"%d ",smd.nb_snodes);
  for (i=0; i < NBMAX_SNODES; i++) {
    if (smd.snodes[i] != NULL) {
      sn = smd.snodes[i];
      len += sprintf(buf+len,"%d %s %ld %lld %d ",
		     i,
		     sn->name,
		     sn->no_sci,
		     quotient64(sn->ndevs[0]->size,1024*1024),
		     reste64(sn->ndevs[0]->size,1024*1024)/1024);
      //TODO : MODIFIER LES 2 LIGNES CI-DESSUS pour le multi-devices
    }
  }

  *eof=1;
  return len;
}

//! fonction appelée lorsqu'une application lit /proc/scimadev/ndevs_enregistres.
/*!
  Renvoie la liste des ndevs enregistrées (sous forme de texte) 
  dans un buffer géré par le noyau.
*/
int smd_read_proc(char *buf, 
		  char **start, 
		  off_t offset, 
		  int count, 
		  int *eof, 
		  void *data) {
  int i, j, len=0;
  //int limite = count - 80;
  int limite = 4096-80; // taille d'une page mémoire - 1 ligne de caractères
  unsigned long nb_reads_writes = 0;
  u64 bytes_transfered = 0;
  long moyenne;
  servernode_t *sn;
  ndevice_t *nd;

#ifdef PERF_STATS
  // maj des stats
  synchro_busy_stats();
#endif  

  len += sprintf(buf+len,"%d snode(s) used. %lld reqs processed\n\n",
		 smd.nb_snodes,
		 smd.nb_req);


  for (i=0; i < NBMAX_SNODES && len <= limite; i++) {
    if (smd.snodes[i] != NULL) {
      sn = smd.snodes[i];
      len += sprintf(buf+len,"Snode '%s' :\tSCI node = %ld, "
		     "status=%d busy=%d\n",
		     sn->name,
		     sn->no_sci,
		     sn->status,
		     sn->busy);

      if ((sn->creq.kreq != NULL) && (sn->busy != IDLE)) {
	struct request *req = sn->creq.kreq;
	int minor = DEVICE_NR(req->rq_dev);
	nd = minor2ndev[minor];
	len += sprintf(buf+len,"current req : kreq=0x%p nd=%s sector=%lld "
		       "size=%ld bytes\n",
		       req,
		       nd->name,
		       ((u64) req->sector) * SMD_HARDSECT,
		       req->nr_sectors * SMD_HARDSECT);
      } else
	len += sprintf(buf+len,"current req : NONE\n");

      for (j=0; j<NBMAX_NDEVS; j++) {
	if (sn->ndevs[j] != NULL) {
	  nd = sn->ndevs[j];
	  len += sprintf(buf+len, "\tndev %d : device name = %s, "
			 "size = %lld KB\n"
			 "\t\tremote path = %s\n"
			 "\t\tlocal path = %s\n",
			 nd->localnb,
			 nd->name,
			 nd->size,
			 nd->rpath, 
			 nd->lpath); 
	}
      }
      
      len += sprintf(buf+len,"\n");
      
      moyenne = -1;
      if (sn->nb_reads + sn->nb_writes != 0)
	moyenne = quotient64(sn->request_bytes_transfered, 
			     (sn->nb_reads + 
			      sn->nb_writes));
     
      len += sprintf(buf+len,"\t%lld bytes transfered in %ld READs, "
		     "%ld WRITEs (req mean size = %ld)\n",
		     sn->request_bytes_transfered,
		     sn->nb_reads,
		     sn->nb_writes,
		     moyenne);
      nb_reads_writes += sn->nb_reads 
	+ sn->nb_writes;
      bytes_transfered += sn->request_bytes_transfered;	
      
#ifdef PERF_STATS
      moyenne = -1;
      if (sn->somme_durees_PIO != 0)
	moyenne = quotient64(sn->somme_tailles_PIO,
			     sn->somme_durees_PIO);  
      
      len += sprintf(buf+len,"\t%lld bytes (CTL & DATA) written to "
		     "briques using SCI at %ld MB/s\n",
		     sn->somme_tailles_PIO, 
		     moyenne); 
      
      moyenne = -1;
      if (sn->nb_transferts_disque != 0)
	moyenne = quotient64(quotient64(sn->somme_durees_disque,1000),
			     sn->nb_transferts_disque);
      len += sprintf(buf+len,"\t%ld disk transfers in %lld ms "
		     "(mean time: %ld ms)\n",
		     sn->nb_transferts_disque,
		     quotient64(sn->somme_durees_disque,1000),
		     moyenne);  
      
      if (smd.busy_time) {
	len += sprintf(buf+len,"\t\tbusy time= %lld %% (%lld ms)\n",
		       quotient64(sn->busy_time,smd.busy_time)*100,
		       quotient64(sn->busy_time,1000)); 
      }
      else len += sprintf(buf+len,"\t\tbusy time: SMD hasn't worked yet\n");
#endif
      
      len += sprintf(buf+len,"\n");	
    }
  }
  
  moyenne = -1;
  if (nb_reads_writes != 0)
    moyenne = quotient64(bytes_transfered,nb_reads_writes);
 
  len += sprintf(buf+len,"TOTAL :\t%lld bytes transfered in %ld requests "
		 "(req mean size = %ld)\n",
		 bytes_transfered,
		 nb_reads_writes,
		 moyenne);
  
#ifdef PERF_STATS
  len += sprintf(buf+len,"\tbusy time= %lld ms\n",
		 quotient64(smd.busy_time,1000));  
#endif
  
  *eof=1;

  return len;
}
  
void reset_sn_stats(servernode_t *sn) {
  sn->nb_reads=0;
  sn->nb_writes=0;
  sn->request_bytes_transfered=0; 	

#ifdef PERF_STATS
    sn->somme_durees_disque=0;
    sn->nb_transferts_disque=0; 
    sn->somme_tailles_PIO=0;
    sn->somme_durees_PIO=0;  
    sn->busy_time=0;
#endif
}

void reset_stats(void) {
  int i;
  for (i=0; i < NBMAX_SNODES; i++) 
    if (smd.snodes[i] != NULL) 
      reset_sn_stats(smd.snodes[i]);

#ifdef PERF_STATS
    smd.busy_time=0;
#endif
}

extern servernode_t *get_snode_from_name(char *name);

//! fonction appelée lorsqu'une application écrit dans /proc/scimapdev/ndevs_enregistres.
/*!
*/
int smd_write_proc(struct file *file, 
		   const char *buffer, 
		   unsigned long count, 
		   void *data) {


  char *ligne_de_configuration = NULL;
  int i, retval;

  int no_sci;
  char *nom_device = NULL;
  char *path_device = NULL;
  char *nom_node = NULL;
  char *commande = NULL;
  u64 nb_sectors;
  servernode_t *sn;
  u32 devnb;

  PDEBUG("Entering\n");
  ligne_de_configuration = vmalloc(TAILLE_LIGNE_CONF);
  nom_device=vmalloc(MAXSIZE_DEVNAME);
  path_device=vmalloc(MAXSIZE_PATHNAME);
  nom_node=vmalloc(MAXSIZE_DEVNAME);
  commande=vmalloc(10);
  if ((ligne_de_configuration == NULL) ||
      (nom_device == NULL) ||
      (path_device == NULL) ||
      (nom_node == NULL) ||
      (commande == NULL)) {
    PERROR("vmalloc\n");
    goto end;
  }
  

  for (i=0;i<TAILLE_LIGNE_CONF;i++) ligne_de_configuration[i]=0;
  copy_from_user(ligne_de_configuration, buffer, count);


  Extraire_mot(ligne_de_configuration, commande);

  switch(commande[0]) {
  case 'a' :    // add a ndev
    PDEBUG("reçu : '%s'\n", ligne_de_configuration);
    retval = sscanf(ligne_de_configuration, 
		    "%s %d %s %s %d %Ld",
		    nom_node,
		    &no_sci,
		    nom_device,
		    path_device,
		    &devnb,
		    &nb_sectors);

    if (retval != 6) {
      PERROR("can't parse the configuration line " 
	     "(error on arg %d)\n", retval);
      goto end;
    }
		
    PDEBUG("node name = %s, no sci = %d "
	   "device = %s (size = %lld sectors)\n",
	   nom_node,
	   no_sci,
	   nom_device,
	   nb_sectors);

    PDEBUG("adding server node\n");
    
    if (new_snode(nom_node)) {
      if (smd_add_snode(nom_node,no_sci) == ERROR) {
	PERROR("error in adding node '%s'\n", nom_node);
	goto end;
      }
    }
    
    sn = get_snode_from_name(nom_node);
    if (sn == NULL) {
      PERROR("can't get pstruct for sn '%s'\n", nom_node);
      goto end;
    }     
    
    if (sn->status == DECONNEXION) {
      PDEBUG("SCI disconnection from snode '%s'\n",nom_node);
      sn->status = DECONNEXION_CAN_SLEEP;
      PDEBUG("SCI node %ld : DECONNEXION -> DECONNEXION_CAN_SLEEP\n",
	     sn->no_sci);
      if (cleanup_sci_sn(sn, NO_FLAGS) == ERROR) {
	PERROR("SCI disconnection failed\n");
      }
      sn->status = DOWN;
    }

    if (sn->status == DOWN) {
      PDEBUG("SCI reconnection to snode '%s'\n",nom_node);
      if (smd_add_snode_sci(sn, no_sci) == ERROR) {
	PERROR("SCI reconnection failed\n");
	goto end;
      }
      //sn->status = UP;
    }

    if (smd_add_ndev(sn, 
		     nom_device, 
		     path_device,
		     devnb,
		     nb_sectors) == ERROR) {
      PERROR("error in adding ndev '%s' (path=%s)\n", 
	     nom_device,
	     path_device);
      goto end;
    }

    PDEBUG("adding networked device\n");
    break;
	  
  case 'r' : // reset stats
    PDEBUG("reseting stats\n");
    // FIXME : ce reset des var de stats est effectué aussi à 
    // l'init => mettre ca dans une proc
    reset_stats();

    break;

  default :
    PERROR("bad command\n");
  }

	
  PDEBUG("all done\n");
	
 end:
  vfree(ligne_de_configuration);
  vfree(nom_device);
  vfree(path_device);
  vfree(nom_node);
  vfree(commande);
  return count;
}


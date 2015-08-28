
// TODO : d'une manière générale, dans tous le source : mettre les messages de traces, de DEBUG, les commandes, etc... EN ANGLAIS.. actuellement c'est un horrible mélange de français et d'anglais

// TODO : je laisse les lignes suivantes comme exemple d'utilisation 'avancée' de doxygen.. faudra exploiter un peu mieux ce soft..
/*! \mainpage Le block device driver SciMapDev
 *
 * \section intro Introduction
 * Cette version de SciMapDev permet de voir rapidement :
 * bla..bla...
 *
 * \section manque Ce qu'il manque
 * bla..bla...
 *
 * \section utilisation Utilisation
 * bla.. bla..
 * Voici un exemple d'utilisation de ce BDD :
 *\verbatim
 * > make              // compile le source et crée un fichier scimapdev.o
 * > insmod scimapdev.o    // attache le module au noyau
 *\endverbatim
 * \section messsage Et maintenant : cliquez sur les liens en haut de cette page pour observer le code source.
 */

/*!\file smd_init.c
\brief définit la fonction appelée lorsqu'on charge le module scimapdev en mémoire (insmod scimapdev.o)
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include "scimapdev.h"
#include "smd_ops.h"
#include "smd_proc.h"
#include "smd_request.h"
#include "smd_divers.h"
#include <linux/config.h>

extern int init_sci(void);
extern request_queue_t *smd_find_queue(kdev_t device);

struct smdstate smd;
ndevice_t *minor2ndev[NBMAX_NDEVS];
servernode_t *itendio2snode[NBMAX_IT];
servernode_t *iterrorio2snode[NBMAX_IT];
servernode_t *itready2snode[NBMAX_IT];

int SMD_MAJOR_NUMBER;

//! création des répertoires/fichiers /proc. Renvoie ERROR/SUCCESS
int init_procfs(void) {
  if (!(smd.proc_rep = proc_mkdir("scimapdev", NULL))) {
    PERROR("can't create proc dir for scimapdev\n");
    return ERROR;
  }

  if (!(smd.proc_ndevs = create_proc_entry("ndevs_enregistres", 
					   0644, 
					   smd.proc_rep))) {
    PERROR("can't create proc entry for ndevs\n");
    return ERROR;
  }
    
  if (!create_proc_read_entry("data_etat", 
			      0, 
			      smd.proc_rep, 
			      smd_proc_read_data_etat,
			      NULL)) {
    PERROR("can't create proc entry data_etat\n");
    return ERROR;
  }

  if (!create_proc_read_entry("data_ndevs", 
			      0, 
			      smd.proc_rep, 
			      smd_proc_read_data_ndevs, 
			      NULL)) {
    PERROR("can't create proc entry data_ndevs\n");
    return ERROR;
  }


  smd.proc_ndevs->read_proc = smd_read_proc;
  smd.proc_ndevs->write_proc  = smd_write_proc;  
  return SUCCESS;
}


//! initialisation du module scimapdev
int init_module(void) {
  PDEBUG("DEMARRAGE\n");
  
  nullify_table((void *)smd.snodes, NBMAX_SNODES);
  nullify_table((void *)minor2ndev, NBMAX_NDEVS); 
  nullify_table((void *)itendio2snode, NBMAX_SNODES); 
  nullify_table((void *)itready2snode, NBMAX_SNODES); 
  nullify_table((void *)iterrorio2snode, NBMAX_SNODES); 
  smd.nb_snodes = 0;
  smd.nb_req = 0;
#ifdef PERF_STATS
  smd.busy_time=0.0;
#endif  

  SMD_MAJOR_NUMBER = register_blkdev(0, MODULE_NAME, &smd_fops); 
  if (SMD_MAJOR_NUMBER < 0) {
    PERROR("can't get a major number\n");
    return -EBUSY;
  }
  PDEBUG("%s major number is %d\n",MODULE_NAME, SMD_MAJOR_NUMBER);

  blk_dev[SMD_MAJOR_NUMBER].queue = smd_find_queue;
  read_ahead[SMD_MAJOR_NUMBER] = SMD_READAHEAD;

  blk_size[SMD_MAJOR_NUMBER] = vmalloc(sizeof(int) * NBMAX_NDEVS);
  blksize_size[SMD_MAJOR_NUMBER] = vmalloc(sizeof(int) * NBMAX_NDEVS);
  hardsect_size[SMD_MAJOR_NUMBER] = vmalloc(sizeof(int) * NBMAX_NDEVS);
  max_readahead[SMD_MAJOR_NUMBER] = vmalloc(sizeof(int) * NBMAX_NDEVS);
  max_sectors[SMD_MAJOR_NUMBER] = vmalloc(sizeof(int) * NBMAX_NDEVS); 
  
  // création du répertoire de dev de scimapdev
  smd.dev_rep = devfs_mk_dir(NULL,"scimapdev",NULL);
  PDEBUG("devfs_mk_dir(NULL,\"scimapdev\",NULL)== %p\n", smd.dev_rep);
  if (smd.dev_rep == NULL) {
    PERROR("can't create devfs rep for ze scimapdev\n");
    return -EBUSY;
  }
   
  if (init_procfs() == ERROR) {
    PERROR("can't create procfs rep & file\n");
    return -EBUSY;
  }  
  PDEBUG("/proc dir&files created\n");

  if (init_sci() == ERROR) {
    PERROR("can't init sci\n");
    return -EBUSY;
  }  
  PDEBUG("SCI initialized\n");
  
  PDEBUG("TERMINE\n");
  return 0;
}

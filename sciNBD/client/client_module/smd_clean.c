/*!\file smd_clean.c
\brief définit la fonction appelée lorsqu'on retire le module scimapdev de la mémoire (rmmod scimapdev)
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include "smd_proc.h"
#include "scimapdev.h"
#include "smd_divers.h"

extern int cleanup_sci(void);
extern int cleanup_sci_sn(servernode_t *sn, int csession_flag);

//! libère les ressources associées au net device 'nd'
int cleanup_ndev(ndevice_t *nd) {
  fsync_dev(MKDEV(SMD_MAJOR_NUMBER, nd->minor));
  PDEBUG("device [%d,%d] for ndev '%s:%s' flushed\n",
	 SMD_MAJOR_NUMBER, nd->minor,
	 nd->sn->name, nd->name);

  blk_cleanup_queue(&nd->queue);
  PDEBUG("devfs unregister(nd->dev_file =%p)\n", nd->dev_file);
  devfs_unregister(nd->dev_file);
  vfree(nd);

  return SUCCESS;
}

//! libère la structure de données + les resources prises par le serveur node 'sn'
int cleanup_snode(servernode_t *sn) {
  int i;
  int retval = SUCCESS;
  ndevice_t *nd;

  if (cleanup_sci_sn(sn, CANCEL_CB) == ERROR) 
    retval = ERROR;

  for (i=0; i<NBMAX_NDEVS; i++) 
    if (sn->ndevs[i] != NULL) {
      nd = sn->ndevs[i];
      sn->ndevs[i] = NULL;
      if (cleanup_ndev(nd) == ERROR) 
	retval = ERROR;
    }

  PDEBUG("devfs unregister(sn>dev_rep =%p)\n", sn->dev_rep);
  devfs_unregister(sn->dev_rep);
  vfree(sn);
  return retval;
}

//! fonction désallouant toutes les ressources de scimapdev.. snif snif, ce n'est qu'un auu revoiiir
void cleanup_module(void) {
  int i;
  servernode_t *sn;

  PDEBUG("DEMARRAGE\n");
  
  // on libère les fichiers et répertoires /dev, associés au driver
  for (i=0;i<NBMAX_SNODES;i++) {
    if (smd.snodes[i] != NULL) {
      sn = smd.snodes[i];
      smd.snodes[i] = NULL;
      cleanup_snode(sn);
    }
    
  }

  blk_dev[SMD_MAJOR_NUMBER].queue=NULL;

  vfree(blk_size[SMD_MAJOR_NUMBER]);
  vfree(blksize_size[SMD_MAJOR_NUMBER]); 
  vfree(hardsect_size[SMD_MAJOR_NUMBER]);
  vfree(max_readahead[SMD_MAJOR_NUMBER]);
  vfree(max_sectors[SMD_MAJOR_NUMBER]); 
  
  PDEBUG("devfs unregister(smd.dev_rep =%p)\n", smd.dev_rep);
  devfs_unregister(smd.dev_rep);
  
  // on libère les fichiers /proc associés au driver
  remove_proc_entry("ndevs_enregistres",smd.proc_rep);
  remove_proc_entry("data_ndevs",smd.proc_rep);
  remove_proc_entry("data_etat",smd.proc_rep);
  remove_proc_entry("scimapdev", NULL);
  
  cleanup_sci();
  unregister_blkdev(SMD_MAJOR_NUMBER,MODULE_NAME);
  PDEBUG("TERMINE\n");
}

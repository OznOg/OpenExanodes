/*!\file smd_clean.c
\brief définit la fonction appelée lorsqu'on retire le module virtualiseur de la mémoire (rmmod virtualiseur)
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/blkdev.h>
#include "vrt_proc.h"
#include "virtualiseur.h"
#include "vrt_group.h"

void cleanup_module(void) {
  int i;
  PDEBUG("DEBUT\n");
  
  for (i=0; i < vs.nb_groups; i++) {
    group_t *g = vs.groups[i];
    if (g != NULL) clean_group(g);
  }
  PDEBUG("groups cleaned up\n");
  
  devfs_unregister(vs.dev_rep);
  PDEBUG("devfs rep unregistered\n");

  // on libère les fichiers /proc associés au driver
  remove_proc_entry("zones_enregistrees",vs.proc_rep);
  remove_proc_entry("data_zones",vs.proc_rep);
  remove_proc_entry("data_etat",vs.proc_rep);
  remove_proc_entry("virtualiseur", NULL);
  PDEBUG("proc entries removed\n");
  
  kfree(blk_size[VRT_MAJOR_NUMBER]);
  kfree(blksize_size[VRT_MAJOR_NUMBER]);
  kfree(hardsect_size[VRT_MAJOR_NUMBER]);
  kfree(max_readahead[VRT_MAJOR_NUMBER]);
  PDEBUG("device tables freed\n");

  unregister_blkdev(VRT_MAJOR_NUMBER,MODULE_NAME);
  PDEBUG("VRT - Retrait du module Virtualiseur : TERMINE\n");
}

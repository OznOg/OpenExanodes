
/*!\file smd_init.c
\brief définit la fonction appelée lorsqu'on charge le module virtualiseur en mémoire (insmod virtualiseur.o)
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include "virtualiseur.h"
#include "vrt_ops.h"
#include "vrt_proc.h"
#include "vrt_request.h"
#include "vrt_divers.h"


static struct vrt_state vs;


char *vrt_mode;
int VRT_MAJOR_NUMBER;

int init_module(void) {
  PDEBUG("DEBUT\n");

  VRT_MAJOR_NUMBER = register_blkdev(0, MODULE_NAME, &vrt_fops); 
  if (VRT_MAJOR_NUMBER < 0) {
    PERROR("can't get a major number\n");
    return -EBUSY;
  }
  PDEBUG("%s major number is %d\n",MODULE_NAME, VRT_MAJOR_NUMBER);

  vs.request_nb=0; // le numéro de la 1ère requête entrante est 1
  
  // création du répertoire devfs du virtualiseur
  vs.dev_rep = devfs_mk_dir(NULL,"virtualiseur",NULL);
  if (vs.dev_rep == NULL) {
    PERROR("can't create devfs rep for ze virtualiseur\n");
    return -EBUSY;
  }
  PDEBUG("devfs rep created\n");

  
  // création des répertoires/fichiers /proc
  vs.proc_rep = proc_mkdir("virtualiseur", NULL);
  if (vs.proc_rep  == NULL) {
    PERROR("can't create proc rep for ze virtualiseur\n");
    return -EBUSY;
  }
    
  vs.gproc_rep = proc_mkdir("groups",vs.proc_rep);
  if (vs.gproc_rep == NULL) {
    PERROR("can't create gproc rep for ze virtualiseur\n");
    return -EBUSY;
  }

  vrt_proc_zones = create_proc_entry("zones_enregistrees", 0644, vs.proc_rep);
  vrt_proc_zones->read_proc = vrt_read_proc;
  vrt_proc_zones->write_proc = vrt_write_proc;
  vrt_proc_zones->owner=THIS_MODULE;

  if (!create_proc_read_entry("data_etat", 
			      0,
			      vs.proc_rep, 
			      vrt_proc_read_data_etat, 
			      NULL)) {
    PERROR("can't create proc entry data_etat\n");
    return -EBUSY;
  }
  if (!create_proc_read_entry("data_zones", 0, vs.proc_rep, vrt_proc_read_data_zones, NULL)) {
    PERROR("can't create proc entry data_zones\n");
    return -EBUSY;
  }  
  PDEBUG("proc rep & files created\n");

    
  // REMARQUE : on pourrait se passer des MAJOR/MINOR number, 
  // car devfs permet de le faire mais :
  // Problème car peu de doc à ce sujet et le bouquin 
  // "Linux Device Drivers" n'utilise pas à fond devfs => 
  // on évitera bp de mises au point si on abuse du RTFM code !)  
  blk_queue_make_request(BLK_DEFAULT_QUEUE(VRT_MAJOR_NUMBER),vrt_make_request);
  read_ahead[VRT_MAJOR_NUMBER]=VRT_READAHEAD;
  
  blk_size[VRT_MAJOR_NUMBER]= kmalloc(sizeof(int)*NBMAX_ZONES, GFP_KERNEL);
  if (!blk_size[VRT_MAJOR_NUMBER]) {
    PERROR("can't allocate memory for blk_size\n");
    return -ENOMEM;
  }

  blksize_size[VRT_MAJOR_NUMBER]=kmalloc(sizeof(int)*NBMAX_ZONES, GFP_KERNEL);
  if (!blksize_size[VRT_MAJOR_NUMBER]) {
    PERROR("can't allocate memory for blksize_size\n");
    return -ENOMEM;
  }
  
  hardsect_size[VRT_MAJOR_NUMBER]=kmalloc(sizeof(int)*NBMAX_ZONES, GFP_KERNEL);
  if (!hardsect_size[VRT_MAJOR_NUMBER]) {
    PERROR("can't allocate memory for hardsect_size\n");
    return -ENOMEM;
  }

  max_readahead[VRT_MAJOR_NUMBER]=kmalloc(sizeof(int)*NBMAX_ZONES, GFP_KERNEL);
  if (!max_readahead[VRT_MAJOR_NUMBER]) {
    PERROR("can't allocate memory for max_readahead\n");
    return -ENOMEM;
  }
    
  nullify_table((void *)vs.groups, NBMAX_GROUPS);  
  nullify_table((void *)vs.minor2zone, NBMAX_ZONES);  
  PDEBUG("dev tables allocated & initialized\n"); 

  PDEBUG( "FIN\n"); 
  return 0; // SUCCESS
}

/*!\file vrt_ops.c
\brief définit les fonctions de bases du BDD virtualiseur
*/
#include "constantes.h"
#include "virtualiseur.h"
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/hdreg.h>
#include <linux/blkpg.h>
#include "vrt_divers.h"

//! cette fonction est appelée dès qu'une application utilise notre BDD
/*!
	Une application utilise notre BDD en faisant un open("/dev/virtualiseur/zoneX") ou X
	est le numéro d'une zone enregistrée
	C'est ce qui se passe par exemple, lorsqu'on monte un file system sur ce BDD.

	Le nombre d'applications qui utilisent le BDD (ie. le nombre d'appli qui ont fait un open sur un /dev
	géré par le BDD) peut être connu en faisant : lsmod et en regardant le nombre d'utilisations en cours
	de scimapdev
*/
int vrt_open(struct inode *inode, struct file *filp) {
  storzone_t *z;
  PDEBUG( "Open : zone%d\n",MINOR(inode->i_rdev));
  
  z = vs.minor2zone[MINOR(inode->i_rdev)];
  z->used++;
  return 0;
};

//! cette fonction est appelée dès qu'une application libère notre BDD (parce qu'elle n'a plus besoin de l'utiliser)
/*!
	Une application signale qu'elle n'utilise plus notre BDD en faisant un "close"
	C'est ce qui se passe par exemple, lorsqu'on unmonte un file system qui utilisait notre BDD

	Lorsqu'aucune application n'utilise plus virtualiser, on peut le supprimer de la mémore en faisant
	un "rmmod  virtualiseur"
*/
int vrt_release(struct inode *inode, struct file *filp){
  storzone_t *z;
  PDEBUG( "Release : zone%d\n",MINOR(inode->i_rdev));
  
  z = vs.minor2zone[MINOR(inode->i_rdev)];

  if (z->used > 0) 
    z->used--;
  
  return 0;
};

//! cette fonction implémente les IOCTL du driver, c'est-à-dire les commandes spéciales
/*!
	j'ai récupérée quasi intégralement le code de sbull.c,
	j'ai juste adapté certains noms de fonction
	Ca permet d'avoir une fonction bien testée et débuggée
*/
int vrt_ioctl (struct inode *inode, 
	       struct file *filp,
	       unsigned int cmd, 
	       unsigned long arg) {
  u64 taille_z;
  int err, size;
  struct hd_geometry *geo = (struct hd_geometry *)arg;
  
  // taille de la zone en secteurs
  PDEBUG( "ioctl 0x%x 0x%lx\n", cmd, arg);
  taille_z = quotient64( vs.minor2zone[MINOR(inode->i_rdev)]->size  
			 * 1024, 
			 VRT_HARDSECT);
  
  switch(cmd) {
    
  case BLKGETSIZE: /* Return the device size, expressed in sectors */
    if (!arg) return -EINVAL; /* NULL pointer: not valid */
    err=verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
    if (err) return err;
    
    PDEBUG( "BLKGETSIZE %lld sectors (=%lld  KB, 1 sect=%d bytes)\n",
	    taille_z,
	    vs.minor2zone[MINOR(inode->i_rdev)]->size,
	    VRT_HARDSECT );
    put_user ( taille_z, (long *) arg);
    
   
    return 0;

  case BLKFLSBUF: /* flush */
    if (!suser()) return -EACCES; /* only root */
    fsync_dev(inode->i_rdev);
    invalidate_buffers(inode->i_rdev);
    PDEBUG( "BLKFLSBUF \n");
    return 0;
    
  case BLKRAGET: /* return the readahead value */
    if (!arg)  return -EINVAL;
    err = verify_area(VERIFY_WRITE, (long *) arg, sizeof(long));
    if (err) return err;
    put_user(read_ahead[MAJOR(inode->i_rdev)],(long *) arg);
    PDEBUG( "BLKRAGET \n");
    return 0;

  case BLKRASET: /* set the readahead value */
    if (!suser()) return -EACCES;
    if (arg > 0xff) return -EINVAL; /* limit it */
    read_ahead[MAJOR(inode->i_rdev)] = arg;
    PDEBUG( "BLKRASET \n");
    return 0;
    
  case BLKRRPART: /* re-read partition table: can't do it */
    PDEBUG( "BLKRRPART \n");
    return -EINVAL;
    
    //RO_IOCTLS(inode->i_rdev, arg); /* the default RO operations */
    
  case HDIO_GETGEO:
    /*
     * get geometry: we have to fake one...  trim the size to a
     * multiple of 64 (32k): tell we have 16 sectors, 4 heads,
     * whatever cylinders. Tell also that data starts at sector. 4.
     */
    
    size = taille_z;
    size &= ~0x3f; /* multiple of 64 */
    if (geo==NULL) return -EINVAL;
    err = verify_area(VERIFY_WRITE, geo, sizeof(*geo));
    if (err) return err;
    put_user(size >> 6, &geo->cylinders);
    put_user(        4, &geo->heads);
    put_user(       16, &geo->sectors);
    put_user(        4, &geo->start);
    PDEBUG( "HDIO_GETGEO \n");
    return 0;
    
  default:
    PDEBUG( "PAR DEFAUT \n");
    return blk_ioctl(inode->i_rdev, cmd, arg);
  }
  
  PDEBUG( "COMMANDE INCONNUE \n");
  return -EINVAL; /* unknown command */
}


//! définition  et déclaration de la structure qui référence les fonctions implémentant open, release et ioclt
struct block_device_operations vrt_fops = {
	open: vrt_open,
	release: vrt_release,
	ioctl: vrt_ioctl,
};


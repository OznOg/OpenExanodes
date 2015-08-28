#include "scimapdev.h"
#include "smd_ops.h"
#include "parser.h"
#include <linux/kernel.h>
#include "smd_divers.h"
#include "constantes.h"

extern int smd_add_snode_sci(servernode_t *sn, long no_sci);
extern void smd_request(request_queue_t *q);
extern void reset_sn_stats(servernode_t *sn);

//! renvoie l'indice du ndev de 'sn' qui suit le ndev d'indice 'ind_cndev' dans le tableau sn->ndevs
int next_valid_ndev(servernode_t *sn, int ind_cndev) {
  int i;

  i = ind_cndev;
  do {
    if (i<NBMAX_NDEVS-1) i++; else i=0;
    if (sn->ndevs[i] != NULL) break;
  }
  while (i != ind_cndev);
    
  return i;
}


//! renvoie le snode correspondant no_sci passé en param. Renvoie NULL sinon.
servernode_t *get_snode_from_nosci(long no_sci) {
  u32 i;

  for (i=0; i<NBMAX_SNODES; i++) 
    if (smd.snodes[i] != NULL) 
      if (smd.snodes[i]->no_sci == no_sci)
	return smd.snodes[i];
      
  return NULL;
}


//! renvoie le snode correspondant au nom passé en param. Renvoie NULL sinon.
servernode_t *get_snode_from_name(char *name) {
  u32 i;

  for (i=0; i<NBMAX_SNODES; i++) 
    if (smd.snodes[i] != NULL) {
      if (strcmp(smd.snodes[i]->name, name) == 0) 
	return smd.snodes[i];
    }
      
  return NULL;
}

//! renvoie TRUE si le 'nom' du snode passé en param n'est pas un nom de snode déjà pris en charge par le NBD
int new_snode(char *name) {
  if (get_snode_from_name(name) == NULL) return TRUE;
  else return FALSE;
}

ndevice_t *get_ndev_from_name(servernode_t * sn, char *name) {
  u32 i;

  for (i=0; i<NBMAX_NDEVS; i++) 
    if (sn->ndevs[i] != NULL) {
      if (strcmp(sn->ndevs[i]->name, name) == 0) 
	return sn->ndevs[i];
    }
      
  return NULL;
}

int new_ndev(servernode_t * sn, char *name) {
  if (get_ndev_from_name(sn, name) == NULL) return TRUE;
  else return FALSE;
}

//! prend en compte un nouveau noeud serveur de devices
/*! 
  - ajout de sa structure de données dans smdstate
  - création et connexion des segments + IT SCI
  - création du rep DEVFS pour ce noeud
*/
int smd_add_snode(char *nom_node, 
		  long no_sci) {
  
  servernode_t *sn;
  u32 no_snode; 

  PDEBUG("entering. Param : nom_node =%s no_sci=%ld\n",
	 nom_node,
	 no_sci);

  no_snode = unused_elt_indice((void *)smd.snodes, NBMAX_SNODES);
  if (no_snode == NBMAX_SNODES) {
    PERROR("can't add the new server node because max number "
	   "( = %d) reached\n",
	   NBMAX_SNODES);
    return ERROR;
  }

  PDEBUG("free elt indice %d of smd.snodes[] chosen\n", no_snode);

  sn = vmalloc(sizeof(struct servernode));
  if (sn == NULL) {
    PERROR("can't allocate memory for new snode '%s'\n", nom_node);
    goto error_vmalloc;
  }
  
  smd.snodes[no_snode] = sn;  
  sn->status = INSANE;
  sscanf(nom_node, "%s", sn->name); 
  sn->dev_rep = devfs_mk_dir(smd.dev_rep, sn->name, NULL);
  PDEBUG("sn->dev_rep = devfs_mk_dir(smd.dev_rep, sn->name, NULL) == %p\n",
	 sn->dev_rep);
  if (sn->dev_rep == NULL) {
    PERROR("can't create devfs rep for ze node %s\n",sn->name);
    goto error_devfs;
  }
  sn->no_sci = no_sci;
  sn->busy = IDLE; 
  sn->creq.kreq = NULL;

  //for (i=0; i<NBMAX_DEVS; i++) sn->wreqs.nd = NULL
  nullify_table((void *)sn->ndevs, NBMAX_NDEVS);

  PDEBUG("devfs rep '%s' created\n", sn->name);

  // init sci pour ce nouveau server node
  if (smd_add_snode_sci(sn,no_sci)) {
    printk(KERN_ERR "smd_add_snode_sci failed\n");
    goto error_sci;
  }				

  PDEBUG("sci structures for snode '%s' created\n", sn->name);
  sn->maxsize_req = smd.dataseg_size / SMD_HARDSECT; 
  reset_sn_stats(sn);

  //sn->status = UP;
  smd.snodes[no_snode] = sn;  
  smd.nb_snodes++;

  PDEBUG("server node '%s' added\n", sn->name);
  return SUCCESS;

 error_sci:
  PDEBUG("devfs_unregister(sn->dev_rep = %p)\n", sn->dev_rep);
  devfs_unregister(sn->dev_rep);
 error_devfs:
  vfree(sn);
  smd.snodes[no_snode] = NULL;
 error_vmalloc:
  return ERROR;
}

//! crée l'image d'un nouveau device distant (situé sur le serveur 'sn')
/*! 
  - structure de données pour le ndev
  - création fichier DEVFS
  - attribution MINOR number
  - init des valeurs des tableaux kernel (cf. blkdev.h)
*/
int smd_add_ndev(servernode_t *sn, 
		 char *nom_device, 
		 char *path_device,
		 int devnb,
		 u64 nb_sectors) {
  int no_ndev;
  ndevice_t *nd;

  PDEBUG("entering. param: sn=%p (name=%s) nom_device=%s "
	 "path_device=%s devnb = %d nb_sectors=%lld\n",
	 sn, 
	 sn->name, 
	 nom_device, 
	 path_device,
	 devnb,
	 nb_sectors);

  // et si le nouveau ndev existe déjà sur ce snode..
  // on met à jour la nouvelle taille si besoin est
  if (new_ndev(sn, nom_device) == FALSE) {
    PDEBUG("ndev '%s' exist => checking if its size has changed\n",
	   nom_device);
    nd = get_ndev_from_name(sn, nom_device);
    if (nd == NULL) {
      PERROR("can't get the nd structure of existing ndev '%s'\n",
	     nom_device);
      return ERROR;
    }
	     
    if (quotient64(nb_sectors*SMD_HARDSECT,1024) != nd->size) {
      // changement de taille
      PDEBUG("size changed: old size = %lld KB, new size = %lld KB\n", 
	     nd->size,
	     quotient64(nb_sectors*SMD_HARDSECT,1024));
      nd->size = quotient64(nb_sectors*SMD_HARDSECT,1024);
      blk_size[SMD_MAJOR_NUMBER][nd->minor] = nd->size;
    }
    return SUCCESS;
  }
     

  no_ndev = unused_elt_indice((void *)sn->ndevs, NBMAX_NDEVS);
  if (no_ndev == NBMAX_NDEVS) {
    PERROR("can't create the new image device because max number "
	   "(= %d) reached for the server node '%s'\n",
	   NBMAX_NDEVS,
	   sn->name);
    return ERROR;
  }
  
  PDEBUG("free elt indice %d of sn->ndevs[] chosen\n", no_ndev);

  nd = vmalloc(sizeof(struct ndevice));
  if (nd == NULL) {
    PERROR("can't allocate memory for new ndev '%s'\n", nom_device);
    return ERROR;
  }

  nd->sn = sn;

  nd->minor = unused_elt_indice((void *)minor2ndev,NBMAX_NDEVS);
  if (nd->minor == NBMAX_NDEVS) {
    PERROR("can't get a minor number for device '%s:%s' "
	   "because max number (= %d) reached \n",
	   nom_device, nd->name,
	   NBMAX_NDEVS);
    return ERROR;
  }
  
  nd->localnb = devnb;
  sscanf(nom_device,"%s", nd->name);
  sscanf(path_device,"%s", nd->rpath);
  sprintf(nd->lpath,"/dev/%s/%s", sn->name, nd->name);
 
	 
  nd->dev_file = 
    devfs_register(sn->dev_rep, nd->name,
		   DEVFS_FL_DEFAULT, 
		   SMD_MAJOR_NUMBER, nd->minor,
		   S_IFBLK | S_IRUSR | S_IWUSR, 
		   &smd_fops, 
		   NULL); 

  PDEBUG("  nd->dev_file = "
	 "devfs_register(sn->dev_rep, nd->name,"
	 "DEVFS_FL_DEFAULT, "
	 "SMD_MAJOR_NUMBER, nd->minor, "
	 "S_IFBLK | S_IRUSR | S_IWUSR," 
	 "&smd_fops," 
	 "NULL) == %p\n",nd->dev_file);

  if (nd->dev_file  == NULL) {
    PERROR("can't create dev_file for ndev '%s:%s'\n",
	   sn->name,
	   nd->name);
    goto error;
  }

  PDEBUG("devfs file '%s' created\n", nd->name);

  nd->nb_io = 0;
  nd->size = quotient64(nb_sectors*SMD_HARDSECT,1024); // en Ko

  // smd_size[no_brique] mise à jour plus tard
  blk_size[SMD_MAJOR_NUMBER][nd->minor] = nd->size;
  blksize_size[SMD_MAJOR_NUMBER][nd->minor] = SMD_BLKSIZE;
  hardsect_size[SMD_MAJOR_NUMBER][nd->minor] = SMD_HARDSECT;
  max_readahead[SMD_MAJOR_NUMBER][nd->minor] = SMD_READAHEAD;
  max_sectors[SMD_MAJOR_NUMBER][nd->minor] = sn->maxsize_req;
  
  // on initialise la queue des requêtes
  blk_init_queue(&nd->queue, smd_request);
  blk_queue_headactive(&nd->queue,0);
  
  PDEBUG("dev queue iniatilized\n");

  minor2ndev[nd->minor] = nd;
  sn->ndevs[no_ndev] = nd;
  PDEBUG("networked device '%s' added\n", nd->name);
  return SUCCESS;

 error:
  vfree(nd);
  return ERROR;

}



extern int cleanup_sci_sn(servernode_t *sn, int csession_flag);

void disconnect_from_snode(servernode_t *sn) {
  if (cleanup_sci_sn(sn, NO_FLAGS) == ERROR) {
    PERROR("can't free/deconnect all SCI resources from node '%s'\n",
	   sn->name);
    PDEBUG("snode '%s' : DECONNEXION -> INSANE\n",sn->name);
    sn->status = INSANE;
  }
}

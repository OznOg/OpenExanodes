#include <net/checksum.h>
#include <linux/time.h>
#include <linux/random.h>
#include <linux/kdev_t.h>
#include <math.h>
#include <linux/locks.h>
#include "vrt_superblock.h"
#include "constantes.h"
#include "virtualiseur.h"
#include "vrt_zone.h"
#include <asm/string.h>



static struct sb_group sbg_aux;
static struct sb_rdevs sbr_aux; 


int read_block_on_device(kdev_t device, 
			 void *block,
			 int block_size,
			 unsigned long long offset) {
  struct buffer_head *bh = NULL;
  
  PDEBUG("device = 0x%x, sb = %p, off = %lld\n", device, block, offset);
  //PDEBUG("fsync_dev(device)\n"); 
  fsync_dev(device);
  //PDEBUG("fsync_dev(device) done\n"); 

  //PDEBUG("set_blocksize (device=0x%x, VRT_SBSIZE=%d)\n",
  //	 device,
  //	 VRT_SBSIZE);
  set_blocksize (device, VRT_SBSIZE);
  //PDEBUG("set_blocksize done\n");
  
  bh = bread (device, offset / VRT_SBSIZE, VRT_SBSIZE);
  //PDEBUG("bh(0x%p) = bread (device, sb_offset / VRT_SBSIZE (=%lld), "
  //       "VRT_SBSIZE) done\n",
  //	 bh,
  //	 offset / VRT_SBSIZE);
 
  
  if (bh) {
    //PDEBUG("memcpy (sb(=0x%p), bh->b_data(=0x%p),sizeof(sb)=%d)\n",
    //	   block,
    //	   bh->b_data,
    //	   block_size);
    
    //PDEBUG("bh : size = %d, dev=0x%x rdev=0x%x\n ", 
    //          bh->b_size, bh->b_dev,bh->b_rdev);
    memcpy (block, bh->b_data, block_size);
    //PDEBUG("memcpy done\n");
  }
  else {
    PERROR("can't read the block!\n");
    set_blocksize (device, VRT_BLKSIZE);
    return ERROR;
  }
  //PDEBUG("VRT - read_block_on_device: block read\n");
  
  if (bh) {
    //PDEBUG("brelse (bh)\n");
    brelse (bh);
    //PDEBUG("brelse (bh) done\n");
  }

  //PDEBUG("set_blocksize (device=0x%x, VRT_BLKSIZE=%d)\n",
  // device,
  // VRT_BLKSIZE);
  set_blocksize (device, VRT_BLKSIZE);
  //PDEBUG("set_blocksize done\n");

  return SUCCESS;
}

int write_block_on_device(kdev_t device, 
			  void *block,
			  int block_size,
			  unsigned long long offset) {
  struct buffer_head *bh;
  
  PDEBUG("device = 0x%x, sb = %p, off = %lld\n", device, block, offset);
  //PDEBUG("fsync_dev(device)\n"); 
  fsync_dev(device);
  //PDEBUG("fsync_dev(device) done\n"); 
     
  //PDEBUG("set_blocksize(device, VRT_SBSIZE=%d)\n", VRT_SBSIZE); 
  set_blocksize(device, VRT_SBSIZE);
  //PDEBUG("set_blocksize(device, VRT_SBSIZE=%d) done\n", VRT_SBSIZE); 

  bh = getblk(device, offset / VRT_SBSIZE, VRT_SBSIZE); 
  //PDEBUG("getblk(device=0x%x, sb_offset / VRT_SBSIZE=%lld, "
  //         "VRT_SBSIZE=%d) done\n",
  //	 device,
  //	 offset / VRT_SBSIZE,
  //	 VRT_SBSIZE); 
  if (!bh) {
    PERROR("can't read the block!\n");
    set_blocksize (device, VRT_BLKSIZE);
    return ERROR;
  }

  //PDEBUG("memcpy(bh->b_data=%p, sb=%p, VRT_SBSIZE=%d)\n",
  // bh->b_data,
  // block,
  // VRT_SBSIZE);
  //PDEBUG("bh : size = %d, dev=0x%x rdev=0x%x\n ",
  // bh->b_size, bh->b_dev,bh->b_rdev);
  memcpy(bh->b_data, block, block_size);
  //PDEBUG("memcpy(bh->b_data=%p, sb=%p, VRT_SBSIZE) done\n",
  // bh->b_data,
  // block);
  
  mark_buffer_uptodate(bh, 1);
  //PDEBUG("mark_buffer_uptodate(bh, 1) done\n");
  mark_buffer_dirty(bh);
  //PDEBUG("mark_buffer_dirty(bh) done\n");

  ll_rw_block(WRITE, 1, &bh);
  //PDEBUG("ll_rw_block(WRITE, 1, &bh) done\n");
  
  wait_on_buffer(bh);
  //PDEBUG("wait_on_buffer(bh) done\n");
  brelse(bh);
  //PDEBUG("brelse(bh) done\n");
  fsync_dev(device);
  //PDEBUG("fsync_dev(device) done\n");
  set_blocksize (device, VRT_BLKSIZE);
  //PDEBUG(" set_blocksize (device, VRT_BLKSIZE) done\n");

  return SUCCESS;
}

static u32 generate_date(void) {
  struct timeval maintenant;
  do_gettimeofday(&maintenant);
  return maintenant.tv_sec;
}

// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// 
// Ces 3 fonctions doivent être synchronisées avec le fichier 
// vrt_superblock.h du répertoire "vrtadm"

// NEW
//! renvoie l'offset d'un superblock de group sur un rdev
inline u64 r_offset_sbg(void) {
  return 0;
}


//! renvoie l'offset d'un superblock de group sur un rdev
inline u64 r_offset_sbr(void) {
  return r_offset_sbg() + VRT_SBSIZE;
}

//! renvoie l'offset de la fin "arrondie" du rdev
/*! la fin "arrondie" d'un rdev est situé à la fin 
  du dernier bloc de 32 Ko entier.
  LIMITE TAILLE rdev : 2^64 octets = 16 Exaoctets
*/
u64 offset_end_dev(u64 size_kb) {
   u64 offset_kb;  
  if (size_kb % 32 == 0) 
    offset_kb = size_kb - 32;
  else 
    offset_kb = 32 * (size_kb / 32 - 1); // (size_kb / 32) permet de faire un arrondi (car arrondi "classique" avec u64 pose des pbs à l'insmod)

  return offset_kb * 1024;
} 

u64 offset_end_rdev(realdev_t *rd) {
  return offset_end_dev(rd->size);
}  
 

//! génère les données constantes du SBG
void generate_sbg_cst(group_t *g,struct sb_group *sbg) {
  int i;
  
  sbg->magic_number = SBG_MAGIC;
  strcpy(sbg->gname,g->name); 
  sbg->vrt_vers = VRT_VERSION;
  for (i=0; i<4; i++) sbg->uuid[i] = g->uuid[i];
  sbg->create_time = generate_date();
  sbg->layout = (*(g->layout_type))();
}


//! génère les données variables du SBG
void generate_sbg_var(group_t *g,struct sb_group *sbg) {
  int i;
   
  sbg->nb_zones = g->nb_zones;
  sbg->nb_rdevs = g->nb_realdevs;
  sbg->update_time = generate_date();  
  for (i=0; i<NBMAX_ZONES; i++) 
    if (g->zones[i] != NULL) 
      sbg->zone_exist[i]=TRUE;
    else 
      sbg->zone_exist[i]=FALSE;
}




//! créer le SBG en tenant compte de l'état courant du virtualiseur
void create_sb_group(group_t *g, 
		     struct sb_group *sbg) {
  generate_sbg_cst(g,sbg);
  generate_sbg_var(g,sbg);
  sbg->checksum = 0;
  sbg->checksum = csum_partial((void *)sbg, sizeof(struct sb_group), 0);
}


//! maj le sbg passé en param 
void update_sb_group(group_t *g, 
		     struct sb_group *sbg) {
  generate_sbg_var(g,sbg);
  sbg->checksum = 0;
  sbg->checksum = csum_partial((void *)sbg, sizeof(struct sb_group), 0);
}


//! génère les données variables du SBR
void create_sb_rdevs(group_t *g, 
		     struct sb_rdevs *sbr) {
  int i,j;
  sbr->magic_number = SBR_MAGIC;

  for (i=0; i<NBMAX_RDEVS; i++) 
    if (g->realdevs[i] != NULL) 
       for (j=0; j<4; j++) 
	 sbr->uuid_rdevs[i][j] = g->realdevs[i]->uuid[j];
    else for (j=0; j<4; j++) sbr->uuid_rdevs[i][j] = 0;

  sbr->checksum = 0;
  sbr->checksum = csum_partial((void *)sbr, sizeof(struct sb_rdevs), 0); 
}



//! écrit le block sur tous les rdev du groupe (retour = ERROR ou SUCCESS)
int write_block_on_all_rdevs(group_t *g, 
			    void *block, 
			    int block_size, 
			    u64 r_offset) {

  int i,error;
  realdev_t *rd;
  for (i=0; i < NBMAX_RDEVS; i++) { 
    if (g->realdevs[i] != NULL) {
      rd = g->realdevs[i];
      
      //PDEBUG("dev[maj=%d,min=%d], &sbb=%p, size=%d, offset=%lld\n",
      //	     rd->major, rd->minor,
      //	     block,
      //	     block_size,
      //	     offset_end_rdev(rd) - r_offset);

      error = write_block_on_device(MKDEV(rd->major,rd->minor),
				    block,
				    block_size,
				    offset_end_rdev(rd) - r_offset);
      
      if (error == ERROR) { 
	PERROR("error in writing block 0x%p on rdev %s "
	       "(offset = %lld)\n",
	       block,
	       rd->name,
	       offset_end_rdev(rd) - r_offset);
	return ERROR; 
      }
    }
  }

  return SUCCESS;
}

//! écrit le SBG passé en param sur tous les rdevs du group. Met à jour le champ rdev_uuid sur SBG pour chaque rdev
int write_sbg_on_all_rdevs(group_t *g, 
			   struct sb_group *sbg, 
			   u64 r_offset) {

  int i,j,error;
  realdev_t *rd;

  for (i=0; i < NBMAX_RDEVS; i++) { 
    if (g->realdevs[i] != NULL) {
      rd = g->realdevs[i];
      
      // voilà pourquoi on a fait une fonction spéciale
      // pour le SBG
      for (j=0; j<4; j++) 
	sbg->rdev_uuid[j] = rd->uuid[j];
     
      //PDEBUG("dev[maj=%d,min=%d], &sbb=%p, size=%d, offset=%lld\n",
      //	     rd->major, rd->minor,
      //	     sbg,
      //	     sizeof(struct sb_group),
      //	     offset_end_rdev(rd) - r_offset);

      error = write_block_on_device(MKDEV(rd->major,rd->minor),
				    (void *)sbg,
				    sizeof(struct sb_group),
				    offset_end_rdev(rd) - r_offset);
      
      if (error == ERROR) { 
	PERROR("error in writing block 0x%p on rdev %s "
	       "(offset = %lld)\n",
	       sbg,
	       rd->name,
	       offset_end_rdev(rd) - r_offset);
	return ERROR; 
      }
    }
  }

  return SUCCESS;
}

//! génère et écrit le SBG sur tous les rdevs du groupe. Renvoie SUCCESS/ERROR.
int vrt_write_SBG(group_t *g) {
  u64 r_offset;
  int retval;

  create_sb_group(g, &sbg_aux); 
  r_offset = r_offset_sbg();    
  
  retval = write_sbg_on_all_rdevs(g, &sbg_aux, r_offset);  
  if (retval == ERROR) {
    PERROR("error in writing SB GROUP on rdevs\n");
    return ERROR;
  }
  else PDEBUG("SB GROUP  updated on the rdevs\n");
  return SUCCESS;
}

//! génère et écrit le SBR sur tous les rdevs du groupe. Renvoie SUCCESS/ERROR.
int vrt_write_SBR(group_t *g) {
  int retval;
  u64 r_offset;
  
  create_sb_rdevs(g, &sbr_aux);
  r_offset = r_offset_sbr();
  retval = write_block_on_all_rdevs(g,
				   (void *)&sbr_aux,
				   sizeof(struct sb_group),
				   r_offset);
   
  if (retval == ERROR) {
    PERROR("error in writing SB RDEVS on rdevs\n");
    return ERROR;
  }
  else PDEBUG("SB RDEVS updated on the rdevs\n");

  return SUCCESS;
}


//! sauvegarde les méta données du group sur ses devices
int vrt_write_group(group_t *g) {
  int i;
  int retval;

  // superblock group
  if (vrt_write_SBG(g) == ERROR) return ERROR;

  // superblock rdev
  if (vrt_write_SBR(g) == ERROR) return ERROR;
  

  // superblocs zone
  for (i=0; i<NBMAX_ZONES; i++) {
    if (is_zone_ptr_valid(g->zones[i])) {
      retval = (*(g->create_and_write_sb_zone))(g, g->zones[i]);
      if (retval == ERROR) {
	PERROR("error in write SB ZONE for zone %s\n", 
	       g->zones[i]->name);
	return ERROR;
      }
    }
  }
  
  return SUCCESS;
}


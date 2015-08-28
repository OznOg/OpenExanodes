#include "scimapdev.h"
#include <genif.h>
#include <sci_types.h>
#include <linux/time.h>
#include "smd_snode.h"

/*!\file smd_init.c
\brief fichier central de scimapdev. C'est ici que les requêtes sont traitées (cf. fonction smd_request()
*/

extern long no_requete;

//extern int connexion_sci_snode(servernode_t *sn, int no_sci);
extern int deconnexion_sci_snode(servernode_t *sn, int csession_flag);
extern int smd_start_sequence(void);
extern int smd_check_sequence(void);



//////////////// SMD_FIND_QUEUE //////////////////
request_queue_t *smd_find_queue(kdev_t device) {
  int minor = DEVICE_NR(device);
  ndevice_t *nd;
  
  // PDEBUG("minor = %d\n", minor);
  nd = minor2ndev[minor];
  
  if (nd == NULL) {
    PERROR("request to a bad minor number (=%d)!!\n",
	   minor);
    return NULL;
  }
  
  /*PDEBUG("SMD - smd_find_queue: device number = %d < NB_MAX_BRIQUES(=%d). Returning queue= 0x%p\n",
   *	 no_brique,
   *	 NB_MAX_BRIQUES,
   *	 &(s_data.brique[no_brique].queue));
   */ 
  return &nd->queue;
}

//! renvoie une file d'un des ndevs du snode 'sn' qui contient encore des requêtes non traitées. On met en place un tourniquet sur les ndevs servis de façon à être équitable. Renvoie NULL si aucun ndevs n'a de requête en attente.
request_queue_t *smd_next_non_empty_queue(servernode_t *sn) {
  static int last_i_ndev = 0; 
  int i;
  
  i = next_valid_ndev(sn, last_i_ndev);
  while ((i != last_i_ndev) &&
	 (list_empty(&sn->ndevs[i]->queue.queue_head)))
    i = next_valid_ndev(sn, i);

  if (list_empty(&sn->ndevs[i]->queue.queue_head)) 
    return NULL;
  else 
    return &sn->ndevs[i]->queue;
}


#ifdef PERF_STATS

//////////////// SMD_NONE_BUSY (STATS) //////////////////
//! est-ce qu'au moins un des noeuds serveurs est en cours d'utilisation ?
int smd_none_busy(void) {
  int i, retval;

  retval = TRUE;

  for (i=0; i<NBMAX_SNODES; i++) 
    if (smd.snodes[i] != NULL) 
      if (smd.snodes[i]->busy == TRUE) { 
	retval = FALSE;
	break;
      }

  return retval;
}
#endif

#define IO_PENDING 1
#define IO_ENDPRELUDE 2


#define MAX_RETRIES 5

extern inline void sci_wc_flush()
{
	asm volatile(
		"cpuid"
		:
		:
		: "eax", "ebx", "ecx", "edx"
	);
}


extern inline void *opt_memcpy(char *dest, char *src, size_t len)
{
	int *d = (int *)dest;
	int *s = (int *)src;

	while (len > 3) {
		*d++ = *s++;
		len -= 4;
	}

	if (len & 2) *((short *)d)++ = *((short *)s)++;
	if (len & 1) *((char *)d) = *((char *)s);

	return dest;
}


extern inline void *sci_memcpy(char *dest, char *src, size_t len)
{
	char *tmp = dest;
	unsigned long n;

	n = 32 - (((unsigned long)src) % 32);
	if (n > len) {
		opt_memcpy(dest, src, len);
		return dest;
	}

	opt_memcpy(dest, src, n);
	len -= n;
	dest += n;
	src += n;
	sci_wc_flush();

	while (len >= 32) {
		long *d = (long *)dest;
		long *s = (long *)src;

		d[0] = s[0];
		d[1] = s[1];
		d[2] = s[2];
		d[3] = s[3];
		d[4] = s[4];
		d[5] = s[5];
		d[6] = s[6];
		d[7] = s[7];

		src += 32;
		dest += 32;
		len -= 32;
		sci_wc_flush();
	}

	opt_memcpy(dest, src, len);

	return tmp;
}

//////////////// SMD_DO_PRELUDE //////////////////
// on transfère les données de CTL + les données de DATA (dans le cas d'une écriture)
inline int smd_write_ctl_block(ndevice_t *nd) {
  servernode_t *sn;
  //unsigned32 s=0, nb_retries=0;
  

  PDEBUG("write CTL seg to server\n");   
  /*if (!SCI_START_SEQUENCE(&(smd.sdip),s)) {
    PERROR("Error SCI_START_SEQUENCE\n");
    return ERROR;
    }*/

  sn = nd->sn;

#ifdef PERF_STATS
  do_gettimeofday(&(sn->debut_transfert_PIO));
#endif

  if (smd_start_sequence() == ERROR) {
    PERROR("in smd_start_sequence for ndev='%s'\n",
	   nd->name);
    return ERROR;
  }

  sci_memcpy(sn->rs_ctl_addr,
	 (char *)(&(nd->c_io)),
	 sizeof(struct iodesc));

  if (smd_check_sequence() == ERROR) {
    PERROR("in smd_check_sequence for ndev='%s'\n",
	   nd->name);
    return ERROR;
  }
  
#ifdef PERF_STATS
  do_gettimeofday(&(sn->fin_transfert_PIO));
  sn->somme_tailles_PIO += sizeof(struct iodesc);
  sn->somme_durees_PIO += 
    (sn->fin_transfert_PIO.tv_sec - 
     sn->debut_transfert_PIO.tv_sec)*1000000
    + (sn->fin_transfert_PIO.tv_usec - 
       sn->debut_transfert_PIO.tv_usec);
#endif

  // statistiques 
  sn->request_bytes_transfered += nd->c_io.size;
  
#ifdef PERF_STATS
  do_gettimeofday(&(sn->debut_transfert_disque)); // stats
#endif

  PDEBUG("end\n");
  return SUCCESS;
  
}

void smd_end_request(servernode_t *sn, int status);

//////////////// SMD_START_REQUEST //////////////////
//! on indique au serveur qu'on a besoin qu'il traite une nouvelle requete
int smd_send_request(ndevice_t *nd) {
  int retval;
  servernode_t *sn;

  sn = nd->sn;

  if (smd_write_ctl_block(nd) == ERROR) {
     PERROR("smd_write_ctl_block on ndev '%s' failed. "
	    "Aborting current request\n",
	    nd->name);
     return ERROR;
  }
  PDEBUG("smd_write_ctl_block done\n");

  retval =  sci_trigger_interrupt(sn->rit_begio_handle);
  if (retval != 0) {
    PERROR("sci_trigger_interrupt failed\n");
    return ERROR;
  }
  PDEBUG("sci_trigger_interrupt BEGIO done\n");

  if (sn->status != UP) {
     PDEBUG("snode '%s' isn't UP => aborting current req\n",
	    sn->name);
     return ERROR;
  }    

  sn->busy = WAIT_ENDIO;
  return SUCCESS;
}


//////////////// SMD_SMALL_BUF_TRANSFER //////////////////
inline int smd_small_buf_transfer(u8 *bh_buffer, 
				  u8 *p_bigbuf, 
				  unsigned long smallbuf_size, 
				  int cmd, 
				  servernode_t *sn) {
  

  if (cmd==READ) {
    PDEBUG("READ - memcopy of p_segment=0x%p in bh_buf=0x%p (size=%ld)\n",
	   p_bigbuf, bh_buffer, smallbuf_size);
    memcpy(bh_buffer, p_bigbuf, smallbuf_size);
  }
  else  
    if (cmd==WRITE) {
#ifdef PERF_STATS
      struct timeval debut_transfert_PIO, fin_transfert_PIO;
#endif

      PDEBUG("memcopy of bh_buf=0x%p in p_segment=0x%p (size=%ld)\n",
	     bh_buffer, p_bigbuf, smallbuf_size);
      
#ifdef PERF_STATS
      do_gettimeofday(&debut_transfert_PIO);
#endif
      
      sci_memcpy(p_bigbuf, bh_buffer, smallbuf_size);
      
#ifdef PERF_STATS
      do_gettimeofday(&fin_transfert_PIO);
      sn->somme_tailles_PIO += smallbuf_size;
      sn->somme_durees_PIO += 
	(fin_transfert_PIO.tv_sec - debut_transfert_PIO.tv_sec)*1000000
	+ (fin_transfert_PIO.tv_usec - debut_transfert_PIO.tv_usec);
#endif

    }
    else {
      PERROR("error smd_small_buf_transfer : bad command\n");
      return ENDREQ_ERROR;
    }


  return ENDREQ_SUCCESS;
}

/////// SMD_TRANSFER_BETWEEN_SMALL_BUFS_AND_BIG_BUF /////////
void smd_transfer_between_small_bufs_and_big_buf(servernode_t *sn, 
						 vkaddr_t seg_addr) {
  u8 *p_bigbuf;
  int status;
  struct request *req;
  struct buffer_head *bh;
  
  req = sn->creq.kreq;
  bh = req->bh; // 1er bh de la req 

  PDEBUG("req->bh->b_rdev=%d\n",
	 req->bh->b_rdev);
  

  p_bigbuf = (u8 *)seg_addr;	
  do {
    int smallbuf_size = req->current_nr_sectors * SMD_HARDSECT;

    status = smd_small_buf_transfer(bh->b_data, 
				    p_bigbuf, 
				    smallbuf_size, 
				    req->cmd,
				    sn);
    bh = bh->b_reqnext; // on passe au bh suivant
    p_bigbuf += smallbuf_size;
  } while (bh != NULL);
    // } while(end_that_request_first(req, status, MODULE_NAME));
  

}


//////////////////////////////////////////////////////////
//! on lance le traitement de la requête courante du snode 'sn'. Renvoie SUCCESS si elle a pu être lancée correctement et ERROR sinon
int smd_start_request(servernode_t *sn) {
  ndevice_t *nd;
  struct request *req;

#ifdef PERF_STATS
  // stats busy time
  if (smd_none_busy()) do_gettimeofday(&(smd.debut_busy));
  do_gettimeofday(&(sn->debut_busy)); 
#endif 

  PDEBUG("STARTING REQUEST %lld\n",
	 sn->creq.no);
   
  nd = sn->creq.nd;
  req = sn->creq.kreq;
    
  // remplissage du segment controle
  // avant que la req se soit modifiée
  nd->c_io.no = sn->creq.no;  
  nd->c_io.cmd = req->cmd;
  nd->c_io.offset = ((u64) req->sector) * SMD_HARDSECT;
  nd->c_io.size = req->nr_sectors * SMD_HARDSECT;
  nd->c_io.ndev = nd->localnb;

  (req->cmd == READ) ? sn->nb_reads++ : sn->nb_writes++;
  

  if (req->cmd == WRITE) 
    smd_transfer_between_small_bufs_and_big_buf(sn, 
						sn->rs_data_write_addr);

  return smd_send_request(nd);
}



extern void disconnect_from_snode(servernode_t *sn);

//////////////// SMD_REQUEST //////////////////
//! fonction request de notre BDD. Elle gère le traitement des lectures et des écritures sur notre BDD.
/*!
*/
void smd_request(request_queue_t *q) {
  int minor;
  struct request *req; 
  ndevice_t *nd; 
  servernode_t *sn;
  int start_result;

  do {
    
    /*PDEBUG("queue 0x%p (desc READ free =%d, desc WRITE free=%d)\n", 
     * q,
     * q->rq[0].count,
     * q->rq[1].count);
     */
    
    req = blkdev_entry_next_request(&q->queue_head);
    minor = DEVICE_NR(req->rq_dev);
    nd = minor2ndev[minor];
    sn = nd->sn;
    
    if (sn->busy != IDLE) {
      PDEBUG("snode '%s' is busy. Try later..\n",
	     sn->name);
      return;
    }
    sn->busy = BUSY_BEGIO;
    smd.nb_req++; // var glob protégée par io_request_lock

    PDEBUG("NEW REQUEST %lld : %s on ndev '%s(s=%d):%s'. offset=%lld "
	   "size=%ld\n",
	   smd.nb_req,
	   (req->cmd == 0) ? "READ" : "WRITE",
	   sn->name, 
	   sn->status,
	   nd->name,
	   ((u64) req->sector) * SMD_HARDSECT,
	   req->nr_sectors * SMD_HARDSECT);
 
    blkdev_dequeue_request(req);
    sn->creq.kreq = req;  
    sn->creq.nd = nd;
    sn->creq.no = smd.nb_req;

    spin_unlock_irq(&io_request_lock);

    if (sn->status != UP) {
      PDEBUG("snode '%s' isn't UP -> aborting req %lld\n",
	     sn->name, sn->creq.no);

      if (sn->status == DECONNEXION) 
	disconnect_from_snode(sn);
      
      start_result = ERROR;
    }
    else 
      start_result = smd_start_request(sn);

    PDEBUG("start_result = %s\n",
	   (start_result == SUCCESS) ? "SUCCESS" : "ERROR");

    if (start_result == ERROR) {
      smd_end_request(sn, ENDREQ_ERROR);
      q = smd_next_non_empty_queue(sn);
    
      spin_lock_irq(&io_request_lock);
      sn->busy = IDLE;
    } 
  } while ((start_result == ERROR) &&
	   (q != NULL));
}	



///////////////// SMD_END_REQUEST /////////////////
void smd_end_request(servernode_t *sn, int status) {
  // on prévient le noyau que cette requête est traitée 
  struct request *req;
  int autolock=FALSE;
  unsigned long flags =0;
  ndevice_t *nd;

  req = sn->creq.kreq;
  nd = sn->creq.nd;
  PDEBUG("begin (nd='%s:%s' end request status=%d) req=0x%p\n",
	 sn->name, nd->name,
	 status,
	 req);
  
  do {PDEBUG("bh = 0x%p\n",req->bh);}
  while (end_that_request_first(req, status, MODULE_NAME));
  
  if (spin_is_locked(&io_request_lock) == FALSE) {
    PDEBUG("io_request_lock isn't locked: I'm locking it\n");
    spin_lock_irqsave(&io_request_lock,flags); 
    autolock = TRUE;
  }

  PDEBUG("terminate kernel request\n");
  end_that_request_last(req); 


  PDEBUG("req=0x%p (end status=%d). dev minor=%d nd=%s\n",
	 req,
	 status,
	 DEVICE_NR(req->rq_dev),
	 minor2ndev[DEVICE_NR(req->rq_dev)]->name);

  sn->creq.kreq = NULL; 

  if (autolock == TRUE) {
    PDEBUG("unlocking io_request_lock AUTO LOCK\n");
    spin_unlock_irqrestore(&io_request_lock, flags);
  } 
 
  PDEBUG("end\n");
}
 
///////////////// END_IO_HANDLER /////////////////
//! callback function for IT end_io
signed32 end_io_handler(unsigned32 IN local_adapter_number,
			void IN *arg,
			unsigned32 IN interrupt_number) {
  servernode_t *sn;
  ndevice_t *nd;
  request_queue_t *q;
  unsigned long flags;
  
  
  // on récupère le no de brique associée à cette IT 
  // (on est obligé de faire comme ca, 
  // parce qu'il est impossible de faire passer un arg, 
  // dans une IT GENIF <-> SISCI
  sn = itendio2snode[interrupt_number];
  if (sn == NULL) {
    PERROR("ERROR bad IT number. Can't get server node. IT nr = %d.\n", 
	   interrupt_number);
    return 0;
  }  
    
  sn->busy = BUSY_ENDIO;  
  

 
  nd = sn->creq.nd; 
  PDEBUG("IT ENDIO for req %lld (0x%p) on ndev '%s:%s'\n", 
	 sn->creq.no,
	 sn->creq.kreq, 
	 sn->name,
	 nd->name);

  if (nd->c_io.cmd == READ) {	  		
    smd_transfer_between_small_bufs_and_big_buf(sn, 
						sn->ls_data_read_addr);
  }
  	
  //spin_lock_irqsave(&io_request_lock,flags);
  PDEBUG("request completed : SUCCESS\n");
  smd_end_request(sn, ENDREQ_SUCCESS);
#ifdef PERF_STATS
  // stats busy time
  do_gettimeofday(&(sn->fin_busy)); 
  sn->busy_time += 
    (sn->fin_busy.tv_sec - 
     sn->debut_busy.tv_sec)*1000000
    + (sn->fin_busy.tv_usec 
       - sn->debut_busy.tv_usec);
  if (smd_none_busy()) {
    do_gettimeofday(&(smd.fin_busy)); 
    smd.busy_time += (smd.fin_busy.tv_sec - 
		      smd.debut_busy.tv_sec)*1000000
      + (smd.fin_busy.tv_usec - smd.debut_busy.tv_usec);
  }	   

  // stats transferts disque
  do_gettimeofday(&(sn->fin_transfert_disque));
  sn->nb_transferts_disque++;
  sn->somme_durees_disque += 
    (sn->fin_transfert_disque.tv_sec - 
     sn->debut_transfert_disque.tv_sec)*1000000
    + (sn->fin_transfert_disque.tv_usec 
       - sn->debut_transfert_disque.tv_usec);
#endif

  
  q = smd_next_non_empty_queue(sn);
  spin_lock_irqsave(&io_request_lock,flags);

  sn->busy = IDLE;

  if (q != NULL) smd_request(q);
 
  
  PDEBUG("unlocking 'io_request_lock' because no req to handle\n");
  spin_unlock_irqrestore(&io_request_lock,flags);
 	
  return 0;
}

///////////////// IT READY_HANDLER /////////////////
//! callback function for IT error_io
signed32 snode_ready_handler(unsigned32 IN local_adapter_number,
			     void IN *arg,
			     unsigned32 IN interrupt_number) {
  servernode_t *sn;
  
  sn = itready2snode[interrupt_number];
			
  PDEBUG("IT READY for snode '%s'\n", sn->name);
  
  if (sn == NULL) {
    PERROR("ERROR bad IT number. Can't get server node. IT nr = %d.\n", 
	   interrupt_number);
    
    return 0;
  }  
  
  sn->status = UP;
  	
  return 0;
}

//! annule la requête en cours de traitement par le snode 'sn', en renvoyant 'erreur' au noyau.
void abort_current_req(servernode_t *sn) {
  request_queue_t *q;
  long flags;
  
  smd_end_request(sn, ENDREQ_ERROR);
  
  q = smd_next_non_empty_queue(sn);
  spin_lock_irqsave(&io_request_lock,flags);
  
  sn->busy = IDLE;
  if (q != NULL) smd_request(q);
  
  
  PDEBUG("unlocking 'io_request_lock' because no req to handle\n");
  spin_unlock_irqrestore(&io_request_lock,flags);
}

///////////////// ERROR_IO_HANDLER /////////////////
//! callback function for IT error_io
signed32 error_io_handler(unsigned32 IN local_adapter_number,
			  void IN *arg,
			  unsigned32 IN interrupt_number) {
  u32 i;
  servernode_t *sn;
  ndevice_t *nd;
  
  sn = iterrorio2snode[interrupt_number];
			

  
  //PDEBUG("IT for brique %d and cmd = %d\n",no_brique, sn->current_cmd);
  
  if (sn == NULL) {
    PERROR("ERROR bad IT number. Can't get server node. IT nr = %d.\n", 
	   interrupt_number);
    for (i=0; i<NBMAX_SNODES; i++) {
      if (smd.snodes[i] != NULL) {
     
      PDEBUG("IT for snode '%s' = %d.\n", 
	     smd.snodes[i]->name, 
	     smd.snodes[i]->no_it_errorio);
      }
    }

    PDEBUG("Nr of snodes =%d\n",smd.nb_snodes);
     return 0;
  }  
 
  nd = sn->creq.nd;    	
  PDEBUG("REQUEST %lld (0x%p) on dev '%s:%s' failed\n", 
	 nd->c_io.no,
	 sn->creq.kreq, 
	 sn->name,
	 nd->name);

  abort_current_req(sn);
 	
  return 0;
}

#ifdef PERF_STATS
// permet de maj les chronometres pour les stats pour le %age de busy
// avnat de les afficher dans le /proc
void synchro_busy_stats(void) {
  int i;
  servernode_t *sn;

  for (i=0; i < NBMAX_SNODES; i++) 
    if (smd.snodes[i] != NULL) {
      sn = smd.snodes[i];
      if (sn->busy != IDLE) {
	do_gettimeofday(&(sn->fin_busy)); 
	sn->busy_time += 
	  (sn->fin_busy.tv_sec - 
	   sn->debut_busy.tv_sec)*1000000
	  + (sn->fin_busy.tv_usec 
	     - sn->debut_busy.tv_usec);
	
	sn->debut_busy.tv_sec = sn->fin_busy.tv_sec;
	sn->debut_busy.tv_usec = sn->fin_busy.tv_usec;
      }
    }
  
    
  if (!smd_none_busy()) {
    do_gettimeofday(&(smd.fin_busy)); 
    smd.busy_time += 
      (smd.fin_busy.tv_sec - smd.debut_busy.tv_sec)*1000000
      + (smd.fin_busy.tv_usec - smd.debut_busy.tv_usec);

    smd.debut_busy.tv_sec = smd.fin_busy.tv_sec;
    smd.debut_busy.tv_usec = smd.fin_busy.tv_usec;
  }
  

}
#endif

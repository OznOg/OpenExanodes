#include <unistd.h>
#include "sisci_api.h"
#include "constantes.h"
#include "smd_server.h"
#include <asm/types.h>
#include <string.h>
#include <sys/time.h>
#include <stdio.h>
#include "smdsv_divers.h"
#include <stdlib.h>
#include <pthread.h>

volatile int veritable_io; 

extern char *Sci_error(int error);
extern int connexion_sci_client(client_t *cl);

//! renvoie le pointeur sur le device de nom 'name'. Renvoie NULL si aucun device n'a ce nom
device_t *get_device(char *dev_name) {
  __u32 i;

  for (i=0; i<NBMAX_DEVICES; i++) 
    if (smdsv.devices[i] != NULL) 
      if (strcmp(smdsv.devices[i]->name, dev_name) == 0)  
	return smdsv.devices[i];
      
  return NULL;
}  

//////////////////////////////////////////////////////
// renvoie une valeur != 0 si l'écriture n'a pas pu se faire sur le device
// TODO : cette fonction est cruciale pour les perf. 
// Est-ce qu'il n'existe pas de moyen plus
// rapide de faire une ecriture, en mode user ?
int ecrire(iodesc_t *io_a_traiter,
	   client_t *cl,
	   device_t *dev) {
  int /*pos_io,*/nb_octets;
  char *donnees;
#ifdef PERF_STATS
  struct timeval beg_time, end_time;
  double duree_req;
#endif

  log_message("io_a_traiter->byte = %lld taille = %d\n",
	      io_a_traiter->byte,
	      io_a_traiter->nr_bytes);
#ifdef PERF_STATS
  gettimeofday(&beg_time,NULL);
#endif
  donnees=(char *) cl->local_data_write_address; 
  log_message("contenu du buffer\n");
  log_donnees(io_a_traiter->byte, donnees, io_a_traiter->nr_bytes);
  
  //pthread_mutex_lock(&dev->mutex);
  nb_octets = pwrite64(dev->fd,
		       (int *)cl->local_data_write_address,
		       io_a_traiter->nr_bytes,
		       io_a_traiter->byte);
  //pos_io = lseek64(dev->fd,io_a_traiter->byte,SEEK_SET);  
  //nb_octets = write(dev->fd,
  //		    (int *)cl->local_data_write_address,
  //		    io_a_traiter->nr_bytes);
  //pthread_mutex_unlock(&dev->mutex);
  pthread_testcancel();

  if (nb_octets == -1) {
    log_error("pwrite64 n'a pas pu écrire les données\n");
    return ERROR;
  }

  if (nb_octets != io_a_traiter->nr_bytes) {
    log_error("wrong number of bytes written\n");
    return ERROR;
  }

#ifdef PERF_STATS
  gettimeofday(&end_time,NULL);

  pthread_mutex_lock(&smdsv.mutex);
  smdsv.somme_transferts += nb_octets;
  smdsv.somme_tailles_ecriture += nb_octets;
  pthread_mutex_unlock(&smdsv.mutex);

  duree_req = (end_time.tv_sec - beg_time.tv_sec)*1000.0   
              + (end_time.tv_usec - beg_time.tv_usec)/1000.0;
  log_message("temps de traitement requête = %.2f ms pour %d octets\n",
	      duree_req,
	      io_a_traiter->nr_bytes);
  log_message("debit = %.2f Mo/s\n", 
	      (io_a_traiter->nr_bytes)/(duree_req * 1000.0));

  pthread_mutex_lock(&smdsv.mutex);
  smdsv.nb_transferts_ecriture++;
  smdsv.somme_debit_ecriture+=(io_a_traiter->nr_bytes)/(duree_req * 1000.0);
  pthread_mutex_unlock(&smdsv.mutex);

#endif
  log_message("local write done\n");
  return SUCCESS;
}

//////////////////////////////////////////////////////
// TODO : cette fonction est cruciale pour les perf. 
// Est-ce qu'il n'existe pas de moyen plus
// rapide de faire une lecutreture, en mode user ?
int lire(iodesc_t *io_a_traiter, 
	 client_t *cl,
	 device_t *dev) {
  int /*pos_io,*/nb_octets;
  sci_error_t retval;

  // sci_sequence_status_t sequenceStatus;
  // unsigned int nostores;
  // int j;

#ifdef PERF_STATS
  struct timeval beg_time, end_time;
  double duree_req;
#endif

#ifdef PERF_STATS
  gettimeofday(&beg_time,NULL);
#endif
  log_message("io_a_traiter->byte = %llu taille = %d\n",
	      io_a_traiter->byte,
	      io_a_traiter->nr_bytes );

  //pthread_mutex_lock(&dev->mutex);
  //pos_io = lseek64(dev->fd,io_a_traiter->byte,SEEK_SET); 
  //nb_octets = read(dev->fd,
  //	   (int *)cl->local_read_temp_address,
  //	   io_a_traiter->nr_bytes);
  nb_octets = pread64(dev->fd,
		      (int *)cl->local_read_temp_address,
		      io_a_traiter->nr_bytes,
		      io_a_traiter->byte);
  //pthread_mutex_unlock(&dev->mutex);
  pthread_testcancel();

  if (nb_octets == -1) {
    log_error("pread64 n'a pas pu lire les données\n");
    return ERROR;
  }

  if (nb_octets != io_a_traiter->nr_bytes) {
    log_message("wrong number of bytes read\n");
    return ERROR;
  }
  
  SCITransferBlock(cl->local_read_temp_map, 
		   0, 
		   cl->map_data_read_segment, 
		   0,
		   io_a_traiter->nr_bytes , 
		   NO_FLAGS,
		   &retval);
  pthread_testcancel();
  if (retval == SCI_ERR_OK) {
    log_message("The data segment is transferred from the local node to the remote node\n");
  } else {
    log_error("SCITransferBlock failed - %s\n",Sci_error(retval));
    return ERROR; 
  } 
    
  /*do {
    // Start data error checking 
    sequenceStatus = SCIStartSequence(cl->seq_data_read_segment,NO_FLAGS,&error);
    } while (sequenceStatus != SCI_SEQ_OK) ;

  nostores = (unsigned int) ceil(io_a_traiter->nr_bytes / sizeof(int)); 
  //Transfer data to remote node 
  for (j=0;j<nostores;j++) {
    cl->addr_data_read_segment[j] = local_read_temp_address[j];
  }
  
  // Check for error after data transfer 
  sequenceStatus = SCICheckSequence(cl->seq_data_read_segment,NO_FLAGS,&error);
  if (sequenceStatus != SCI_SEQ_OK) {
    log_error("lire: Data transfer failed\n");
    return SCI_ERR_TRANSFER_FAILED;
  } */   


#ifdef PERF_STATS
  gettimeofday(&end_time,NULL);

  pthread_mutex_lock(&smdsv.mutex);
  smdsv.somme_transferts += nb_octets;
  smdsv.somme_tailles_lecture += nb_octets;
  pthread_mutex_unlock(&smdsv.mutex);

  duree_req = (end_time.tv_sec - beg_time.tv_sec)*1000.0 
    + (end_time.tv_usec - beg_time.tv_usec)/1000.0;
  log_message("temps de traitement requête = %.2f ms pour %d octets\n",
	      duree_req,
	      io_a_traiter->nr_bytes);
  log_message("debit = %.2f Mo/s\n", 
	      (io_a_traiter->nr_bytes)/(duree_req * 1000.0));

  pthread_mutex_lock(&smdsv.mutex);
  smdsv.nb_transferts_lecture++;
  smdsv.somme_debit_lecture+=(io_a_traiter->nr_bytes)/(duree_req * 1000.0);
  pthread_mutex_unlock(&smdsv.mutex);

#endif
  log_message("local read done\n");

  log_message("contenu du buffer\n");
  log_donnees(io_a_traiter->byte,
	      (char *)cl->addr_data_read_segment, 
	      io_a_traiter->nr_bytes);

#ifdef PERF_STATS
  // transfert des données par DMA 
  gettimeofday(&beg_time,NULL);
#endif

  

#ifdef PERF_STATS
  gettimeofday(&end_time,NULL);
  duree_req = (end_time.tv_sec - beg_time.tv_sec)*1000.0 
    + (end_time.tv_usec - beg_time.tv_usec)/1000.0;
  log_message("temps du transfert DMA = %.2f ms pour %d octets\n",
	      duree_req,
	      io_a_traiter->nr_bytes);
  log_message("debit = %.2f Mo/s\n", (io_a_traiter->nr_bytes)/(duree_req * 1000.0));
  log_message("local read done\n");

  pthread_mutex_lock(&smdsv.mutex);
  smdsv.nb_transferts_sci++;
  smdsv.somme_debit_sci+=(io_a_traiter->nr_bytes)/(duree_req * 1000.0);
  pthread_mutex_unlock(&smdsv.mutex);

#endif
  log_message("DMA transfer done\n");
  return SUCCESS;
}



void send_ioerror(client_t *cl) {
  sci_error_t retval;
 
  log_message("IO failed - triggering IT ERROR_IO\n");
  if (cl->remote_error_io_interrupt != NULL) {
    SCITriggerInterrupt(cl->remote_error_io_interrupt, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) {
      log_error("SCITriggerInterrupt failed\n");
      log_error("%s\n",Sci_error(retval));
    }
  }
  else log_error("can't trigger IT coz its handle is NULL\n");
}


int deconnexion_sci_client(client_t *cl);


//////////////////////////////////////////////////////
void traitement_d_une_io(void *p)  {
#ifdef PERF_STATS
  struct timeval beg_time, end_time;
#endif
  sci_error_t retval;
  iodesc_t *io_a_traiter;
  client_t *cl;

  cl = (client_t *) p;
  log_message("thread for client SCI %d started\n", cl->no_sci);

  while (1) {
  wait_debio:

    // faire marcher l'alarm signal
    pthread_testcancel();
    SCIWaitForInterrupt(cl->local_deb_io_interrupt,
			SCI_INFINITE_TIMEOUT,
			NO_FLAGS,
			&retval);
    pthread_testcancel();
    if (retval != SCI_ERR_OK) {
      log_error("SCIWaitForInterrupt DEBIO\n");
      goto wait_debio;
    }
    log_message("SCIWaitForInterrupt passed : NEW IO\n");
    
    log_message("cl->status = %d\n",cl->status);
    if (cl->status != UP) {
      log_error("client SCI %d is down. I don't process the io\n",
		cl->no_sci);
      send_ioerror(cl); // on sait jamais l'IT est peut-être encore levable
      goto wait_debio;
    }
      
    // on traite l'I/O
#ifdef PERF_STATS
    gettimeofday(&beg_time,NULL);
#endif
  
    io_a_traiter = (iodesc_t *) cl->local_ctl_address;
    log_message("processing I/O %lld for client SCI n° %d\n",
		io_a_traiter->no,
		cl->no_sci);
    log_message("io_desc->no = %lld\n",io_a_traiter->no);
    log_message("io_desc->cmd = %d\n",io_a_traiter->cmd);
    log_message("io_desc->byte = %lld\n",io_a_traiter->byte);
    log_message("io_desc->nr_bytes = %d\n",io_a_traiter->nr_bytes);
    log_message("io_desc->device number = %d\n",io_a_traiter->devnb);
  
    switch(io_a_traiter->cmd) {
    case READ:
      retval = lire(io_a_traiter,
		    cl,
		    smdsv.devices[io_a_traiter->nodev]); 
      break;
    case WRITE:
      retval = ecrire(io_a_traiter,
		      cl,
		      smdsv.devices[io_a_traiter->nodev]); 
      break;
    default:
      log_message("error - unknown command\n");
    }
  
    if (retval == ERROR) {
      send_ioerror(cl);
      goto wait_debio;
    }

    // on signale au client que l'IO est terminée
    log_message("triggering interrupt END_IO\n");
    SCITriggerInterrupt(cl->remote_fin_io_interrupt, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) {
      log_message("SCITriggerInterrupt failed\n");
      log_message("%s\n",Sci_error(retval));
      goto wait_debio;
    }
  
    log_message("I/O %lld done for client SCI n° %d\n",
		io_a_traiter->no, cl->no_sci);
    log_message("-----------------------------------------------------");
 
    pthread_mutex_lock(smdsv.mutex);
    smdsv.nb_io++;
    pthread_mutex_unlock(smdsv.mutex);
  
    cl->nb_io++;
  
#ifdef PERF_STATS
    gettimeofday(&end_time,NULL);
  
    pthread_mutex_lock(&smdsv.mutex);
    smdsv.somme_durees +=  (end_time.tv_sec - beg_time.tv_sec)*1000.0 
      + (end_time.tv_usec - beg_time.tv_usec)/1000.0;
    pthread_mutex_unlock(&smdsv.mutex);
  
    log_message("debit moyen = %.2f Mo/s (%.2f octets en %.2f ms). "
		"I/O mean size = %.2f bytes\n",
		(smdsv.somme_transferts)/(smdsv.somme_durees * 1000.0),
		smdsv.somme_transferts,
		smdsv.somme_durees,
		smdsv.somme_transferts/cl->nb_io);
#endif
  }
}

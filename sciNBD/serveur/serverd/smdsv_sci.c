#include "sisci_api.h"
#include "smd_server.h"
#include <stdlib.h>


int connexion_sci_client(client_t *cl);
int deconnexion_sci_client(client_t *cl);

char *Sci_error(int error) {
   char **string_error=(char **)(&string_err);

   *string_error = "Error UNKNOWN\n";

   if (error == SCI_ERR_OK              	   ) 
     *string_error = "NO ERROR"; 
   if (error == SCI_ERR_BUSY              	   ) 
     *string_error = "Error SISCI 'SCI_ERR_BUSY'"; 
   if (error == SCI_ERR_FLAG_NOT_IMPLEMENTED       ) 
     *string_error = "Error SISCI 'SCI_ERR_FLAG_NOT_IMPLEMENTED'";  
   if (error == SCI_ERR_ILLEGAL_FLAG               ) 
     *string_error = "Error SISCI 'no SCI_ERR_ILLEGAL_FLAG'";  
   if (error == SCI_ERR_NOSPC                      ) 
     *string_error = "Error SISCI 'SCI_ERR_NOSPC'";  
   if (error == SCI_ERR_API_NOSPC                  ) 
     *string_error = "Error SISCI 'SCI_ERR_API_NOSPC'";         
   if (error == SCI_ERR_HW_NOSPC                   ) 
     *string_error = "Error SISCI 'SCI_ERR_HW_NOSPC'";  
   if (error == SCI_ERR_NOT_IMPLEMENTED            ) 
     *string_error = "Error SISCI 'SCI_ERR_NOT_IMPLEMENTED'";  
   if (error == SCI_ERR_ILLEGAL_ADAPTERNO          ) 
     *string_error = "Error SISCI 'SCI_ERR_ILLEGAL_ADAPTERNO'";   
   if (error == SCI_ERR_NO_SUCH_ADAPTERNO          ) 
     *string_error = "Error SISCI 'SCI_ERR_NO_SUCH_ADAPTERNO'";
   if (error == SCI_ERR_TIMEOUT                    ) 
     *string_error = "Error SISCI 'SCI_ERR_TIMEOUT'";
   if (error == SCI_ERR_OUT_OF_RANGE               ) 
     *string_error = "Error SISCI 'SCI_ERR_OUT_OF_RANGE'";
   if (error == SCI_ERR_NO_SUCH_SEGMENT            ) 
     *string_error = "Error SISCI 'SCI_ERR_NO_SUCH_SEGMENT'";
   if (error == SCI_ERR_ILLEGAL_NODEID             ) 
     *string_error = "Error SISCI 'SCI_ERR_ILLEGAL_NODEID'";
   if (error == SCI_ERR_CONNECTION_REFUSED         ) 
     *string_error = "Error SISCI 'SCI_ERR_CONNECTION_REFUSED'";
   if (error == SCI_ERR_SEGMENT_NOT_CONNECTED      ) 
     *string_error = "Error SISCI 'SCI_ERR_SEGMENT_NOT_CONNECTED'";
   if (error == SCI_ERR_SIZE_ALIGNMENT             ) 
     *string_error = "Error SISCI 'no SCI_ERR_SIZE_ALIGNMENT'";
   if (error == SCI_ERR_OFFSET_ALIGNMENT           ) 
     *string_error = "Error SISCI 'SCI_ERR_OFFSET_ALIGNMENT'";
   if (error == SCI_ERR_ILLEGAL_PARAMETER          ) 
     *string_error = "Error SISCI 'SCI_ERR_ILLEGAL_PARAMETER'";
   if (error == SCI_ERR_MAX_ENTRIES                ) 
     *string_error = "Error SISCI 'SCI_ERR_MAX_ENTRIES'";   
   if (error == SCI_ERR_SEGMENT_NOT_PREPARED       ) 
     *string_error = "Error SISCI 'SCI_ERR_SEGMENT_NOT_PREPARED'";
   if (error == SCI_ERR_ILLEGAL_ADDRESS            ) 
     *string_error = "Error SISCI 'SCI_ERR_ILLEGAL_ADDRESS'";
   if (error == SCI_ERR_ILLEGAL_OPERATION          ) 
     *string_error = "Error SISCI 'SCI_ERR_ILLEGAL_OPERATION'";
   if (error == SCI_ERR_ILLEGAL_QUERY              ) 
     *string_error = "Error SISCI 'SCI_ERR_ILLEGAL_QUERY'";
   if (error == SCI_ERR_SEGMENTID_USED             ) 
     *string_error = "Error SISCI 'SCI_ERR_SEGMENTID_USED'";
   if (error == SCI_ERR_SYSTEM                     ) 
     *string_error = "Error SISCI 'SCI_ERR_SYSTEM'";
   if (error == SCI_ERR_CANCELLED                  )
     *string_error = "Error SISCI 'SCI_ERR_CANCELLED'";
   if (error == SCI_ERR_NOT_CONNECTED              ) 
     *string_error = "Error SISCI 'SCI_ERR_NOT_CONNECTED'";
   if (error == SCI_ERR_NOT_AVAILABLE              ) 
     *string_error = "Error SISCI 'SCI_ERR_NOT_AVAILABLE'";
   if (error == SCI_ERR_INCONSISTENT_VERSIONS      ) 
     *string_error = "Error SISCI 'SCI_ERR_INCONSISTENT_VERSIONS'";
   if (error == SCI_ERR_COND_INT_RACE_PROBLEM      ) 
     *string_error = "Error SISCI 'SCI_ERR_COND_INT_RACE_PROBLEM'";
   if (error == SCI_ERR_OVERFLOW                   ) 
     *string_error = "Error SISCI 'SCI_ERR_OVERFLOW'";
   if (error == SCI_ERR_NOT_INITIALIZED            ) 
     *string_error = "Error SISCI 'SCI_ERR_NOT_INITIALIZED'";
   if (error == SCI_ERR_ACCESS                     ) 
     *string_error = "Error SISCI 'SCI_ERR_ACCESS'";
   if (error == SCI_ERR_NO_SUCH_NODEID             ) 
     *string_error = "Error SISCI 'SCI_ERR_NO_SUCH_NODEID'";
   if (error == SCI_ERR_NODE_NOT_RESPONDING        ) 
     *string_error = "Error SISCI 'SCI_ERR_NODE_NOT_RESPONDING'";  
   if (error == SCI_ERR_NO_REMOTE_LINK_ACCESS      ) 
     *string_error = "Error SISCI 'SCI_ERR_NO_REMOTE_LINK_ACCESS'";
   if (error == SCI_ERR_NO_LINK_ACCESS             ) 
     *string_error = "Error SISCI 'SCI_ERR_NO_LINK_ACCESS'";
   if (error == SCI_ERR_TRANSFER_FAILED            ) 
     *string_error = "Error SISCI 'SCI_ERR_TRANSFER_FAILED'";

   return *string_error;
}

//! fonction qui transforme l'id relatif d'un segment ou d'un IT en véritable ID (cette implémentation utilise le no_sci du noeud qui contient le segment)
long real_id(long no_sci, int id_relatif) {
  return no_sci*10+id_relatif;
}


void do_connexion(client_t *cl) {
  cl->status = DO_CONNEXION;
  cl->cxstate_changed = TRUE;
  pthread_mutex_lock(cl->mutex_cxstate);
  pthread_cond_signal(&cl->cond_cxstate);
  pthread_mutex_unlock(cl->mutex_cxstate);
}

void do_send_ready(client_t *cl) {
  cl->status = DO_SENDREADY;
  cl->cxstate_changed = TRUE;
  pthread_mutex_lock(cl->mutex_cxstate);
  pthread_cond_signal(&cl->cond_cxstate);
  pthread_mutex_unlock(cl->mutex_cxstate);
}

void do_deconnexion(client_t *cl) {
  cl->status = DO_DECONNEXION;
  cl->cxstate_changed = TRUE;
  pthread_mutex_lock(cl->mutex_cxstate);
  pthread_cond_signal(&cl->cond_cxstate);
  pthread_mutex_unlock(cl->mutex_cxstate);
}

sci_callback_action_t local_segment_cb(void* arg,
				       sci_local_segment_t segment,
				       sci_segment_cb_reason_t reason,
				       unsigned int remote_sci,
				       unsigned int adapter_no,
				       sci_error_t status) {
  client_t *cl;

  log_message("lseg=0x%p callback_arg=0x%p reason = 0x%x  status=0x%x "
	      "remote_sci = %d adapter_no = %d\n",
	      segment, arg, reason, status, remote_sci, adapter_no);

  cl = (client_t *) arg; 

 switch(reason) {
  case SCI_CB_CONNECT:
    log_message("LOCAL SCI_CB_CONNECT for client SCI %d\n",
		cl->no_sci);
    break;
  case SCI_CB_DISCONNECT:
    log_message("LOCAL SCI_CB_DISCONNECT for client SCI %d\n",
		cl->no_sci);
    do_deconnexion(cl); 
    break;
  case SCI_CB_OPERATIONAL:
    log_message("LOCAL SCI_CB_OPERATIONAL for client SCI %d\n",
		cl->no_sci);
     break;
  case SCI_CB_NOT_OPERATIONAL:
    log_message("LOCAL SCI_CB_NOT_OPERATIONAL for client SCI %d\n",
		cl->no_sci);
    break;
  case SCI_CB_LOST:
    log_message("LOCAL SCI_CB_LOST for client SCI %d\n",
		cl->no_sci);
    do_deconnexion(cl); 
    break;
  default: 
    log_error("LOCAL SEG: unknown callback reason !\n");
  }  
  
  log_message("end\n");
  return SCI_CALLBACK_CONTINUE;
} 

sci_callback_action_t it_clready_cb(void* arg,
				    sci_local_interrupt_t lit,
				    sci_error_t error) {
  client_t *cl; 
  
  log_message("lit=0x%p arg= 0x%p error=%s\n",
	      lit, arg, Sci_error(error));
  
  cl = (client_t *) arg; 
  
  log_message("getting IT CLREADY from client SCI %d: "
	      "connection de client SCI resources\n",
	      cl->no_sci);

  do_connexion(cl); 

  return SCI_CALLBACK_CONTINUE;
}
  

sci_callback_action_t remote_segment_cb(void* arg,
					sci_remote_segment_t segment,
					sci_segment_cb_reason_t reason,
					sci_error_t status) {
  client_t *cl; 
  
  log_message("rseg=0x%p callback_arg=0x%p reason = 0x%x  status=0x%x\n",
	      segment, arg, reason, status);

  cl = (client_t *) arg; 
  log_message("cl sci = %d, status = %d\n",
	      cl->no_sci,
	      cl->status);

  switch(reason) {
  case SCI_CB_CONNECT:
    log_message("REMOTE SCI_CB_CONNECT for client SCI %d\n",
		cl->no_sci);
    break;
  case SCI_CB_DISCONNECT:
    log_message("REMOTE SCI_CB_DISCONNECT for client SCI %d\n",
		cl->no_sci);
    break;
  case SCI_CB_OPERATIONAL:
    log_message("REMOTE SCI_CB_OPERATIONAL for client SCI %d",
		cl->no_sci);
    if ((cl->status == DO_DECONNEXION) ||
	(cl->status == SUSPEND)) { 
      log_message("sending ready IT\n");
      do_send_ready(cl);
    }
    break;
  case SCI_CB_NOT_OPERATIONAL:
    log_message("REMOTE SCI_CB_NOT_OPERATIONAL for client SCI %d\n",
		cl->no_sci);
    cl->status = SUSPEND;
    //set_client_down(cl);
    break;
  case SCI_CB_LOST:
    log_message("REMOTE SCI_CB_LOST for client SCI %d\n",cl->no_sci);
    break;
  default: 
    log_error("REMOTE SEG: unknown callback reason !\n");
  }
  
  log_message("end\n");
  return SCI_CALLBACK_CONTINUE;
} 




int deconnexion_sci_client(client_t *cl) {
  int ret;
  sci_error_t retval; 

  ret = SUCCESS;
  log_message("begining\n");
  if (cl->remote_fin_io_interrupt != NULL) {
    SCIDisconnectInterrupt(cl->remote_fin_io_interrupt, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
    cl->remote_fin_io_interrupt = NULL;
  }
  log_message("SCIDisconnectInterrupt(cl->remote_fin_io_interrupt) done\n");
  if (cl->remote_error_io_interrupt != NULL) {
    SCIDisconnectInterrupt(cl->remote_error_io_interrupt, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
    cl->remote_error_io_interrupt = NULL;
  }
  log_message("SCIDisconnectInterrupt(cl->remote_error_io_interrupt) done\n");
  if (cl->remote_ready_interrupt != NULL) {
    SCIDisconnectInterrupt(cl->remote_ready_interrupt, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
    cl->remote_ready_interrupt = NULL;
  }
  log_message("SCIDisconnectInterrupt(cl->remote_ready_interrupt) done\n");
  //SCIRemoveSequence(cl->seq_data_read_segment, NO_FLAGS, &retval);
  //if (retval != SCI_ERR_OK) ret = ERROR;

  if (cl->map_data_read_segment != NULL) {
    SCIUnmapSegment(cl->map_data_read_segment, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
    cl->map_data_read_segment = NULL;
  }
  log_message("SCIUnmapSegment(cl->map_data_read_segment) done\n");
  if (cl->rs_data_read_segment != NULL) {
    SCIDisconnectSegment(cl->rs_data_read_segment,  NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;   
    cl->rs_data_read_segment = NULL;
  }
  log_message("CIDisconnectSegment(cl->rs_data_read_segment) done\n");
  log_message("end: retval = %d",ret);

  return ret;

}

//! lève l'IT SERVER_READY sur le client passé en param. renvoie SUCCESS/ERROR
int send_ready_sci(client_t *cl) {
  sci_error_t retval;

  if (cl->remote_ready_interrupt != NULL) {
    SCITriggerInterrupt(cl->remote_ready_interrupt, NO_FLAGS, &retval);
    if (retval != SCI_ERR_OK) {
      log_error("SCITriggerInterrupt READY failed\n");
      log_error("%s\n",Sci_error(retval));
      return ERROR;
    }
    log_message("IT READY triggered on cl %d\n", cl->no_sci);
  } else {
    log_error("IT ptr is NULL, can't trigger READY IT for cl %d\n",
	      cl->no_sci);
    return ERROR;
  }
  return SUCCESS;
}

int connexion_sci_client(client_t *cl) {
  int seg_size;
  sci_error_t retval; 
  int ret;
  
  ret = SUCCESS;

  log_message("beginning - no_sci client = %d\n", cl->no_sci);
  log_message("connection to IT END_IO %ld on node %d\n",
	      real_id(smdsv.THIS_NODE_ID,IT_END_IO) , 
	      cl->no_sci);
  SCIConnectInterrupt(cl->vd_finio, 
		      &cl->remote_fin_io_interrupt, 
		      cl->no_sci, ADAPTER_NO,
		      real_id(smdsv.THIS_NODE_ID,IT_END_IO), 
		      SCI_INFINITE_TIMEOUT,
		      NO_FLAGS,&retval);
  if (retval != SCI_ERR_OK) {
    log_error(" SCIConnectInterrupt IT_END_IO failed\n");
    log_error("%s\n",Sci_error(retval));
    ret = ERROR;
  }
  log_message("connection succeeded\n");
  
  log_message("connection to IT ERROR_IO %ld on node %d\n",
	      real_id(smdsv.THIS_NODE_ID,IT_ERROR_IO) , 
	      cl->no_sci);
  SCIConnectInterrupt(cl->vd_errorio, 
		      &cl->remote_error_io_interrupt, 
		      cl->no_sci, ADAPTER_NO,
		      real_id(smdsv.THIS_NODE_ID,IT_ERROR_IO), 
		      SCI_INFINITE_TIMEOUT,
		      NO_FLAGS,&retval);
  if (retval != SCI_ERR_OK) {
    log_error(" SCIConnectInterrupt IT_ERROR_IO failed\n");
    log_error("%s\n",Sci_error(retval));
    ret = ERROR;
  }
  log_message("connection succeeded\n");


  log_message("connection to IT READY %ld on node %d\n",
	      real_id(smdsv.THIS_NODE_ID,IT_READY) , 
	      cl->no_sci);
  SCIConnectInterrupt(cl->vd_ready, 
		      &cl->remote_ready_interrupt, 
		      cl->no_sci, ADAPTER_NO,
		      real_id(smdsv.THIS_NODE_ID,IT_READY), 
		      SCI_INFINITE_TIMEOUT,
		      NO_FLAGS,&retval);
  if (retval != SCI_ERR_OK) {
    log_error(" SCIConnectInterrupt IT_READY failed\n");
    log_error("%s\n",Sci_error(retval));
    ret = ERROR;
  }
  log_message("connection succeeded\n");
 
  log_message("connection to seg DATA_READ_SEG_ID %ld on node %d (cb_arg=%p)\n", 
	      real_id(smdsv.THIS_NODE_ID, DATA_READ_SEG_ID), 
	      cl->no_sci,
	      cl);
  SCIConnectSegment(cl->vd_dataread, 
		    &cl->rs_data_read_segment, 
		    cl->no_sci, 
		    real_id(smdsv.THIS_NODE_ID, DATA_READ_SEG_ID),
		    ADAPTER_NO,
		    remote_segment_cb,
		    (void *)cl,
		    SCI_INFINITE_TIMEOUT,
		    SCI_FLAG_USE_CALLBACK,
		    &retval);
  
  if (retval != SCI_ERR_OK) {
    log_error(" SCIConnectSegment DATA_READ_SEG_ID failed\n");
    log_error("%s\n",Sci_error(retval));
    ret = ERROR;
  }

  seg_size = SCIGetRemoteSegmentSize(cl->rs_data_read_segment);

  log_message("connection succeeded seg %ld size = %d  bytes \n",
	      real_id(smdsv.THIS_NODE_ID, DATA_READ_SEG_ID), 
	      seg_size); 

   cl->addr_data_read_segment =
     (volatile int*)SCIMapRemoteSegment(cl->rs_data_read_segment, 
					&cl->map_data_read_segment,
					0 /* offset */,
					seg_size,
					0 /* address hint */,
					NO_FLAGS, 
					&retval);
  if (retval != SCI_ERR_OK) {
    log_error("SCIMapRemoteSegment DATA_READ_SEG_ID failed\n");
    log_error("%s\n",Sci_error(retval));
    ret = ERROR;
  }
  
  /* SCICreateMapSequence(cl->map_data_read_segment,
		       &cl->seq_data_read_segment,
		       NO_FLAGS,
		       &retval);
  
  if (retval != SCI_ERR_OK) {
    log_message("SCICreateMapSequenceDATA_READ_SEG_ID failed\n");
    log_message("%s\n",Sci_error(retval));
    ret = ERROR;
    }*/
 
  return ret;
}





///////////////////////////////////////////////////////////////////////////////////////////
void sci_print_dma_state(dma_state) {
  switch (dma_state) {
  case SCI_DMAQUEUE_IDLE:
    log_message("SCI_DMAQUEUE_IDLE\n");
    break;
  case SCI_DMAQUEUE_GATHER:
    log_message("SCI_DMAQUEUE_GATHER\n");
    break;
  case SCI_DMAQUEUE_POSTED:
    log_message("SCI_DMAQUEUE_POSTED\n");
    break;
  case SCI_DMAQUEUE_DONE:
    log_message("SCI_DMAQUEUE_DONE\n");
    break;
  case SCI_DMAQUEUE_ABORTED:
    log_message("SCI_DMAQUEUE_ABORTED\n");
    break;
  case SCI_DMAQUEUE_ERROR:
    log_message("SCI_DMAQUEUE_ERROR\n");
    break;
  default:
    log_message("UNKNOWN\n");
    break;
  }
}
    
////////////////////////////////////////
sci_error_t sci_segment_create_and_export(sci_desc_t v_device, 
					  sci_local_segment_t *local_segment, 
					  sci_map_t *local_map,
					  volatile int **local_address, 
					  unsigned int mem_id, 
					  unsigned int mem_size,
					  struct client *cl) {
  sci_error_t error;

  // allocation du segment controle et du segment donnees 
  SCICreateSegment(v_device, 
		   local_segment,
		   mem_id, 
		   mem_size,
		   local_segment_cb, 
		   (void*)cl, 
		   SCI_FLAG_USE_CALLBACK, 
		   &error);
  if (error != SCI_ERR_OK) {
    log_message("erreur SCICreateSegment local_ctl_segment\n");
    log_message("%s",Sci_error(error));
    return error;
   }

 
  log_message("creating segment %d size %d\n",
	      mem_id, mem_size);

  /* mapping des segments locaux et reception des adresses */
  *local_address =
    (int*)SCIMapLocalSegment(*local_segment, local_map,
			     0 /* offset */, mem_size,
			     0 /* address hint */, NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    log_message("erreur SCIMapLocalSegment\n");
    log_message("%s",Sci_error(error));
    return error;  
  }
  
  log_message("mapping segment %d size %d l_address=%p\n",
	      mem_id, mem_size,*local_address);
  
  /* export local segment */
  SCIPrepareSegment(*local_segment, ADAPTER_NO, NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    log_message("erreur SCIPrepareSegment\n");
    log_message("%s\n",Sci_error(error));    
    return error;
  }
  
  SCISetSegmentAvailable(*local_segment, ADAPTER_NO, NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    log_message("erreur SCISetSegmentAvailable\n");
    log_message("%s\n",Sci_error(error));    
    return error;
  }

  log_message("exporting segment %d size %d\n",
	      mem_id, mem_size);

  return SCI_ERR_OK;
 
}

//////////////////////////////////////////////////////
// 1) création des segments partagés de controle et de données. 
// Le segment de contrôle contient les paramètres des commandes et 
// le segment de données contient les données du tranfert.
//
// 2) création de l'interruption début d'E/S
//
// renvoie ERROR s'il y a un pb et 0 si tout se passe bien.

// note : comme on est dans l'espace user, on utilise SISCI pour 
// réaliser ces créations de structure

// TODO : faire ça un peu mieux.. notamment desallouer correctement les ressources en cas d'erreur (voir bouquin sur les drivers pour faire ça bien)
int sci_init(void) {
  sci_query_adapter_t query;
  sci_error_t retval;
  
  // initialize the SCI environment 
  SCIInitialize(NO_FLAGS, &retval);
  if (retval != SCI_ERR_OK) {
    log_message("erreur SCIInitialize\n");
    log_message("%s\n",Sci_error(retval));
    return ERROR;
  }

  // détermine le node_id SCI du PC sur lequel s'exécute ce démon
  query.localAdapterNo = 0;
  query.subcommand = SCI_Q_ADAPTER_NODEID;
  query.data = &smdsv.THIS_NODE_ID;
  SCIQuery(SCI_Q_ADAPTER,&query,NO_FLAGS,&retval);
  if (retval != SCI_ERR_OK) {
    log_message("erreur SCIQuery\n");
    log_message("%s\n",Sci_error(retval));
    return ERROR;
  }  

  log_message("n° SCI du noeud local (THIS_NODE) = %d\n", smdsv.THIS_NODE_ID);
  
    
  return SUCCESS;
}

//////////////////////////////////////////////////////
// désallocation des ressources SCI
// TODO : faire ça un peu mieux.. mettre des messages
int sci_cleanup(void) {
  // fermeture de la lib SISCI
  SCITerminate();
  return SUCCESS;

}

//! ouvre le virtual device passé en param. renvoie ERROR s'il n'y arrive pas et SUCCESS sinon
int open_vd(sci_desc_t *vd) { 
  sci_error_t retval;
  SCIOpen(vd, NO_FLAGS, &retval);

  if (retval != SCI_ERR_OK) 
    return ERROR;
  else return SUCCESS;
}

//! ferme le virtual device passé en param. renvoie ERROR s'il n'y arrive pas et SUCCESS sinon
int close_vd(sci_desc_t *vd) { 
  sci_error_t retval;
  
  if (vd != NULL) {
    SCIClose(*vd, NO_FLAGS, &retval);

    if (retval != SCI_ERR_OK) return ERROR;
  }
  
  return SUCCESS;
}

void raz_sci_ptr(client_t *cl) {
  cl->vd_debio = NULL;
  cl->vd_finio = NULL;
  cl->vd_clready = NULL;
  cl->vd_errorio = NULL;
  cl->vd_ready = NULL;
  cl->vd_dataread = NULL;
  cl->vd_datareadtemp = NULL; 
  cl->vd_datawrite = NULL;
  cl->vd_ctl = NULL;

  // REMOTE (sur CLIENT)
  cl->remote_fin_io_interrupt = NULL;
  cl->remote_error_io_interrupt = NULL;
  cl->remote_ready_interrupt = NULL;
  cl->rs_data_read_segment = NULL;
  cl->map_data_read_segment = NULL;
  cl->addr_data_read_segment = NULL;
  cl->seq_data_read_segment = NULL;

  // LOCAL (sur ce SERVEUR)
  cl->local_deb_io_interrupt = NULL;  
  cl->local_clready_interrupt = NULL;  
  cl->local_ctl_segment = NULL;
  cl->local_data_write_segment = NULL;
  cl->local_read_temp_segment = NULL;
  cl->local_ctl_map = NULL;
  cl->local_data_write_map = NULL;
  cl->local_read_temp_map = NULL;
  cl->local_ctl_address = NULL;
  cl->local_data_write_address = NULL;
  cl->local_read_temp_address = NULL;  
}
  
//! libère les structures sci associées au client. Renvoie SUCCESS/ERROR.
int cleanupsci_client(client_t *cl) {
  sci_error_t retval;
  int ret;
  
  ret = SUCCESS;

  log_message("cleaning up SCI resources for client SCI '%d'\n",
	      cl->no_sci);
	      
  // désallocation des segments de mémoire partagée
  if (cl->local_ctl_segment != NULL) {
    SCISetSegmentUnavailable(cl->local_ctl_segment, 
			     ADAPTER_NO, 
			     NO_FLAGS, 
			     &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }
  
  if (cl->local_data_write_segment != NULL) {
    SCISetSegmentUnavailable(cl->local_data_write_segment, 
			     ADAPTER_NO, 
			     NO_FLAGS, 
			     &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }

  if (cl->local_ctl_segment != NULL) {
    SCIRemoveSegment(cl->local_ctl_segment, 
		     NO_FLAGS, 
		     &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }

  if (cl->local_data_write_segment != NULL) {
    SCIRemoveSegment(cl->local_data_write_segment, 
		     NO_FLAGS, 
		     &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }
  
  if (cl->local_read_temp_segment != NULL) {
    SCIRemoveSegment(cl->local_read_temp_segment, 
		     NO_FLAGS, 
		     &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }

  // desallocation des IT
  if (cl->local_deb_io_interrupt != NULL) {
    SCIRemoveInterrupt(cl->local_deb_io_interrupt, 
		       NO_FLAGS, 
		       &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }

  if (cl->local_clready_interrupt != NULL) {
    SCIRemoveInterrupt(cl->local_clready_interrupt, 
		       NO_FLAGS, 
		       &retval);
    if (retval != SCI_ERR_OK) ret = ERROR;
  }

  retval = deconnexion_sci_client(cl);
  if (retval == ERROR) {
    log_error("pb pour se déconnecter des ressources SCI du client "
	      "SCI n°'%d'\n", cl->no_sci);
    ret = ERROR;
  }
	      


  if ((close_vd(&cl->vd_finio) != SUCCESS) ||
      (close_vd(&cl->vd_debio) != SUCCESS) ||
      (close_vd(&cl->vd_clready) != SUCCESS) ||
      (close_vd(&cl->vd_errorio) != SUCCESS) ||
      (close_vd(&cl->vd_ready) != SUCCESS) ||
      (close_vd(&cl->vd_dataread) != SUCCESS) ||
      (close_vd(&cl->vd_datareadtemp) != SUCCESS) ||
      (close_vd(&cl->vd_datawrite) != SUCCESS) ||
      (close_vd(&cl->vd_ctl) != SUCCESS))
    ret = ERROR;

  raz_sci_ptr(cl);
  return ret;
}


//! init les structures SCI associée au client. renvoie SUCCESS/ERROR.
int init_localsci_client(client_t *cl, int no_sci_client) {
  sci_error_t retval;
  
  raz_sci_ptr(cl);
  
  if ((open_vd(&cl->vd_finio) != SUCCESS) ||
      (open_vd(&cl->vd_debio) != SUCCESS) ||
      (open_vd(&cl->vd_clready) != SUCCESS) ||
      (open_vd(&cl->vd_errorio) != SUCCESS) ||
      (open_vd(&cl->vd_ready) != SUCCESS) ||
      (open_vd(&cl->vd_dataread) != SUCCESS) ||
      (open_vd(&cl->vd_datareadtemp) != SUCCESS) ||
      (open_vd(&cl->vd_datawrite) != SUCCESS) ||
      (open_vd(&cl->vd_ctl) != SUCCESS)) { 
    log_error("erreur SCIOpen à l'ouverture d'un des VD - %s\n",
	      Sci_error(retval));
    goto error;
  }

  // création des ressources SCI locales
  // allocation du segment controle et du segment donnees 
  retval=sci_segment_create_and_export(cl->vd_datawrite, 
				       &cl->local_data_write_segment, 
				       &cl->local_data_write_map,
				       &cl->local_data_write_address,  	  
				       real_id(no_sci_client,DATA_WRITE_SEG_ID), 
				       DATA_SEG_SIZE,
				       cl);
  if (retval != SCI_ERR_OK) {
    log_error("can't create & export DATA_WRITE_SEG_ID : ");
    log_error("%s\n",Sci_error(retval));
    goto error;
  }


  retval=sci_segment_create_and_export(cl->vd_ctl, 
				       &cl->local_ctl_segment, 
				       &cl->local_ctl_map, 
				       &cl->local_ctl_address,  	  
				       real_id(no_sci_client,CTL_SEG_ID), 
				       CTL_SEG_SIZE, 
				       cl);
  if (retval != SCI_ERR_OK) {
    log_error("can't create & export CTL_SEG_ID : ");
    log_error("%s\n",Sci_error(retval));
    goto error;
  }

  retval=sci_segment_create_and_export(cl->vd_datareadtemp, 
				       &cl->local_read_temp_segment, 
				       &cl->local_read_temp_map, 
				       &cl->local_read_temp_address,  	  
				       real_id(no_sci_client,READ_TEMPSEG_ID), 
				       DATA_SEG_SIZE,
				       cl);
  if (retval != SCI_ERR_OK) {
    log_error("can't create & export READ_TEMPSEG_ID : ");
    log_error("%s\n",Sci_error(retval));
    goto error;
  }

  // allocation de l'IT debut d'I/O
  cl->interrupt_deb_io_no = real_id(no_sci_client,IT_BEG_IO);
  SCICreateInterrupt(cl->vd_debio, 
		     &cl->local_deb_io_interrupt, 
		     ADAPTER_NO, 
		     &cl->interrupt_deb_io_no, 
		     NO_CALLBACK, NO_ARG, 
		     SCI_FLAG_FIXED_INTNO, 
		     &retval);
  if (retval != SCI_ERR_OK) {
    log_error("erreur  SCICreateInterrupt local_deb_io_interrupt\n");
    log_error("%s\n",Sci_error(retval));
    goto error;  
  }
  log_message("interrupt IT_BEG_IO created\n");

  // allocation de l'IT client READY
  cl->interrupt_clready_no = real_id(no_sci_client,IT_CLREADY);
  SCICreateInterrupt(cl->vd_clready, 
		     &cl->local_clready_interrupt, 
		     ADAPTER_NO, 
		     &cl->interrupt_clready_no, 
		     it_clready_cb, 
		     (void *) cl, 
		     SCI_FLAG_FIXED_INTNO | SCI_FLAG_USE_CALLBACK, 
		     &retval);
  if (retval != SCI_ERR_OK) {
    log_error("erreur  SCICreateInterrupt local_clready interrupt\n");
    log_error("%s\n",Sci_error(retval));
    goto error;  
  }
  log_message("interrupt IT_CLREADY created\n");

  log_message("SCI configuré\n");
  return SUCCESS;

 error:
  cleanupsci_client(cl);
  return ERROR;
}





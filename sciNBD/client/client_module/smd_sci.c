#include <genif.h>
#include <sci_types.h>
#include <linux/byteorder/little_endian.h>
#include <linux/types.h>
#include <linux/wait.h>
#include "scimapdev.h"
#include "constantes.h"
#include <math.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/delay.h>
#include "smd_request.h"

// SCI
static probe_status_t probeStatus;
//static sci_device_info_t  sci_dolphin_device_info;

extern signed32 end_io_handler(unsigned32 IN local_adapter_number,
			       void IN *arg,
			       unsigned32 IN interrupt_number);

extern signed32 error_io_handler(unsigned32 IN local_adapter_number,
				 void IN *arg,
				 unsigned32 IN interrupt_number);

extern signed32 snode_ready_handler(unsigned32 IN local_adapter_number,
				    void IN *arg,
				    unsigned32 IN interrupt_number);

DECLARE_WAIT_QUEUE_HEAD(wq_rseg);
DECLARE_WAIT_QUEUE_HEAD(wq_session);

static int connexion_ok;
static sci_binding_t smd_binding;

int init_sci(void) {
  int retval;
	
  PDEBUG("beginning\n");
  if (sci_get_genif_rev()!=GENIF_REV) {
    PERROR("incompatible IRM driver (current rev = %d, "
	   "requested rev GENIF_REV= %d)\n",
	   sci_get_genif_rev(),
	   GENIF_REV);
    return ERROR;
  }

  PDEBUG("IRM version test : OK\n");
  if (! sci_initialize(SMD_MAJOR_NUMBER)) {
    PERROR("sci_initialize failed\n");
    return ERROR;
  }			
		
  if ((retval=sci_bind(&smd_binding))) {
    PERROR("Error in creating smd_binding. Error=0x%x\n",retval);
    return ERROR;
  }
	 
  if ((retval=sci_query_adapter_number(ADAPTER_NO, 
				       Q_ADAPTER_NODE_ID, 
				       NO_FLAGS,
				       &(smd.no_sci)))) {
    PERROR("Error in retrieving local node sci "
	   "number. Error=0x%x\n",retval);
    return ERROR;
  }
  PDEBUG("sci_query : this node SCI number = %d\n",smd.no_sci);
	  
  IRM19_sci_get_device_info(ADAPTER_NO, NO_FLAGS, &(smd.sdip));
  
  if ((retval=sci_create_adapter_sequence(ADAPTER_NO, 
					  NO_FLAGS, 
					  &(smd.sci_sequence)))) {
    PERROR("Error in creating sequence sci number. Error=0x%x\n",retval);
    return FAILURE;
  }
	  
  PDEBUG("end\n");
  return SUCCESS;
}



int deconnexion_sci_snode(servernode_t *sn, int csession_flag);

int close_local_sci_snode(servernode_t *sn) {
  int ret = SUCCESS;
  int retval;

  if (sn->ls_data_read_handle != NULL) {
    sci_set_local_segment_unavailable(sn->ls_data_read_handle,
				      ADAPTER_NO);
    if ((retval = sci_unexport_segment(sn->ls_data_read_handle, 
				       ADAPTER_NO, 
				       NO_FLAGS))) {
      PERROR("sci_unexport_segment DATA READ failed "
	     "for snode '%s'. Error=0x%x\n",
	     sn->name, 
	     retval);
      ret = ERROR;
    }

    if ((retval = sci_remove_segment(&(sn->ls_data_read_handle),
				     NO_FLAGS))) {
      PERROR("sci_remove_segment DATA READ failed for snode '%s'. "
	     "Error=0x%x\n",
	     sn->name, 
	     retval);
      ret = ERROR;
    }

    PDEBUG("ls data READ segment freed\n");
  }
       		
  // IT ENDIO
  if (sn->ls_shad_endio_handle != NULL) {
    sci_set_local_segment_unavailable(sn->ls_shad_endio_handle,
				      ADAPTER_NO);
    sci_unexport_segment(sn->ls_shad_endio_handle, 
			 ADAPTER_NO, 
			 NO_FLAGS);
			
    // FIX ME ? normalement avant de supprimer 
    // ce segment, il faut tester que plus personne n'est
    // connecté dessus au moyen de sci_currently_connected 
    // (cf core_int.c du drv SISCI)
    sci_remove_segment(&(sn->ls_shad_endio_handle),
		       NO_FLAGS);
    PDEBUG("endio shadow segment freed\n");
  }
		
  if (sn->lit_endio_handle != NULL) {
    sci_remove_interrupt_flag(&(sn->lit_endio_handle), 
			      NO_FLAGS);	
    PDEBUG("disconnected interrupt endio\n");
  }
		
  // IT ERRORIO
  if (sn->ls_shad_errorio_handle != NULL) {
    sci_set_local_segment_unavailable(sn->ls_shad_errorio_handle,
				      ADAPTER_NO);
    sci_unexport_segment(sn->ls_shad_errorio_handle, 
			 ADAPTER_NO, 
			 NO_FLAGS);
    
    // FIX ME ? normalement avant de supprimer 
    // ce segment, il faut tester que plus personne n'est
    // connecté dessus au moyen de sci_currently_connected 
    // (cf core_int.c du drv SISCI)
    sci_remove_segment(&(sn->ls_shad_errorio_handle),
		       NO_FLAGS);
    PDEBUG("errorio shadow segment freed\n");
  }
		
  if (sn->lit_errorio_handle != NULL)  {
    sci_remove_interrupt_flag(&(sn->lit_errorio_handle), 
			      NO_FLAGS);	
    PDEBUG("disconnected interrupt errorio\n");
  }
  
  // IT READY
  if (sn->ls_shad_ready_handle != NULL) {
    sci_set_local_segment_unavailable(sn->ls_shad_ready_handle,
				      ADAPTER_NO);
    sci_unexport_segment(sn->ls_shad_ready_handle, 
			 ADAPTER_NO, 
			 NO_FLAGS);
    
    // FIX ME ? normalement avant de supprimer 
    // ce segment, il faut tester que plus personne n'est
    // connecté dessus au moyen de sci_currently_connected 
    // (cf core_int.c du drv SISCI)
    sci_remove_segment(&(sn->ls_shad_ready_handle),
		       NO_FLAGS);
    PDEBUG("ready shadow segment freed\n");
  }
		
  if (sn->lit_ready_handle != NULL)  {
    sci_remove_interrupt_flag(&(sn->lit_ready_handle), 
			      NO_FLAGS);	
    PDEBUG("disconnected interrupt ready\n");
  }

  return ret;
}

void raz_sci_snode(servernode_t *sn);

int cleanup_sci_sn(servernode_t *sn, int csession_flag) {
  //int retval; 

  
  if (close_local_sci_snode(sn) == ERROR) {
    PERROR("failure in freeing SCI resources of "
	   "snode '%s'\n", sn->name);
  }

  if (deconnexion_sci_snode(sn,  csession_flag) == ERROR) {
    PERROR("failure in deconnexion of SCI resources of "
	   "snode '%s'\n", sn->name);
  }
	       
  // on libère le buffer de requête
  //if (sn->big_buffer) {
  //  vfree(sn->big_buffer);
  //  PDEBUG("freeing big buffer = 0x%p\n", sn->big_buffer);
  //}
		

  raz_sci_snode(sn);
  return SUCCESS;
}

int deconnexion_sci_snode(servernode_t *sn, int csession_flag) {
  int ret = SUCCESS;
  int retval;



  // DATA WRITE SEG
  if (sn->mp_data_write_handle != NULL) {
    sci_unmap_segment(&(sn->mp_data_write_handle), 
		      NO_FLAGS);
    sn->mp_data_write_handle = NULL;
  }

  if (sn->rs_data_write_handle != NULL) {
    if ((retval = sci_disconnect_segment(&(sn->rs_data_write_handle), 
					 NO_FLAGS))) {
      PERROR("sci_disconnect_segment DATA WRITE failed for "
	     "snode '%s'. Error=0x%x\n", 
	     sn->name, 
	     retval);
      ret = ERROR;
    }
    sn->rs_data_write_handle = NULL;
    PDEBUG("rs data WRITE segment disconnected\n");
  }
 
  // CTL SEG
  if (sn->mp_ctl_handle != NULL) {
    sci_unmap_segment(&(sn->mp_ctl_handle), 
		      NO_FLAGS);
    sn->mp_ctl_handle = NULL;
  }

  if (sn->rs_ctl_handle != NULL) {
    sci_disconnect_segment(&(sn->rs_ctl_handle), 
			   NO_FLAGS);
    sn->rs_ctl_handle = NULL;
    PDEBUG("rs ctl segment disconnected\n");
  }

  // IT BEGIO
  if (sn->mp_shad_begio_handle != NULL) {		
    sci_unmap_segment(&(sn->mp_shad_begio_handle), 
		      NO_FLAGS);
    sn->mp_shad_begio_handle = NULL;
  }
   
  
  if (sn->rs_shad_begio_handle != NULL) {
    sci_disconnect_segment(&(sn->rs_shad_begio_handle), 
			   NO_FLAGS);
    sn->rs_shad_begio_handle = NULL;
    PDEBUG("begio shadow segment disconnected\n");
  }
	
  if (sn->rit_begio_handle != NULL) {
    sci_disconnect_interrupt_flag(&(sn->rit_begio_handle), 
				  NO_FLAGS);
    sn->rit_begio_handle = NULL;
    PDEBUG("disconnected interrupt begio\n");
  }

 // IT CLREADY
  if (sn->mp_shad_clready_handle != NULL) {		
    sci_unmap_segment(&(sn->mp_shad_clready_handle), 
		      NO_FLAGS);
    sn->mp_shad_clready_handle = NULL;
  }
   
  
  if (sn->rs_shad_clready_handle != NULL) {
    sci_disconnect_segment(&(sn->rs_shad_clready_handle), 
			   NO_FLAGS);
    sn->rs_shad_clready_handle = NULL;
    PDEBUG("clready shadow segment disconnected\n");
  }
	
  if (sn->rit_clready_handle != NULL) {
    sci_disconnect_interrupt_flag(&(sn->rit_clready_handle), 
				  NO_FLAGS);
    sn->rit_clready_handle = NULL;
    PDEBUG("disconnected interrupt clready\n");
  }

  // fermeture de la session avec le snode
  if (sn->session_sci_opened == TRUE) {
    PDEBUG("session closing\n");
    retval = sci_close_session(SMD_MAJOR_NUMBER, 
			       csession_flag,
			       sn->no_sci, 
			       ADAPTER_NO);
		  
      
    if (retval) PERROR("sci_close_session failed\n");

    if ((csession_flag == NO_FLAGS) &&
	(sn->status == DECONNEXION_CAN_SLEEP)) 
      interruptible_sleep_on(&wq_session);
  
      PDEBUG("sci session for node %ld closed\n",
	   sn->no_sci);
   }

  sn->session_sci_opened = FALSE;

  return ret;
}

int cleanup_sci(void) {	
  PDEBUG("beginning\n");
	
  /*retval = sci_unbind(&smd_binding);
    if (retval != 0) {
    // FIXME : normalement il faut refaire un unbind tant qu'il n'est pas ok.. il faut mettre en place des 
    // timer, pour éviter de pomper tout le CPU
    PERROR("SMD -  cleanup_sci : Error in binding\n");
    }*/

  if (sci_remove_adapter_sequence(&(smd.sci_sequence), 
				  NO_FLAGS)) {
    PERROR("removing sequence failed\n");
    return ERROR;
  }
  PDEBUG("sci sequence removed\n");

  sci_terminate(SMD_MAJOR_NUMBER);
  PDEBUG("SCI IRM closed\n");

  PDEBUG("end\n");
  return 0;
}

//! renvoi le no du segment généré en fonction d'un n° de brique et du type de segment
long real_id(long no_sci, int type_de_segment) {
  return no_sci*10+type_de_segment;
}

extern servernode_t *get_snode_from_nosci(long no_sci);

//! indique que la connexion avec le serveur 'no_sci' est rompue 
void set_snode_suspend(u32 no_sci) {
  servernode_t *sn;

  sn = get_snode_from_nosci(no_sci);
  if (sn == NULL)
    PERROR("getting cb message from an invalid snode\n");
  else {
    if (sn->status == UP) {
        PDEBUG("SCI node %d : UP -> SUSPEND\n", no_sci);
	sn->status = SUSPEND;
	if (sn->busy == WAIT_ENDIO) {
	  // le driver va bloquer sur un wait IT ENDIO
	  // qui ne viendra jamais
	  PDEBUG("node is in WAIT_ENDIO. Aborting current req\n");
	  abort_current_req(sn);
	}
    }
  }
}

//! indique que le serveur 'no_sci' n'existe plus
void set_snode_deconnexion(u32 no_sci) {
  servernode_t *sn;

  sn = get_snode_from_nosci(no_sci);
  if (sn == NULL)
    PERROR("getting cb message from an invalid snode\n");
  else {
    if ((sn->status == UP) || 
	(sn->status == SUSPEND) ||
	(sn->status == INSANE)) {
	sn->status = DECONNEXION;
        PDEBUG("SCI node %d : X -> DECONNEXION\n", no_sci);
	if (sn->busy == WAIT_ENDIO) {
	  // le driver va bloquer sur un wait IT ENDIO
	  // qui ne viendra jamais
	  PDEBUG("node is in WAIT_ENDIO. Aborting current req\n");
	  abort_current_req(sn);
	}
    }
  }
}


void set_snode_up(u32 no_sci) {
  servernode_t *sn;

  PDEBUG("SCI = %d\n", no_sci);
  sn = get_snode_from_nosci(no_sci);
  if (sn == NULL)
    PERROR("getting cb message from an invalid snode\n");
  else {
    //if (sn->status == SUSPEND) {
    //    PDEBUG("SCI node %d : SUSPEND -> UP\n", no_sci);
    //sn->status = UP;
    //}
      
    if (sn->status == CONNEXION) {
      PDEBUG("wake up wq_session\n");
      wake_up_interruptible(&wq_session); // ce n'est pas une reconnexion
    }
  }
}

void set_snode_down(u32 no_sci) {
  servernode_t *sn;

  sn = get_snode_from_nosci(no_sci);
  if (sn == NULL)
    PERROR("getting cb message from an invalid snode\n");
  else {
    if (sn->status == DECONNEXION) {
      PDEBUG("SCI node %d : DECONNEXION -> DOWN\n", no_sci);
      sn->status = DOWN;
    }

    if (sn->status == DECONNEXION_CAN_SLEEP) {
      wake_up_interruptible(&wq_session); 
    }     
  }
}

//! callback function when a remote note connects via the local adapter node to a local segment
signed32 ls_connection_cb(void IN *arg, 
			  sci_l_segment_handle_t IN local_segment_handle,
			  unsigned32 IN reason,
			  unsigned32 IN source_node,
			  unsigned32 IN local_adapter_node) {
  switch(reason) {
  case CB_CONNECT :
    PDEBUG("Receiving connection of snode %d on local segment %p\n",
	   source_node,
	   local_segment_handle);
    break;
  case CB_RELEASE:
    PDEBUG("Remote snode %d has released his connection to local "
	   "segment %p\n",
	   source_node,
	   local_segment_handle);
    set_snode_deconnexion(source_node);
    break;
  case CB_DISABLED:
    PDEBUG("LOCAL CB_DISABLED for snode %d\n", source_node);
    break;
  case CB_ENABLED:
    PDEBUG("LOCAL CB_ENABLED for snode %d\n", source_node); 
    break;
  case CB_LOST:
    PDEBUG("LOCAL CB_LOST for snode %d\n", source_node); 
    break; 
  default:
    PDEBUG("callback called. Reason = 0x%x\n",reason); 
  }
  return 0;
}

//! callback function for connecting to a remote segment
signed32 connection_cb(IN void *arg,
		       sci_r_segment_handle_t IN remote_segment_handle,
		       unsigned32 IN reason,
		       unsigned32 IN status) {
  servernode_t *sn;

  sn = (servernode_t *) arg;
  connexion_ok=-1;

  switch(reason) {
  case CB_CONNECT:
    	
    if (status==0) {
      PDEBUG("remote segment reachable\n");
      connexion_ok=1;
    } else {
      PERROR("remote segment unreachable "
	     "(CB_CONNECT but bad status = 0x%x)\n",
	     status);
      connexion_ok=-1;
    }
    break;
  case CB_RELEASE:
    PDEBUG("REMOTE CB_RELEASE\n");
    set_snode_deconnexion(sn->no_sci);
    break;
  case CB_DISABLED:
    PDEBUG("REMOTE CB_DISABLED\n");
    break;
  case CB_ENABLED:
    PDEBUG("REMOTE CB_ENABLED\n");
    break;
  case CB_LOST:
    PDEBUG("REMOTE CB_LOST\n");
    break;
  default:
    PERROR("REMOTE unknown callback reason !\n");
  }

  wake_up_interruptible(&wq_rseg);
  return 0;
}


//! se connecte à un segment distant, en mode PIO, et renvoie les handle et l'adresse du mapping
int smd_rsegment_pio_connection(unsigned32 no_sci, 
				unsigned32 module_id, 
				unsigned32 segment_id,
				volatile vkaddr_t *v_addr, 
				sci_r_segment_handle_t *rs_handle,
				sci_map_handle_t *map_handle, 
				unsigned32 *rseg_size,
				unsigned32 map_flags,
				servernode_t *sn) {
				
  int retval;
  
  PDEBUG("module_id = %d, no_sci=%d, segment_id=%d\n",
	 module_id, no_sci, segment_id);

  retval = sci_connect_segment(smd_binding,
			       no_sci, 
			       ADAPTER_NO,
			       module_id, 
			       segment_id, 
			       NO_FLAGS,
			       connection_cb,
			       (void *)sn,
			       rs_handle);

  if (retval!=0) {
    PERROR("Error in connecting remote segment %d, "
	   "node %d (sci error = 0x%x)\n", 
	   segment_id,
	   no_sci,
	   retval);
    return ERROR;
  }
 	
	
  // waiting connection
  interruptible_sleep_on(&wq_rseg);
 
  // FIXME : sans doute un pb de race condition sur 
  // connexion_ok (au cas où on enregistre 2 ndevs quasi simultanément)
  if (connexion_ok == 1) {     		
    PDEBUG("Connection to remote segment %d, node %d "
	   ": completed\n", 
	   segment_id,
	   no_sci);

    *rseg_size=sci_remote_segment_size(*rs_handle);

    PDEBUG("Remote segment size : %lu \n",
	   (unsigned long) *rseg_size);

    retval=sci_map_segment(*rs_handle,
			   map_flags,
			   0,
			   *rseg_size,map_handle);
	  	
    if (retval !=0) {
      PERROR("Error in mapping remote segment\n");
      return ERROR;
    }	
	
    PDEBUG("Mapping of remote segment : complete\n");
	
    *v_addr = sci_kernel_virtual_address_of_mapping(*map_handle);
    PDEBUG("Virtual addr of mapping  = %p\n",*v_addr);
  }
  else {
    PERROR("Remote connection callback failed for remote "
	   "segment %d, node %d\n",
	   segment_id,
	   no_sci);
    return ERROR;
  }
	
  return SUCCESS;
}

//! crée un segment local (qu'on exporte + on récupère l'adresse)
int smd_lsegment_create(unsigned32 module_id_local, 
			unsigned32 segment_id_local, 
			unsigned32 seg_size, 
			sci_l_segment_handle_t *ls_handle,
			volatile vkaddr_t *v_addr) {
				
  int retval;
 	  
  retval = sci_create_segment(smd_binding, 
			      module_id_local, 
			      segment_id_local,
			      NO_FLAGS, 
			      seg_size, 
			      ls_connection_cb, 
			      NO_ARG,
			      ls_handle);
  if (retval != 0) {
    PERROR("sci_create_segment  failed\n");
    return ERROR;
  }
  PDEBUG("creating local segment %d, phandle=%p\n",
	 segment_id_local,
	 ls_handle);
  
  retval = sci_export_segment(*ls_handle, 
			      ADAPTER_NO, 
			      NO_FLAGS);	
  if (retval !=0 ) {
    PERROR("SMD - smd_lsegment_create: sci_export_segment failed\n");
    return ERROR;
  }
  PDEBUG("exporting local segment %d\n",segment_id_local);
  
  *v_addr = sci_local_kernel_virtual_address(*ls_handle);
  PDEBUG("getting vaddr of local segment = 0x%p\n",*v_addr);
  PDEBUG("local segment size = %u\n",
	 sci_size_of_local_segment(*ls_handle));
  
  return SUCCESS;
}

extern int reconnexion_heartbeat(int no_sci);
extern void try_reconnexion(int no_sci);
//int open_session(servernode_t *sn, int no_sci);
int connexion_sci_segit_snode(servernode_t *sn,  int no_sci);



//! callback pour tous ce qui est changement dans la session
signed32 session_io_cb(session_cb_arg_t IN arg,
		       session_cb_reason_t IN reason,
		       session_cb_status_t IN status,
		       unsigned32 IN target_node,
		       unsigned32 IN local_adapter_number) {
		
  PDEBUG("changement état session reason = 0x%x status = 0x%x\n", 
	 reason, 
	 status);
  	
  switch (reason) {
  case SR_OK :
    PDEBUG("session fonctionne\n");
    break;
  case SR_WAITING:
    PDEBUG("SR_WAITING\n");
    break;
  case SR_CHECKING:
    PDEBUG("SR_CHECKING\n");
    break;
  case SR_CHECK_TIMEOUT:
    PDEBUG("SR_CHECK_TIMEOUT\n");
    break;
  case SR_OPEN_TIMEOUT:
    PDEBUG("SR_OPEN_TIMEOUT\n");
    break;  		
  case SR_HEARTBEAT_RECEIVED:
    PDEBUG("heartbeat recu\n");
    set_snode_up(target_node);   
    break;
  case SR_DISABLED:
    PDEBUG("session disabled\n");
    set_snode_suspend(target_node);
    //try_reconnexion(target_node);
    break;
  case SR_LOST:
    PDEBUG("session perdue\n");
    set_snode_down(target_node); 
    break;
  default:
  }

  return 0;
}

/* int connexion_sci_snode(servernode_t *sn, */
/* 			 int no_sci) { */
/*   PDEBUG("sn = %s no_sci = %d\n", sn->name, no_sci); */
/*   PDEBUG("opening session\n"); */
/*   if (open_session(sn, no_sci) == ERROR)   */
/*     return ERROR; */

/*   PDEBUG("waiting session opening\n"); */
/*   interruptible_sleep_on(&wq_session); */


/*   PDEBUG("connecting to ressources\n"); */
/*    if (connexion_sci_segit_snode(sn, no_sci) == ERROR)  */
/*     return ERROR; */

/*   return TRUE; */
/* } */

/* int open_session(servernode_t *sn, */
/* 		 int no_sci) { */
/*   int retval; */

/*   if (sn->session_sci_opened == FALSE) { */
/*     retval = sci_open_session(SMD_MAJOR_NUMBER,  */
/* 			      NO_FLAGS,  */
/* 			      no_sci,  */
/* 			      ADAPTER_NO, */
/* 			      session_io_cb,  */
/* 			      NO_ARG); */
/*     if (retval != 0) { */
/*       PERROR("sci_open_session to node %d failed. Error Ox%x \n",  */
/* 	     no_sci, retval); */
/*       return ERROR; */
/*     } */
 
/*     sn->session_sci_opened = TRUE; */
/*   } */
/*   return SUCCESS; */
/* } */
  
//! connexion aux ressources du serveur node de no 'no_sci'
int connexion_sci_segit_snode(servernode_t *sn,
			      int no_sci) {
  int retval;
  u32 dummint;
  volatile int* r_vaddr;
  int it_no;



  retval = smd_rsegment_pio_connection(no_sci,
				       SYSCALL_MODULE_ID,
				       real_id(smd.no_sci,
					       DATA_WRITE_SEG_ID),
				       &(sn->rs_data_write_addr),
				       &(sn->rs_data_write_handle),
				       &(sn->mp_data_write_handle),
				       &(smd.dataseg_size),
				       NO_FLAGS,
				       sn);
  if (retval != 0) {
    PERROR("smd_rsegment_pio_connection DATA_WRITE_SEG_ID\n");
    return ERROR;
  }
  
  PDEBUG("smd_rsegment_pio_connection to DATA_WRITE_SEG_ID done\n");

  retval = smd_rsegment_pio_connection(no_sci,
				       SYSCALL_MODULE_ID,
				       real_id(smd.no_sci,CTL_SEG_ID),
				       &(sn->rs_ctl_addr),
				       &(sn->rs_ctl_handle),
				       &(sn->mp_ctl_handle),
				       &(smd.ctlseg_size),
				       NO_FLAGS,
				       sn);
  if (retval != 0) {
    PERROR("smd_rsegment_pio_connection CTL_SEG_ID\n");
    return ERROR;
  }
  PDEBUG("smd_rsegment_pio_connection to CTL_SEG_ID done\n");

			       
  // CONNEXION A L'IT BEGIO DU SERVEUR
  // connexion au shadow seg
  retval = smd_rsegment_pio_connection(no_sci, 
				       SYSCALL_INTFLAG_MODULE_ID, 
				       real_id(smd.no_sci,IT_BEG_IO),
				       &(sn->rs_shad_begio_addr),
				       &(sn->rs_shad_begio_handle),
				       &(sn->mp_shad_begio_handle),
				       &dummint,
				       PHYS_MAP_ATTR_STRICT_IO,
				       sn);	
  if (retval != 0) {
    PERROR("smd_rsegment_pio_connection IT_BEG_IO\n");
    return ERROR;
  }


  PDEBUG("smd_rsegment_pio_connection to IT_BEGIO done\n");

  r_vaddr = (int *)sn->rs_shad_begio_addr;
	
  // récupération du no de l'IT SISCI à partir 
  // du segment shad + mise en little endian
  it_no = __swab32(*(r_vaddr)); 

  PDEBUG("getting IT BEGIO nb = %d from shadow segment\n",it_no);
 	
  retval = sci_connect_interrupt_flag(smd_binding, 
				      no_sci,
				      ADAPTER_NO,
				      it_no,
				      NO_FLAGS, 
				      &(sn->rit_begio_handle));
  if (retval == 0)
    PDEBUG("sci_connect_interrupt_flag BEGIO completed\n");
  else {
    PERROR("sci_connect_interrupt_flag BEGIO failed to node %d. "
	   "Error Ox%x \n",
	   no_sci,retval);
    return ERROR;
  }

  // CONNEXION A L'IT CLREADY DU SERVEUR
  // connexion au shadow seg
  retval = smd_rsegment_pio_connection(no_sci, 
				       SYSCALL_INTFLAG_MODULE_ID, 
				       real_id(smd.no_sci,IT_CLREADY),
				       &(sn->rs_shad_clready_addr),
				       &(sn->rs_shad_clready_handle),
				       &(sn->mp_shad_clready_handle),
				       &dummint,
				       PHYS_MAP_ATTR_STRICT_IO,
				       sn);	
  if (retval != 0) {
    PERROR("smd_rsegment_pio_connection IT_CLREADY\n");
    return ERROR;
  }
  PDEBUG("smd_rsegment_pio_connection to IT_CLREADY done\n");

  r_vaddr = (int *)sn->rs_shad_clready_addr;
	
  // récupération du no de l'IT SISCI à partir 
  // du segment shad + mise en little endian
  it_no = __swab32(*(r_vaddr)); 

  PDEBUG("getting IT CLREADY nb = %d from shadow segment\n",it_no);
 	
  retval = sci_connect_interrupt_flag(smd_binding, 
				      no_sci,
				      ADAPTER_NO,
				      it_no,
				      NO_FLAGS, 
				      &(sn->rit_clready_handle));
  if (retval == 0)
    PDEBUG("sci_connect_interrupt_flag CLREADY completed\n");
  else {
    PERROR("sci_connect_interrupt_flag CLREADY failed to node %d."
	   " Error Ox%x \n",
	   no_sci,retval);
    return ERROR;
  }
  
  return SUCCESS;
}

void raz_sci_snode(servernode_t *sn) {
  sn->session_sci_opened=FALSE;
  sn->ls_data_read_handle=NULL;
  sn->ls_data_read_addr=NULL;
  sn->rs_data_write_handle=NULL;
  sn->mp_data_write_handle=NULL;
  sn->rs_data_write_addr=NULL;
  sn->rs_ctl_handle=NULL;
  sn->mp_ctl_handle=NULL;
  sn->rs_ctl_addr=NULL;
  sn->rs_shad_begio_handle=NULL;
  sn->rs_shad_begio_addr=NULL;
  sn->mp_shad_begio_handle=NULL;
  sn->rit_begio_handle=NULL;
  sn->rs_shad_clready_handle=NULL;
  sn->rs_shad_clready_addr=NULL;
  sn->mp_shad_clready_handle=NULL;
  sn->rit_clready_handle=NULL;
  sn->ls_shad_endio_handle=NULL;
  sn->lit_endio_handle=NULL;
  sn->ls_shad_errorio_handle=NULL;
  sn->lit_errorio_handle=NULL;
  sn->ls_shad_ready_handle=NULL;
  sn->lit_ready_handle=NULL;
}

//! crée les ressources local pour que le serveur puisse s'y connecter
int sci_create_local_segit(servernode_t *sn, int no_sci) {
  int retval;
  vkaddr_t temp;
  int it_no;

  PDEBUG("creating local seg DATA_READ_SEG_ID\n");
  retval = smd_lsegment_create(SYSCALL_MODULE_ID, 
			       real_id(no_sci,DATA_READ_SEG_ID),
			       smd.dataseg_size, 
			       &(sn->ls_data_read_handle),
			       &(sn->ls_data_read_addr));					
	 
  if (retval != SUCCESS) {
    PERROR("failure in connexion & exportation of SCI segments\n");
    return ERROR;
  }



  // CREATION SEGMENT SHADOW + IT : ENDIO
  PDEBUG("init IT end I/O\n");

  retval = sci_allocate_interrupt_flag(smd_binding, ADAPTER_NO, 0,
				       NO_FLAGS, end_io_handler, NULL,
				       &(sn->lit_endio_handle));
  if (retval != 0) {
    PERROR("sci_allocate_interrupt_flag IT_END_IO failed\n");
    return ERROR;
  }
  	
  retval = sci_create_segment(smd_binding, 
			      SYSCALL_INTFLAG_MODULE_ID, 
			      real_id(no_sci,IT_END_IO),
			      NO_FLAGS, 
			      4, 
			      ls_connection_cb,
			      NO_ARG,
			      &(sn->ls_shad_endio_handle));
  if (retval != 0) {
    PERROR("sci_create_segment  END_IO_IT_SEGMENT failed\n");
    return ERROR;
  }
  PDEBUG("IT_END_IO shadow seg %ld (phandle=%p) created\n",
	 real_id(no_sci,IT_END_IO),
	 sn->ls_shad_endio_handle);
	
  // on met le no qui nous a été attribué 
  // par genif dans le segment  END_IO_IT_SEGMENT	
  temp = sci_local_kernel_virtual_address(sn->ls_shad_endio_handle);
  it_no = sci_interrupt_number(sn->lit_endio_handle);
  if (it_no >= NBMAX_IT) {
    PERROR("it number is too large ! %d >= %d(=NBMAX_IT)\n",
	   it_no,
	   NBMAX_IT);
    return ERROR;
  }
	
  PDEBUG("no IT END_IO_IT =%ld (internal = %d)\n",
	 real_id(no_sci,IT_END_IO),
	 it_no);

  // on passe le n° en LITTLE ENDIAN
  *(u32 *)temp =  ___swab32(it_no);   
	
  // on exporte le segment shadow pour l'IT END I/O
  retval = sci_export_segment(sn->ls_shad_endio_handle, 
			      ADAPTER_NO, 
			      NO_FLAGS);	
  if (retval !=0 ) {
    PERROR("sci_export_segment  END_IO_IT_SEGMENT failed\n");
    return ERROR;
  }
	
  // on mémorise l'it end io associée à ce server node
  sn->no_it_endio = it_no; 
  itendio2snode[it_no] = sn;
	
  // CREATION SEGMENT SHADOW + IT : ERRORIO
  PDEBUG("init IT ERROR I/O\n");

  retval = sci_allocate_interrupt_flag(smd_binding, ADAPTER_NO, 0,
				       NO_FLAGS, error_io_handler, NULL,
				       &(sn->lit_errorio_handle));
  if (retval != 0) {
    PERROR("sci_allocate_interrupt_flag IT_ERROR_IO failed\n");
    return ERROR;
  }
  	
  retval = sci_create_segment(smd_binding, 
			      SYSCALL_INTFLAG_MODULE_ID, 
			      real_id(no_sci,IT_ERROR_IO),
			      NO_FLAGS, 
			      4, 
			      ls_connection_cb,
			      NO_ARG,
			      &(sn->ls_shad_errorio_handle));
  if (retval != 0) {
    PERROR("sci_create_segment  ERROR_IO_IT_SEGMENT failed\n");
    return ERROR;
  }

  PDEBUG("IT_ERROR_IO shadow seg %ld (phandle=%p) created\n",
	 real_id(no_sci,IT_ERROR_IO),
	 sn->ls_shad_errorio_handle);
	
  // on met le no qui nous a été attribué 
  // par genif dans le segment  ERROR_IO_IT_SEGMENT	
  temp = sci_local_kernel_virtual_address(sn->ls_shad_errorio_handle);
  it_no = sci_interrupt_number(sn->lit_errorio_handle);
  if (it_no >= NBMAX_IT) {
    PERROR("it number is too large ! %d >= %d(=NBMAX_IT)\n",
	   it_no,
	   NBMAX_IT);
    return ERROR;
  }
	
  PDEBUG("no IT ERROR_IO_IT =%ld (internal = %d)\n",
	 real_id(no_sci,IT_ERROR_IO),
	 it_no);

  // on passe le n° en LITTLE ENDIAN
  *(u32 *)temp =  ___swab32(it_no);   
	
  // on exporte le segment shadow pour l'IT ERROR I/O
  retval = sci_export_segment(sn->ls_shad_errorio_handle, 
			      ADAPTER_NO, 
			      NO_FLAGS);	
  if (retval !=0 ) {
    PERROR("sci_export_segment ERROR_IO_IT_SEGMENT failed\n");
    return ERROR;
  }
	
  // on mémorise l'it end io associée à ce server node
  sn->no_it_errorio = it_no; 
  iterrorio2snode[it_no] = sn;

  // CREATION SEGMENT SHADOW + IT : READY
  PDEBUG("init IT READY I/O\n");

  retval = sci_allocate_interrupt_flag(smd_binding, ADAPTER_NO, 0,
				       NO_FLAGS, snode_ready_handler, NULL,
				       &(sn->lit_ready_handle));
  if (retval != 0) {
    PERROR("sci_allocate_interrupt_flag IT_READY failed\n");
    return ERROR;
  }
  	
  retval = sci_create_segment(smd_binding, 
			      SYSCALL_INTFLAG_MODULE_ID, 
			      real_id(no_sci,IT_READY),
			      NO_FLAGS, 
			      4, 
			      ls_connection_cb,
			      NO_ARG,
			      &(sn->ls_shad_ready_handle));
  if (retval != 0) {
    PERROR("sci_create_segment  READY_IT_SEGMENT failed\n");
    return ERROR;
  }

  PDEBUG("IT_READY shadow seg %ld (phandle=%p) created\n",
	 real_id(no_sci,IT_READY),
	 sn->ls_shad_ready_handle);
	
  // on met le no qui nous a été attribué 
  // par genif dans le segment  READY_IT_SEGMENT	
  temp = sci_local_kernel_virtual_address(sn->ls_shad_ready_handle);
  it_no = sci_interrupt_number(sn->lit_ready_handle);
  if (it_no >= NBMAX_IT) {
    PERROR("it number is too large ! %d >= %d(=NBMAX_IT)\n",
	   it_no,
	   NBMAX_IT);
    return ERROR;
  }
	
  PDEBUG("no IT READY_IT =%ld (internal = %d)\n",
	 real_id(no_sci,IT_READY),
	 it_no);

  // on passe le n° en LITTLE ENDIAN
  *(u32 *)temp =  ___swab32(it_no);   
	
  // on exporte le segment shadow pour l'IT READY
  retval = sci_export_segment(sn->ls_shad_ready_handle, 
			      ADAPTER_NO, 
			      NO_FLAGS);	
  if (retval !=0 ) {
    PERROR("sci_export_segment READY_IT_SEGMENT failed\n");
    return ERROR;
  }
	
  // on mémorise l'it ready associée à ce server node
  sn->no_it_ready = it_no; 
  itready2snode[it_no] = sn;

  PDEBUG("all done\n");
  
  

  return SUCCESS;  
}


//! fonction qui se connecte aux 2 segments data et ctl de la brique, au segment shadow begin_io, et qui crée le segment shadow + IT end_io ed idem pour IT error_io. Renvoie ERROR/SUCCESS
int smd_add_snode_sci(servernode_t *sn, long no_sci) {
  int retval;
  raz_sci_snode(sn);

	
  // PROBING DISTANT NODE
  retval = sci_probe_node(SMD_MAJOR_NUMBER, NO_FLAGS, no_sci,
			  ADAPTER_NO,
			  &probeStatus);

  if (retval == 0 && probeStatus == PS_OK ) 
    PDEBUG("probeStatus distant node = PS_OK\n");
  else {
    PERROR("sci_probe_node failed \n");
    return ERROR;
  }
 	
	
  sn->status = CONNEXION;
  // OUVERTURE D'UNE SESSION AVEC LE SERVEUR
  retval = sci_open_session(SMD_MAJOR_NUMBER, 
			    NO_FLAGS, 
			    no_sci, 
			    ADAPTER_NO,
			    session_io_cb, 
			    NO_ARG);
  if (retval != 0) {
    if (retval == ESCI_EXIST) {
      PDEBUG("session with node %ld already exists\n", no_sci);
    } 
    else {
      PERROR("sci_open_session with node %ld failed. Error Ox%x \n", 
	     no_sci,retval);
      return ERROR;
    }
  }

  if (retval == 0) {
    interruptible_sleep_on(&wq_session);
    sn->session_sci_opened=TRUE;
    PDEBUG("sci_open_session with node %ld succeeded\n",no_sci);
  }
 

 // CONNEXION AUX RESSOURCES SCI DU SERVER NODE 
  retval = connexion_sci_segit_snode(sn,no_sci);
  if (retval == ERROR) {
    PERROR("can't connect to remote SCI resources (node=%ld)\n",
	   no_sci);
    goto error;
  }

  
  // CREATION DES RESOURCES LOCALES POUR COMMUNIQUER
  retval = sci_create_local_segit(sn,no_sci);
  if (retval == ERROR) {
    PERROR("can't create local SCI resources for snode '%ld'\n",
	   no_sci);
    goto error;
  }

  // LE CLIENT EST PRET : 
  // il l'indique au serveur, qui se connectera alors
  // aux ressources du client
  retval =  sci_trigger_interrupt(sn->rit_clready_handle);
  if (retval != 0) {
    PERROR("sci_trigger_interrupt CLREADY failed\n");
    goto error;
  }
  PDEBUG("sci_trigger_interrupt CLREADY done\n");
  
  return SUCCESS;

 error:
  cleanup_sci_sn(sn, CANCEL_CB);
  return ERROR;
}



int smd_start_sequence(void) {
  sequence_error_status_t errStatus;
  scierror_t retval;
  
  errStatus =  SE_PENDING_RETRYABLE_ERROR;
  
  retval = sci_open_adapter_sequence(smd.sci_sequence, 
				     0/* flags*/, 
				     &errStatus);
  if ((retval != ESCI_OK) || (errStatus != SE_OK)) {
    PERROR("sci_open_adapter_sequence failed (0x%x)", retval);
    return ERROR;
  }
  
  return SUCCESS;
}

int smd_check_sequence(void) {
  sequence_error_status_t errStatus;
  sequence_barrier_status_t barStatus;
  scierror_t retval;
  
  retval = sci_close_adapter_sequence(smd.sci_sequence, 
				      0, 
				      &errStatus, 
				      &barStatus);
  if (retval != ESCI_OK) {
   PERROR("sci_close_adapter_sequence failed (0x%x)", retval);
    return TRUE;
  }

  if ((errStatus != SE_OK) || barStatus != SB_IDLE) {
    PERROR("sequence failed err=0x%x bar=0x%x\n",
	   errStatus,
	   barStatus);
    return ERROR;
  }
  return SUCCESS;
}

#include "smd_server.h"
#include "smdsv_sci.h"
#include <time.h>

#define TIMEOUT 2 // secondes d'attente avant de sortir du wait cond

void gestion_connexion(void *p)  {
  client_t *cl;
  struct timespec tbreak;
  time_t tnow;
  int ret;

  cl = (client_t *) p;
  log_message("init thread gestion_connexion for cl %d\n", cl->no_sci);

  while(1) {
    pthread_mutex_lock(cl->mutex_cxstate);
    
    // pour que l'annulation du thread se fasse correctement
    // cf p305 du BB
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, 
			 cl->mutex_cxstate);


    tnow = time(NULL);
    tbreak.tv_sec = tnow + TIMEOUT; // en sec écoulées depuis 01/01/1970
    log_message("tnow = %ld tbreak.tv_sec = %ld\n", tnow, tbreak.tv_sec);
    tbreak.tv_nsec = 0;
    pthread_cond_timedwait(&cl->cond_cxstate, 
			    cl->mutex_cxstate,
			    &tbreak);
  
    pthread_cleanup_pop(1); // réalise le unlock

    pthread_testcancel();
    log_message("cl->status = %d cl->cxstate_changed=%d\n",
		cl->status,
		cl->cxstate_changed);
 
    // LES ACTIONS POUVANT ETRE REPETEES A CHAQUE TIMEOUT

    // si le client a eu pb de segment SCI => on deconnecte
    // toutes ses ressources pour reconnecter plus tard
    if ((cl->status == DO_DECONNEXION) || 
	(cl->status == INSANE)) {
      log_message("deconnexion_sci_client for client %d\n", cl->no_sci);
      if (deconnexion_sci_client(cl) == SUCCESS)
	cl->status = DOWN;
      else {
	log_error("can't deconnect ressources of client SCI %d: "
		  "switching connection to INSANE state\n",
		  cl->no_sci);
	cl->status = INSANE;
      }
    }		   

    // LES ACTIONS A FAIRE SEULEMENT QUAND
    // L'ETAT DE LA CONNEXION A CHANGE
    if (cl->cxstate_changed == TRUE) {
      cl->cxstate_changed = FALSE;

      // on se connecte aux ressources SCI du client
      if (cl->status == DO_CONNEXION) {
	log_message("connecting to sci resources of client SCI %d \n",
		    cl->no_sci);
	ret = connexion_sci_client(cl); 
	if (ret != SUCCESS) {
	  log_error("can't connect to sci resources of client %d: "
		    "switching connection to INSANE state\n",
		    cl->no_sci);
	  cl->status = INSANE;
	} else cl->status = DO_SENDREADY;
	  
	    
      }

      // on se signale au cl qu'on est prêt à recevoir
      // requêtes
      if (cl->status == DO_SENDREADY) {
	log_message("sending READY to client %d\n", cl->no_sci);
	if (send_ready_sci(cl) == SUCCESS) {
	  cl->status = UP;
	} else {
	  log_error("sending READY failed for cl %d: "
		    "switching connection to INSANE state\n",
		    cl->no_sci);
	  cl->status = INSANE;
	}
      }
    }
  }
}
    

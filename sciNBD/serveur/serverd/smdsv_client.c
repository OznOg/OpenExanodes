#include "smd_server.h"
#include "smdsv_sci.h"
#include "smdsv_divers.h"
#include <stdlib.h>
#include <malloc.h>
#include <asm/errno.h>


//! libère la mémoire des structures de données pour les clients
void cleanup_clients(void) {
  int i,retval;
  client_t *cl;

  for (i=0; i<NBMAX_CLIENTS; i++) 
    if (smdsv.clients[i] != NULL) {
      cl = smdsv.clients[i];
      log_message("freeing resources of client SCI %d\n",cl->no_sci);
      pthread_cond_destroy(&cl->cond_cxstate);
      pthread_mutex_destroy(cl->mutex_cxstate);
      free(cl->mutex_cxstate);
      retval = pthread_cancel(cl->thread_io);
      if (retval != 0) {
	log_error("can't cancel thread_io\n");
      }
      retval = pthread_cancel(cl->thread_connex);
      if (retval != 0) {
	log_error("can't cancel thread_connex\n");
      }
      cleanupsci_client(cl);
      free(cl);
    }
}

//////////////////////////////////////////////////////
// renvoie TRUE si le noeud sci client est déja
// en train d'importer ce device
// et FALSE sinon
int deja_client(int no_sci_client) {
  int i, resultat = FALSE;
  log_message("début. no_sci_client = %d nb_clients = %d\n",
	      no_sci_client, smdsv.nb_clients);

  for (i=0; i<smdsv.nb_clients; i++) {
    if (smdsv.clients[i]->no_sci ==  no_sci_client) {
      log_message("clients[i].no_sci(%d) ==  no_sci_client(%d)\n",
		  smdsv.clients[i]->no_sci,
		  no_sci_client);
      resultat = TRUE;
      break;
    }
  }
  log_message("fin. RESULTAT = %d (1=TRUE, 0=FALSE)\n", resultat);
  return resultat;
}

extern void *traitement_d_une_io(void *p);
extern void *gestion_connexion(void *p);

//! init des structure de données du client. renvoie SUCCESS/ERROR
int init_client(int no_sci_client) { 
  client_t *cl;
  int no, retval, oldtype;
  
  log_message("debut (node %d)\n",no_sci_client);

  cl = malloc(sizeof (struct client));
  if (cl == NULL) {
    log_error("échec malloc de la structure 'client'\n");
    return ERROR;
  }
  
  
  if (init_localsci_client(cl, no_sci_client) == ERROR) {
    log_error("ne peut pas initialiser SCI pour le client SCI '%d'\n",
	      no_sci_client);
    goto error_sci;
  }

  cl->no_sci = no_sci_client;
  cl->nb_io = 0;
  cl->status = DOWN;
  cl->cxstate_changed = FALSE;

  smdsv.nb_clients++;


  // create thread_io
  retval = pthread_create(&cl->thread_io,
			  NULL,
			  traitement_d_une_io,
			  cl);
  if (retval != 0) {
    log_error("ne peut créer le thread_io pour servir le client "
	      "n° SCI %d\n",
	      cl->no_sci);
    goto error_thio;
  }

  
  retval = pthread_cond_init(&cl->cond_cxstate, NULL);
  if (retval != 0) {
    log_error("initialisation de la condition cx_state pour client "
	      "n° SCI %d\n", cl->no_sci); 
    goto error_thcond;
  }

  cl->mutex_cxstate = malloc(sizeof(pthread_mutex_t));
  if (cl->mutex_cxstate == NULL) {
    log_error("malloc cl->mutex_cxstate pour client n° SCI %d\n",
	      cl->no_sci);
    goto error_mutex;
  }
  pthread_mutex_init(cl->mutex_cxstate, NULL);
  // pthread_mutex_unlock(cl->mutex_cxstate);

  // create thread_connex
  retval = pthread_create(&cl->thread_connex,
			  NULL,
			  gestion_connexion,
			  cl);
  if (retval != 0) {
    log_error("ne peut créer le thread_connex pour le client "
	      "n° SCI %d\n",
	      cl->no_sci);
    goto error_thcx;
  }

  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);


  no = unused_elt_indice((void *)smdsv.clients, NBMAX_CLIENTS);
  smdsv.clients[no] = cl;

  log_message("fin (node %d)\n", cl->no_sci);
  return SUCCESS;

 error_mutex:
  pthread_cond_destroy(&cl->cond_cxstate);
 error_thcond:
  pthread_cancel(cl->thread_connex);
 error_thcx:
  pthread_cancel(cl->thread_io);
 error_thio:
  cleanupsci_client(cl);  
 error_sci:
  free(cl);
  return ERROR;
}

//////////////////////////////////////////////////////
// smd_exportd signale qu'un noeud demande l'importation
// du device local. renvoie EXIT_FAILURE/SUCCESS
int new_client(int no_sci_client) {

  if (!(deja_client(no_sci_client))) {
    if (init_client(no_sci_client) != SUCCESS) {
      log_message("échec importation %d du noeud %d\n",
		  smdsv.nb_clients, no_sci_client);
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }
  else return EXIT_FAILURE;
     
}

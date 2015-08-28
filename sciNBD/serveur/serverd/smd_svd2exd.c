#include <pthread.h>
#include <strings.h>
#include <asm/types.h>
#include "constantes.h"
#include "smdsv_divers.h"
#include "smd_server.h"
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <asm/errno.h>

//#include <sys/types.h>
//#include <errno.h>
//#include <math.h>

extern int new_client(int no_sci_client);


//! un nouveau device est pris en charge par le serveur. on fait ce qu'il faut pour pouvoir faire des I/O dessus. renvoie ERROR/SUCCESS 
int ouvrir_device(char *devname, char *devpath, __u32 no_dev) {
  device_t *dev;

  log_message("open %s (devname=%s)\n", devpath, devname);

  dev = malloc(sizeof(struct device));
  if (dev == NULL) {
    log_error("malloc struct device failed\n");
    return ERROR;
  }
  
  if ((dev->fd = open(devpath,O_RDWR | O_LARGEFILE))<0) {
    log_error("open failed\n");
    goto error_open;
  }

  strcpy(dev->name, devname);
  strcpy(dev->path, devpath);

 
  smdsv.devices[no_dev] = dev;
  return SUCCESS;

 error_open:
  free(dev);
  return ERROR;
}
 
//! fermeture de tous les devices pris en charge par le serveur.
void fermer_devices(void) {
  device_t  *dev;
  int i;

  log_message("closing devices currently opened\n");
  for (i=0; i<NBMAX_DEVICES; i++) {
    dev = smdsv.devices[i];
    if (dev != NULL) {	
      close(dev->fd);     
      free(dev);
      smdsv.devices[i] = NULL;
    }
  }
}


void gestionnaire_signaux(int numero, struct siginfo *info, void *inutile) {
  union sigval valeur;
  int retval;
  log_message("signal recu %d info = 0x%p\n",numero, info);
   
     
  if (numero == SIGNEWIMPORT) {
    int no_sci_client = info->si_value.sival_int;
    pid_t pid_exportd;
    log_message("signal IMPORT reçu : nouveau noeud client = %d\n", 
		no_sci_client);

    retval = new_client(no_sci_client);

   
    valeur.sival_int = no_sci_client;
    pid_exportd  = info->si_pid;
    if (sigqueue(pid_exportd,SIGENDIMPORT,valeur)<0) {
    log_error("sigqueue(pid_exportdd=%d,SIGENDIMPORT,no_sci_importateur=%d) "
	      "failed\n", 
		pid_exportd,
		no_sci_client);
    }
    log_message("signal SIGENDIMPORT (%d) à exportd (l'import est terminé)\n",
		SIGENDIMPORT);
    return;
  }

  if (numero == SIGNEWDEVICE) {
    int nb_car, retval;
    int newdevice_fd;
    char buffer[BUFFER_MAX_SIZE+1];
    char nom_device[MAXSIZE_DEVNAME];
    char path_device[MAXSIZE_DEVPATH];
    pid_t pid_exportd;
    __u32 no_dev;
    
    log_message("signal NEWDEVICE reçu\n");
 
    memset(buffer, 0, BUFFER_MAX_SIZE+1);
    memset(nom_device, 0, BUFFER_MAX_SIZE+1);
		    
    newdevice_fd = open(NEWDEVICE2TR, O_RDWR);
    if (newdevice_fd<0) {
      log_message("erreur à l'ouverture de " NEWDEVICE2TR "\n");
      exit(ERROR);
    }
    log_message("named pipe '%s' ouvert\n",NEWDEVICE2TR);
    
    nb_car=read(newdevice_fd,buffer,BUFFER_MAX_SIZE+1);
    retval = sscanf(buffer,"%s %s %d",path_device, nom_device, &no_dev);
    close(newdevice_fd);

    if (retval != 3) {
      log_error("impossible de parser correctement le msg '%s'\n",
		buffer);
      return ;
    }

    log_message("ouverture du device %s (path=%s, no=%d)\n", 
		nom_device,
		path_device,
		no_dev);

    valeur.sival_int = ouvrir_device(nom_device, path_device, no_dev);
    pid_exportd  = info->si_pid;
    if (sigqueue(pid_exportd,SIGENDNEWDEV,valeur)<0) {
    log_error("ERROR - sigqueue(pid_exportdd=%d,SIGENDNEWDEV) failed\n", 
		pid_exportd);
    }
    log_message("signal SIGENDNEWDEV (%d) à exportd avec "
		"valeur = %d\n", 
		SIGENDNEWDEV,
		valeur.sival_int);
    return;
  }

  log_message("gestionnaire : signal reçu INCONNU\n");
}


struct sigaction action, action2;

void prep_signal(void) {
  action.sa_sigaction = gestionnaire_signaux;
  sigemptyset(&(action.sa_mask));
  sigaddset(&(action.sa_mask),SIGALRM);
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  if (sigaction(SIGNEWIMPORT, &action, NULL)) {
    log_error("erreur dans sigaction SIGNEWIMPORT\n");
    exit(ERROR);
  }
  log_message("signal SIGNEWIMPORT configured\n");

  action2.sa_sigaction = gestionnaire_signaux;
  sigemptyset(&(action2.sa_mask));
  sigaddset(&(action2.sa_mask),SIGALRM);
  action2.sa_flags = SA_SIGINFO | SA_RESTART;
  if (sigaction(SIGNEWDEVICE, &action2, NULL)) {
    log_error("erreur dans sigaction SIGNEWDEVICE\n");
    exit(ERROR);
  }
  log_message("signal SIGNEWDEVICE configured\n");
}

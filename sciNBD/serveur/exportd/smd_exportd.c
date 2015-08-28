
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../include/constantes.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include "parser.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <signal.h>
#include "sisci_api.h"
#include "table.h"
#include <syslog.h>



#ifdef DEBUG_MAQUETTE
#define log_message(fmt, args...) \
{ \
syslog(LOG_DEBUG, " %s():%d - " fmt, \
       __FUNCTION__, \
       __LINE__, ## args);  } 
#else
#define log_message(fmt, args...) \
{ } // on n'affiche rien si on ne débugge pas
#endif

#define log_error(fmt, args...) \
{ \
syslog(LOG_ERR, " %s():%d - ERROR: " fmt, \
       __FUNCTION__, \
       __LINE__, ## args);  }


// descripteur des tubes nommés ouverts par smdd
int to_smd_exporte_fd = -1;
int from_smd_exporte_fd = -1;
int sock_fd = -1;
int newdevice_to_tr_fd = -1;

struct exported_device {
  char dev_path[MAXSIZE_LINE];
  char dev_name[MAXSIZE_DEVNAME];
  __u64 nb_sectors;
};

struct exported_device *edevs[NBMAX_EDEVS];
int nb_exported_devices;

int SCI_NODE_ID; // no SCI du noeud sur lequel tourne ce démon
char string_err[256]; 

int pid_serverd = -1;

void clean_up(void) {
  free_table((void *)edevs, NBMAX_EDEVS);
  if (to_smd_exporte_fd != -1) close(to_smd_exporte_fd);
  if (from_smd_exporte_fd != -1) close(from_smd_exporte_fd);
  if (newdevice_to_tr_fd != -1) close(newdevice_to_tr_fd);
  if (sock_fd != -1) close(sock_fd);
  unlink(SMDD2EXPORTE);
  unlink(EXPORTE2SMDD);
  unlink(NEWDEVICE2TR);
  closelog();
}

volatile int attente_sig = 1;

void wait_sig(int no_signal) {
  // exemple pris du très bon bouquin 'prg° syst en C sour Linux'
  // de l'attente d'un signal sans risque de blocage 
  // ni occupation du CPU trop forte
  sigset_t ensemble;
  sigset_t ancien;
  int sig_dans_masque = 0;

  sigemptyset(&ensemble);
  sigaddset(&ensemble, no_signal);
  sigprocmask(SIG_BLOCK, &ensemble, &ancien);
  if (sigismember(&ancien, no_signal)) {
    sigdelset(&ancien, no_signal);
    sig_dans_masque = 1;
  }
 
  while (attente_sig != 0) {
    sigsuspend(&ancien);
  }
  attente_sig = 1;

  if (sig_dans_masque)
    sigaddset(&ancien,no_signal);
  sigprocmask(SIG_SETMASK,&ancien,NULL);
}

int ouverture_ok;

///////////////////////////////////////////////////////
// gestion des SIGNAUX (SIGENDIMPORT)
void gestionnaire_signaux(int numero, struct siginfo *info, void *inutile) {
  log_message("signal recu %d info = 0x%p\n",
	      numero, 
	      info);
  
  if (numero == SIGENDIMPORT) {
    log_message("signal SIGENDIMPORT pour noeud importateur %d\n",
		info->si_value.sival_int);
    attente_sig=0;
    return;
  }

  if (numero == SIGENDNEWDEV) {
    log_message("signal SIGENDNEWDEV reçu. ouverture reussie = %d\n",
		info->si_value.sival_int);	
    ouverture_ok = info->si_value.sival_int;
    attente_sig=0;
    return;
  } 
  
  log_message("signal reçu INCONNU\n");    
}
 

struct sigaction action, action2;

void prep_signal(void) {
  action.sa_sigaction = gestionnaire_signaux;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = SA_SIGINFO;
  if (sigaction(SIGENDIMPORT, &action, NULL)) {
    log_error("sigaction SIGENDIMPORT\n");
    exit(ERROR);
  } 

  action2.sa_sigaction = gestionnaire_signaux;
  sigemptyset(&(action2.sa_mask));
  action2.sa_flags = SA_SIGINFO;
  if (sigaction(SIGENDNEWDEV, &action2, NULL)) {
    log_error("sigaction SIGENDNEWDEV\n");
    exit(ERROR);
  }
}

void signal_handler(sig)
int sig;
{
	switch(sig) {
	case SIGHUP:
		log_message("hangup signal catched\n");
		break;
	case SIGTERM:
		log_message("terminate signal catched\n");
		clean_up();
		exit(SUCCESS);
		break;
	}
}
// fonction qui signale à serverd qu'un nouveau device est exporté
int signaler_tr_nouveau_device(struct exported_device *edev, __u32 no_dev) {
  // FIXME : d'une manière générale : choisir une langue pour les messages de log !!
  union sigval valeur;
  int nb_car;
  char buf[MAXSIZE_LINE]; 

  valeur.sival_int = 0;
  if (sigqueue(pid_serverd,SIGNEWDEVICE,valeur)<0) {
    log_error("sigqueue(pid_serverd,SIGNEWDEVICE,valeur) failed\n");
    return ERROR;
  }
  
  log_message("sigqueue(pid_serverd,SIGNEWDEVICE (%d),valeur) succeeded\n",
	      SIGNEWDEVICE );
 
  sprintf(buf,"%s %s %d", edev->dev_path, edev->dev_name, no_dev);
  
  log_message("écriture msg '%s' dans le pipe %s\n",
	      buf,
	      NEWDEVICE2TR);
  
  nb_car=write(newdevice_to_tr_fd,
	       buf,
	       strlen(buf));
  
  log_message("attente réponse smd_serverd\n");
  wait_sig(SIGENDNEWDEV);

  log_message("smd_serverd a pris en compte le device %s\n",
	      edev->dev_name);

  
  log_message("ouverture_ok = %d\n", ouverture_ok);
  return ouverture_ok;  
}

//! indique si le dev_name passé en param n'a pas déjà été donné à un des dev exportés par ce démon. Renvoie TRUE/FALSE
int dev_name_est_unique(char *dev_name) {
  int i;
  for (i=0; i<NBMAX_EDEVS; i++)
    if (edevs[i] != NULL) 
      if (strcmp(dev_name, edevs[i]->dev_name) == 0) 
	return FALSE;
  
  return TRUE;
}

//! indique si le dev_path passé en param n'est pas déjà exporté par ce démon. Renvoie TRUE/FALSE
int dev_path_est_unique(char *dev_path) {
  int i;
  for (i=0; i<NBMAX_EDEVS; i++)
    if (edevs[i] != NULL) 
      if (strcmp(dev_path, edevs[i]->dev_path) == 0) 
	return FALSE;
  
  return TRUE;
}

void Raz_buffer(char *buffer) {
  int i;
  for (i=0;i<MAXSIZE_BUFFER+1;i++) buffer[i]=0;
}

//! fonction qui gère les requêtes provenant de la commande "smd_exporte". Renvoie SUCCESS/ERROR
int handle_msg_smd_exporte(int read_fd, int write_fd) {	
  char buffer[MAXSIZE_BUFFER+1];
  char buffer_emission[MAXSIZE_BUFFER+1];
  int nb_car;
  int no_device,trouve;
  char sortie_log[100];
  char chaine[MAXSIZE_LINE+1];
  int no_dev, retval;
  struct exported_device *new_dev;
  
  Raz_buffer(buffer);
  Raz_buffer(buffer_emission);

  log_message("Traitement du msg smd_exporte\n");
  nb_car=read(read_fd,buffer,MAXSIZE_BUFFER+1);
  
  log_message("msg smd_exporte lu\n");
  sprintf(sortie_log, "%d caractères lu",nb_car);
  log_message("%s\n",sortie_log);	
  
  Extraire_mot(buffer, chaine);
  log_message("commande extraite : ");
  log_message("%s\n",chaine);
  
  
  if (strcmp(chaine,"A")==0) {
    // ajout d'un nouveau device à exporter
    no_dev = unused_elt_indice((void *)edevs, NBMAX_EDEVS);
    new_dev = malloc(sizeof(struct exported_device));
    sscanf(buffer, "%s %s %Ld", 
	   new_dev->dev_path, 
	   new_dev->dev_name,
	   &new_dev->nb_sectors);
	   
    log_message("ajout d'un nouveau dev %d: path = %s, name = %s, "
		"nb sectors = %lld\n",
		no_dev,
		new_dev->dev_path, 
		new_dev->dev_name,
		new_dev->nb_sectors);
	
    if (dev_name_est_unique(new_dev->dev_name) == FALSE) {
      log_error("ECHEC de la prise en compte du nouveau device : "
		"le nom du dev n'est pas unique\n");
      nb_car=write(write_fd,"NU",2); // NON UNIQUE = NU 
      goto error_new_dev;
    }
      
    if (dev_path_est_unique(new_dev->dev_path) == FALSE) {
      log_error("ECHEC de la prise en compte du nouveau device : "
		"le path du dev n'est pas unique\n");
      nb_car=write(write_fd,"PN",2); // PATH NON UNIQUE = PN 
      goto error_new_dev;
    }
    
	
    if (signaler_tr_nouveau_device(new_dev, no_dev) == ERROR) {
      log_error("ECHEC de la prise en compte du nouveau device : "
		"le démon serverd n'a pas pu le prend en compte\n");
      nb_car=write(write_fd,"ED",2); // ERROR DEVICE = ED
      goto error_new_dev;
    }
    
    nb_exported_devices++;
    edevs[no_dev] = new_dev;
    log_message("SUCCES de la prise en compte du nouveau device %d (%p)\n",
		no_dev,edevs[no_dev]);    
    nb_car=write(write_fd,"OK",2);
    return SUCCESS; 
    
    
  error_new_dev:	  
    free(new_dev);
    return ERROR;
  }
 
 	

  if (strcmp(chaine,"D")==0) {
    retval = sscanf(buffer,"%s",chaine);
    if (retval != 1) {
      nb_car=write(write_fd,"ERROR",strlen("ERROR"));
      return ERROR;
    }

    // on traite le retrait du device
    log_message("Demande de suppression d'un device exporté %s\n",chaine);
    trouve=0;	
    
    for (no_device = 0; no_device < NBMAX_EDEVS; no_device++) {
      if (edevs[no_device] != NULL) 
	if (strcmp(edevs[no_device]->dev_name, chaine) == 0) {
	  log_message("Device trouvé\n");
	  trouve=1;
	
	  free(edevs[no_device]);
	  edevs[no_device] = NULL;
	  nb_exported_devices--;
	  break;
	}
    }	
    if (trouve) {
      nb_car=write(write_fd,"OK",strlen("OK"));
      return SUCCESS;
    }
    else {
      nb_car=write(write_fd,"ERROR",strlen("ERROR"));
      return ERROR;
    }
  }
    
  if (strcmp(chaine,"L")==0) {
	
    // on traite la demande de la liste des devices exportés
    log_message("Demande de la liste des devices exportés\n");
		
    if (nb_exported_devices>0) {
      nb_car=0;
      nb_car+=sprintf(buffer_emission+nb_car,"Liste des devices exportés :\n");
      for (no_device=0;no_device<NBMAX_EDEVS;no_device++) 
	if (edevs[no_device] != NULL) {
	  
	  nb_car+=sprintf(buffer_emission+nb_car,
			  "device %s : no = %d, path = %s, "
			  "size = %lld secteurs\n",
			  edevs[no_device]->dev_name,
			  no_device,
			  edevs[no_device]->dev_path,
			  edevs[no_device]->nb_sectors);
	}
    }
    else {
      log_message("Aucun device n'est exporté\n");	
      sprintf(buffer_emission,"Aucun device n'est exporté\n");
    }
			
    nb_car=write(write_fd,buffer_emission,strlen(buffer_emission));
    return SUCCESS;
  }
   

  log_message("Commande reçue de smd_exporte : inconnue = ");
  log_message("%s\n",chaine);
  return ERROR;
  }

void daemonize()
{
  int i,lfp;
  time_t t;
  char str[10];

  // on note la date et l'heure du lancement du démon dans le fichier log
  chdir(RUNNING_DIR); /* change running directory */
  time(&t);
  log_message("Starting daemon %s\n",ctime(&t));

  if(getppid()!=1) { // si le process n'est pas déjà un démon
    i=fork();
    if (i<0) exit(ERROR); /* fork error */
    if (i>0) exit(SUCCESS); /* parent exits */
  }
  /* child (daemon) continues */
  setsid(); /* obtain a new process group */
  for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
  i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
  umask(027); /* set newly created file permissions */
  lfp=open(EXPORTD_LOCKFILE,O_RDWR|O_CREAT,0640);
  if (lfp<0) exit(ERROR); /* can not open */
  if (lockf(lfp,F_TLOCK,0)<0) exit(ERROR); /* can not lock */
  
  /* first instance continues */
  sprintf(str,"%d",getpid());
  write(lfp,str,strlen(str)); /* record pid to lockfile */
  signal(SIGCHLD,SIG_IGN); /* ignore child */
  signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  signal(SIGHUP,signal_handler); /* catch hangup signal */
  signal(SIGTERM,signal_handler); /* catch kill signal */
	
}	

// création des tubes nommés qui permettront au démon de communiquer avec la commande
// smd_exporte et le démon smd_serverd
void create_named_pipes(void) {	
  //// POUR LA COMMANDE smd_exporte
  if (mkfifo(SMDD2EXPORTE,S_IWUSR | S_IRUSR) == -1) {
    log_error("à la création de %s\n", SMDD2EXPORTE);
    exit(ERROR);
  }
  
  if (mkfifo(EXPORTE2SMDD,S_IWUSR | S_IRUSR) == -1) {
    log_error("à la création de %s \n", EXPORTE2SMDD);
    exit(ERROR);
  }	       
  
  to_smd_exporte_fd = open(SMDD2EXPORTE, O_RDWR);
  if (to_smd_exporte_fd<0) {
    log_error(" erreur à l'ouverture de %s\n", SMDD2EXPORTE);
    exit(ERROR);
  }
  log_message("%s ouvert\n",SMDD2EXPORTE);
  
  from_smd_exporte_fd = open(EXPORTE2SMDD, O_RDWR);
  if (from_smd_exporte_fd<0) {
    log_error("erreur à l'ouverture de %s\n", EXPORTE2SMDD);
    exit(ERROR);
  }	
  log_message("%s ouvert\n",EXPORTE2SMDD);

  // pour smd_serverd
  if (mkfifo(NEWDEVICE2TR,S_IWUSR | S_IRUSR) == -1) {
    log_error("erreur à la création de %s\n", NEWDEVICE2TR);
    exit(ERROR);
  }
  newdevice_to_tr_fd = open(NEWDEVICE2TR, O_RDWR);
  if (newdevice_to_tr_fd<0) {
    log_error("erreur à l'ouverture de %s\n",NEWDEVICE2TR);
    exit(ERROR);
  }
  log_message("%s ouvert\n",NEWDEVICE2TR);
  
  log_message("tous les tubes nommés ouverts\n");
}

// renvoie un descripteur sur le socket qui permet d'attendre des requêtes d'importation des clients
int listen_socket_port(void) {
  int sd;
  struct sockaddr_in sin;
  int autorisation;
  
  /* get an internet domain socket */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    log_error("socket error\n");
    exit(ERROR);
  }
  log_message("création du socket\n");
  
  // on change les options du socket pour pouvoir relancer
  // le démon toute de suite après un arrêt (sinon, le bind
  // bloque pendant 60 sec, car mode TCP)
  autorisation = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &autorisation, sizeof (int)); 
  
  /* complete the socket structure */
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(SOCKET_PORT);
  
  /* bind the socket to the port number */
  if (bind(sd, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
    log_error("bind error\n");
    exit(ERROR);
  }
  log_message("bind du socket\n");
  
  /* show that we are willing to listen */
  if (listen(sd, 5) == -1) {
    log_error("listen error\n");
    exit(ERROR);
  }
  log_message("ecoute du socket\n");
  
  return sd;
}

int exporter_devices(int msg_sock, int no_sci_importateur) {
  char buffer_emission[MAXSIZE_BUFFER+1];
  int nb_car,no_device;
  
  union sigval valeur;
  struct exported_device *edev;
  

  // on traite la demande d'import de devices en revoyant 
  // la liste des devices exportés par ce serveur
  log_message("demande d'exportation de devices "
	      "reçue du noeud %d \n", 
	      no_sci_importateur);


  valeur.sival_int = no_sci_importateur;
  if (sigqueue(pid_serverd,SIGNEWIMPORT,valeur)<0) {
    log_error("sigqueue(pid_serverd=%d,SIGNEWIMPORT,no_sci_importateur=%d) failed\n", 
		pid_serverd,
		no_sci_importateur);
    return ERROR;
  }
 

  log_message("sigqueue(pid_serverd=%d,SIGNEWIMPORT,no_sci_importateur=%d) succeeded\n", 
	      pid_serverd,
	      no_sci_importateur);

  // attente d'un signal venant de smd_serverd
  wait_sig(SIGENDIMPORT);

  log_message("serverd prêt à répondre aux requêtes du nouvel importateur\n");

  // envoi de message à l'importateur pour autorisation importation
  if (nb_exported_devices>0) {
    nb_car=0;
    for (no_device=0;no_device<NBMAX_EDEVS;no_device++) {
      edev = edevs[no_device];
      if (edev != NULL) 
	nb_car+=sprintf(buffer_emission+nb_car,"%s %s %d %lld %d ",
			edev->dev_name,
			edev->dev_path,
			no_device,
			edev->nb_sectors,
			SCI_NODE_ID);
      
    }
    
  }
  else {
    log_message("aucun device n'est exporté\n");	
    sprintf(buffer_emission,"RIEN");
  }
  
  log_message("envoie msg '%s' au client '%d'\n",
	      buffer_emission,
	      no_sci_importateur);
  nb_car=write(msg_sock,buffer_emission,strlen(buffer_emission));
  if (nb_car <= 0) return ERROR; 
  return SUCCESS;
}

void handle_sock_read(int msg_sock) {
  char buffer_reception[MAXSIZE_BUFFER+1];
  char sortie_log[MAXSIZE_LINE+1];
  char chaine[MAXSIZE_LINE+1];
  int arg1;
  int nb_car;

  log_message("traitement du msg smd_exporte\n");
  nb_car=read(msg_sock,buffer_reception,MAXSIZE_BUFFER+1);
  
  log_message("msg smd_exporte lu\n");
  sprintf(sortie_log, "%d caractères lu",nb_car);
  log_message("%s\n",sortie_log);	
  
  sscanf(buffer_reception, "%s %d", chaine, &arg1);
  log_message("commande extraite : ");
  log_message("%s\n",chaine);
  
  
  if (strcmp(chaine,"IMPORT")==0) {
    if (exporter_devices(msg_sock,arg1) == ERROR) {
      log_message("importation du device par %d est un ECHEC\n",arg1);
      nb_car=write(msg_sock,"ERROR",strlen("ERROR"));
    }
 
  }
  else {
    log_message("Commande reçue de smd_importe : inconnue = ");
    log_message("%s\n",chaine);
  }
  log_message("sortie de la fonction de traitement\n");	
}

//////////////////////////////////////////////////////
char *Sci_error(int error) {
  char **string_error=(char **)(&string_err);

  *string_error = "Error UNKNOWN\n";

  if (error == SCI_ERR_BUSY              	  ) *string_error = "Error SISCI no 0x900"; 
  if (error == SCI_ERR_FLAG_NOT_IMPLEMENTED       ) *string_error = "Error SISCI no 0x901";  
  if (error == SCI_ERR_ILLEGAL_FLAG               ) *string_error = "Error SISCI no 0x902";  
  if (error == SCI_ERR_NOSPC                      ) *string_error = "Error SISCI no 0x904";  
  if (error == SCI_ERR_API_NOSPC                  ) *string_error = "Error SISCI no 0x905";
  if (error == SCI_ERR_HW_NOSPC                   ) *string_error = "Error SISCI no 0x906";  
  if (error == SCI_ERR_NOT_IMPLEMENTED            ) *string_error = "Error SISCI no 0x907";  
  if (error == SCI_ERR_ILLEGAL_ADAPTERNO          ) *string_error = "Error SISCI no 0x908";   
  if (error == SCI_ERR_NO_SUCH_ADAPTERNO          ) *string_error = "Error SISCI no 0x909";
  if (error == SCI_ERR_TIMEOUT                    ) *string_error = "Error SISCI no 0x90A";
  if (error == SCI_ERR_OUT_OF_RANGE               ) *string_error = "Error SISCI no 0x90B";
  if (error == SCI_ERR_NO_SUCH_SEGMENT            ) *string_error = "Error SISCI no 0x90C";
  if (error == SCI_ERR_ILLEGAL_NODEID             ) *string_error = "Error SISCI no 0x90D";
  if (error == SCI_ERR_CONNECTION_REFUSED         ) *string_error = "Error SISCI no 0x90E";
  if (error == SCI_ERR_SEGMENT_NOT_CONNECTED      ) *string_error = "Error SISCI no 0x90F";
  if (error == SCI_ERR_SIZE_ALIGNMENT             ) *string_error = "Error SISCI no 0x910";
  if (error == SCI_ERR_OFFSET_ALIGNMENT           ) *string_error = "Error SISCI no 0x911";
  if (error == SCI_ERR_ILLEGAL_PARAMETER          ) *string_error = "Error SISCI no 0x912";
  if (error == SCI_ERR_MAX_ENTRIES                ) *string_error = "Error SISCI no 0x913";   
  if (error == SCI_ERR_SEGMENT_NOT_PREPARED       ) *string_error = "Error SISCI no 0x914";
  if (error == SCI_ERR_ILLEGAL_ADDRESS            ) *string_error = "Error SISCI no 0x915";
  if (error == SCI_ERR_ILLEGAL_OPERATION          ) *string_error = "Error SISCI no 0x916";
  if (error == SCI_ERR_ILLEGAL_QUERY              ) *string_error = "Error SISCI no 0x917";
  if (error == SCI_ERR_SEGMENTID_USED             ) *string_error = "Error SISCI no 0x918";
  if (error == SCI_ERR_SYSTEM                     ) *string_error = "Error SISCI no 0x919";
  if (error == SCI_ERR_CANCELLED                  ) *string_error = "Error SISCI no 0x91A";
  if (error == SCI_ERR_NOT_CONNECTED              ) *string_error = "Error SISCI no 0x91B";
  if (error == SCI_ERR_NOT_AVAILABLE              ) *string_error = "Error SISCI no 0x91C";
  if (error == SCI_ERR_INCONSISTENT_VERSIONS      ) *string_error = "Error SISCI no 0x91D";
  if (error == SCI_ERR_COND_INT_RACE_PROBLEM      ) *string_error = "Error SISCI no 0x91E";
  if (error == SCI_ERR_OVERFLOW                   ) *string_error = "Error SISCI no 0x91F";
  if (error == SCI_ERR_NOT_INITIALIZED            ) *string_error = "Error SISCI no 0x920";
  if (error == SCI_ERR_ACCESS                     ) *string_error = "Error SISCI no 0x921";
  if (error == SCI_ERR_NO_SUCH_NODEID             ) *string_error = "Error SISCI no 0xA00";
  if (error == SCI_ERR_NODE_NOT_RESPONDING        ) *string_error = "Error SISCI no 0xA02";  
  if (error == SCI_ERR_NO_REMOTE_LINK_ACCESS      ) *string_error = "Error SISCI no 0xA04";
  if (error == SCI_ERR_NO_LINK_ACCESS             ) *string_error = "Error SISCI no 0xA05";
  if (error == SCI_ERR_TRANSFER_FAILED            ) *string_error = "Error SISCI no 0xA06";

  return *string_error;
}


int sci_query(void) {
  sci_query_adapter_t query;
  sci_error_t error;
  
  // initialize the SCI environment 
  SCIInitialize(NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    log_error("sci_query : erreur SCIInitialize - ");
    log_error("%s\n",Sci_error(error));
    return ERROR;
  }

  // détermine le node_id SCI du PC sur lequel s'exécute ce démon
  query.localAdapterNo = 0;
  query.subcommand = SCI_Q_ADAPTER_NODEID;
  query.data = &SCI_NODE_ID;
  SCIQuery(SCI_Q_ADAPTER,&query,NO_FLAGS,&error);
  if (error != SCI_ERR_OK) {
    log_error("sci_query : erreur SCIQuery - ");
    log_error("%s\n",Sci_error(error));
    return ERROR;
  }  

  log_message("sci_query : SCI node_id = %d\n", SCI_NODE_ID);

  SCITerminate();
  return SUCCESS;
}

void usage(void) {
  printf("AIDE POUR LE DEMON smd_exportd <arg1>:\n");
  printf("--------------------------------------\n");
  printf("arg 1 : PID de smd_serverd\nExemples : \n");
  printf("> ./smd_exportd 18690 \n");
  printf("> ./smd_exportd `pidof smd_serverd`\n");       
}

void init_daemon(void) {
  daemonize();
  create_named_pipes();
  prep_signal();
  nullify_table((void *)edevs, NBMAX_EDEVS);
  nb_exported_devices=0;
}

int main(int argc, char *argv[]) {
  fd_set read_set;
   fd_set write_set;
  int max_fd;
  int addrlen;
  struct sockaddr_in pin;
  int msg_sock;
  int retval;
  
  
  openlog(argv[0], LOG_PID, LOG_USER);
  clean_up(); // pour éliminer les pipes en cas de crash antérieur
  if (argc!=2) {
    usage();
    exit(ERROR);
  } 
  sscanf(argv[1],"%d",&pid_serverd);
  
  init_daemon();
   
  if (sci_query() == ERROR) {
    log_error("dans l'allocation des ressources SCI (sci_init)\n");
    return ERROR;
  }
  
  sock_fd=listen_socket_port();  
  max_fd=from_smd_exporte_fd;
  if (sock_fd>max_fd) max_fd=sock_fd;
  
  log_message("Attente de messages\n");	
  
  while(1) {
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    FD_SET(from_smd_exporte_fd,&read_set);
    FD_SET(sock_fd,&read_set);

    retval = select(max_fd+1,&read_set,&write_set,NULL,NULL);

    log_message("sortie select = %d\n", retval);

    if (retval < 0) {
      log_error("select des descripteurs : %s\n",strerror(errno));
      goto fail;
    }
    

    if (FD_ISSET(from_smd_exporte_fd,&read_set)) // la commande smd_exporte vient d'envoyer un message
      handle_msg_smd_exporte(from_smd_exporte_fd,to_smd_exporte_fd);
    
    if (FD_ISSET(sock_fd,&read_set)) { // réception d'une demande d'importation de devices
      msg_sock=accept(sock_fd,(struct sockaddr *) &pin, &addrlen);
      if (msg_sock<0) {
	log_error("pendant l'acceptation de la connexion\n");
	exit(ERROR);
      }
      handle_sock_read(msg_sock);
      close(msg_sock);
      log_message("fin de traitement du message\n");
    }
  }
  clean_up();
  return SUCCESS;
  
 fail:
  return ERROR;
}





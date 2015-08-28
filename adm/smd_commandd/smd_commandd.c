#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "constantes.h"
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include "parser.h" 
#include <sys/time.h>

// macro qui affiche des messages dans le log
FILE *logfile=NULL;

#ifdef DEBUG_MAQUETTE
#define log_message(fmt, args...) \
logfile=fopen(LOG_FILE,"a"); \
fprintf(logfile, fmt, ## args); \
fprintf(logfile,"\n"); \
fclose(logfile)
#else
#define log_message(fmt, args...) // on n'affiche rien si on ne débugge pas
#endif

struct Charge {
  double mesure[NB_MAX_BRIQUES][NB_MAX_MESURES];
  double somme_mesures[NB_MAX_BRIQUES];
  int index[NB_MAX_BRIQUES];
  long busy_time_precedent[NB_MAX_BRIQUES];
  struct timeval date_derniere_mesure;
  long busy_time_ssh_prec;
};

struct Speedup{
  double mesure[NB_MAX_MESURES];
  long busy_time_precedent[NB_MAX_BRIQUES];
  double somme;
  int index;
  long temps_travail_prec;
}; 


struct Kos {
  double mesure[NB_MAX_MESURES];
  double somme;
  int index;
  struct timeval date_derniere_mesure;
  long long bytes_transfered_prec;
};

struct Ios {
  double mesure[NB_MAX_MESURES];
  double somme;
  int index;
  struct timeval date_derniere_mesure;
  long ios_transfered_prec;
}; 

struct Charge *charge;
struct Speedup *speedup;
struct Ios *ios;
struct Kos *kos;

void init_donnees(void) {
  int no_brique, i;

  charge = malloc(sizeof(struct Charge));
  speedup = malloc(sizeof(struct Speedup));
  ios = malloc(sizeof(struct Ios));
  kos = malloc(sizeof(struct Kos));
  if ((charge == NULL) ||(speedup == NULL) ||
      (ios == NULL) || (kos == NULL)) {
    log_message("ERREUR dans l'allocation des structures de données\n");
    exit(EXIT_FAILURE);
  }
      
  
  charge->busy_time_ssh_prec=0;
  kos->bytes_transfered_prec=0;
  ios->ios_transfered_prec=0;

  kos->somme = 0;
  kos->index = 0;
  gettimeofday(&(kos->date_derniere_mesure), NULL);

  ios->somme = 0;
  ios->index = 0;
  gettimeofday(&(ios->date_derniere_mesure), NULL);

  speedup->temps_travail_prec = 0;
  speedup->somme = 0;
  speedup->index = 0;

  for (i=0; i<NB_MAX_MESURES; i++) {
    speedup->mesure[i]=1.0; // car si pas de travail => speed up = 1 (seq = //)
    ios->mesure[i]=0.0;
    kos->mesure[i]=0.0;
  }
  
  for (no_brique =0; no_brique < NB_MAX_BRIQUES; no_brique++) {
    for (i=0; i<NB_MAX_MESURES; i++) {
      charge->mesure[no_brique][i]=0.0;
      }
    speedup->busy_time_precedent[no_brique] = 0.0;
    charge->somme_mesures[no_brique] = 0.0;
    charge->index[no_brique] = 0;
    gettimeofday(&(charge->date_derniere_mesure), NULL);
  }

}

int sock_fd;

void clean_up(void) {
  free(charge);
  free(speedup);
  free(ios);
  free(kos);
  close(sock_fd);
}

void signal_handler(sig)
int sig;
{
	switch(sig) {
	case SIGHUP:
		log_message("hangup signal catched");
		break;
	case SIGTERM:
		log_message("terminate signal catched");
		clean_up();
		exit(OK);
		break;
	}
}


void daemonize()
{
int i,lfp;
//time_t *t;
char str[10];
	if(getppid()!=1) { // si le process n'est pas déjà un démon
		i=fork();
		if (i<0) exit(ERROR); /* fork error */
		if (i>0) exit(OK); /* parent exits */
	}
	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
	i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
	umask(027); /* set newly created file permissions */
	chdir(RUNNING_DIR); /* change running directory */
	lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
	if (lfp<0) exit(ERROR); /* can not open */
	if (lockf(lfp,F_TLOCK,0)<0) exit(ERROR); /* can not lock */
	
	/* first instance continues */
	sprintf(str,"%d\n",getpid());
	write(lfp,str,strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,signal_handler); /* catch hangup signal */
	signal(SIGTERM,signal_handler); /* catch kill signal */
}

// renvoie un descripteur sur le socket qui permet d'attendre des requêtes d'importation des clients
int listen_socket_port(void) {
	int 	 sd;
	struct   sockaddr_in sin;
	int autorisation;

 	
	/* get an internet domain socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("listen_socket_port : socket");
		exit(1);
	}
	log_message("listen_socket_port : Création du socket\n");

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
		perror("listen_socket_port : bind");
		exit(1);
	}
	log_message("listen_socket_port : Bind du socket\n");

	/* show that we are willing to listen */
	if (listen(sd, 5) == -1) {
		perror("listen_socket_port : listen");
		exit(1);
	}
	log_message("listen_socket_port : Ecoute du socket\n");
	
	return sd;
}
  
void calculer_speedup(double *speed_up, 
		      long busy_time_brique[], 
		      long busy_time_ssh,
		      long nb_briques) { 
  long seq_time_sum = 0;
  long work_time;
  double mesure_speedup;
  int no_brique,i ;

  for (no_brique = 0; no_brique < nb_briques; no_brique++) {
    seq_time_sum += busy_time_brique[no_brique] - speedup->busy_time_precedent[no_brique];
    /*log_message("B%d seq_time_sum=%ld prec=%ld", no_brique,
		busy_time_brique[no_brique],
		speedup->busy_time_precedent[no_brique]);
    */
    speedup->busy_time_precedent[no_brique] = busy_time_brique[no_brique];
  }
  
  work_time = busy_time_ssh - speedup->temps_travail_prec;
  speedup->temps_travail_prec = busy_time_ssh;

  if (work_time !=0)
    mesure_speedup = (1.0*seq_time_sum)/work_time;
  else 
    mesure_speedup = 1.0; // cas bordure : si personne travaille, en seq ou en //, ca va aussi vite !
  
  /*log_message("seq_time_sum = %ld work_time=%ld mes_speedup = %.2f",
	      seq_time_sum,
	      work_time,
	      mesure_speedup);
  */

  if (speedup->index < NB_MAX_MESURES) {
      speedup->mesure[kos->index] = mesure_speedup;
      speedup->somme += mesure_speedup;
      speedup->index++;
    }
    else {
      speedup->somme += mesure_speedup - speedup->mesure[0];
      for (i=0; i < NB_MAX_MESURES - 1; i++)
	speedup->mesure[i] = speedup->mesure[i+1];

      speedup->mesure[NB_MAX_MESURES - 1] = mesure_speedup;
    }

 
  *speed_up = speedup->somme / speedup->index;
  /*log_message("s_somme = %.2f index=%d speed_up=%.2f",
	      speedup->somme,
	      speedup->index,
	      *speed_up);
  */
}

void calculer_charge_briques(long busy_time_brique[], 
			     int charge_brique[], int nb_briques) {
  struct timeval maintenant;
  double temps_ecoule; // en ms
  double mesure;
  int i, no_brique;


  // calcul des charges de travail par rapport réel au temps écoulé
  gettimeofday(&maintenant,NULL);
  temps_ecoule =  (maintenant.tv_sec - charge->date_derniere_mesure.tv_sec)*1000.0 
    + (maintenant.tv_usec - charge->date_derniere_mesure.tv_usec)/1000.0;
  //log_message("temps ecoule %.2f",temps_ecoule);
  
  charge->date_derniere_mesure.tv_usec = maintenant.tv_usec;
  charge->date_derniere_mesure.tv_sec = maintenant.tv_sec;

  for (no_brique = 0; no_brique < nb_briques; no_brique++) {
    if (temps_ecoule != 0.0 && charge->index[no_brique]!=0) 
       mesure = (busy_time_brique[no_brique] - charge->busy_time_precedent[no_brique])/temps_ecoule;
    else
      mesure = 0.0;
    
    charge->busy_time_precedent[no_brique] = busy_time_brique[no_brique];

    /*log_message("B%d : busy_t=%ld busy_t_prec=%ld mesure=%.2f ",
		no_brique,
		busy_time_brique[no_brique],
		charge->busy_time_precedent[no_brique],
		mesure);*/

    if (charge->index[no_brique] < NB_MAX_MESURES) {
      charge->mesure[no_brique][charge->index[no_brique]]=mesure;
      charge->somme_mesures[no_brique] += mesure;
      charge->index[no_brique]++;
      /*log_message("<NB_MAX : som_mes=%.2f ind=%d\n",
		  charge->somme_mesures[no_brique], 
		  charge->index[no_brique]);
      */
    }
    else {
      charge->somme_mesures[no_brique] += mesure - charge->mesure[no_brique][0];
      /*log_message(">=NB_MAX : som_mes=%.2f ind=%d\n",
		  charge->somme_mesures[no_brique], 
		  charge->index[no_brique]);*/

      for (i=0; i < NB_MAX_MESURES - 1; i++) 
	charge->mesure[no_brique][i] = charge->mesure[no_brique][i+1];

      charge->mesure[no_brique][NB_MAX_MESURES - 1]=mesure;
    }

    charge_brique[no_brique] = (int) ((100.0* charge->somme_mesures[no_brique]) / charge->index[no_brique]); 

    /*{
      int j;
      log_message("charge brique %d = %d (somme = %.2f, index = %d)",
		no_brique, 
		charge_brique[no_brique],
		charge->somme_mesures[no_brique],
		charge->index[no_brique]);

      for (j=0; j < 5; j++) { // charge->index[no_brique]; j++) {
	log_message("Mesure %d B%d = %.2f",j, no_brique, charge->mesure[no_brique][j]);
      } 
      }*/
  }   
}

void  calculer_kos_ssh(long long bytes_transfered_ssh, int *kos_ssh) {
  struct timeval maintenant;
  double temps_ecoule; // en ms
  int mesure_kos;
  int i;

  gettimeofday(&maintenant,NULL);
  temps_ecoule =  (maintenant.tv_sec - kos->date_derniere_mesure.tv_sec)*1000.0 
    + (maintenant.tv_usec - kos->date_derniere_mesure.tv_usec)/1000.0;
  
  kos->date_derniere_mesure.tv_usec = maintenant.tv_usec;
  kos->date_derniere_mesure.tv_sec = maintenant.tv_sec;
  
  if (temps_ecoule !=0)
    mesure_kos = (bytes_transfered_ssh - kos->bytes_transfered_prec)/ temps_ecoule;
  else mesure_kos = 0;
  

  /*log_message("b trans=%lld b trans_prec=%lld temps_ecoule=%.2f mesure=%d",
   	      bytes_transfered_ssh,
              kos->bytes_transfered_prec,
	      temps_ecoule,
	      mesure_kos);
  */

  kos->bytes_transfered_prec = bytes_transfered_ssh;

  if (kos->index < NB_MAX_MESURES) {
      kos->mesure[kos->index] = mesure_kos;
      kos->somme += mesure_kos;
      kos->index++;
    }
    else {
      kos->somme += mesure_kos - kos->mesure[0];
      for (i=0; i < NB_MAX_MESURES - 1; i++)
	kos->mesure[i] = kos->mesure[i+1];

      kos->mesure[NB_MAX_MESURES - 1] = mesure_kos;
    }

 
  *kos_ssh = kos->somme / kos->index;

  /*log_message("mod.somme=%.2f index=%d kos_ssh=%d",
	      kos->somme,
	      kos->index,
	      *kos_ssh);
    log_message("   "); 
  */
 }

void  calculer_ios_ssh(long ios_transfered_ssh, int *ios_ssh) {
  struct timeval maintenant;
  double temps_ecoule; // en ms
  int mesure_ios;
  int i;

  gettimeofday(&maintenant,NULL);
  temps_ecoule =  (maintenant.tv_sec - ios->date_derniere_mesure.tv_sec)*1000.0 
    + (maintenant.tv_usec - ios->date_derniere_mesure.tv_usec)/1000.0;
  
  ios->date_derniere_mesure.tv_usec = maintenant.tv_usec;
  ios->date_derniere_mesure.tv_sec = maintenant.tv_sec;
  
  if (temps_ecoule !=0)
    mesure_ios = (ios_transfered_ssh - ios->ios_transfered_prec)/ (temps_ecoule/1000.0);
  else mesure_ios = 0;

  ios->ios_transfered_prec = ios_transfered_ssh;

  if (ios->index < NB_MAX_MESURES) {
      ios->mesure[ios->index] = mesure_ios;
      ios->somme += mesure_ios;
      ios->index++;
    }
    else {
      ios->somme += mesure_ios - ios->mesure[0];
      for (i=0; i < NB_MAX_MESURES - 1; i++)
	ios->mesure[i] = ios->mesure[i+1];

      ios->mesure[NB_MAX_MESURES - 1] = mesure_ios;
    }

  *ios_ssh = ios->somme / ios->index;
}

void send_ssh_state(int msg_sock) {
  FILE *etat_briques=NULL;
  FILE *etat_zones=NULL;  
  char msg[BUFFER_MAX_SIZE+1];
  long busy_time_brique[NB_MAX_BRIQUES];
  long busy_time_ssh;
  int charge_brique[NB_MAX_BRIQUES];
  int ios_ssh;
  int kos_ssh;
  long ios_transfered_ssh;
  long long bytes_transfered_ssh;
  int occupation_brique[NB_MAX_BRIQUES];
  int nb_briques;
  int nb_zones;
  int no_brique;
  double val_speedup;
  long taille_go_usable;
  long reste_mo_usable;
  int len=0;

  // parsing des fichiers /proc
  if ( (etat_briques=fopen(PROC_ETAT_BRIQUES,"r")) == NULL) {
    log_message("send_ssh_state: error in opening %s\n", PROC_ETAT_BRIQUES);
    //exit(1);
  }

  if ( (etat_zones=fopen(PROC_ETAT_ZONES,"r")) == NULL) {
    log_message("send_ssh_state: error in opening %s\n", PROC_ETAT_ZONES);
    //exit(1);
  }
  
  fscanf(etat_zones,"%ld ",&taille_go_usable);
  fscanf(etat_zones,"%ld ",&reste_mo_usable);
  fscanf(etat_briques,"%d ",&nb_briques);
  fscanf(etat_zones,"%d ",&nb_zones);
  
  for (no_brique=0; no_brique < nb_briques; no_brique++) {
    fscanf(etat_briques,"%ld ", &(busy_time_brique[no_brique]));
    fscanf(etat_zones,"%d ", &(occupation_brique[no_brique]));
  }

  fscanf(etat_briques,"%ld ",&busy_time_ssh);
  fscanf(etat_briques,"%lld ",&bytes_transfered_ssh);
  fscanf(etat_briques,"%ld ",&ios_transfered_ssh);

  fclose(etat_briques);
  fclose(etat_zones);

  calculer_charge_briques(busy_time_brique, charge_brique, nb_briques);
  calculer_speedup(&val_speedup, busy_time_brique,busy_time_ssh, nb_briques);   
  calculer_kos_ssh(bytes_transfered_ssh, &kos_ssh);
  calculer_ios_ssh(ios_transfered_ssh, &ios_ssh);

  // création du message
  len += sprintf(msg+len,"%d ",nb_briques);
  for (no_brique=0; no_brique < nb_briques; no_brique++) 
    len += sprintf(msg+len,"%d %d ",charge_brique[no_brique], occupation_brique[no_brique]);
  len += sprintf(msg+len,"%d ",nb_zones); 
  len += sprintf(msg+len,"%d ",(int)(val_speedup*100)); // pour avoir 2 chiffres après la vigule (dans le moniteur)
  len += sprintf(msg+len,"%d ",kos_ssh); 
  len += sprintf(msg+len,"%d ",ios_ssh); 
  len += sprintf(msg+len,"%ld ",taille_go_usable); 
  len += sprintf(msg+len,"%ld ",reste_mo_usable); 
  
  msg[len++] = '\0';

  //log_message("send_ssh_state: envoi du msg : %s\n", msg);
  write(msg_sock,msg,strlen(msg));
}

void send_info_zone(int msg_sock, int no_zone) {
  // pour l'instant, on n'envoie aucune info supplémentaire
  char msg[BUFFER_MAX_SIZE+1]="NONE";
  write(msg_sock,msg,strlen(msg));
}

void send_info_zones(int msg_sock) {
  FILE *etat_zones=NULL;  
  char msg[BUFFER_MAX_SIZE+1];
  int taille_go[NB_MAX_ZONES];
  int reste_mo[NB_MAX_ZONES];
  int no_reel_zone[NB_MAX_ZONES];
  long taille_go_used;
  long reste_mo_used;
  long taille_go_usable;
  long reste_mo_usable;
  int nb_zones;
  int no_zone;
  int len=0, aux;


  // parsing des fichiers /proc
  if ( (etat_zones=fopen(PROC_ZONES,"r")) == NULL) {
    log_message("send_info_zones: error in opening %s\n", PROC_ZONES);
    //exit(1);
  }
  
  fscanf(etat_zones,"%d ",&nb_zones);
  
  for (no_zone=0; no_zone < nb_zones; no_zone++) {
    fscanf(etat_zones,"%d ", &(no_reel_zone[no_zone]));
    fscanf(etat_zones,"%d ", &(taille_go[no_zone]));
    fscanf(etat_zones,"%d ", &(reste_mo[no_zone]));
  }

  fscanf(etat_zones,"%ld ",&taille_go_usable);
  fscanf(etat_zones,"%ld ",&reste_mo_usable);

  fscanf(etat_zones,"%ld ",&taille_go_used);
  fscanf(etat_zones,"%ld ",&reste_mo_used);

 
  fclose(etat_zones);

  // création du message
  len += sprintf(msg+len,"%d ",nb_zones);

  len += sprintf(msg+len,"%ld ",taille_go_usable);
  len += sprintf(msg+len,"%ld ",reste_mo_usable);

  len += sprintf(msg+len,"%ld ",taille_go_used);
  len += sprintf(msg+len,"%ld ",reste_mo_used);

  for (no_zone=0; no_zone < nb_zones; no_zone++) 
    len += sprintf(msg+len,"%d %d %d ",
		   no_reel_zone[no_zone],
		   taille_go[no_zone],
		   reste_mo[no_zone]);

  msg[len++] = '\0';

  aux = write(msg_sock,msg,strlen(msg));  
  log_message("send_info_zones: envoi du msg %d bytes (nb ecrits = %d) : <%s>\n", 
	      strlen(msg), aux, msg);}

void send_info_briques(int msg_sock) {
  FILE *etat_briques=NULL;
  FILE *etat_zones=NULL;  
  FILE *info_briques=NULL;  
  char msg[BUFFER_MAX_SIZE+1];
  int charge_brique[NB_MAX_BRIQUES];
  int occupation_brique[NB_MAX_BRIQUES];
  long busy_time_brique[NB_MAX_BRIQUES];
  char nom_brique[NB_MAX_BRIQUES+1][TAILLE_MAX_NOM_DEV];
  int no_sci_brique[NB_MAX_BRIQUES];
  int taille_go_brique[NB_MAX_BRIQUES];
  int reste_mo_brique[NB_MAX_BRIQUES];
  int nb_briques;
  int no_brique;
  int len=0;
  int aux;

  // parsing des fichiers /proc
  if ( (etat_briques=fopen(PROC_ETAT_BRIQUES,"r")) == NULL) {
    log_message("send_info_briques: error in opening %s\n", PROC_ETAT_BRIQUES);
    //exit(1);
  }

  if ( (info_briques=fopen(PROC_BRIQUES,"r")) == NULL) {
    log_message("send_info_briques: error in opening %s\n", PROC_BRIQUES);
    //exit(1);
  }

  if ( (etat_zones=fopen(PROC_ETAT_ZONES,"r")) == NULL) {
    log_message("send_info_briques: error in opening %s\n", PROC_ETAT_ZONES);
    //exit(1);
  }
  
  fscanf(etat_briques,"%d ",&nb_briques);
  fscanf(info_briques,"%d ",&aux);
  fscanf(etat_zones,"%d ",&aux);
  fscanf(etat_zones,"%d ",&aux);
  fscanf(etat_zones,"%d ",&aux);
  
  for (no_brique=0; no_brique < nb_briques; no_brique++) {
    fscanf(etat_briques,"%ld ", &(busy_time_brique[no_brique]));
    fscanf(etat_zones,"%d ", &(occupation_brique[no_brique]));
    fscanf(info_briques,"%d %s %d %d %d ",
	   &aux,
	   nom_brique[no_brique],
	   &(no_sci_brique[no_brique]),
	   &(taille_go_brique[no_brique]),
	   &(reste_mo_brique[no_brique]));
  }

  fclose(info_briques);
  fclose(etat_briques);
  fclose(etat_zones);

  calculer_charge_briques(busy_time_brique, charge_brique, nb_briques);

  // création du message
  len += sprintf(msg+len,"%d ",nb_briques);
  for (no_brique=0; no_brique < nb_briques; no_brique++) 
    len += sprintf(msg+len,"%d %d ",charge_brique[no_brique], occupation_brique[no_brique]);

  for (no_brique=0; no_brique < nb_briques; no_brique++) 
    len += sprintf(msg+len,"%d %s %d %d %d ", 
		   no_brique, 
		   nom_brique[no_brique], 
		   no_sci_brique[no_brique], 
		   taille_go_brique[no_brique],
		   reste_mo_brique[no_brique]);


  msg[len++] = '\0';

  aux = write(msg_sock,msg,strlen(msg));
  log_message("send_info_briques: envoi du msg %d bytes (nb ecrits = %d) : <%s>\n", strlen(msg), aux, msg);
}

void send_info_cluster(int msg_sock) {
  FILE *ftest;
  int cluster_ok = 1;
  char msg[BUFFER_MAX_SIZE+1];
  int len = 0;

  msg[len++] = 'o';

  if ((ftest=fopen(PROC_ETAT_ZONES,"r")) == NULL) {
    cluster_ok = 0;
    msg[len++] = 'n';
  }
  if (cluster_ok) fclose(ftest); 

  msg[len++] = '\0';
  write(msg_sock,msg,strlen(msg));
}

void handle_sock_msg(int msg_sock) {
	char buffer_reception[BUFFER_MAX_SIZE+1];
	//char buffer_emission[BUFFER_MAX_SIZE+1];
	//char sortie_log[LINE_MAX_SIZE+1];
	int nb_car; 
	char cmd;
	long arg1;
	long arg2;
	char commande[LINE_MAX_SIZE+1];
	
	nb_car=read(msg_sock,buffer_reception,BUFFER_MAX_SIZE+1);
	buffer_reception[nb_car]='\0';

	sscanf(buffer_reception,"%c %ld %ld",&cmd, &arg1, &arg2);

	if (cmd != 'e' && cmd != 'c') {
	  log_message("handle_sock_read : Nb car reçus = %d Chaine = %s",nb_car, buffer_reception);
	}	

	switch(cmd) {
	case 'a' : // ADD ZONE
	  //sprintf(commande,"echo \"%c %d\" > " PROC_VIRTUALISEUR "\n",cmd,arg1);
	  // FIXME : le chemin ne devrait pas être envoyé en dur mais passé en argument au lancement du démon
	  sprintf(commande,"/root/scimapdev_controleur_v1.5/smd_scripts/device_hd/add_zone %ld %ld\n",arg1,arg2*1024);
	  log_message("commande envoyée : %s",commande);
	  system(commande);
      	  break;
	case 'd' : // DELETE ZONE
	  //sprintf(commande,"echo \"%c %d\" > " PROC_VIRTUALISEUR "\n",cmd,arg1);
	  // FIXME : le chemin ne devrait pas être envoyé en dur mais passé en argument au lancement du démon
	  sprintf(commande,"/root/scimapdev_controleur_v1.5/smd_scripts/device_hd/del_zone %ld\n",arg1);
	  log_message("commande envoyée : %s",commande);
	  system(commande);
	  break;
	case 'r' : // RESIZE ZONE
	  sprintf(commande,"echo \"%c %ld %ld\" > " PROC_VIRTUALISEUR "\n",cmd,arg1,arg2);
	  log_message("commande envoyée : %s",commande);
	  system(commande);
	  break;
	case 'e':
	  // DEMANDE DE L'ETAT DU SSH
	  send_ssh_state(msg_sock);
	  break;
	case 'z':	  
	  if (arg1 == -1) 
	    // DEMANDE d'INFO SUR TOUTES LES ZONES
	    send_info_zones(msg_sock);
	  else
	    // DEMANDE d'INFO SUR UNE ZONE
	    send_info_zone(msg_sock, arg1);
	  break;
	case 'b':
	  // DEMANDE d'INFO SUR TOUTES LES BRIQUES
	  send_info_briques(msg_sock);
	  break;
	case 'g': // DEMARRAGE DU SSH (GO)
	  sprintf(commande,"/root/scimapdev_controleur_v1.5/smd_scripts/device_hd/smd_lancer_hd\n");
	  log_message("commande envoyée : %s",commande);
	  init_donnees(); // init des données pour mesures
	  system(commande);	  
	  break;
	case 's': // ARRET DU SSH (STOP)
	  sprintf(commande,"/root/scimapdev_controleur_v1.5/smd_scripts/device_hd/smd_arreter_hd\n");
	  log_message("commande envoyée : %s\n",commande);
	  system(commande);	  
	  break;
	case 'c': // EST-CE QUE LE CLUSTER FONCTIONNE
	  send_info_cluster(msg_sock);
	  break;
	default:
	  log_message("handle_sock_read : erreur - mauvaise commande");
	} 
}


int main()
{
	fd_set read_set;
	fd_set write_set;
	//int max_fd;
	int addrlen;
	struct sockaddr_in pin;  
	int msg_sock;
	
	init_donnees();
	daemonize();


	sock_fd=listen_socket_port();
	
	
	log_message("Attente de messages");	
	
	while(1) {	
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
		FD_SET(sock_fd,&read_set);
		if (select(sock_fd+1,&read_set,&write_set,NULL,NULL) <0) {
			fprintf(stderr,"smd_exportd : erreur dans le select des descripteurs : %s\n",strerror(errno));
			goto fail;
		}
		
			
		if (FD_ISSET(sock_fd,&read_set)) { // réception d'une demande d'importation de devices
			msg_sock=accept(sock_fd,(struct sockaddr *) &pin, &addrlen);
			if (msg_sock<0) {
				log_message("erreur pendant l'acceptation de la connexion");
				exit(ERROR);
			}
			handle_sock_msg(msg_sock);
			close(msg_sock);
		}
	}
	clean_up();
	return OK;
	
fail:
	return ERROR;
}


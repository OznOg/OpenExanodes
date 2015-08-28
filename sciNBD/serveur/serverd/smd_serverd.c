#include "constantes.h"
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <asm/types.h>
#include <sys/time.h>
#include <math.h>
#include "smdsv_client.h" 
#include "smdsv_divers.h"
#include "smd_server.h"


FILE *logfile=NULL;
struct smd_server smdsv;
char string_err[256]; //!< error sci 

extern int sci_cleanup(void);
extern void fermer_devices(void);
extern int sci_init(void);
extern void prep_signal(void);
extern void traitement_d_une_io(void);

void clean_up(void) {
  int retval;

  log_message("libération mémoire/SCI clients\n");  
  cleanup_clients();
  sci_cleanup(); 
  fermer_devices(); 

  retval = pthread_mutex_destroy(smdsv.mutex);
  if (retval == EBUSY) 
    log_error("destroying a busy mutex 'smdsv.mutex' !\n");

  closelog();
}

void sync_log(void) {
  int fd;
  fd=fileno(logfile);
  fsync(fd);
}

int init_serverd(void) {
  nullify_table((void *)smdsv.clients, NBMAX_CLIENTS);
  nullify_table((void *)smdsv.devices, NBMAX_DEVICES);
  smdsv.nb_io = 0;
  smdsv.nb_clients = 0;
  smdsv.nb_devices = 0;

  smdsv.mutex = malloc(sizeof(pthread_mutex_t));
  if (smdsv.mutex == NULL) {
    log_error("malloc struct pthread_mutex_t\n");
    return ERROR;
  }
  pthread_mutex_init(smdsv.mutex, NULL);

#ifdef PERF_STATS
  smdsv.somme_transferts = 0.0;
  smdsv.somme_durees = 0.0;
  smdsv.nb_transferts_sci =0;
  smdsv.nb_transferts_ecriture=0;
  smdsv.nb_transferts_lecture=0;
  smdsv.somme_debit_sci =0.0;
  smdsv.somme_debit_ecriture=0.0;
  smdsv.somme_debit_lecture=0.0;
  smdsv.somme_tailles_ecriture =0.0;
  smdsv.somme_tailles_lecture=0.0; 
#endif
  return SUCCESS;
}

void signal_handler(int sig) {
	switch(sig) {
	case SIGHUP:
	  log_message("hangup signal catched\n");
	  break;
	case SIGTERM:
	  log_message("terminate signal catched\n");
#         ifdef PERF_STATS
	  ecrire_stats();
#         endif
	  clean_up();
	  exit(EXIT_SUCCESS);
	  break;
	}
}

void daemonize() {
  int i,lfp;
  time_t t;
  char str[10];
  
  // test que le log est ok
  chdir(RUNNING_DIR); 
  logfile=fopen(LOG_FILE,"a");
  if(!logfile) return;
  fclose(logfile);

  time(&t);
  log_message("Starting daemon %s\n",ctime(&t));
 
  
  if(getppid()!=1) { // si le process n'est pas déjà un démon
    i=fork();
    if (i<0) exit(EXIT_FAILURE); /* fork error */
    if (i>0) exit(EXIT_SUCCESS); /* parent exits */
  }
  
  /* child (daemon) continues */
  setsid(); /* obtain a new process group */
  for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
  i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
  umask(027); /* set newly created file permissions */
  lfp=open(LOCKFILE_SERVERD,O_RDWR|O_CREAT,0640);
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
  signal(SIGALRM,signal_handler); 

}









int main(int arg,  char *argv[]) {  

  openlog(argv[0], LOG_PID, LOG_USER);
  mkdir(LOGREP, 0700);
  if (init_serverd() == ERROR) {
    log_error("erreur dans init du serveur\n");
    return EXIT_FAILURE;
  }
  log_message("serverd init done\n");

  daemonize();
  log_message("serverd daemonized\n");

  prep_signal();
  log_message("sig handler ready\n");
  
  if (sci_init() != SUCCESS) {
    log_error("erreur dans l'allocation des ressources SCI (sci_init)\n");
    return EXIT_FAILURE;
  }
  log_message("SCI initialisé\n");
  
  while (1) 
    pause(); // attente passive d'un SIGTERM etc..
  
}





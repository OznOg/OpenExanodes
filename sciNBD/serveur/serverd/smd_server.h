#ifndef __SMD_SERVER
#define __SMD_SERVER

#include "sisci_api.h" 
#include "constantes.h" 
#include <stdio.h>
#include <asm/types.h>
#include <pthread.h>

struct io_desc {
  __u64 no;
  __u32 cmd;
  __u64 byte; 
  __u32 nr_bytes;
  __u32 nodev;
};

typedef struct io_desc iodesc_t;

struct client {
  int status; //!< état du client
  int no_sci;
  unsigned long nb_io; // nb d'io traitees pour ce client
  pthread_t thread_io;
  pthread_t thread_connex;
  pthread_mutex_t *mutex_cxstate;
  pthread_cond_t cond_cxstate;
  int cxstate_changed;

  sci_desc_t vd_debio;
  sci_desc_t vd_finio;
  sci_desc_t vd_clready;
  sci_desc_t vd_errorio;
  sci_desc_t vd_ready;
  sci_desc_t vd_dataread;
  sci_desc_t vd_datareadtemp;  
  sci_desc_t vd_datawrite;
  sci_desc_t vd_ctl;

  // REMOTE (sur CLIENT)
  sci_remote_interrupt_t remote_fin_io_interrupt;
  sci_remote_interrupt_t remote_error_io_interrupt;
  sci_remote_interrupt_t remote_ready_interrupt;
  sci_remote_segment_t rs_data_read_segment;
  sci_map_t map_data_read_segment;
  volatile int* addr_data_read_segment;
  sci_sequence_t seq_data_read_segment;

  // LOCAL (sur ce SERVEUR)
  unsigned int interrupt_deb_io_no;
  sci_local_interrupt_t local_deb_io_interrupt;  
  unsigned int interrupt_clready_no;
  sci_local_interrupt_t local_clready_interrupt;  
  sci_local_segment_t local_ctl_segment;
  sci_local_segment_t local_data_write_segment;
  sci_local_segment_t local_read_temp_segment;
  sci_map_t local_ctl_map;
  sci_map_t local_data_write_map;
  sci_map_t local_read_temp_map;
  volatile int* local_ctl_address;
  volatile int* local_data_write_address;
  volatile int* local_read_temp_address;

};

typedef struct client client_t;

struct device {
  int fd;
  char name[MAXSIZE_DEVNAME];
  char path[MAXSIZE_DEVPATH];
};

typedef struct device device_t;

struct smd_server {
   int THIS_NODE_ID;
   unsigned long nb_io; // nb d'IO traitées par ce serveur
  
  int nb_clients; // nb de noeuds importateurs des devs exportés par ce serveur
  int nb_devices;
  client_t *clients[NBMAX_CLIENTS]; //  clients des devices
  device_t *devices[NBMAX_DEVICES]; //!< devices locaux sur lesquels portent les I/Os


#ifdef PERF_STATS
  double somme_transferts;
  double somme_durees;
  long nb_transferts_sci;
  long nb_transferts_ecriture;
  long nb_transferts_lecture;
  double somme_debit_sci;
  double somme_debit_ecriture;
  double somme_debit_lecture; 
  double somme_tailles_ecriture;
  double somme_tailles_lecture;
#endif

 pthread_mutex_t *mutex; //!< pour concurrence des threads sur les var stats et la var nb_io
};

extern struct smd_server smdsv;
extern char string_err[256]; //!< error sci 
extern FILE *logfile; //!< pour logger des traces de taille très importante (hors messages erreurs/debug). exemple : données lues et écrites à partir de ce noeud
#endif



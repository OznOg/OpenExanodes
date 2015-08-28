#ifndef _SCIMAPDEV_H
#define _SCIMAPDEV_H
/*!\file scimapdev.h
\brief ce fichier contient les d�clarations des prinpales structures de donn�es utilis�es dans scimapdev
*/

#include <linux/devfs_fs_kernel.h>
#include "constantes.h"
#include <genif.h>
#include <sci_types.h>
#include <linux/blk.h>


#define DEVICE_NR(device) MINOR(device)
#define DEVICE_REQUEST smd_request

struct servernode;
typedef struct servernode servernode_t;
extern int SMD_MAJOR_NUMBER; //!< major number de scimapdev (allou� dynamiquement par le noyau)


//! d�crit une requ�te d'E/S que le client envoie � un serveur
struct iodesc {
  u64 no; //!< n� de la requete
  u32 cmd; //!< lecture ou �criture ?
  u64 offset; //!< n� octet
  u32 size; //!< en octets
  u32 ndev; //!< n� du dev concern� par la requ�te (no sur le serveur)
};

typedef struct iodesc iodesc_t;

//! d�crit un networked device (i.e. un device lointain auquel acc�de scimapdev)
struct ndevice {
  servernode_t *sn; //!< serveur auquel appartient ce ndevice
  char name[MAXSIZE_DEVNAME]; //!< nom du device. Exemple : "disqueATA"
  u32 localnb; //!< no du dev sur le server (� passer dans chaque E/S)
  u64 size; //!< taille du ndev en Ko
  char rpath[MAXSIZE_PATHNAME];  //!< remote path. path du device sur son noeud (serveur). Exemple : "/dev/hda1"
  char lpath[MAXSIZE_PATHNAME];  //!< local path. path du device 'image' sur le client . Exemple : "/dev/scimapdev/sam1/disqueATA"
  int minor; //!< minor number de l'image du ndev
  devfs_handle_t dev_file; //!< fichier /dev par lequel une appli peut acc�der au ndevice 
  u64 nb_io; //!< nb d'E/S effectu�es sur ce ndevice depuis le chargement de scimapdev
  request_queue_t queue; //!< file d'attente des requ�tes E/S
  iodesc_t c_io; //!< desc de l'E/S en cours de traitement (current io)
};

typedef struct ndevice ndevice_t;

//! requ�te prise en compte par le snode. 
struct managedreq {
  u64 no;
  ndevice_t *nd;
  struct request *kreq; //!< req dans les structures du kernel
};

typedef struct managedreq managedreq_t;

//! noeud serveur utilis� par scimapdev. Ce noeud contient des networked devices sur lequelles arrivent les requ�tes d'E/S.
struct servernode {
  char name[MAXSIZE_DEVNAME]; //!< nom du snode. Exemple : "noeud1"
  long no_sci; //!< no sci
  ndevice_t *ndevs[NBMAX_NDEVS]; //!< tableau des ndevs de ce server node
  int busy; //!< indique si le snode est en train de traiter une E/S
  int status; //!< indique l'�tat de la connexion SCI avec le snode
  u32 maxsize_req; //!< taille max (en secteurs) d'une req d'E/S CLUSTEREE entre le serveur et le client ( = taille DATA seg SCI)
  managedreq_t creq; //!< req en cours (current req) de traitement par le snode
  
  
  // SEGMENT DATA READ (LOCAL)
  sci_l_segment_handle_t ls_data_read_handle;  //!< handle du segment local servant � transf�rer des data pour les lectures
  vkaddr_t ls_data_read_addr; //!< m�moire locale dans laquelle on �crit les donn�es avant les envoyer..
  
  // SEGMENT DATA WRITE (REMOTE)
  sci_r_segment_handle_t rs_data_write_handle;  //!< handle du segment distant servant � transf�rer des data pour les �critures
  sci_map_handle_t mp_data_write_handle;
  vkaddr_t rs_data_write_addr; //!< m�moire locale dans laquelle on �crit les donn�es avant les envoyer..
	
  // SEGMENT CTL (REMOTE)
  sci_r_segment_handle_t rs_ctl_handle; //!< handle du segment qui sert � transmettre les infos que la requ�te a effectue
  sci_map_handle_t mp_ctl_handle;
  vkaddr_t rs_ctl_addr; //!< m�moire locale dans laquelle on �crit les infos de controle avant les envoyer..

  // IT BEGIN IO  (REMOTE)
  sci_r_segment_handle_t rs_shad_begio_handle; 
  volatile vkaddr_t rs_shad_begio_addr;
  sci_map_handle_t mp_shad_begio_handle;
  sci_r_interrupt_handle_t rit_begio_handle;	

  // IT CLREADY  (REMOTE)
  sci_r_segment_handle_t rs_shad_clready_handle; 
  volatile vkaddr_t rs_shad_clready_addr;
  sci_map_handle_t mp_shad_clready_handle;
  sci_r_interrupt_handle_t rit_clready_handle;	

  // IT END IO (LOCAL)
  sci_l_segment_handle_t ls_shad_endio_handle; //<!  handle du segment qui contient le n� de l'IT que le serveur doit utiliser pour signaler qu'elle a fini de traiter la req
  sci_l_interrupt_handle_t lit_endio_handle;	
  u32 no_it_endio;
  
  // IT READY (LOCAL)
  sci_l_segment_handle_t ls_shad_ready_handle; //<!  handle du segment qui contient le n� de l'IT que le serveur doit utiliser pour signaler qu'elle a fini de traiter la req
  sci_l_interrupt_handle_t lit_ready_handle;	
  u32 no_it_ready;
  

  // IT ERROR IO (LOCAL)
  sci_l_segment_handle_t ls_shad_errorio_handle; //<!  handle du segment qui contient le n� de l'IT que le serveur doit utiliser pour signaler qu'elle a fini de traiter la req
  sci_l_interrupt_handle_t lit_errorio_handle;	
  u32 no_it_errorio;

  int session_sci_opened;
  
  // pour les stats du /proc
  unsigned long nb_reads; //!< nb de requ�tes READ trait�es depuis l'importation du snode
  unsigned long nb_writes; //!< nb de requ�tes WRITE trait�es 
  unsigned long long request_bytes_transfered; //!< nb d'octets transf�r�s entre les buffers <-> device image, depuis la cr�ation de l'image du ndevice

#ifdef PERF_STATS
  struct timeval debut_transfert_disque; 
  struct timeval fin_transfert_disque; 
  u64 somme_durees_disque; // en micro-secondes
  long nb_transferts_disque; 
  struct timeval debut_transfert_PIO; 
  struct timeval fin_transfert_PIO; 
  unsigned long taille_transfert_PIO; // en octets
  u64 somme_tailles_PIO; // en octets
  u64 somme_durees_PIO; // en microsecondes
  struct timeval debut_busy; 
  struct timeval fin_busy; 
  u64 busy_time; // temps cumul� de travail de snode (en microsecondes)
#endif

  devfs_handle_t dev_rep;
};

//! structure de donn�es qui contient l'�tat de scimapdev (i.e les serveurs de devices enregistr�s)
struct smdstate {
  servernode_t *snodes[NBMAX_SNODES]; //!< tableau des server nodes utilis� par scimapdev
  devfs_handle_t dev_rep; //!< r�pertoire /dev/ utilis� par scimapdev
  u32 nb_snodes; //!< nombre de server nodes en cours d'exploitation
 u64 nb_req; //!< nb de requ�tes trait�es par scimapdev depuis son chargement en m�moire

 struct proc_dir_entry *proc_rep; //!< r�pertoire /proc utilis� par scimapdev
 struct proc_dir_entry *proc_ndevs; //!< r�p /proc dans lequels on place les fichier d'info sur les ndevices exploit�s par scimapdev

#ifdef PERF_STATS
  struct timeval debut_busy; 
  struct timeval fin_busy; 
  u64 busy_time;
#endif

  sci_device_info_t sdip; //!< utilis� par START/CHECK_SEQUENCE
  sci_adapter_sequence_t sci_sequence;

  u32 dataseg_size; //!< taille du DATA segment SCI 
  u32 ctlseg_size; //!< taille du CTL segment SCI 
  u32 no_sci; //!< no sci du noeud sur lequel s'ex�cute ce module
};

extern struct smdstate smd;

// tableaux permettant de retrouver les ndevices ou server nodes associ�s � un n� d'IT, ou � un MINOR NUMBER.
extern ndevice_t *minor2ndev[NBMAX_NDEVS];  
extern servernode_t *itendio2snode[NBMAX_IT];
extern servernode_t *iterrorio2snode[NBMAX_IT];
extern servernode_t *itready2snode[NBMAX_IT];
#endif

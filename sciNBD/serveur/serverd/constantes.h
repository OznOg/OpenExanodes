#ifndef _localconstantes_h
#define _localconstantes_h

#include <syslog.h>
#include "../include/constantes.h"

#ifdef DEBUG_MAQUETTE
#define log_message(fmt, args...) \
{ \
syslog(LOG_DEBUG, " %s():%d - " fmt, \
       __FUNCTION__, \
       __LINE__, ## args);  } 
#else
#define log_message(fmt, args...) \
{ } // on n'affiche rien si on ne d�bugge pas
#endif


#ifdef DEBUG_AFFICHER_DATA 
#define log_donnees(offset, donnees, taille) \
 afficher_donnees(offset, donnees, taille)
#else
#define log_donnees(offset, donnees, taille) // on n'affiche rien si on ne d�bugge pas
#endif

#define log_error(fmt, args...) \
{ \
syslog(LOG_ERR, " %s():%d - ERROR: " fmt, \
       __FUNCTION__, \
       __LINE__, ## args);  }


#define LOCKFILE_SERVERD	"/var/lock/smd_serverd.lock"

#define LOGREP          "/var/log/storagency"
#define LOG_FILE	"/var/log/storagency/smd_serverd.log" // pour afficher les donn�es
#define STAT_FILE       "/var/log/storagency/smd_serverd.stat"

 
// donne l'�tat de la connexion avec le client
#define UP              6   // connexion valide : les I/O s'effectuent
#define DO_DECONNEXION  5   // en cours de d�connexion car on a d�tect� un pb
#define DO_SENDREADY    4
#define DO_CONNEXION    3
#define SUSPEND         2   // connexion suspendue. wait retour � la normale
#define DOWN            1   // connexion ferm�e, mais peut �tre relanc�e
#define INSANE          0   // la connexion est dans un �tat incoh�rent => on ne peut plus l'utiliser ni la relancer => on doit tenter une autre deconnexion


#define BUFFER_MAX_SIZE 100
#define SMD_BLKSIZE 1024 // attention : doit �tre �gale � la valeur entr�e dans le fichier constantes.h du smd_bdd (FIXME : quand l'ensemble du code sera termin� : faire en sorte que cette valeur soit prise directement de l'include du smd_bdd)

// commandes d' E/S qui arrivent sur smd_transfertd
#define READ 0
#define WRITE 1

#define MAXSIZE_DEVNAME 16 //!< taille max d'un nom de device (ex : disqueATA)
#define MAXSIZE_DEVPATH 200 //!< taille max d'un path (ex: /dev/hda6)
#define NBMAX_DEVICES NBMAX_EDEVS//!< nb max devices que peut exporter ce serveur

// constantes pour SCI
#define NBMAX_CLIENTS 16     // nb max de clients qui peuvent importer les devices de ce server


// IDENT 'RELATIFS' DES SEGMENTS MEMOIRE et IT SCI
#define IT_BEG_IO 0          // SERVEUR : IT d�but d'E/S. 
#define CTL_SEG_ID 1         // SERVEUR : "segment contr�le" 
#define DATA_WRITE_SEG_ID 2  // SERVEUR : "segment pour les donn�es-�criture" 
#define DATA_READ_SEG_ID  3  // CLIENT :  "segment pour les donn�es-lecture" 
#define READ_TEMPSEG_ID 4    // SERVEUR : "segment interm�diaire pour lecture"
#define IT_END_IO 5          // CLIENT : IT fin E/S
#define IT_ERROR_IO 6        // CLIENT : IT erreur E/S 
#define IT_READY 7           // CLIENT : serveur pr�t � recevoir des E/S
#define IT_CLREADY 8         // SERVEUR : client est pr�t � recevoir une connexion � ses ressources SCI

#define CTL_SEG_SIZE 1024    // octets
#define DATA_SEG_SIZE 262144 // doit �tre sup�rieure ou egale � la taille d'un bloc du smd

#define ADAPTER_NO 0         // n� de l'adapter SCI (local et remote) 
#define NO_FLAGS 0 
#define NO_ARG 0
#define NO_CALLBACK 0

#define NB_VC_TO_OPEN 20 // ouverture de 20 virtual channels SCI pour g�rer les segment/mapping et it

// pour indiquer le type du segment
#define NO_DMA 0 // PIO mode
#define DMA 1  // DMA mode
#endif



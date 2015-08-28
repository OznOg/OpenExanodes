/*!\file constantes.h
\brief ce fichier contient les constantes principales utilisées par scimapdev.o
*/
#ifndef _constantes_h
#define _constantes_h

#define MODULE_NAME "scimapdev"
#define MODULE_VERSION "1.5"

#define SMD_DEVREP MODULE_NAME //!< nom du répertoire /dev pour scimapdev 

// constantes pour le BDD scimapdev et son intégration au noyau
#define SMD_READAHEAD 8 //!< en SECTEURS (et pas blocs) préfetchés lors de chaque lecture sur le device  (normal = 2)
#define SMD_BLKSIZE 1024 //!< taille bloc en octets
#define SMD_HARDSECT 512 //!< taille secteur en octets

// constantes limitant le nb de 'server nodes' ou de 'network devices'
// que scimapdev peut gérer sur 1 noeud client.
#define NBMAX_NDEVS 256 //!< nb max de devices gérés par le smd
#define NBMAX_SNODES 16	//!< nb max de noeud serveurs gérés par le smd
#define NBMAX_IT 256 //!< no max d'une IT GENIF SCI (limite du nb de serveurs auxquels le client peut accéder =  NBMAX_IT / 2)

// IDENT 'RELATIFS' DES SEGMENTS MEMOIRE et IT SCI
#define IT_BEG_IO 0          //!< SERVEUR : IT début d'E/S. 
#define CTL_SEG_ID 1         //!< SERVEUR : segment contrôle
#define DATA_WRITE_SEG_ID 2  //!< SERVEUR : segment pour les données-écriture
#define DATA_READ_SEG_ID 3   //!< CLIENT : segment pour les données-lecture 
#define READ_TEMPSEG_ID 4    //!< SERVEUR : segment intermédiaire pour lecture
#define IT_END_IO 5          //!< CLIENT : IT fin E/S
#define IT_ERROR_IO 6        //!< CLIENT : IT erreur d'E/S signalée par le cl
#define IT_READY 7           //!< CLIENT : IT levée par le serveur lorsqu'il est prêt à recevoir nos requêtes 
#define IT_CLREADY 8         //!< SERVEUR : IT que le client lève lorsque ses ressources SCI pour communiquer avec le serveur sont prêtes 
   
// taille des segment mémoire partagée de contrôle et de données 
#define CTL_SEG_SIZE 1024    //!< en octets
#define DATA_SEG_SIZE 262144 //!< doit être supérieure ou egale à la taille d'un bloc du smd

// constantes pour communiquer avec l'API GENIF
#define NO_CALLBACK 0
#define ADAPTER_NO 0
#define NO_CALLBACK 0
#define NO_ARG 0
#define NO_FLAGS 0
#define SYSCALL_MODULE_ID 0x8880
#define SYSCALL_INTFLAG_MODULE_ID 0x8882

// état de la connexion avec le serveur
#define UP                    6
#define CONNEXION             5
#define SUSPEND               4
#define DECONNEXION           3
#define DECONNEXION_CAN_SLEEP 2
#define DOWN                  1
#define INSANE                0

// état du snode (phase du traitement de la creq ou IDLE)
#define BUSY_ENDIO            3
#define WAIT_ENDIO            2
#define BUSY_BEGIO            1
#define IDLE                  0


#define ENDREQ_SUCCESS 1 //!< le traitement de l'E/S est un succès
#define ENDREQ_ERROR 0 //!< le traitement de l'E/S est un échec

// le do.. while est là pour puvoir faire un bloc 
// utilisable dans un if.. else
#ifdef DEBUG_MAQUETTE
#define PDEBUG(fmt, args...) \
 do { \
  printk(KERN_EMERG "%s:%d - %s: ",__FILE__,__LINE__,__FUNCTION__); \
  printk(fmt, ## args); } \
 while(0)
#define PERROR(fmt, args...) \
 do { \
  printk(KERN_ERR "%s:%d (%s): ERROR - ",__FILE__,__LINE__,__FUNCTION__); \
  printk(fmt, ## args); } \
 while(0)
#else
#define PDEBUG(fmt, args...) 
#define PERROR(fmt, args...) 
#endif
	
#define SUCCESS 0
#define FAILURE 1
#define ERROR 1

#define TRUE  1
#define FALSE 0

#define MAXSIZE_PATHNAME 200 //!<  max size d'un path de dev (ex: /dev/hda1)
#define MAXSIZE_DEVNAME 16 //!< max size d'un nom de dev (ex: disqueATA);
#define TAILLE_LIGNE_CONF 200 //!< taille d'une ligne de configuration (passée à vers le fichier /proc/scimapdev/briques_enregistrees)
#endif


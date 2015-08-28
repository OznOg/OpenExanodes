/*!\file constantes.h
\brief ce fichier contient les constantes principales utilisées pour le virtualiseur
*/

#ifndef _constantes_h
#define _constantes_h

// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// 
// Ces 3 constantes doivent être synchronisées avec le fichier 
// vrt_superblock.h du répertoire "vrtadm"
#define NBMAX_RDEVS 128	//!< Nb max de real devices (attention s'il y a trop de rdev, le superbloc Z risque d'exploser (16 octets/rdev))
#define NBMAX_ZONES 256	//!< NBMAX_ZONES-1 est également la valeur max d'un minor number (attention si y a trop de zones, le superbloc group risque d'exploser (8 octets/zone)
#define NBMAX_GROUPS 32 //!< nb groupes que le virtua peut gérer simultanément 
#define VRT_UESIZE 16 //!< taille UE = 16 Ko

#define MAXSIZE_LAYNAME 16 //!< taille nom du placement = 16 car max
#define MAXSIZE_GROUPNAME 16 //!< taille nom du groupe = 16 car max
#define MAXSIZE_ZONENAME 16 //!< taille nom de la zone = 16 car max
#define MAXSIZE_DEVNAME 100 //!< taille nom d'un device /dev/.. etc


#define VRT_VERSION 125 //!< this version = 1.2.5
#define MODULE_VERSION "1.2.5"
#define MODULE_NAME "virtualiseur"

#define NB_MAX_UE_ZONE 100000000 //!< nombre max d'UE par zone
#define NB_MAX_UE_BRIQUE 100000000 //!< nombre max d'UE par brique

#define VRT_READAHEAD 128 //!< 2 SECTEURS (et pas blocs) préfetchés lors de chaque lecture sur le device
#define VRT_BLKSIZE 1024 //!< blocs de taille 1024 octets
#define VRT_HARDSECT 512 //!< secteur de taille 512 octets
#define VRT_PLAGESIZE 131072 //<! en Ko. taille plage = 128 Mo
#define VRT_SBSIZE 4096 // en octets
//#define VRT_LBSIZE_SSTRIPING 4096 // en octets

///// ATTENTION !! ATTENTION !! ATTENTION !! ATTENTION !! 
// constante à synchroniser avec cette du fichier vrtcommands.h du 
// répertoire "vrtcommandes"
#define MAXSIZE_DEVNAME 100 //!< taille nom d'un device /dev/.. etc
#define TAILLE_LIGNE_CONF 100 //!< taille d'une ligne de configuration (passée à vers le fichier /proc/virtualiseur/zones_enregistrees)
#define SSTRIPING_LAYOUT 0x01 //!< code du placement SSTRIPING
#define SSTRIPING_NAME "sstriping" //!< nom du placement SSTRIPING

#define ERROR 1
#define SUCCESS 0

#define TRUE 1
#define FALSE 0

typedef enum {false=0, true} bool; // hum.. va falloir choisir entre les 2 implémentations

#define NON_NULL (NULL+1) //!< par opposition à NULL
#define ZONE_NON_INIT  NON_NULL //!< marqueur d'une zone existante mais donc la struct n'a pas encore été créée dans g->zones[]

#ifdef DEBUG_MAQUETTE
#define PDEBUG(fmt, args...) \
 do { \
  printk(KERN_EMERG "%s:%d (%s): ",__FILE__,__LINE__,__FUNCTION__); \
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
	
#endif

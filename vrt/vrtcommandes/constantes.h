#ifndef _CONSTANTES_H
#define _CONSTANTES_H

#define READ 0
#define WRITE 1

#define FALSE 0
#define TRUE 1

#define BASIC_SSIZE 25 //taille pour une string de base
#define LINE_SIZE 256 // taille d'une ligne de texte (pour commande, etc..)
 
#define PROC_VIRTUALISEUR "/proc/virtualiseur/zones_enregistrees"
#define VRT_GPROCPATH "/proc/virtualiseur/groups"
#define VRT_PROCAZ "active_zones"
#define VRT_PROCIZ "idling_zones"
#define VRT_PROCUZ "used_zones"
#define CONF_FILE "/etc/vrtadm.conf"

// pour rechercher les rdevs importés par notre nbd
#define NBD_DIRNAME "/dev/scimapdev"
#define NBD_DEVMOTIF "brique"

#define MAXSIZE_ZONENAME 16 //!< taille nom de la zone = 16 car max
#define NBMAX_ZONES 256 //!< nb max zones
#define NBMAX_RDEVS 128 //!< nb max real devices
#endif

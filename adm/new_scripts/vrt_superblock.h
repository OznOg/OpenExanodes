#ifndef __VRT_SUPERBLOCK
#define __VRT_SUPERBLOCK

#include <asm/types.h>
#include "../../vrt/virtualiseur/constantes.h"
//#include "constantes.h"

// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// 
// Ce fichier doit être synchronisé avec le fichier 
// vrt_superblock.h du répertoire "vrtadm"

// NEW
#define VRT_SBSIZE 4096 // en octets

#define SBG_MAGIC           0x6DCA6E8E
#define SBR_MAGIC           0x7B9120A1
#define SBZ_SSTRIPING_MAGIC 0x1EBB790D

#define NAME_MAX_SZ 16 //!< taille max (en char) d'un nom de groupe ou zone

struct sb_group { 
  // info constante 
  __u32 rdev_uuid[4]; //!< UUID du dev qui contient ce SBG
  __u32 magic_number; //!< pour reconnaitre un sb brique
  __u32 vrt_vers; //!< version du virtualiseur qui a crée ce sb
  __u32 uuid[4];
  __u32 thisdev_uuid[4]; //!< nom du dev qui contient ce SBG
  __u8  gname[NAME_MAX_SZ]; //!< nom du groupe
  __u32 create_time; //!< date de la création 
  __u8  layout; //!< code layout utilisée dans le groupe

  // info variable 
  __u32 checksum; //!< checksum du superblock
  __u32 update_time; //!< date de la dernière maj de ce superbloc
  __u32 nb_zones; //!< nb de zones crées
  __u32 nb_rdevs; //!< nb de rdevs utilisés par ce groupe
  __u8 zone_exist[NBMAX_ZONES]; //!< indique si la zone i existe (TRUE/FALSE)
};

struct sb_rdevs { 
  __u32 magic_number;
  __u32 checksum;
  // 16 octets par rdevs (max = 255 rdevs / group) 
  __u32 uuid_rdevs[NBMAX_RDEVS][4]; // UUID des realdevs du group
};

// superbloc pour une zone (dupliqué sur chaque rdev)
#define NB_ETENDUES 127 
struct sb_zone_sstriping { 
  // info constante (64 octets)  
  __u32 magic_number; 
  __u8  name[NAME_MAX_SZ]; //!< nom de la zone
  __u32 zone_uuid[4];
  __u32 create_time;
  __u32 pad1[3];

  // info variable (32 octets)
  __u32 update_time;
  __u64 zone_size; //!< capa de stockage de la zone en KB
  __u32 nb_etendues;
  __u32 pl_start[NB_ETENDUES];
  __u32 pl_end[NB_ETENDUES];
};

extern inline __u64 r_offset_sbg(void);
extern inline __u64 r_offset_sbr(void);
extern inline __u64 offset_end_rdev(__u64 size_kb /*en Ko */);
extern inline __u64 r_offset_z_sstriping(int indice_zone);


#endif

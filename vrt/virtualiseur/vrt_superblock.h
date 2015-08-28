#ifndef _SUPERBLOCK_H
#define _SUPERBLOCK_H
#include <asm/types.h>
#include "constantes.h"
#include "virtualiseur.h"

extern int vrt_write_SBG(group_t *g);

// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// ATTENTION !!! ATTENTION !!! ATTENTION !!! ATTENTION !!!
// 
// Ce fichier doit être synchronisé avec le fichier 
// vrt_superblock.h du répertoire "vrtadm"

// NEW
#define SBG_MAGIC           0x6DCA6E8E
#define SBR_MAGIC           0x7B9120A1
#define SBZ_SSTRIPING_MAGIC 0x1EBB790D

#define NAME_MAX_SZ 16 //!< taille max (en char) d'un nom de groupe ou zone

struct sb_group { 
  // info constante 
  u32 rdev_uuid[4]; //!< UUID du dev qui contient ce SBG
  u32 magic_number; //!< pour reconnaitre un sb brique
  u32 vrt_vers; //!< version du virtualiseur qui a crée ce sb
  u32 uuid[4];
  u32 thisdev_uuid[4]; //!< nom du dev qui contient ce SBG
  // quelle utilité ?
  u8  gname[NAME_MAX_SZ]; //!< nom du groupe
  u32 create_time; //!< date de la création 
  u8  layout; //!< code layout utilisée dans le groupe

  // info variable 
  u32 checksum; //!< checksum du superblock
  u32 update_time; //!< date de la dernière maj de ce superbloc
  u32 nb_zones; //!< nb de zones crées
  u32 nb_rdevs; //!< nb de rdevs utilisés par ce groupe
  u8 zone_exist[NBMAX_ZONES]; //!< indique si la zone i existe (TRUE/FALSE)
};

struct sb_rdevs { 
  u32 magic_number;
  u32 checksum;
  // 16 octets par rdevs (max = 255 rdevs / group) 
  u32 uuid_rdevs[NBMAX_RDEVS][4]; // UUID des realdevs du group
};

// superbloc pour une zone (dupliqué sur chaque brique)
#define NB_ETENDUES 127 
struct sb_zone_sstriping { 
  // info constante (64 octets)  
  u32 magic_number; 
  u8  name[NAME_MAX_SZ]; //!< nom de la zone
  u32 zone_uuid[4];
  u32 create_time;
  u32 pad1[3];

  // info variable (32 octets)
  u32 update_time;
  u64 zone_size; //!< capa de stockage de la zone en KB
  u32 nb_etendues;
  u32 pl_start[NB_ETENDUES];
  u32 pl_end[NB_ETENDUES];
};










/*OLD

#define SB_BR_MAGIC 0x6DCA6E8E
#define SB_BZ_MAGIC 0x3895204C

#define SBB_PAD1_SZ 9
#define SBB_PAD2_SZ 6
#define NAME_MAX_SZ 16 //!< taille max (en char) d'un nom de groupe ou zone

struct sb_brique { // structure d'un superbloc pour une brique
  // info constante (64 octets)
  u32 magic_number; // pour reconnaitre un sb brique
  u32 vrt_vers; // version du virtualiseur qui a crée ce sb
  u32 brique_uuid[4];
  u32 create_time; // date de la création 
  u32 pad1[SBB_PAD1_SZ];

  // info group (TEMPORAIRE)
  u8 gname[NAME_MAX_SZ]; // nom du groupe
  u8 layout; // layout utilisée dans le groupe

  // info variable (32 octets)
  u32 sb_checksum;
  u32 update_time; // date de la dernière maj de ce superbloc
  u32 nb_zones; // nb de zones crées
  u32 pad2[SBB_PAD2_SZ];

  // info pour chaque zone (8 octets par zones)
  u64 pos_sbz_r[NB_MAX_ZONES]; // pos du sb de la zone (par rapport à l'offset du sbb)
};

struct sb_zone { // superbloc pour une zone (dupliqué sur chaque brique)
  // info constante (64 octets)  
  u32 magic_number; 
  u32 vrt_vers; // version du virtualiseur qui a crée ce sb
  u32 zone_uuid[4];
  u32 z_type;
  u32 create_time;
  u32 nb_briques;
  u64 zone_size; // sans compter les blocs de meta données (en Ko)
  u64 zone_raw_size; // NON UTILISE pour l'instant  
  u32 pad1[3];

  // info variable (32 octets)
  u32 update_time;
  u32 nb_layout_blocks;
  u64 first_lb_r; // offset du premier lb
  u32 pad2[5];
  
  //info pour chaque brique qui contient une partie de la zone
  u32 brique_uuid[NB_MAX_BRIQUES][4];
};

#define NB_ETENDUES 127 // on peut pousser jusqu'à 510 pour un bloc de 4096 octets 
#define LB_SSTRIPING_MAGIC 0x1EBB790D

//! structure d'un layout bloc pour le placement sstriping
struct lb_sstriping {
  u32 magic_number;
  u32 create_time;
  u32 nb_etendues;
  u32 pl_start[NB_ETENDUES];
  u32 pl_end[NB_ETENDUES];
};

*/

#endif

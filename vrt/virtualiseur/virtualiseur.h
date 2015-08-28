#ifndef _VIRTUALISEUR_H
#define _VIRTUALISEUR_H
/*!\file virtualiseur.h
  \brief ce fichier contient les déclarations des prinpales structures de données utilisées dans Virtualiseur
*/

#include <linux/devfs_fs_kernel.h>
#include "constantes.h"

#define DEVICE_NR(device) MINOR(device)
#define DEVICE_REQUEST vrt_request
extern int VRT_MAJOR_NUMBER;

struct plage_sstriping;
typedef struct plage_sstriping plage_sstriping_t;

struct group;
typedef struct group group_t;

union z_layout;
typedef union z_layout z_layout_t;
struct realdev;
typedef struct realdev realdev_t;
struct storzone;
typedef struct storzone storzone_t;
struct z_sstriping;
typedef struct z_sstriping z_sstriping_t;
union g_layout;
typedef union g_layout g_layout_t;
struct g_sstriping;
typedef struct g_sstriping g_sstriping_t;



struct plage_sstriping {
  u64 no_stripe_debut; //! no de la première stripe de la plage (% au n° stripe SSH entier)
  u64 no_stripe_fin; //! no de la derniere stripe de la plage (idem)
  char free; //! free == TRUE => la plage est attribuée à aucune zone
  char largeur; //! nb d'UE dans les stripes de la plage (une plage contient des UE de meme taille)
  u64 hauteur; //! nb de stripes dans la plage 
};

struct z_sstriping {
  u32 *plages; //!< no des plages utilisées par la zone
  u32 nb_plages; //!< nb de plages utilisées par la zone  
};

union z_layout {
  z_sstriping_t *sstriping;
};

struct g_sstriping {
  plage_sstriping_t **plages; //!< info sur les plages du groupe
  u64 nb_plages; //!< nb plages stockables par le groupe
  u8 sorted_devs[NBMAX_RDEVS]; //!< minor nb des devs triés par taille croissante des devs
};

union g_layout {
  g_sstriping_t *sstriping;
};

struct group {
  char name[MAXSIZE_GROUPNAME]; //!< nom du groupe
  bool active; //!< groupe actif ? ie. est-ce qu'il accepte les opération sur les vl ?
  u32 uuid[4];
  u32 nb_realdevs; //!< nb de devices dans le groupe
  u32 nb_zones; //!< nb zones créées sur le groupe
  
  realdev_t *realdevs[NBMAX_RDEVS];
  storzone_t *zones[NBMAX_ZONES];
  g_layout_t lay; //!< métadata de base pour placer les blocs des zones sur les devices
  devfs_handle_t dev_rep; //! repertoire de dev du groupe
  struct proc_dir_entry *proc_rep; //! répertoire proc du groupe
  struct proc_dir_entry *proc_az; //! liste zones actives
  struct proc_dir_entry *proc_iz; //! liste zones oisives
  struct proc_dir_entry *proc_uz; //! liste zones en cours d'utilisation
  
  // fonction de placement
  int (*init_layout)(group_t *g); //!< création des structures de données du placement
  void (*clean_layout)(group_t *g); //!< destruction des structures de données du placement du group 'g' et de ses zones
  void (*clean_layout_zone)(storzone_t *z); //!< destruction des structures de données du placement de la zone 'z'

  u64 (*min_rdev_size)(void); //!< renvoie la taille min en KB que doit avoir un rdev
  int (*layout_type)(void); //!< renvoie le type de la layout
  char* (*layout_name)(void); //!< renvoie le nom de la layout
  u64 (*usable_cs)(group_t *g); //!< renvoie la CS exploitable sur le groupe en KB
  u64 (*used_cs)(group_t *g); //!< renvoie la CS déjà consommée 
  int (*init_zone_layout)(group_t *g, storzone_t *z); //!< init layout zone
  int (*resize_layout_zone)(storzone_t *z, u64 newsize);  //!< retaille la layout zone
  u64 (*r_offset_sbz)(storzone_t *z); //!< renvoie l'offset relatif du SB ZONE
  //!< génère et écrit le SB Z sur le rdevice 'rd'
  int (*create_and_write_sb_zone)(group_t *g,
				  storzone_t *);  
  //!< convertit une pos sur une zone en pos sur un rdev 
  void (*zone2rdev)(storzone_t *z, 
		    unsigned long zsector, 
		    realdev_t **rd, 
		    unsigned long *rdsector);
  //!< lit les SBZ du groupe et reconstitue les struct + layout zones
  int (*read_sbz_and_rebuild_lay)(group_t *g); 
  //!< calcule la CS utilisée par les zones sur les rdevs du groupe
  void (*calculate_capa_used)(group_t *g); 
};

struct realdev {
  group_t *g; //!< pointeur vers le groupe auquel appartient ce device 
  u32 uuid[4];
  char name[MAXSIZE_DEVNAME]; //!< nom du dev. ex:/dev/scimapdev/sam1/disqueATA
  u64 size; //!< en Ko
  u8 major; //!< major number du device
  u8 minor; //!< minor number du device
  u64 capa_used; //!< en Ko. CS allouée à des zones.
};


struct storzone {
  group_t *g; //!< pointeur vers le groupe auquel appartient cette zone  
  bool active; //!< zone active ou pas ? active = elle peut traiter des requêtes d'E/S
  int used; //!< donne la différence entre le nb d'open et de close sur le dev
  char name[MAXSIZE_ZONENAME]; //!< nom de la zone
  u32 uuid[4];
  u64 size; //!< en Ko
  z_layout_t lay; //! placement pour cette zone (métadonnées)
  int minor; //!< minor number de cette zone ('int' pour mettre -1)
  devfs_handle_t dev_file; //! fichier /dev de la zone
};

struct vrt_state {
  devfs_handle_t dev_rep; //!< repertoire de dev du virtualiseur
  struct proc_dir_entry *proc_rep; //!< répertoire procdu virtualiseur
  struct proc_dir_entry *gproc_rep; //!< répertoire proc pour les groupes

  group_t *groups[NBMAX_GROUPS]; //!< groupes actif 
  u32 nb_groups;
  
  storzone_t *minor2zone[NBMAX_ZONES]; //!< permet d'obtenir le un ptr sur la zone sous-jacente à un minor number. == NULL si le minor number n'a pas utilisé
  u64 request_nb; //!< numéro de la dernière requête que le virtualiseur à commencer à traiter
};

extern struct vrt_state vs;

#endif

#ifndef _constantes_h
#define _constantes_h

#define PROC_VIRTUALISEUR "/proc/virtualiseur/zones_enregistrees"
#define RUNNING_DIR				"/tmp"
#define LOCK_FILE					"smd_commandd.lock"
#define LOG_FILE					"smd_commandd.log"
#define BUFFER_MAX_SIZE 	16*1024	// ces 2 valeurs d�signent..
#define LINE_MAX_SIZE 		256	// ..des tailles max de cha�nes de car pour le parsing des msgs entre les processus
#define SOCKET_PORT 			0x7654
#define OK 0
#define ERROR -1 	
 
// FIXME : ces constantes reviennent dans plusieurs composants de mon 
// soft => regrouper �a dans un fichier unique pour tous les composants..
// (une fois que la structures des r�pertoire du code source sera bien
// fix�e)
#define TAILLE_MAX_NOM_DEV 200 //!< taille max d'un nom (pour le virt et scimapdev)
#define NB_MAX_BRIQUES 16       //!< Nb max de briques
#define NB_MAX_ZONES 16 //!< Nb max de zones
#define NB_MAX_MESURES 30 //!< Nb max de mesures sur lesquelles on calcule la moyenne des charges, mo/s, io/s, .. etc

#define PROC_ETAT_BRIQUES "/proc/scimapdev/data_etat"
#define PROC_ETAT_ZONES "/proc/virtualiseur/data_etat"
#define PROC_BRIQUES "/proc/scimapdev/data_briques"
#define PROC_ZONES "/proc/virtualiseur/data_zones"

#define MODE_BLOC 1
#define MODE_FICHIER 2

#endif

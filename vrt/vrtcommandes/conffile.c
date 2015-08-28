#include "constantes.h"


//! ajoute le group dans le fichier de configuration. 
/*! 
  Format ligne ajoutée : 
  GROUP_NAME UUID_GROUP NB_REALDEVICES UUID_RDEV(1) UUID_RDEV(2).. UUID_RDEV(NB_REALDEVICES)
 */
// TODO: - lecture du /proc/virtualiseur/groups/GROUP_NAME
//       - contrôle que les données sont au bon format
//       - écriture en tête du fichier de conf
int add_group_conf_file(char *group_name) {
  return TRUE;
}

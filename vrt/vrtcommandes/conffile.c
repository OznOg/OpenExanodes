#include "constantes.h"


//! ajoute le group dans le fichier de configuration. 
/*! 
  Format ligne ajout�e : 
  GROUP_NAME UUID_GROUP NB_REALDEVICES UUID_RDEV(1) UUID_RDEV(2).. UUID_RDEV(NB_REALDEVICES)
 */
// TODO: - lecture du /proc/virtualiseur/groups/GROUP_NAME
//       - contr�le que les donn�es sont au bon format
//       - �criture en t�te du fichier de conf
int add_group_conf_file(char *group_name) {
  return TRUE;
}

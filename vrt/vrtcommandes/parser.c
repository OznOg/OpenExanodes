#include "constantes.h"

typedef enum {false=0, true} bool;

bool Est_un_chiffre(char c) {
	return (c>='0' && c<='9');
}

int Valeur_chiffre(char c) {
	return (c-'0');
}



//! prend une chaîne de caractère ("s") et en retire le premier mot trouvé ("retour"). renvoie TRUE s'il a trouvé un mot à extraire et FALSE sinon
/*! prend une chaîne, retire le premier mot en tête (s'il y en a un),
    et le renvoie
    un mot = tout groupe de caractères n'incluant pas d'espace
*/
int extraire_mot(char *s, char *retour) {
  int i,j;
  int taille_mot;
  int pos_premier_car_mot=0;
  bool dans_le_mot=false, continuer=true;
  
  // on copie les caractères du mot
  i=0;
  taille_mot=0;
  while (s[i] && s[i]!='\n' && continuer) {
    
    if (s[i]!=' ') {
      if (dans_le_mot==false) {
	dans_le_mot=true;
	pos_premier_car_mot=i;
      }
      
      retour[taille_mot]=s[i];
      taille_mot++;
      
    } else {
      if (dans_le_mot) continuer=false; // on est sorti du mot		
    }
    i++;
  }
  
  retour[taille_mot]=0; // on termine la chaîne 
  
  if (taille_mot) {
    // on supprime le mot de la chaîne
    j=pos_premier_car_mot;
    while (s[j]) {
      s[j]=s[j+taille_mot];
      j++;
    }
  }
  else return FALSE;
  
  return TRUE;
}



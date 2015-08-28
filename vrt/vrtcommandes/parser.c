#include "constantes.h"

typedef enum {false=0, true} bool;

bool Est_un_chiffre(char c) {
	return (c>='0' && c<='9');
}

int Valeur_chiffre(char c) {
	return (c-'0');
}



//! prend une cha�ne de caract�re ("s") et en retire le premier mot trouv� ("retour"). renvoie TRUE s'il a trouv� un mot � extraire et FALSE sinon
/*! prend une cha�ne, retire le premier mot en t�te (s'il y en a un),
    et le renvoie
    un mot = tout groupe de caract�res n'incluant pas d'espace
*/
int extraire_mot(char *s, char *retour) {
  int i,j;
  int taille_mot;
  int pos_premier_car_mot=0;
  bool dans_le_mot=false, continuer=true;
  
  // on copie les caract�res du mot
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
  
  retour[taille_mot]=0; // on termine la cha�ne 
  
  if (taille_mot) {
    // on supprime le mot de la cha�ne
    j=pos_premier_car_mot;
    while (s[j]) {
      s[j]=s[j+taille_mot];
      j++;
    }
  }
  else return FALSE;
  
  return TRUE;
}



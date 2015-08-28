/*!\file parser.c
\brief fichier pas important : d�finit les fonctions permettant d'extraire les arguments d'une ligne de commande

Ces fonctions sont utilis�es pour extraire les arguments de la ligne de commande envoy�e vers
/proc/scimapdev/briques_enregistrees
*/

typedef enum {false=0, true} bool;

bool Est_un_chiffre(char c) {
	return (c>='0' && c<='9');
}

int Valeur_chiffre(char c) {
	return (c-'0');
}

//! prend une cha�ne de caract�res, retire le premier entier non sign� (s'il y en a un), et le renvoie (renvoie -1 si pas d'entier en t�te de cha�ne)
/*! Note : un entier est forc�ment situ� entre 2 espaces (pour �viter
    de retourner les entiers situ� dans des mots (comme le "6" de
    "/dev/hda6", par exemple). Sauf lorsque l'entier est situ� en d�but de cha�ne
    ou en fin de cha�ne
*/
long Extraire_entier_long(char *s) {
	int i,j;
	char entier[50];
	long valeur_entier;
	int poids;
	int taille_entier;
	int pos_premier_car_entier=-1;
	bool dans_l_entier=false, continuer=true, car_avant_est_un_espace;

	// on copie les caract�res de l'entier
	i=0;
	taille_entier=0;
 	while (s[i] && continuer) {
      
	  if (Est_un_chiffre(s[i])) {

	    // est ce que le caract�re avant est un espace ?
	    car_avant_est_un_espace=false;
	    if (i==0) car_avant_est_un_espace=true;
	    if (i>=0 && s[i-1]==' ') car_avant_est_un_espace=true;

	    if (dans_l_entier==false && car_avant_est_un_espace) {
	      dans_l_entier=true;
	      pos_premier_car_entier=i;
	    }
	
	    if (dans_l_entier) {
		entier[taille_entier]=s[i];
		taille_entier++;
	    }
	   
	    
	  } else {
	    if (dans_l_entier && s[i]==' ') continuer=false; // on est sorti de l'entier long et comme il faut (i.e. par un espace)
	    else {
		// l'entier qui avait trouv� �tait en fait inclu dans un mot => on cherche un autre entier long dans la cha�ne
		taille_entier=0;
		dans_l_entier=false;
	    }
	  }
	  i++;
 	}
 	
       
 	// on calcule la valeur de l'entier
 	if (taille_entier) {
	  poids=1;
	  valeur_entier=0;
	  for (j=taille_entier-1; j>=0; j--) {
	    valeur_entier+=poids*Valeur_chiffre(entier[j]);
	    poids=poids*10; 	
	  }
	
 	
	  // on supprime l'entier de la cha�ne
	  j=pos_premier_car_entier;
	  while (s[j]) {
	    s[j]=s[j+taille_entier];
	    j++;
	  }
	}
	else valeur_entier=-1;
 	
 	return valeur_entier;
}

//! prend une cha�ne de caract�re ("s") et en retire le premier mot trouv� ("retour")
/*! prend une cha�ne, retire le premier mot en t�te (s'il y en a un),
    et le renvoie
    renvoie  ' ' (espace) dans le 1er caract�re si pas de mot en t�te de cha�ne
    ie. si c'est une chaine vide ou ne contenant que des espaces)
    un mot = tout groupe de caract�res n'incluant pas d'espace
*/
void Extraire_mot(char *s, char *retour) {
	int i,j;
	int taille_mot;
	int pos_premier_car_mot=-1;
	bool dans_le_mot=false, continuer=true;

	// on copie les caract�res du mot
	i=0;
	taille_mot=0;
	while (s[i] && continuer) {

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

	retour[taille_mot]=0; // on termine la cha�ne qui contient le mot extrait
 	
	if (taille_mot) {
		// on supprime le mot de la cha�ne
		j=pos_premier_car_mot;
		while (s[j]) {
		s[j]=s[j+taille_mot];
		j++;
		}
	}
	else retour[0]=' ';
}



#include <asm/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#define FALSE 0
#define TRUE 1

//! indique si l'UUID passé en param appartient bien à une zone existante
// TODO : lire les superblocs de tous les devices accessibles par le NBD
int valid_zuuid(__u32 *zuuid) {
  return TRUE;
}

//! détermine l'UUID de la zone de nom "zname". Renvoie EXIT_FAILURE s'il n'y arrive pas et EXIT_SUCCESS sinon.
// TODO : lire les superblocs de tous les devices accessibles par le NBD
int zname2zuuid(char *gname, __u32 *zuuid) {
  
  return EXIT_SUCCESS;
}

//! indique si l'UUID passé en param appartient bien à un groupe existant
// TODO : lire les superblocs de tous les devices accessibles par le NBD
int valid_guuid(__u32 *guuid) {
  return TRUE;
}

//! détermine l'UUID du groupe de nom "gname". Renvoie EXIT_FAILURE s'il n'y arrive pas et EXIT_SUCCESS sinon.
// TODO : lire les superblocs de tous les devices accessibles par le NBD
int gname2guuid(char *gname, __u32 *guuid) {
  
  return EXIT_SUCCESS;
}

//! renvoie TRUE si les uuid1 == uuid2 et FALSE sinon.
inline int same_uuid(__u32 *uuid1, __u32 *uuid2) {
  int i;
  for (i=0; i<4; i++) 
    if (uuid1[i]!=uuid2[i]) 
      return FALSE;
  
  return TRUE;  
}

//! copie (affecte) le UUID source dans le UUID destination
inline void cpy_uuid(__u32 *uuid_dest, __u32 *uuid_source) {
  int i;
  for (i=0; i<4; i++) 
    uuid_dest[i]=uuid_source[i];
}

//! convertit l'UUID sous forme de chaîne en un UUID sous forme d'un tableau de quatre __u32
int str2uuid(char *str_uuid, __u32 *uuid) {
  int tokens_read;
  tokens_read = sscanf(str_uuid,
		       "%x:%x:%x:%x",
		       &uuid[3], &uuid[2], &uuid[1], &uuid[0]);
  if (tokens_read != 4) 
    return EXIT_FAILURE;
  else return EXIT_SUCCESS;
}

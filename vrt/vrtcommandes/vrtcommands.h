#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <asm/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <math.h>
#include "vrt_superblock.h"

#define FALSE 0
#define TRUE 1
#define LINE_SIZE 256 //!< taille d'une ligne de texte (pour commande, etc..)
#define PROC_VIRTUALISEUR "/proc/virtualiseur/zones_enregistrees"
#define BASIC_SSIZE 25 //!< taille pour une string de base

///// ATTENTION !! ATTENTION !! ATTENTION !! ATTENTION !! 
// constantes à synchroniser avec cette du fichier constantes.h du 
// répertoire "virtualiseur"
#define TAILLE_MAX_NOM_DEV 100 //!< taille max du path d'un /dev
#define MAXSIZE_GROUPNAME 16 //!< taille nom du groupe = 16 car max


#define VRTCMD_VERSION "1.0.0" //!< version des commandes

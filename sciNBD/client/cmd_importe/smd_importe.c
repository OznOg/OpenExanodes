#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "constantes.h"
#include <netdb.h>
#include "parser.h"
#include "sisci_api.h"
#include <asm/types.h>

char string_err[256];
//////////////////////////////////////////////////////
char *Sci_error(int error) {
   char **string_error=(char **)(&string_err);

   *string_error = "Error UNKNOWN\n";

   if (error == SCI_ERR_BUSY              	   ) *string_error = "Error SISCI no 0x900"; 
   if (error == SCI_ERR_FLAG_NOT_IMPLEMENTED       ) *string_error = "Error SISCI no 0x901";  
   if (error == SCI_ERR_ILLEGAL_FLAG               ) *string_error = "Error SISCI no 0x902";  
   if (error == SCI_ERR_NOSPC                      ) *string_error = "Error SISCI no 0x904";  
   if (error == SCI_ERR_API_NOSPC                  ) *string_error = "Error SISCI no 0x905";         
   if (error == SCI_ERR_HW_NOSPC                   ) *string_error = "Error SISCI no 0x906";  
   if (error == SCI_ERR_NOT_IMPLEMENTED            ) *string_error = "Error SISCI no 0x907";  
   if (error == SCI_ERR_ILLEGAL_ADAPTERNO          ) *string_error = "Error SISCI no 0x908";   
   if (error == SCI_ERR_NO_SUCH_ADAPTERNO          ) *string_error = "Error SISCI no 0x909";
   if (error == SCI_ERR_TIMEOUT                    ) *string_error = "Error SISCI no 0x90A";
   if (error == SCI_ERR_OUT_OF_RANGE               ) *string_error = "Error SISCI no 0x90B";
   if (error == SCI_ERR_NO_SUCH_SEGMENT            ) *string_error = "Error SISCI no 0x90C";
   if (error == SCI_ERR_ILLEGAL_NODEID             ) *string_error = "Error SISCI no 0x90D";
   if (error == SCI_ERR_CONNECTION_REFUSED         ) *string_error = "Error SISCI no 0x90E";
   if (error == SCI_ERR_SEGMENT_NOT_CONNECTED      ) *string_error = "Error SISCI no 0x90F";
   if (error == SCI_ERR_SIZE_ALIGNMENT             ) *string_error = "Error SISCI no 0x910";
   if (error == SCI_ERR_OFFSET_ALIGNMENT           ) *string_error = "Error SISCI no 0x911";
   if (error == SCI_ERR_ILLEGAL_PARAMETER          ) *string_error = "Error SISCI no 0x912";
   if (error == SCI_ERR_MAX_ENTRIES                ) *string_error = "Error SISCI no 0x913";   
   if (error == SCI_ERR_SEGMENT_NOT_PREPARED       ) *string_error = "Error SISCI no 0x914";
   if (error == SCI_ERR_ILLEGAL_ADDRESS            ) *string_error = "Error SISCI no 0x915";
   if (error == SCI_ERR_ILLEGAL_OPERATION          ) *string_error = "Error SISCI no 0x916";
   if (error == SCI_ERR_ILLEGAL_QUERY              ) *string_error = "Error SISCI no 0x917";
   if (error == SCI_ERR_SEGMENTID_USED             ) *string_error = "Error SISCI no 0x918";
   if (error == SCI_ERR_SYSTEM                     ) *string_error = "Error SISCI no 0x919";
   if (error == SCI_ERR_CANCELLED                  ) *string_error = "Error SISCI no 0x91A";
   if (error == SCI_ERR_NOT_CONNECTED              ) *string_error = "Error SISCI no 0x91B";
   if (error == SCI_ERR_NOT_AVAILABLE              ) *string_error = "Error SISCI no 0x91C";
   if (error == SCI_ERR_INCONSISTENT_VERSIONS      ) *string_error = "Error SISCI no 0x91D";
   if (error == SCI_ERR_COND_INT_RACE_PROBLEM      ) *string_error = "Error SISCI no 0x91E";
   if (error == SCI_ERR_OVERFLOW                   ) *string_error = "Error SISCI no 0x91F";
   if (error == SCI_ERR_NOT_INITIALIZED            ) *string_error = "Error SISCI no 0x920";
   if (error == SCI_ERR_ACCESS                     ) *string_error = "Error SISCI no 0x921";
   if (error == SCI_ERR_NO_SUCH_NODEID             ) *string_error = "Error SISCI no 0xA00";
   if (error == SCI_ERR_NODE_NOT_RESPONDING        ) *string_error = "Error SISCI no 0xA02";  
   if (error == SCI_ERR_NO_REMOTE_LINK_ACCESS      ) *string_error = "Error SISCI no 0xA04";
   if (error == SCI_ERR_NO_LINK_ACCESS             ) *string_error = "Error SISCI no 0xA05";
   if (error == SCI_ERR_TRANSFER_FAILED            ) *string_error = "Error SISCI no 0xA06";

   return *string_error;
}

int get_this_node_sci_number(void) {
  int sci_number;
  sci_error_t error;
  sci_query_adapter_t query;
  
  // initialize the SCI environment 
  SCIInitialize(NO_FLAGS, &error);
  if (error != SCI_ERR_OK) {
    printf("sci_init : erreur SCIInitialize - ");
    printf("%s\n",Sci_error(error));
    exit(ERROR);
  }

  // détermine le node_id SCI du noeud local
  query.localAdapterNo = 0;
  query.subcommand = SCI_Q_ADAPTER_NODEID;
  query.data = &sci_number;
  SCIQuery(SCI_Q_ADAPTER,&query,NO_FLAGS,&error);
  if (error != SCI_ERR_OK) {
    printf("sci_init : erreur SCIQuery - ");
    printf("%s\n",Sci_error(error));
    SCITerminate();
    exit(ERROR);
  }  

  printf("sci_init : no SCI du noeud local (THIS_NODE) = %d\n", sci_number);
  SCITerminate();
  return sci_number;
}

// pour chaque device de la liste : créer un BDD qui lui est associé
int importer_liste_devices(char *liste, char *hostname) {
  int no_sci, i, nb_car_lus, nb_lus;
  char devname[LINE_MAX_SIZE], devpath[LINE_MAX_SIZE], commande[LINE_MAX_SIZE];
  //int fich_proc; 
  static int nb_ajouts=0;
  __u64 nb_sectors;
  __u32 devnb;
	
  
  i = 0;
  while (1) {
    nb_lus = sscanf(&(liste[i]),
		    "%s %s %d %Ld %d %n",
		    devname, 
		    devpath,
		    &devnb,
		    &nb_sectors,
		    &no_sci,
		    &nb_car_lus);

    if (nb_lus != 5) {
      if (nb_lus != -1) {
	fprintf(stderr,"ERREUR impossible de parser la liste de dev "
		"exportés par le serveur '%s'. Erreur sur l'arg n° %d\n", 
		hostname, nb_lus);
      }
      break;
    }
    i += nb_car_lus;
		        
    // ajouter un bdd
    printf("importer_liste_devices : ajout du device n°%d\n",nb_ajouts);
    printf("importer_liste_devices : no sci du serveur  = %d\n",no_sci);
    printf("importer_liste_devices : nom du device : <%s>\n",devname);
    printf("importer_liste_devices : path du device : <%s>\n",devpath);
    printf("importer_liste_devices : n° du dev %d\n",devnb);
    printf("importer_liste_devices : nb sectors %lld\n",nb_sectors);
 
    sprintf(commande,"echo \"a %s %d %s %s %d %lld\" > " PROC_SCIMAPDEV,
	    hostname,no_sci,devname, devpath, devnb, nb_sectors);
    system(commande);

    //write(fich_proc,commande,strlen(commande));
		
    printf("importer_liste_devices : commande envoyée = %s\n",commande);
			
    nb_ajouts++;
  }
	
  //close(fich_proc);
  return OK;
}

// get_devices : demande la liste des devices exportés par un hôte et les importe (en fait des devices locaux)
// les arguments :
// no_sci = no SCI du noeud à partir duquel on importe les devices (noeud exportateur)
// hostname = nom de l'hôte à partir duquel on importe les devices (hote exportateur)
int get_devices(char *hostname) {
	char buffer[BUFFER_MAX_SIZE+1];		
	int	sd;
	struct sockaddr_in pin;
	struct hostent *hp;
	char msg[100];
	

	/* go find out about the desired host machine */
	if ((hp = gethostbyname(hostname)) == 0) {
		perror("get_devices : ERREUR - gethostbyname");
		exit(ERROR);
	}

	printf("get_devices : Résolution du nom du serveur\n");
	/* fill in the socket structure with host information */
	memset(&pin, 0, sizeof(pin));
	pin.sin_family = AF_INET;
	pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
	pin.sin_port = htons(SOCKET_PORT);

	/* grab an Internet domain socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("get_devices : ERREUR - socket");
		exit(ERROR);
	}
	printf("get_devices : Création du socket\n");
	
	/* connect to PORT on HOST */
	if (connect(sd,(struct sockaddr *)  &pin, sizeof(pin)) == -1) {
		perror("connect");
		exit(ERROR);
	}
	printf("get_devices : Connexion au socket\n");	
	
	
	
	sprintf(msg,"IMPORT %d",get_this_node_sci_number());

	write(sd,msg,strlen(msg));
	printf("msg '%s' envoyé au serveur\n", msg);

	read(sd,buffer,BUFFER_MAX_SIZE+1);
	printf("msg '%s' reçu du serveur\n", buffer);

	if (strcmp(buffer,"RIEN")==0) {
		printf("get_devices : Aucun device n'est exporté par "
		       "%s\n",hostname);
		close(sd);
		return OK;
	}
	else 
	  if(strcmp(buffer,"ERROR")==0) {
		printf("get_devices : ERREUR dans l'exportation de "
		       "devices sur %s\n",hostname);
		close(sd);
		return ERROR;
	  }
	  else {
	    close(sd);
	    return importer_liste_devices(buffer, hostname);
	  }
}


void usage(void) {
	printf("\nAIDE POUR LA COMMANDE smd_importe :\n");
	printf("-----------------------------------\n");
	printf("> ./smd_importe sam3\nImporte les devices exportés par sam3\n\n");

}

int main(int argc, char **argv) {

  if (argc==2) return get_devices(argv[1]);
  else usage();

  return OK;
}


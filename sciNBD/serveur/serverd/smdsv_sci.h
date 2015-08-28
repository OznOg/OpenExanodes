#ifndef _SMDSV_SCI
#define _SMDSV_SCI

#include "smd_server.h"

extern int cleanupsci_client(client_t *cl);
extern int init_localsci_client(client_t *cl, int no_sci_client);
extern int send_ready_sci(client_t *cl);
extern int deconnexion_sci_client(client_t *cl);
extern int connexion_sci_client(client_t *cl);

#endif

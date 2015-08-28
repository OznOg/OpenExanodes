#ifndef __SMDSV_CLIENT
#define __SMDSV_CLIENT

extern int deja_client(int no_sci_client);
extern int new_client(int no_sci_client);
extern int init_client(int no_sci_client);
extern void cleanup_clients(void);

#endif

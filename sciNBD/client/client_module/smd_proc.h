#ifndef _smd_proc_h
#define _smd_proc_h


#include <linux/proc_fs.h>
#include "constantes.h"
#include "scimapdev.h"

extern int smd_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data);
extern int smd_write_proc(struct file *file, const char *buffer, unsigned long count, void *data);
extern int smd_proc_read_data_etat(char *buf, char **start, off_t offset, int count, int *eof, void *data);
extern int smd_proc_read_data_ndevs(char *buf, 
			     char **start, 
			     off_t offset, 
			     int count, 
			     int *eof, 
			     void *data);
extern struct proc_dir_entry *smd_proc_dir;
extern struct proc_dir_entry *smd_proc_briques;

#endif

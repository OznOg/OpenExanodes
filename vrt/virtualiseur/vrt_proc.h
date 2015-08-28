#ifndef _vrt_proc_h
#define _vrt_proc_h


#include <linux/proc_fs.h>
#include "constantes.h"
#include "virtualiseur.h"

extern int vrt_read_proc(char *buf, char **start, off_t offset, int count, int *eof, void *data);
extern int vrt_write_proc(struct file *file, const char *buffer, unsigned long count, void *data);
extern int vrt_proc_read_data_etat(char *buf, char **start, off_t offset, int count, int *eof, void *data);
extern int vrt_proc_read_data_zones(char *buf, char **start, off_t offset, int count, int *eof, void *data);
extern struct proc_dir_entry *vrt_proc_dir;
extern struct proc_dir_entry *vrt_proc_zones;

extern int vrt_read_idling_zones(char *buf, 
				 char **start, 
				 off_t offset, 
				 int count, 
				 int *eof, 
				void *data);
extern int vrt_read_active_zones(char *buf, 
				 char **start, 
				 off_t offset, 
				 int count, 
				int *eof, 
				 void *data);

extern int vrt_read_used_zones(char *buf, 
			char **start, 
			off_t offset, 
			int count, 
			int *eof, 
			       void *data);

#endif

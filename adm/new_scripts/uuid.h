#ifndef __UUIDCOMMANDS
#define __UUIDCOMMANDS

extern int valid_guuid(__u32 *guuid);
extern int gname2guuid(char *gname, __u32 *guuid);
extern int str2uuid(char *str_uuid, __u32 *uuid);
extern inline int same_uuid(__u32 *uuid1, __u32 *uuid2);
extern inline void cpy_uuid(__u32 *uuid_dest, __u32 *uuid_source);

#endif

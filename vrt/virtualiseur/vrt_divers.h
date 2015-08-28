#ifndef _VRT_DIVERS 
#define _VRT_DIVERS
extern u32 indice_elt(void *elt, void *table[], u32 size);
extern u32 unused_elt_indice(void *table[], u32 size);
extern void *get_elt(u32 n, void *table[], u32 size);
extern void nullify_table(void *table[], u32 size);
extern void vfree_table(void *table[], u32 size);
extern u64 quotient64(u64 a, u32 q);
extern u64 quotient_ceil64(u64 a, u32 q);
extern u32 reste64(u64 a, u32 q);
#endif

/* $Id$  */

/*******************************************************************************
 *                                                                             *
 * Copyright (C) 1993 - 2000                                                   * 
 *         Dolphin Interconnect Solutions AS                                   *
 *                                                                             *
 * This program is free software; you can redistribute it and/or modify        * 
 * it under the terms of the GNU General Public License as published by        *
 * the Free Software Foundation; either version 2 of the License,              *
 * or (at your option) any later version.                                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *                                                                             *
 *                                                                             *
 *******************************************************************************/ 

#ifndef _SYSCALL_COTYPES_H
#define _SYSCALL_COTYPES_H

#include "sci_types.h"
#include <sisci_types.h>

/* The CRITICAL_COMPILATION_REV value must be updated (incremented)            */
/* every time the some critical changes has been made (parameter changes etc.) */ 
/* CRITICAL_COMPILATION_REV value is compared with the value in core.c         */
/* to avoid inconstant versions between SYSCALL and API.                       */
#define CRITICAL_COMPILATION_REV 17

#define MAX_DEVICE_NUMBER   128

#ifdef SUPPORT_CALLBACK
#define MAX_CALLBACKS_EVENT 32

#endif

typedef union {
#if defined(SOLARIS)
	unsigned64  value;
#else
	unsigned32  value;
#endif
	void       *vptr;
	register32 *rptr;
} shared_pointer_t;

typedef shared_pointer_t ListKey_t;
#define VPTR(x) x.vptr
#define RPTR(x) x.rptr

/*
 * ================================================================================================
 * E R R   S T A T U S
 * ================================================================================================
 */
typedef struct _ErrStatus {
   sci_error_t    errcode;
} ErrStatus;

/*
 * ================================================================================================
 * S H M   P a c k e t
 * ================================================================================================
 */
typedef struct _SHMPacket {
   sci_error_t    errcode;
   unsigned8      minorId; /* internal use */    
   unsigned32     nodeId;
   unsigned32     adapterId;
   unsigned32     errorInfo;
#if defined (_WIN32) /* ENABLE On Linux/LYNX/NT as NEEDED, DON'T ENABLE FOR SOLARIS */
   void          *errorPtr;
#endif
   unsigned32     segId;
   unsigned32     segSize;
   scibool        local;
   unsigned32     flags;
   shared_pointer_t usermem;
   unsigned32     userSize;
   sci_address_t  sciAddr;
   unsigned32     protectionKey;
   ListKey_t      listkey;
    sci_ioaddr_t  iomem;
    unsigned32    iobus;
} SHMPacket;

#define sp_listkey VPTR(listkey)
#define sp_usermem VPTR(usermem)

/*
 * ================================================================================================
 * M A P   P a c k e t
 * ================================================================================================
 */
typedef struct _MAPPacket {
    sci_error_t    errcode;
    shared_pointer_t addr;
    unsigned32   offset;
    unsigned32   size;
    unsigned32   accessMode;
    SHMPacket    sp;
    unsigned32   flags;    
#if defined (_WIN32) /* ENABLE On Linux/LYNX/NT as NEEDED, DON'T ENABLE FOR SOLARIS */
    ListKey_t    listkey;
#endif
}  MAPPacket;

#define mp_addr    VPTR(addr)
#define mp_listkey VPTR(listkey)


/*
 * ================================================================================================
 * D M A   P a c k e t
 * ================================================================================================
 */
typedef struct _DMAPacket {
    sci_error_t  errcode;
    SHMPacket    lsp;
    SHMPacket    rsp;
    unsigned32   adaptno;
    unsigned32   size;
    unsigned32   sourceoffset;
    unsigned32   targetoffset;
    unsigned32   entries;
    unsigned32   state;
    unsigned32   timeout;
    unsigned32   flags;
    ListKey_t    listkey;
}  DMAPacket;

#define dp_listkey VPTR(listkey)

/*
 * ================================================================================================
 * E R R O R   I N F O
 * ================================================================================================
 */
typedef struct _ErrorInfo {
    sci_error_t    errcode;
    unsigned32           adapterId;
    unsigned32           errStatMask;
    unsigned32           storeBarDummy;
    union {
        struct {
            /*
             * The order of registers is significant. The U**X code 
             * for mapping the registers is based on the register 
             * order below.
             */
            shared_pointer_t     ei_errPtr;         
            shared_pointer_t     ei_statRegPtr;
            shared_pointer_t     ei_storeBarPtr;
            shared_pointer_t     ei_flushRegPtr;
            shared_pointer_t     ei_checkRegPtr;
            shared_pointer_t     ei_mapValidPtr;
        } _f;
        shared_pointer_t         ei_registers[6];
    } _u;
#if defined (_WIN32) /* ENABLE On Linux/LYNX/NT as NEEDED, DON'T ENABLE FOR SOLARIS */
    void                  *listkey;
#endif
}   ErrorInfo;

#define errPtr        RPTR(_u._f.ei_errPtr)
#define statRegPtr    RPTR(_u._f.ei_statRegPtr)
#define storeBarPtr   RPTR(_u._f.ei_storeBarPtr)
#define mapValidPtr   RPTR(_u._f.ei_mapValidPtr)
#define flushRegPtr   RPTR(_u._f.ei_flushRegPtr)
#define checkRegPtr   RPTR(_u._f.ei_checkRegPtr)


/*
 * ================================================================================================
 * I N T   P A C K E T
 * ================================================================================================
 */
typedef struct _INTPacket {
    sci_error_t  errcode;
    unsigned32   intno;
    unsigned32   nodeid;
    unsigned32   adaptno;
    unsigned32   flags;
    ListKey_t    listkey;
    unsigned32   timeout;
    unsigned32   event_count;
    ListKey_t    segmentListkey;
    unsigned32   segmentOffset;
    ListKey_t    disInfoSegmentListkey;
    unsigned32   disInfoSegmentOffset;
}  INTPacket;

#define ip_listkey VPTR(listkey)
#define ip_segmentListkey VPTR(segmentListkey)
#define ip_disInfoSegmentListkey VPTR(disInfoSegmentListkey)


/*
 * ================================================================================================
 * D R I V E R   I N F O
 * ================================================================================================
 */
typedef struct _DriverInfo {
   sci_error_t    errcode;
    unsigned32      irm_revision;
    unsigned32      sc_revision;
    char            sc_revision_string[200];
} DriverInfo;


/*
 * ================================================================================================
 * C S R   I N F O
 * ================================================================================================
 */
typedef struct _CSRInfo {
    sci_error_t    errcode;
    unsigned32      adapterNo;
    unsigned32      nodeId;
    unsigned32      csrOffset;
    unsigned32      csrValue;
    unsigned32      flags;
} CSRInfo;


/*
 * ================================================================================================
 * P R O B E   I N F O
 * ================================================================================================
 */
typedef struct _ProbeInfo {
    sci_error_t    errcode;
    unsigned32      adapterNo;
    unsigned32      nodeId;
    unsigned32      flags;
} ProbeInfo;


/*
 * ================================================================================================
 * Q U E R Y   S Y S T E M
 * ================================================================================================
 */
typedef struct _QuerySystem {
    sci_error_t     errcode;
    unsigned32      command;
    unsigned32      data;
    unsigned32      flags;
} QuerySystem;


/*
 * ================================================================================================
 * Q U E R Y   A D A P T E R
 * ================================================================================================
 */
typedef struct _QueryAdapter {
    sci_error_t     errcode;
    unsigned32      adapterNo;
    unsigned32      portNo;
    unsigned32      command;
    unsigned32      data;
    unsigned32      flags;
} QueryAdapter;

/*
 * =====================================================================================
 *    Q U E R Y   L O C A L   S E G M E N T   T
 * =====================================================================================
 */
typedef struct {
    sci_error_t  errcode;
    SHMPacket    segment;
    unsigned32   command;
    unsigned32   data;          /* @HASV@ 64 bit issue ...? */
    unsigned32   flags;
} QueryLocalSegment_t;

/*
 * =====================================================================================
 *    Q U E R Y   R E M O T E   S E G M E N T   T
 * =====================================================================================
 */
typedef struct {
    sci_error_t  errcode;
    SHMPacket    segment;
    unsigned32   command;
    unsigned32   data;          /* @HASV@ 64 bit issue ...? */
    unsigned32   flags;
} QueryRemoteSegment_t;


typedef struct {
    sci_error_t  errcode;
    MAPPacket    map;
    unsigned32   command;
    unsigned32   data;         
    unsigned32   flags;
} QueryMap_t;

/*
 * ================================================================================================
 * Q U E R Y   E R R O R   I N F O
 * ================================================================================================
 */
typedef struct _QueryErrorInfo {
    sci_error_t    errcode;
    union {
        struct {
            unsigned32    errcnt;
            unsigned32    statreg;
            unsigned32    storebar;
            unsigned32    flushreg;
            unsigned32    checkreg;
            unsigned32    mapvalid;
            unsigned32    errmask;
            unsigned32    storedummy;
        } _f;
        unsigned32        offsets[8];
    } _u;
} QueryErrorInfo;

#define qei_errcnt     _u._f.errcnt
#define qei_statreg    _u._f.statreg
#define qei_storebar   _u._f.storebar
#define qei_flushreg   _u._f.flushreg
#define qei_checkreg   _u._f.checkreg
#define qei_mapvalid   _u._f.mapvalid
#define qei_errmask    _u._f.errmask
#define qei_storedummy _u._f.storedummy
#define qei_offsets  _u.offsets

typedef struct {
    sci_error_t errcode;
    unsigned32  data[62];
} TESTPACKET;

/*
 * ================================================================================================
 * S E G   C B   P A C K E T
 * ================================================================================================
 */
#ifdef SUPPORT_CALLBACK
typedef struct _SEG_CB_Packet {
    sci_error_t    errcode;
    ListKey_t          listkey;
    scibool            overflow_flg;
    union {
	    unsigned32                  count[MAX_CB_REASON];
        struct {
            sci_segment_cb_reason_t      cause;
            unsigned32                   nodeid;
            unsigned32                   adapter;
        }                           trace;
    }                  info;
} SEG_CB_Packet;


#define lep_listkey VPTR(listkey)
#endif

/*
 * ================================================================================================
 * C A C H E   E N A B L E   P A C K E T
 * ================================================================================================
 */
#if defined(LYNXOS)
typedef struct _CacheEnablePacket {
    sci_error_t    errcode;
    void* what_smem_create_gave_me;
    void* physical_address;
    int   length;
} CacheEnablePacket;

#endif

/*
 * ================================================================================================
 * M A P   Q U E R Y   R E Q U E S T
 * ================================================================================================
 */
#if defined(LYNXOS) || defined(OS_IS_VXWORKS)
typedef struct _MapQueryRequest {
    sci_error_t    errcode;
    unsigned32  offset;
    void       *mapaddr;
} MapQueryRequest;
#endif

/* Some constants shared between user and kernel */
#define _INTERNAL_SCI_INFINITE_TIMEOUT  0xffffffff

/* FLAG values used in both user and kernel part of API */
#define _INTERNAL_SCI_FLAG_USE_CALLBACK                      0x1
#define _INTERNAL_SCI_FLAG_EMPTY                             0x2
#define _INTERNAL_SCI_FLAG_PRIVATE                           0x4
#define _INTERNAL_SCI_FLAG_DMA_SOURCE_ONLY                   0x8
#define _INTERNAL_SCI_FLAG_PHYSICAL                          0x10


#define _INTERNAL_SCI_FLAG_DMA_POST                          0x2
#define _INTERNAL_SCI_FLAG_DMA_WAIT                          0x4
#define _INTERNAL_SCI_FLAG_DMA_RESET                         0x8
#define _INTERNAL_SCI_FLAG_DMA_READ                          0x10

#ifdef REG_INTFLAG
#define _INTERNAL_SCI_FLAG_INTERNAL_SEG                      0x10
#endif

#define _INTERNAL_SCI_FLAG_THREAD_SAFE                       0x1
#define _INTERNAL_SCI_FLAG_FIXED_INTNO                       0x2

#define _INTERNAL_SCI_FLAG_FIXED_MAP_ADDR                    0x1
#define _INTERNAL_SCI_FLAG_READONLY_MAP                      0x2
#ifdef REG_INTFLAG
#define _INTERNAL_SCI_FLAG_CONDITIONAL_INTERRUPT_MAP         0x8
#define _INTERNAL_SCI_FLAG_UNCONDITIONAL_DATA_INTERRUPT_MAP  0x10
#endif

#define _INTERNAL_SCI_FLAG_LOCK_OPERATION                    0x4
#define _INTERNAL_SCI_FLAG_READ_PREFETCH_AGGR_HOLD_MAP       0x20
#define _INTERNAL_SCI_FLAG_READ_PREFETCH_NO_HOLD_MAP         0x40 
#define _INTERNAL_SCI_FLAG_IO_MAP_IOSPACE                    0x80
#define _INTERNAL_SCI_FLAG_DMOVE_MAP                         0x100
#define _INTERNAL_SCI_FLAG_WRITES_DISABLE_GATHER_MAP         0x200
#define _INTERNAL_SCI_FLAG_DISABLE_128_BYTES_PACKETS         0x400
#define _INTERNAL_SCI_FLAG_NO_MEMORY_LOOPBACK                0x800


#ifdef REG_INTFLAG
#define _INTERNAL_SCI_FLAG_CONDITIONAL_INTERRUPT             0x8
#endif

#if (defined(CPU_ARCH_IS_PPC) &&(defined(OS_IS_VXWORKS) || defined(OS_IS_LYNXOS))) || (defined(OS_IS_VXWORKS) && defined(CPU_ARCH_IS_X86))
#define _INTERNAL_SCI_FLAG_WRITE_BACK_CACHE_MAP              0x800
#endif



#endif              /* _SYSCALL_COTYPES_H */

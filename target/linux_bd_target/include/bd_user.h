/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef EXA_BD_USER
#define EXA_BD_USER

#include "os/include/os_inttypes.h"

/* IOCTL contants */

#define BD_IOCTL_INIT           0x2d    /* create a new session */
#define BD_IOCTL_SEM_ACK        0x2e    /* up the bd_rq_ok */
#define BD_IOCTL_SEM_NEW        0x2f    /* down the bd_new_rq */
#define BD_IOCTL_NEWMINOR       0x30    /* Set the size of one minor blockdevice */
#define BD_IOCTL_SEM_NEW_UP     0x31    /* up BdBewRq, used to repost a new request */
#define BD_IOCTL_DELMINOR       0x35
#define BD_IOCTL_SETSIZE        0x38
#define BD_IOCTL_IS_INUSE       0x39

struct bd_barrillet_queue
{
    int *last_index_add;
    int *last_index_read_plus_one;
    int *next_elt;
};

struct bd_init
{
    int   bd_buffer_size;
    int   bd_max_queue;
    int   bd_page_size;
    int   bd_barrier_enable;
    void *buffer;
};

struct bd_new_minor
{
    long     bd_minor;
    uint64_t size_in512_bytes; /**< size in block of 512 bytes */
    bool     readonly;
};


#define BD_INFO_BARRIER              2

#ifdef __KERNEL__
struct bd_request
{
    struct bio      *first_bio;
    struct bd_minor *bd_minor;
    int info;
    char rw;
};
#else
struct bd_request
{
    void *first_bio;
    void *bd_minor;
    int   info;
    char  rw;
};
#endif

#define BDUSE_FREE      0  /*! free queue entry */
#define BDUSE_USED      1  /*! used queue entry */
#define BDUSE_SUSPEND   2  /*! supended queue entry */

/*
 * struct bd_kernel_queue and struct bd_user_queue are used to comminicate
 * between user and kernel bd_kernel_queue are read/write by kernel, but only
 * read by user, so it's read only user page Each entry of the array
 * struct bd_kernel_queue
 */
struct bd_kernel_queue
{
    uint64_t           bd_blk_num; /*! blk number ( blk size == 512 ) */
    uint64_t           bd_size_in_sector; /*! Size to transfert  in sector==512 bytes */
    struct bd_request *bd_req;    /*! Used to make a fast end_request on this request */
    void *bd_buf_user;            /*! Address of the buffer in the user process */
#ifdef __KERNEL__
    struct bd_session *bd_session; /* used to know how session it is */
#else
    void *dummy3;
#endif
    volatile int       next;      /*! link list, only use to remove and proccessed entry */
    int bd_minor;
    char bd_op;                   /*! 0 READ or 1 WRITE */
    volatile char      bd_use;    /*! Kernel set this to BDUSE_USED when it's valid, and BDUSE_SUSPEND if supespended
                                   * Before setting this to BDUSE_USED, kernel must to zeroed
                                   * the associated bd_user_queue entry
                                   * note if bduse != BDUSE_FREE, it's the buffer is mapped in user */
} __attribute__((aligned(sizeof(long))));   /* aligned on the bigest elements to be sure that no variable
                                             * cross cache lines, and so all read or write are atomic */

struct bd_user_queue
{
    long bd_info;     /*! info get from another component */
    int  bd_result;
#ifdef WITH_PERF
    uint64_t submit_date;
#endif
} __attribute__((aligned(sizeof(uint64_t))));   /* aligned on the bigest elements to be sure that no variable
                                             * cross cache lines, and so all read or write are atomic */
#define MAX_BD_MINOR  4096
#define MINOR_IS_VALID(minor)  ((minor) < MAX_BD_MINOR && (minor) >= 0)

#endif


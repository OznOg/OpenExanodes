/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef _OS_ASSERT_H
#define _OS_ASSERT_H

#ifdef __KERNEL__

#include <linux/errno.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/version.h>

#ifdef DEBUG

#ifdef CONFIG_MAGIC_SYSRQ

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#define OS_HANDLE_SYSRQ(c) handle_sysrq((c), NULL, NULL)
#else
#define OS_HANDLE_SYSRQ(c) handle_sysrq((c), NULL)
#endif /* LINUX_VERSION_CODE */

#define OS_ASSERT_SYSRQ() ({ \
    OS_HANDLE_SYSRQ ('w'); \
    OS_HANDLE_SYSRQ ('m'); \
    OS_HANDLE_SYSRQ ('t'); \
})

#else  /* CONFIG_MAGIC_SYSRQ */

#define OS_ASSERT_SYSRQ()

#endif /* CONFIG_MAGIC_SYSRQ */

#define OS_ASSERT(expr) ({                                                   \
    if (unlikely(! (expr))) {                                                \
        int _i;                                                              \
        printk(KERN_EMERG "[%s@%s:%d] Assertion '%s' failed\n",              \
               __PRETTY_FUNCTION__, __FILE__, __LINE__, # expr);             \
        for (_i = 0; _i < 1000; _i++)                                        \
            udelay(1000);                                                    \
        OS_ASSERT_SYSRQ();                                                   \
        BUG();                                                               \
    }                                                                        \
})

#define OS_ASSERT_VERBOSE(expr, fmt, ...) ({                                 \
    if (unlikely(! (expr))) {                                                \
        int _i;                                                              \
        printk(KERN_EMERG "[%s@%s:%d] Assertion '%s' failed: " fmt "\n",     \
               __PRETTY_FUNCTION__,  __FILE__, __LINE__, # expr,             \
            ## __VA_ARGS__);                                                 \
        for (_i = 0; _i < 1000; _i++)                                        \
            udelay(1000);                                                    \
        OS_ASSERT_SYSRQ();                                                   \
        BUG();                                                               \
    }                                                                        \
})

#else /* DEBUG */

#define OS_ASSERT(expr) ({                                                   \
    if (! (expr)) {                                                          \
        printk(KERN_EMERG "[%s@%s:%d] Assertion '%s' failed\n",              \
               __PRETTY_FUNCTION__, __FILE__, __LINE__, # expr);             \
        BUG();                                                               \
    }                                                                        \
})

#define OS_ASSERT_VERBOSE(expr, fmt, ...) ({                                 \
    if (! (expr)) {                                                          \
        printk(KERN_EMERG "[%s@%s:%d] Assertion '%s' failed: " fmt "\n",     \
               __PRETTY_FUNCTION__, __FILE__, __LINE__, # expr,              \
               ## __VA_ARGS__);                                              \
        BUG();                                                               \
    }                                                                        \
})

#endif /* DEBUG */

#else /* __KERNEL__ */

#include <stdlib.h>
#include "os/include/os_syslog.h"

#define OS_ASSERT(expr) do {                                                 \
    if (! (expr)) {                                                          \
        os_syslog(OS_SYSLOG_ERROR, "[%s@%s:%d] Assertion '%s' failed",       \
                  __PRETTY_FUNCTION__, __FILE__, __LINE__, # expr);          \
        abort();                                                             \
    }                                                                        \
} while(0)

/* The only difference between the two macros below is the ## operator.
 * This is because icl does not handle ## operator but gcc requires it.
 * See http://software.intel.com/en-us/articles/intel-c-compiler-error-handling-_va_args_-macro-parameter/
 */

#ifdef WIN32

#define OS_ASSERT_VERBOSE(expr, fmt, ...) do {                               \
    if (! (expr)) {                                                          \
        os_syslog(OS_SYSLOG_ERROR, "[%s@%s:%d] Assertion '%s' failed: " fmt, \
                  __PRETTY_FUNCTION__, __FILE__, __LINE__, # expr,           \
                  __VA_ARGS__);                                              \
        abort();                                                             \
    }                                                                        \
} while (0)

#else /* WIN32 */

#define OS_ASSERT_VERBOSE(expr, fmt, ...) do {                               \
    if (! (expr)) {                                                          \
        os_syslog(OS_SYSLOG_ERROR, "[%s@%s:%d] Assertion '%s' failed: " fmt, \
                  __PRETTY_FUNCTION__, __FILE__, __LINE__, # expr,           \
                  ## __VA_ARGS__);                                           \
        abort();                                                             \
    }                                                                        \
} while (0)

#endif /* WIN32 */

#endif /* __KERNEL__ */

/*
 * Static assert: if the predicate is false, the 2nd case will be a duplicate
 * of the 1st case.
 * */
#define COMPILE_TIME_ASSERT(predicate)          \
  switch (0) { case 0: case (predicate): ; }

#endif /* _OS_ASSERT_H */

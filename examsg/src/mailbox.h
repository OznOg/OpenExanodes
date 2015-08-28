/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#ifndef H_EXAMSG_MAILBOX
#define H_EXAMSG_MAILBOX

#include <stdarg.h>

typedef struct mbox_set mbox_set_t;

/**
 * \brief add a mailbox id to a mailbox set
 * \param[in] box_set   mailbox set
 * \param[in] id        mailbox id.
 */
void mboxset_add(mbox_set_t *box_set, ExamsgID id);

/**
 * \brief del a mailbox id from a mailbox set
 * \param[in] box_set   mailbox set
 * \param[in] id        mailbox id.
 */
void mboxset_del(mbox_set_t *box_set, ExamsgID id);

/**
 * \brief alloc a new _empty_ mailbox set
 * return the new mailboxset or NULL if alloc failed
 */
mbox_set_t *mboxset_alloc(const char *file, unsigned int line);

/**
 * \brief delete a mailbox set
 */
void mboxset_free(mbox_set_t *box_set);

/** Message delivery flags */
typedef enum ExamsgFlags {
  EXAMSGF_NONE =		0x0,    /**< no flag */
  EXAMSGF_NOBLOCK =		0x1,    /**< never wait */
} ExamsgFlags;

/** examsgMboxCreate
 * \brief Create a mailbox.
 *
 * \param[in] owner: the id of the owner of the box (this is needed because
 *                   one owner may have more than one mbox).
 * \param[in] id: mailbox id.
 * \param[in] num_msg   Maximum number of messages
 * \param[in] msg_size  Maximum message size
 * \return 0 on succes or a negative error code.
 */
int examsgMboxCreate(ExamsgID owner, ExamsgID mbox_id, size_t num_msg, size_t msg_size);

/** examsgMboxDelete
 * \brief Delete a mailbox.
 *
 * \param[in] id: mailbox id.
 * \return 0 on success or a negative error code.
 */
int examsgMboxDelete(ExamsgID id);

/** examsgMboxSend
 * \brief Write a message to a mailbox
 *
 * \param[in] to    Recipient id.
 * \param[in]...    Tuples (buffer, size) in which data is copied (split)
 *                  The parame size MUST BE EXPLICITLY CAST as a size_t.
 *                  calling this function __examsgMboxSend(to, buff, 10) IS A BUG
 *                  as 10 is cast in int and sizeof(int) != sizeof(size_t) on
 *                  64bit arch.
 *
 * \return number of bytes written, or a negative error code.
 */
int __examsgMboxSend(ExamsgID from, ExamsgID to, ExamsgFlags flags, ...);

#define examsgMboxSend(mid, from, to, buffer, nbytes) \
  __examsgMboxSend((from), (to), EXAMSGF_NOBLOCK, (mid), sizeof(*(mid)),\
	           (buffer), (size_t)(nbytes), NULL)


/** examsgMboxRecv
 * \brief Pass each message to a filter and return the first matching
 * message.
 *
 * \param[in] id	Mailbox id.
 * \param[out] mid	Sender id.
 * \param[in] buffer	Where to put data
 * \param[in] maxbytes	Maximum amout of data to read.
 *
 * \return size of message, or a negative error code
 */
int examsgMboxRecv(ExamsgID id, void *mid, size_t mid_size,
	           void *buffer, size_t maxbytes);

/** examsgMboxWait
 * \brief Wait for a message to be received in one of the maiboxes
 * listed in the watched array
 *
 * \param[in] waiter   The components which wishes to wait
 * \param[in] mbox_set mailboxes set of watched mailboxes
 * \param[in] timeout  Max amount to time to wait for a message
 *                     May be NULL, in this case there is no tiemout.
 *                     The tiemout value is updated upon return.
 *
 * return 0 in case of success or a negative error code.
 */
int examsgMboxWait(ExamsgID waiter, const mbox_set_t *mbox_set,
	           struct timeval *timeout);

/**
 * \brief Map the mailboxe space in the process context.
 * This function does not create it.
 * Once this fucntion is called, the mailboxes space is reachable.
 */
int examsgMboxMapAll(void);

/**
 * \brief Unmap mailboxes.
 * Once done, mailboxes cannot be accessed anymore.
 * By the way, mailboxe space is not deleted, just unmaped.
 */
void examsgMboxUnmapAll(void);

/**
 * \brief Create the memory space that holds the mailboxes.
 * This space is also mapped into memory (like done by
 * \ref examsgMboxMapAll )
 */
int examsgMboxCreateAll(void);

/**
 * \brief Destroy the memory space that holds mailboxes.
 */
void examsgMboxDeleteAll(void);

/**
 * examsgMboxShowStats
 * \brief Print all informations about mailboxes on stdout
 */
void examsgMboxShowStats(void);

#endif /* H_EXAMSG_MAILBOX */

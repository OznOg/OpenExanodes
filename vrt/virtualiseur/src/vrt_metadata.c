/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "vrt/virtualiseur/include/vrt_metadata.h"

#include "vrt/common/include/waitqueue.h"
#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_group.h"

#include "common/include/exa_error.h"
#include "common/include/threadonize.h"

#include "os/include/os_atomic.h"
#include "os/include/os_error.h"

#include "log/include/log.h"

#define VRT_METADATA_WRITE_DELAY_MSEC 5000


/**
 * Main function to be executed in the metadata thread.
 *
 * @param[in] data The group to metadata
 */
static void vrt_metadata_thread(void *data)
{
    struct vrt_group *group = (struct vrt_group *)data;
    int ret = EXA_SUCCESS;
    void *context = NULL;

    exalog_as(EXAMSG_VRT_ID);

    if (group->layout->group_metadata_flush_context_alloc)
    {
        context = group->layout->group_metadata_flush_context_alloc(group->layout_data);
        EXA_ASSERT(context != NULL);
    }

    while (!group->metadata_thread.ask_terminate)
    {
	/* Wait 5 seconds or termination. */
        bool more_work = false;
        int err = os_sem_waittimeout(&group->metadata_thread.sem,
                                     VRT_METADATA_WRITE_DELAY_MSEC);
        EXA_ASSERT(err == 0 || err == -EINTR || err == -ETIMEDOUT);

	os_atomic_inc(&group->metadata_thread.running);

        do {
 	    if (!group->metadata_thread.run
                || group->metadata_thread.ask_terminate)
	        break;

           /* Run metadata layout callback */
	    if (group->layout->group_metadata_flush_step)
	        ret = group->layout->group_metadata_flush_step(group->layout_data,
                                                          context, &more_work);
            else
	        ret = EXA_SUCCESS;
        } while (ret == EXA_SUCCESS && more_work);

	if (ret != EXA_SUCCESS)
	    exalog_info("Interrupted metadata flush of group %s", group->name);


        if (group->layout->group_metadata_flush_context_reset)
            group->layout->group_metadata_flush_context_reset(context);

	os_atomic_dec(&group->metadata_thread.running);
	wake_up_all(&group->metadata_thread.wq);
    }

    if (group->layout->group_metadata_flush_context_free)
        group->layout->group_metadata_flush_context_free(context);

    /* Decrement the group usage counter, which has been previously
       incremented in vrt_group_metadata() */
    vrt_group_unref(group);
}

/**
 * Start the metadata process for the given group.
 *
 * @param[in] group The group
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int
vrt_group_metadata_thread_start(struct vrt_group *group)
{
    exalog_debug("Create metadata thread for group '%s'", group->name);

    os_sem_init(&group->metadata_thread.sem, 0);
    group->metadata_thread.run = false;
    group->metadata_thread.ask_terminate = false;

    if (!exathread_create_named(&group->metadata_thread.tid,
                                VRT_THREAD_STACK_SIZE,
                                vrt_metadata_thread, group, "vrt_metadata"))
        return -EXA_ERR_DEFAULT;

    return EXA_SUCCESS;
}

void vrt_group_metadata_thread_resume(vrt_group_t *group)
{
    exalog_debug("Resuming metadata thread for group %s", group->name);
    group->metadata_thread.run = true;
    os_sem_post(&group->metadata_thread.sem);
    exalog_debug("Metadata thread for group %s resumed", group->name);
}

void vrt_group_metadata_thread_suspend(vrt_group_t *group)
{
    exalog_debug("Suspending metadata thread for group %s", group->name);

    group->metadata_thread.run = false;

    /* Wait for suspend to be effective */
    wait_event(group->metadata_thread.wq,
            os_atomic_read(&group->metadata_thread.running) == 0);

    exalog_debug("Metadata thread for group %s suspended", group->name);
}

/**
 * Stop the metadata process for the given group.
 *
 * @param[in] group The group
 */
void vrt_group_metadata_thread_cleanup(struct vrt_group *group)
{
    group->metadata_thread.ask_terminate = true;

    os_sem_post(&group->metadata_thread.sem);

    os_thread_join(group->metadata_thread.tid);

    os_sem_destroy(&group->metadata_thread.sem);
}



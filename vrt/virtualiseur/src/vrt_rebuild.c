/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */


#include "vrt/virtualiseur/include/vrt_rebuild.h"

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_realdev.h"
#include "vrt/virtualiseur/include/vrt_volume.h"
#include "vrt/virtualiseur/include/vrt_layout.h"
#include "vrt/virtualiseur/include/vrt_group.h"

#include "common/include/exa_error.h"
#include "common/include/threadonize.h"

#include "os/include/os_atomic.h"

#include "log/include/log.h"


/**
 * Main function to be executed in the rebuild thread.
 *
 * @param[in] data The group to rebuild
 */
static void vrt_rebuild_thread(void *data)
{
    struct vrt_group *group = (struct vrt_group *)data;
    void *context = NULL;

    exalog_as(EXAMSG_VRT_ID);

    if (group->layout->group_rebuild_context_alloc != NULL)
    {
        context = group->layout->group_rebuild_context_alloc(group);
        EXA_ASSERT(context != NULL);
    }

    /* FIXME this crap works only because we know that there is only one
     * thread that may wake up the rebuild thread. This code is full of races
     */
    while (!group->rebuild_thread.ask_terminate)
    {
        int err;
        bool work_to_do = false;

	/* wait signal */
        os_sem_wait(&group->rebuild_thread.sem);
	os_atomic_inc(&group->rebuild_thread.running);

        do
        {
            /* When group is suspended during rebuilding, the rebuild process must
             * be aborted to allow the recovery to be done, even if rebuild
             * failed at this point (maybe an IO error occured) */
 	    if (!group->rebuild_thread.run
                || group->rebuild_thread.ask_terminate)
            {
	        err = -VRT_ERR_REBUILD_INTERRUPTED;
                break;
            }

            if (group->layout->group_rebuild_step)
                err = group->layout->group_rebuild_step(context, &work_to_do);
            else
                err = EXA_SUCCESS;

        } while (err == EXA_SUCCESS && work_to_do);

        if (err != EXA_SUCCESS && err != -VRT_ERR_REBUILD_INTERRUPTED)
            exalog_error("Rebuilding of group '%s' failed: %s (%d)",
                         group->name, exa_error_msg(err), err);

        if (err != -VRT_ERR_REBUILD_INTERRUPTED
            && group->layout->group_rebuild_context_reset != NULL)
            group->layout->group_rebuild_context_reset(context);

	os_atomic_dec(&group->rebuild_thread.running);
	wake_up_all(&group->rebuild_thread.wq);
    }

    if (group->layout->group_rebuild_context_free != NULL)
        group->layout->group_rebuild_context_free(context);


    /* Decrement the group usage counter, which has been previously
       incremented in vrt_group_rebuild() */
    vrt_group_unref(group);
}


/**
 * Start the rebuilding process for the given group.
 *
 * @param[in] group The group
 *
 * @return EXA_SUCCESS on success, a negative error code on failure
 */
int
vrt_group_rebuild_thread_start(struct vrt_group *group)
{
    exalog_debug("Create rebuilding thread for group '%s'", group->name);

    group->rebuild_thread.tid = 0;
    group->rebuild_thread.ask_terminate = false;
    os_sem_init(&group->rebuild_thread.sem, 0);

    if (!exathread_create_named(&group->rebuild_thread.tid,
                                VRT_THREAD_STACK_SIZE,
                                vrt_rebuild_thread, group, "vrt_rebuild"))
	return -EXA_ERR_DEFAULT;

    return EXA_SUCCESS;
}

void vrt_group_rebuild_thread_resume(vrt_group_t *group)
{
    /* We shouldn't have this thread running on suspended groups */
    EXA_ASSERT(!group->suspended);

    exalog_debug("Resuming rebuild thread for group %s", group->name);
    group->rebuild_thread.run = true;
    os_sem_post(&group->rebuild_thread.sem);
    exalog_debug("Rebuild thread for group %s resumed", group->name);
}

void vrt_group_rebuild_thread_suspend(vrt_group_t *group)
{
    exalog_debug("Suspending rebuild thread for group %s", group->name);
    group->rebuild_thread.run = false;

    /* Wait for suspend to be effective */
    wait_event(group->rebuild_thread.wq,
            os_atomic_read(&group->rebuild_thread.running) == 0);
    exalog_debug("Rebuild thread for group %s suspended", group->name);
}


/**
 * Stop the rebuilding process for the given group.
 *
 * @param[in] group The group
 */
void vrt_group_rebuild_thread_cleanup(struct vrt_group *group)
{
    group->rebuild_thread.ask_terminate = true;

    os_sem_post(&group->rebuild_thread.sem);

    os_thread_join(group->rebuild_thread.tid);

    os_sem_destroy(&group->rebuild_thread.sem);
}



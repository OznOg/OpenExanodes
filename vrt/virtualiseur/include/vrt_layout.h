/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#ifndef __VRT_LAYOUT_H__
#define __VRT_LAYOUT_H__

#include "vrt/virtualiseur/include/storage.h"
#include "vrt/virtualiseur/include/vrt_group.h"

#include "vrt/assembly/src/assembly_group.h"

#include "vrt/common/include/vrt_stream.h"

#include "common/include/exa_constants.h"
#include "common/include/exa_nodeset.h"
#include "common/include/uuid.h"


/* Forward declaration */
struct vrt_volume;
struct vrt_request;

/** Global status of a virtualizer request (vrt_request_t). */
typedef enum
{
    /** VRT engine says to the layout that the IO is ok */
    VRT_REQ_SUCCESS,

    /** VRT engine says to the layout that the IO failed */
    VRT_REQ_FAILED,

    /** layout says (via build_io_for_req) to the VRT engine that it has more IO
     * to submit */
    VRT_REQ_UNCOMPLETED,

    /** layout says to the VRT engine to postpone this request. The VRT engine
     * will postpone this request until the layout call vrt_wakeup_request. */
    VRT_REQ_POSTPONED
} vrt_req_status_t;

/* FIXME: add comments about callbacks */
/**
 * Structure describing a layout, i.e a method for placing data on
 * physical devices.
 */
struct vrt_layout
{
    /** Chaining of registered layout */
    struct list_head list;

    /** Name of the layout */
    char name[EXA_MAXSIZE_LAYOUTNAME + 1];

    /* Group management callbacks */

    /**
     * Create a group in the layout.
     *
     * @param[in]   storage             The storage in which we'll create the group
     * @param[out]  private_data        The layout's private data
     * @param[in]   slot_width          The slot width
     * @param[in]   chunk_size          The chunk size
     * @param[in]   su_size             The striping unit size
     * @param[in]   dirty_zone_size     The dirty zone size
     * @param[in]   blended_stripes     Whether stripes are to be blended
     * @param[in]   nb_spare            Number of spares
     * @param[out]  error_msg           An error message, filled in case of error
     *
     * @return 0 if successful, a negative error code otherwise.
     */
    int (*group_create) (storage_t *storage, void **private_data,
                         uint32_t slot_width, uint32_t chunk_size, uint32_t su_size,
                         uint32_t dirty_zone_size, uint32_t blended_stripes,
                         uint32_t nb_spare, char *error_msg);

    /**
     * Start a group in the layout.
     *
     * @param[in]   group           The vrt_group FIXME Should disappear
     * @param[in]   storage         The storage in which we'll create the group
     *
     * @return 0 if successful, a negative error code otherwise.
     */
    int (*group_start)  (struct vrt_group *group, const storage_t *storage);

    /**
     * Stop a group in the layout.
     *
     * @param[in]  private_data    The layout's private data
     *
     * @return 0 if successful, a negative error code otherwise.
     */
    int (*group_stop)   (void *private_data);

    /**
     * Serialize a group's layout data.
     *
     * @param[in] private_data  The layout's private data
     * @param     stream        Stream to write to
     *
     * @return 0 if successful, a negative error code otherwise
     */
    int (*serialize)(const void *private_data, stream_t *stream);

    /**
     * Deserialize a group's layout data.
     *
     * @param[out] private_data  The layout's deserialized private data
     * @param     storage        The storage
     * @param     stream         Stream to read from
     *
     * @return 0 if successful, a negative error code otherwise
     */
    int (*deserialize)(void **private_data, const storage_t *storage,
                       stream_t *stream);

    /**
     * Size in bytes of a layout's serialized private data
     *
     * @param[in] private_data  The layout's private data
     *
     * @return size in bytes
     */
    uint64_t (*serialized_size)(const void *private_data);

    /**
     * Get a group's assembly group.
     *
     * @param[in] private_data          The layout's private data
     *
     *@return the assembly_group
     */
    const assembly_group_t *(*get_assembly_group)(const void *private_data);

    /**
     * Verifies whether two layout datas are equal
     *
     * @param[in] private_data1          The first private data
     * @param[in] private_data2          The second private data
     *
     *@return true if both are equal
     */
    bool (*layout_data_equals)(const void *private_data1, const void *private_data2);

    /**
     * Cleanup a group's layout data.
     *
     * @param[out]  private_data        The layout's private data
     */
    void (*group_cleanup)(void **private_data, const storage_t *storage);

    /**
     * Allocate a layout's metadata flush context. This is used by the layouts
     * to keep track of their progress flushing their metadata between successive
     * calls to group_metadata_flush_step().
     *
     * @param[in] private_data  The layout's data
     *
     * @return their initialised context
     */
    void *(*group_metadata_flush_context_alloc) (void *private_data);

    /**
     * Free a layout's metadata flush context.
     *
     * @param[in,out] context  The context to free
     */
    void (*group_metadata_flush_context_free) (void *context);

    /**
     * Reset a layout's metadata flush context. This is used to reset the
     * metadata flush context so that the next call to group_metadata_flush_step()
     * starts from zero.
     *
     * @param[in] context  The context to free
     */
    void (*group_metadata_flush_context_reset) (void *context);

    /**
     * Ask the layout's to flush metadata, step by step. The value of "step" is
     * up to the layout, but it has to be "fast enough" to be suspendable or
     * resumed in a timely manner, and it has to be safe against groups' content
     * modification (volume create, resize, delete, ...)
     *
     * @param[in] private_data  The layout's data
     * @param[in] context       The layout's metadata context
     * @param[in] more_work     Whether there's still flushing to do
     *
     * @return EXA_SUCCESS if the layout finished its flush step,
     * or a negative error code.
     */
    int (*group_metadata_flush_step) (void *private_data, void *context,
                                      bool *more_work);

    int (*group_reset) (void *private_data);
    int (*group_check) (void *private_data);

    /* Logical (sub)space management */
    int (*create_subspace)(void *layout_data, const exa_uuid_t *uuid,
                           uint64_t size, struct assembly_volume **av,
                           storage_t *storage);
    void (*delete_subspace)(void *layout_data, struct assembly_volume **av,
                            storage_t *storage);

    /* Volume management callbacks */

    int (*volume_create)(void *private_data, struct vrt_volume *volume, uint64_t size);
    int (*volume_delete)(void *private_data, struct vrt_volume *volume);
    int (*volume_resize)(struct vrt_volume *volume, uint64_t newsize,
                         const storage_t *storage);
    int (*volume_get_status)(const struct vrt_volume *volume);
    uint64_t (*volume_get_size)(const struct vrt_volume *volume);

    /* Real devices management callbacks */

    int (*group_rdev_up)         (vrt_group_t *group);
    int (*group_rdev_down)       (vrt_group_t *group);
    int (*group_insert_rdev)	        (void *group_layout_data,
                                         struct vrt_realdev *rdev);

    void (*rdev_reset)		        (const void *group_layout_data,
                                         const struct vrt_realdev *rdev);
    int (*rdev_reintegrate)		(vrt_group_t *group, vrt_realdev_t *rdev);
    int (*rdev_post_reintegrate)  	(vrt_group_t *group, vrt_realdev_t *rdev);
    int (*rdev_get_reintegrate_info)	(const void *group_layout_data,
                                         const struct vrt_realdev *rdev,
					 bool *need_reintegrate);

    int (*rdev_get_rebuild_info)	(const void *group_layout_data,
                                         const struct vrt_realdev *rdev,
					 uint64_t *logical_rebuilt_size,
					 uint64_t *logical_size_to_rebuild);


    exa_realdev_status_t (*rdev_get_compound_status)	(const void *group_layout_data,
                                        const struct vrt_realdev *rdev);
    /* Compute status */

    int (*group_compute_status) (struct vrt_group *group);
    int (*group_going_offline)  (const struct vrt_group *group, const exa_nodeset_t *stop_nodes);

    /* Resync/rebuild callbacks */

    int (*group_resync)     	(struct vrt_group *group, const exa_nodeset_t *nodes);
    int (*group_post_resync)	(void *layout_data);
    bool (*group_is_rebuilding)	(const void *layout_data);

    /**
     * Allocate a layout's rebuild context. This is used by the layouts
     * to keep track of their progress rebuilding between successive
     * calls to group_rebuild_step().
     *
     * @param[in] group  The vrt group
     *
     * @return their initialised context
     */
    void *(*group_rebuild_context_alloc) (struct vrt_group *group);

    /**
     * Free a layout's rebuild context.
     *
     * @param[in,out] context  The context to free
     */
    void (*group_rebuild_context_free) (void *context);

    /**
     * Reset a layout's rebuild context. This is used to reset the
     * rebuild context so that the next call to group_rebuild_step()
     * starts from zero.
     *
     * @param[in] context  The context to reset
     */
    void (*group_rebuild_context_reset) (void *context);

    /**
     * Ask the layout's to rebuild its storage, step by step. The value of "step" is
     * up to the layout, but it has to be "fast enough" to be suspendable or
     * resumed in a timely manner, and it has to be safe against groups' content
     * modification (volume create, resize, delete, ...)
     *
     * @param[in] context       The layout's rebuild context
     * @param[out] more_work    Whether the rebuild is finished
     *
     * @return EXA_SUCCESS if the layout finished its rebuild step successfully,
     * or a negative error code.
     */
    int (*group_rebuild_step) (void *context, bool *more_work);

    /**
     * Declare the number of parallel I/O needed by the layout to
     * perform the request.
     *
     * @param[in] vrt_req   The request header describing the current
     * request
     *
     * @param[out] io_count The number of needed I/O
     *
     * @param[out] sync_afterward  Ask to perform a barrier once IO is done
     */
    void (*declare_io_needs)(struct vrt_request *vrt_req,
			     unsigned int *io_count,
			     bool *sync_afterward);

    /**
     * Initialize a request. Called before the first call of
     * build_io_for_req(). In particular, it can be used to initialize
     * the private_data field of the vrt_request structure.
     *
     * @param[in] vrt_req The request to initialize.
     */
    void (*init_req)(struct vrt_request *vrt_req);

    /**
     * Fill the list of I/O in the given request header to perform the
     * request. The status returned by this method will allow the
     * virtualizer engine to know which actions to perform:
     *
     * - status VRT_REQ_SUCCESS means that no more I/O are needed to
     *   finalize the request, and that the engine can return success
     *   to the process that made the original request.
     *
     * - status VRT_REQ_FAILED means that the I/O cannot be performed,
     *   because not enough devices are available to do so. This can
     *   only happen for read requests when the group is in error.
     *
     * - status VRT_REQ_UNCOMPLETED informs the virtualizer engine that
     *   the I/O list has been changed to describe new I/Os that must be
     *   executed.
     *
     * It is important to know that this method is called with the
     * group->suspend_lock read-acquired. This lock protects against a
     * change of group and realdevs' status.
     *
     * @param[in] vrt_req The vrt_request that describes the request for
     * which the I/O list must be filled
     *
     * @return Either VRT_REQ_SUCCESS, VRT_REQ_FAILED or
     * VRT_REQ_UNCOMPLETED.
     */
    vrt_req_status_t (*build_io_for_req)(struct vrt_request *vrt_req);

    /**
     * Cancel a request. This method is called when a request must be
     * re-initialized by the virtualizer engine. It should restore the
     * request in a state similar to the state of a request initialized
     * through the init_req() method. This method can be called several
     * times on the same request, so it must be resistant to this.
     *
     * @param[in] vrt_req The request to reinitialize
     */
    void (*cancel_req)(struct vrt_request *vrt_req);

    /**
     * Callback to get the slot width
     *
     * @param[in]  private_data The layout's data
     */
    uint32_t (*get_slot_width)(const void *private_data);

    /**
     * Callback to get the number of spares
     *
     * @param[in]  private_data The layout's data
     * @param[out] nb_spare     Pointer to the number of spare
     * @param[out] nb_spare_available
     *                          Pointer to the number of spare available
     */
    void (*get_nb_spare)(const void *private_data, int *nb_spare, int *nb_spare_available);

    /**
     * Callback to get the size of a striping unit (in sectors)
     *
     * @param[in]  private_data The layout's data
     */
    uint32_t (*get_su_size)(const void *private_data);

    /**
     * Callback to get the size of a dirty zone (in sectors)
     *
     * @param[in]  private_data The layout's data
     */
    uint32_t (*get_dirty_zone_size)(const void *private_data);

    /**
     * Callback to know if placement uses blended stripes
     *
     * @param[in]  private_data The layout's data
     */
    uint32_t (*get_blended_stripes)(const void *private_data);

    /**
     * Asks the layout the total capacity available in a group.
     *
     * @param[in]  private_data The layout's data
     *
     * return the total capacity available in the group in bytes.
     */
    uint64_t (*get_group_total_capacity)(const void *private_data,
                                         const storage_t *storage);

    /**
     * Asks the layout the capacity used in a group.
     *
     * @param[in]  private_data The layout's data
     *
     * return the capacity used in the group in bytes.
     */
    uint64_t (*get_group_used_capacity)(const void *private_data);
};

const struct vrt_layout *vrt_get_layout(const char *name);

int  vrt_register_layout  (struct vrt_layout *layout);
void vrt_unregister_layout(struct vrt_layout *layout);

#endif /* __VRT_LAYOUT_H__ */

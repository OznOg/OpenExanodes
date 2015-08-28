/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "common/include/exa_error.h"

#include "log/include/log.h"

#include "vrt/virtualiseur/include/constantes.h"
#include "vrt/virtualiseur/include/vrt_layout.h"

#include "os/include/os_error.h"
#include "os/include/os_string.h"

/** List of all registered layouts */
static LIST_HEAD(layouts);

/**
 * Registers a layout, so that groups using this layout can be
 * handled.
 *
 * @param[in] new_layout The layout to register
 *
 * @return EXA_SUCCESS on success, -EEXIST if a layout with the same
 * type is already registered.
 */
int
vrt_register_layout(struct vrt_layout *new_layout)
{
    struct list_head *pos, *n;

    /* Check whether a layout with the same type already exists */
    list_for_each_safe(pos, n, &layouts)
    {
	struct vrt_layout *layout = list_entry(pos, list, struct vrt_layout);
	if (strncmp(layout->name, new_layout->name, EXA_MAXSIZE_LAYOUTNAME) == 0)
	{
	    exalog_error("A layout '%s' is already registered", layout->name);
	    return -EEXIST;
	}
    }

    /* Register the layout in the list */
    list_add(&new_layout->list, &layouts);

    return EXA_SUCCESS;
}


/**
 * Unregisters a layout.
 *
 * @param[in] layout The layout to unregister
 */
void
vrt_unregister_layout(struct vrt_layout *layout)
{
    EXA_ASSERT(!list_empty(&layout->list));
    list_del(&layout->list);
}


/**
 * Find a layout given its name.
 *
 * @param[in] name Name of the layout
 *
 * @return A pointer to the corresponding struct vrt_layout on success,
 * NULL on failure.
 */
const struct vrt_layout *vrt_get_layout(const char *name)
{
    struct list_head *pos, *n;

    list_for_each_safe(pos, n, &layouts)
    {
	struct vrt_layout *layout = list_entry(pos, list, struct vrt_layout);
	if (os_strcasecmp(name, layout->name) == 0)
	    return layout;
    }

    exalog_error("Layout '%s' not found", name);

    return NULL;
}

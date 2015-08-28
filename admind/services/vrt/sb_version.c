/*
 * Copyright 2002, 2010 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/services/vrt/sb_version.h"

#include "common/include/exa_assert.h"
#include "common/include/exa_env.h"

#include "os/include/os_dir.h"
#include "os/include/os_error.h"
#include "os/include/os_file.h"
#include "os/include/os_filemap.h"
#include "os/include/os_mem.h"

#include <string.h> /* for memset */

#define SB_METADATA_FORMAT      \
    "format_version=%d\n"       \
    "sb_version=%"PRIu64"\n"    \
    "next_version=%"PRIu64"\n"  \
    "pending_step=%u\n"         \
    "pending_version=%"PRIu64"\n"

/* 4096 is bigger than needed, but Windows wants at least 4KB to be
 * able to map the file.
 */
#define SB_METADATA_FILE_SIZE 4096
#define SB_METADATA_FORMAT_VERSION 1


typedef enum
{
#define STEP__FIRST  SBV_PREPARED
    SBV_PREPARED = 1,
    SBV_DONE,
    SBV_COMMITED
#define STEP__LAST  SBV_COMMITED
} step_t;
#define STEP_IS_VALID(x) ((x) >= STEP__FIRST && (x) <= STEP__LAST)

typedef struct {
    uint64_t version; /*< current version */
    /* Next value for version if a new transition is requested.*/
    uint64_t next_version;
    struct {
        /* step in transaction */
        step_t step;
        uint64_t version; /*< version that is currently being committed */
    } pending;
} sb_metadata_t;

struct sb_version
{
    os_fmap_t *fmap;
    sb_metadata_t metadata;
};

static int sb_metadata_to_text(const sb_metadata_t *metadata, char *text,
                               size_t size)
{
    int r;

    memset(text, 0, size);
    r = os_snprintf(text, size, SB_METADATA_FORMAT,
            SB_METADATA_FORMAT_VERSION,
            metadata->version,
            metadata->next_version,
            metadata->pending.step,
            metadata->pending.version);
    if (r >= size)
        return -ENOSPC;

    return r;
}

static int sb_metadata_from_text(sb_metadata_t *metadata, const char *text)
{
    int r;
    int format_version;
    uint64_t version, next_version, pending_version;
    step_t pending_step;

    r = sscanf(text, SB_METADATA_FORMAT, &format_version, &version,
               &next_version, &pending_step, &pending_version);

    if (r < 5)
        return -EINVAL;

    if (format_version != SB_METADATA_FORMAT_VERSION)
        return -EINVAL;

    if (!STEP_IS_VALID(pending_step))
        return -EINVAL;

    metadata->version = version;
    metadata->next_version = next_version;
    metadata->pending.step = pending_step;
    metadata->pending.version = pending_version;

    return 0;
}


int sb_version_remove_directory(void)
{
    char dir[OS_PATH_MAX];

    exa_env_make_path(dir, sizeof(dir), exa_env_cachedir(), "groups");
    return os_dir_remove(dir);
}

/**
 * Create an sb_version_t struct filled according to the parameters:
 *
 * @param[in] uuid      the UUID of the group for which to load the sb_version
 * @param[in] valid     whether the created sb_version should be valid or not.
 *                      Invalid sb_versions are used to force re-synchronisation
 *                      from a better node.
 *
 * @return the sb_version, or NULL if an error happened.
 */
static sb_version_t *__sb_version_new(const exa_uuid_t *uuid, bool valid)
{
    char dir[OS_PATH_MAX];
    char path[OS_PATH_MAX];
    char contents[SB_METADATA_FILE_SIZE];
    exa_uuid_str_t str_uuid;
    int err;

    sb_version_t *sb_version = os_malloc(sizeof(sb_version_t));
    if (sb_version == NULL)
        return NULL;

    err = exa_env_make_path(dir, sizeof(dir), exa_env_cachedir(), "groups");
    if (err == 0)
        err = os_dir_create_recursive(dir);
    if (err == 0)
    {
        /* Use underscores in place of colons, as colons are illegal
         * in filenames on Windows. */
        uuid2str_with_sep(uuid, '_', str_uuid);
        err = exa_env_make_path(path, sizeof(path), dir, str_uuid);
    }
    if (err != 0)
    {
        os_free(sb_version);
        return NULL;
    }

    sb_version->fmap = os_fmap_create(path, sizeof(contents));
    if (sb_version->fmap == NULL)
    {
        os_free(sb_version);
        return NULL;
    }

    if (valid)
    {
        sb_version->metadata.version = 1;
        sb_version->metadata.next_version = 2;
        sb_version->metadata.pending.step = SBV_PREPARED;
        sb_version->metadata.pending.version = 2;
    }
    else
    {
        sb_version->metadata.version = 0;
        sb_version->metadata.next_version = 0;
        sb_version->metadata.pending.step = SBV_PREPARED;
        sb_version->metadata.pending.version = 0;
    }

    err = sb_metadata_to_text(&sb_version->metadata, contents,
                             sizeof(contents));
    if (err < 0)
    {
        sb_version_unload(sb_version);
        return NULL;
    }
    os_fmap_write(sb_version->fmap, 0, contents, sizeof(contents));

    return sb_version;
}

sb_version_t *sb_version_new(const exa_uuid_t *uuid)
{
    return __sb_version_new(uuid, true);
}

sb_version_t *sb_version_new_invalid(const exa_uuid_t *uuid)
{
    return __sb_version_new(uuid, false);
}

sb_version_t *sb_version_load(const exa_uuid_t *uuid)
{
    char path[OS_PATH_MAX];
    char contents[SB_METADATA_FILE_SIZE];
    exa_uuid_str_t str_uuid;
    int err;

    sb_version_t *sb_version = os_malloc(sizeof(sb_version_t));
    if (sb_version == NULL)
        return NULL;

    /* Use underscores in place of colons, as colons are illegal
     * in filenames on Windows. */
    if (uuid2str_with_sep(uuid, '_', str_uuid) == NULL)
        err = -ENOMEM;
    else
        err = exa_env_make_path(path, sizeof(path), exa_env_cachedir(),
                                "groups%s%s", OS_FILE_SEP, str_uuid);

    if (err != 0)
    {
        os_free(sb_version);
        return NULL;
    }

    sb_version->fmap = os_fmap_open(path, sizeof(contents), FMAP_RDWR);
    if (sb_version->fmap == NULL)
    {
        os_free(sb_version);
        return NULL;
    }

    err = os_fmap_read(sb_version->fmap, 0, contents, sizeof(contents));
    if (err < sizeof(contents))
    {
        sb_version_unload(sb_version);
        return NULL;
    }

    err = sb_metadata_from_text(&sb_version->metadata, contents);
    if (err < 0)
    {
        sb_version_unload(sb_version);
        return NULL;
    }

    return sb_version;
}

void sb_version_unload(sb_version_t *sb_version)
{
    EXA_ASSERT(sb_version->fmap != NULL);
    os_fmap_close(sb_version->fmap);
    os_free(sb_version);
}

void sb_version_delete(sb_version_t *sb_version)
{
    EXA_ASSERT(sb_version->fmap != NULL);
    os_fmap_delete(sb_version->fmap);
    os_free(sb_version);
}

static void sb_version_write_metadata(sb_version_t *sb_version)
{
    char contents[SB_METADATA_FILE_SIZE];
    int err;

    err = sb_metadata_to_text(&sb_version->metadata, contents, sizeof(contents));
    EXA_ASSERT(err >= 0);
    os_fmap_write(sb_version->fmap, 0, contents, sizeof(contents));
}

uint64_t sb_version_new_version_prepare(sb_version_t *sb_version)
{
    sb_version->metadata.pending.version = sb_version->metadata.next_version;
    /* If a new commit is asked this will ensure, that the next version is
     * NOT the same as the pending one, even if the pending abort */
    sb_version->metadata.next_version++;

    sb_version->metadata.pending.step = SBV_PREPARED;

    sb_version_write_metadata(sb_version);

    return sb_version->metadata.pending.version;
}

void sb_version_new_version_done(sb_version_t *sb_version)
{
    sb_version->metadata.pending.step = SBV_DONE;

    sb_version_write_metadata(sb_version);
}

void sb_version_new_version_commit(sb_version_t *sb_version)
{
    sb_version->metadata.pending.step = SBV_COMMITED;
    sb_version->metadata.version = sb_version->metadata.pending.version;

    sb_version_write_metadata(sb_version);
}

uint64_t sb_version_get_version(const sb_version_t *sb_version)
{
    return sb_version->metadata.version;
}

bool sb_version_is_valid(const sb_version_t *sb_version)
{
    return sb_version->metadata.version > 0;
}

void sb_version_local_recover(sb_version_t *sb_version)
{
    switch (sb_version->metadata.pending.step)
    {
    case SBV_PREPARED:
        /* Just do nothing; the operation of commit failed, that's all,
         * the previous version remains valid. */
        break;

    case SBV_DONE:
        /* Commit did not complete, but we are certain that all nodes have
         * written superblocks so we can commit */
        sb_version_new_version_commit(sb_version);
        break;

    case SBV_COMMITED:
        /* No pb, this is nominal state. */
        break;
    }
}

void sb_version_serialize(const sb_version_t *sb_version, sb_serialized_t *buffer)
{
    COMPILE_TIME_ASSERT(sizeof(sb_serialized_t) >= sizeof(sb_version->metadata));

    /* XXX It would probably be more optimal to only send version and
     * next_version, but I keep it like that because it may be easy to debug
     * will full metadata */
    memcpy(buffer, &sb_version->metadata, sizeof(sb_version->metadata));
}

static bool sb_metadata_matches(const sb_metadata_t *md1, const sb_metadata_t *md2)
{
    return md1->version == md2->version
           && md1->next_version == md2->next_version;
}

void sb_version_update_from(sb_version_t *sb_version, const sb_serialized_t *buffer)
{
    const sb_metadata_t *metadata = (const sb_metadata_t *)buffer;

    /* FIXME handle wrapping */

    /* If metadata contains outdated information from our point of view,
     * just ignore. This just means that the peer previously crashed while
     * updating its own data. */
    if (metadata->version < sb_version->metadata.version
        || metadata->next_version < sb_version->metadata.next_version)
        return;

    /*
     * Take the highest next_version known cluster wide. A node may have
     * crashed during a transaction and the others crashed during the next one
     */
    if (sb_version->metadata.next_version < metadata->next_version)
    {
        sb_version->metadata.next_version = metadata->next_version;
        sb_version_write_metadata(sb_version);
    }

    /* If the node has a ancient version, simply update, no need to check the
     * local content. */
    if (metadata->version > sb_version->metadata.version)
    {
        sb_version->metadata = *metadata;
        sb_version_write_metadata(sb_version);
    }
    else
        EXA_ASSERT(sb_metadata_matches(metadata, &sb_version->metadata));
}


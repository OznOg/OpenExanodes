/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */
#include <stdlib.h>
#include <string.h>

#include "common/include/exa_assert.h"
#include "admind/src/rpc_command.h"

/**
 * List of loacal commands that can be called thru the RPC framework
 */
static LocalCommand rpc_commands[RPC_COMMAND_LAST+1];

/**
 * \brief Initialize all the pointers to the local commands processing func of
 * each commands.
 */
void
rpc_command_resetall(void)
{
  /* Make sure to fill the local command table with NULLs */
  memset(&rpc_commands, 0, sizeof(rpc_commands));
}

/*----------------------------------------------------------------------------*/
/** \brief return a pointer to the local command cmd_id
 *
 * \param cmd_id the command id is the index in the local command table
 * \return a local command or NULL
 */
/*----------------------------------------------------------------------------*/
LocalCommand
rpc_command_get(rpc_command_t cmd_id)
{

  if (cmd_id < RPC_COMMAND_FIRST || cmd_id > RPC_COMMAND_LAST)
    return NULL;

  return rpc_commands[cmd_id];
}

/**
 * Set a LocalCommand for a given cmd_id.
 * The function ASSERT if the user tries to give 2 functions for an id
 * or for an id out of range.
 */
void
rpc_command_set(rpc_command_t cmd_id, LocalCommand fct)
{
  EXA_ASSERT_VERBOSE(rpc_commands[cmd_id] == NULL,
                     "RPC command id=%d already set @=%p",
		     cmd_id, rpc_commands[cmd_id]);

  EXA_ASSERT_VERBOSE(cmd_id >= RPC_COMMAND_FIRST && cmd_id <= RPC_COMMAND_LAST,
                     "Value %d out of range [%d-%d]",
		     cmd_id, RPC_COMMAND_FIRST, RPC_COMMAND_LAST);

  rpc_commands[cmd_id] = fct;
}


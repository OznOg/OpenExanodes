/*
 * Copyright 2002, 2009 Seanodes Ltd http://www.seanodes.com. All rights
 * reserved and protected by French, UK, U.S. and other countries' copyright laws.
 * This file is part of Exanodes project and is subject to the terms
 * and conditions defined in the LICENSE file which is present in the root
 * directory of the project.
 */

#include "admind/src/cli_server.h"

#include "os/include/os_error.h"
#include "os/include/os_thread.h"
#include "os/include/os_network.h"
#include "os/include/os_mem.h"

#include "common/include/threadonize.h"
#include "common/include/exa_constants.h"
#include "common/include/exa_error.h"

#include "log/include/log.h"

#include "admind/src/adm_command.h"
#include "admind/src/adm_workthread.h"

/* Import XML library support */
#include "admind/src/xml_proto/xml_proto_api.h"

/* We won't accept commands biggers than this.  1024 bytes is an
 * estimation of the max size needed per node for the exa_clcreate xml
 * command, which is by far the largest command we have.
 */
#define PROTOCOL_BUFFER_MAX_SIZE EXA_MAX_NODES_NUMBER * 1024

/* Will realloc PROTOCOL_BUFFER_LEN chunks to receive large XML
 * commands */
#define PROTOCOL_BUFFER_LEN 1024

/* Number of accepted connection on the XML protocol side. */
#define MAX_CONNECTION 10

/** Timeout value (in us) of the examsgWait() call; this means that the tcp
 * socket is checked every EXAMSG_TIMEOUT */
#define EXAMSG_TIMEOUT 100000

/* Array of connected sockets so we know who we are talking to */
typedef struct
{
  int fd;        /**< fd of the client socket */
  cmd_uid_t uid; /**< uid of the command running */
} connection_t;

static connection_t connectlist[MAX_CONNECTION];

static os_thread_t thr_xml_proto;

static ExamsgHandle cli_mh;
static int listen_fd;
static fd_set setSocks;

static bool stop = false;

static void write_result(int msg_sock, const char *result);

static void cli_command_end_complete(int sock_fd,
                                     const cl_error_desc_t *err_desc);

static int
send_command_to_evmgr(cmd_uid_t uid, const AdmCommand *adm_cmd,
                      const exa_uuid_t *cluster_uuid,
		      const void *data, size_t data_size)
{
  Examsg msg;
  command_t *cmd = (command_t *)msg.payload;
  size_t payload_size;
  int s;

  /* Message to trigger the command execution on thread thr_nb */
  msg.any.type = EXAMSG_ADM_CLUSTER_CMD;

  payload_size = data_size + sizeof(command_t);

  if (payload_size >= EXAMSG_PAYLOAD_MAX)
    {
      exalog_error("Command %s parameter structure is too big to fit in "
          "a message. %zu > %zu", adm_cmd->msg, payload_size, EXAMSG_PAYLOAD_MAX);
      return -E2BIG;
    }

  cmd->code = adm_cmd->code;
  cmd->uid  = uid;
  memcpy(cmd->data, data, data_size);
  cmd->data_size = data_size;
  uuid_copy(&cmd->cluster_uuid, cluster_uuid);

  exalog_debug("Scheduling command '%s', uid=%d", adm_cmd->msg, cmd->uid);

  s = examsgSend(cli_mh, EXAMSG_ADMIND_EVMGR_ID, EXAMSG_LOCALHOST,
                 &msg, sizeof(ExamsgAny) + payload_size);
  if (s != sizeof(ExamsgAny) + payload_size)
    return s;

  return 0;
}

static char *
cli_peer_from_fd(int fd)
{
  struct sockaddr_in addr;
  int len = sizeof(addr);
  os_getpeername(fd, (struct sockaddr *)&addr, &len);
  return os_inet_ntoa(addr.sin_addr);
}

/** \brief Return a descritor on a socket that allow us to wait GUI/CLI Request
 *
 * \param[in] port: The IP port to wait on
 * \return Return a descritor on a socket if > 0 or a negative error code
 */
static int
listen_socket_port(int port)
{
  int sd;
  struct sockaddr_in sin;
  int autorisation;
  int retval;

  /* get an internet domain socket */
  if ((sd = os_socket (AF_INET, SOCK_STREAM, 0)) < 0)
    return sd;

  /* Change socket options to be able to restart the daemon right
   * after it has been stopped -- otherwise, bind() would block for
   * 60 secs because of TCP */
  autorisation = 1;
  os_setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &autorisation, sizeof(int));

  /* complete the socket structure */
  memset(&sin, 0, sizeof (sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons (port);

  /* bind the socket to the port number */
  retval = os_bind(sd, (struct sockaddr *) &sin, sizeof (sin));
  if (retval < 0)
    return retval;

  /* show that we are willing to listen */
  retval = os_listen (sd, 5);
  if (retval < 0)
    return retval;

  return sd;
}

/** select a free connection
 *
 * \param[in] sock_xml: socket on which it receives commands.
 */
static void
handle_connection(int msg_sock)
{
  int i;
  static cl_error_desc_t busy = { .code = -EXA_ERR_ADM_BUSY,
                                  .msg  = "Too many connections."};

  /* Search free slot in connection array */
  for (i = 0; i < MAX_CONNECTION; i++)
    {
      if (connectlist[i].uid == CMD_UID_INVALID && connectlist[i].fd == -1)
	{
	  exalog_debug("CONNECTION %d: Using sock %d", i, msg_sock);

	  /* uid is set when the command is actually scheduled */
	  connectlist[i].fd = msg_sock;
	  FD_SET(msg_sock, &setSocks);
	  return;
	}
    }

  /*
   * No free connection found. We disconnect the caller now because we
   * are no room in our connectionlist
   */
  exalog_error("All connections are busy sock=%d", msg_sock);

  cli_command_end_complete(msg_sock, &busy);
  os_closesocket(msg_sock);
}

/*-------------------------------------------------------------------------*/
/** \brief Connection thread: wait on xml message to pass the command
 * to the work thread.
 *
 * \param[in] sock_xml_proto: socket xml on which it receives commands.
 * \return the selected working thread or a negative error code
 *
 */
/*-------------------------------------------------------------------------*/
static void
accept_new_client(void)
{
  int fd = os_accept(listen_fd, NULL, NULL);

  exalog_debug("Got an incoming XML Request on socket: %d", fd);
  if (fd < 0)
    {
      exalog_error("Failed during admind xml connection accept: error=%s",
                   exa_error_msg(-fd));
      return;
    }

  handle_connection(fd);
}


/**
 * \brief receive a data from a socket.
 * When returning EXA_SUCCESS, xml_tree points on a valid xml tree.
 *
 * \param[in]  sock_fd   socket file descriptor of the incoming data
 * \param[out] xml_tree  Pointer on the xmlDocPtr pointing on a XML tree
 *                          extracted from the data read on the socket.
 *
 * \return EXA_SUCCESS or a negative error.
 */
static int
receive(int sock_fd, char **buffer)
{
  int current_read = 0;
  char *buffer_receiving = NULL;

  EXA_ASSERT(buffer);

  *buffer = NULL;

  exalog_debug("SOCKET %d: XML Message processing", sock_fd);

  /* We read and merge each chunk of incomming data in the hope to get
   * the END of command. Here the input buffer is sized as to handle
   * all the command in one chunk, so not having the end of command is
   * an error. */
  do
    {
      int nb_car;
      char *new_buffer_receiving;

      if (current_read > PROTOCOL_BUFFER_MAX_SIZE)
	{
	  cl_error_desc_t error;
	  set_error(&error, -EXA_ERR_CMD_PARSING,
	           "Received command is bigger than our internal limit (%d bytes)",
		   PROTOCOL_BUFFER_MAX_SIZE);

	  exalog_error("Socket %d peer '%s': %s", sock_fd,
	               cli_peer_from_fd(sock_fd), error.msg);

	  cli_command_end_complete(sock_fd, &error);
	  /* Don't need the receiving buffer any more */
	  os_free(buffer_receiving);
	  return -EXA_ERR_CMD_PARSING;
	}

      new_buffer_receiving = os_realloc(buffer_receiving,
                                        PROTOCOL_BUFFER_LEN + current_read
                                        + sizeof(char)  /* for '\0' */);
      if (!new_buffer_receiving)
	{
	  os_free(buffer_receiving);
	  exalog_error("Socket %d peer '%s': Failed to realloc()", sock_fd,
	               cli_peer_from_fd(sock_fd));
	  return -ENOMEM;
	}

      buffer_receiving = new_buffer_receiving;

      do
	nb_car = os_recv(sock_fd, buffer_receiving + current_read,
			 PROTOCOL_BUFFER_LEN, 0);
      while (nb_car == -EINTR);

      if (nb_car == 0)
	{
	  exalog_debug("SOCKET %d: Error = Client did disconnect itself",
		       sock_fd);

	  os_free(buffer_receiving);
	  return -ECONNABORTED;
	}

      if (nb_car < 0 && errno != EAGAIN)
	{
	  if (nb_car == -ECONNRESET)
	    exalog_debug("SOCKET %d: Error %d", sock_fd, nb_car);
	  else
	    exalog_error("Socket %d peer '%s': %s (%d)", sock_fd,
		         cli_peer_from_fd(sock_fd), exa_error_msg(nb_car), nb_car);

	  os_free(buffer_receiving);
	  return nb_car;
	}

      if(nb_car > 0)
	current_read += nb_car;

      /* Make sure to be properly ended */
      buffer_receiving[current_read] = '\0';

      /* Make sure the XML input is complete */
    } while(xml_buffer_is_partial(buffer_receiving));

  exalog_debug("SOCKET %d: Receive Message lg = %" PRIzu " :  [%s]",
	       sock_fd, strlen(buffer_receiving), buffer_receiving);
  *buffer = buffer_receiving;

  return EXA_SUCCESS;
}

/*-------------------------------------------------------------------------*/
/** \brief Close a connection.
 *
 * \param[in] connection_id
 */
/*-------------------------------------------------------------------------*/
static void
close_connection(int connection_id)
{
  int cli_fd = connectlist[connection_id].fd;

  EXA_ASSERT_VERBOSE(cli_fd > 2 , "fd = %d", cli_fd);

  FD_CLR(cli_fd, &setSocks);
  os_closesocket(cli_fd);
  exalog_debug("CONNECTION: %d  Socket %d closed state_work NOT_USED",
	       connection_id, cli_fd);

  /* Do not reset uid and free field because there may be a command still
   * running on the worker thread. Those fields will be reset when it ends. */
  connectlist[connection_id].fd = -1;
}

/**
 * Process a connection that has incoming data.
 *
 * \param[in] conn_id  Connection id
 * \param[in] sock_fd  Connection socket
 */
static void
handle_inputdata(int conn_id, int sock_fd)
{
  char *buffer = NULL;
  void *data = NULL;
  size_t data_size;
  adm_command_code_t cmd_code;
  const struct AdmCommand *command;
  exa_uuid_t cluster_uuid;
  cl_error_desc_t err_desc;
  int retval;

  /* Receive the xml tree parsed in message */
  retval = receive(sock_fd, &buffer);
  if (retval < 0)
    {
      if (retval == -ECONNRESET || retval == -ECONNABORTED)
	exalog_debug("CONNECTION %d: An error occurred : %d [%s]",
		     conn_id, retval, exa_error_msg(retval));
      else
	exalog_error("Socket %d peer '%s': An error occurred : %s (%d)",
		     sock_fd, cli_peer_from_fd(sock_fd), exa_error_msg(retval),
		     retval);

      close_connection(conn_id);
      return;
    }

  /* Parse the tree we just received and get a newly allocated payload data */
  xml_command_parse(buffer, &cmd_code, &cluster_uuid,
                    &data, &data_size, &err_desc);

  /* buffer is now parsed, let's free it */
  os_free(buffer);

  if (err_desc.code != EXA_SUCCESS)
    {
      /* No need to free data buffer if parsing returned an error */
      exalog_error("Failed to parse command on socket %d (from peer '%s'): %s (%d)",
	           sock_fd, cli_peer_from_fd(sock_fd), err_desc.msg, err_desc.code);

      cli_command_end_complete(sock_fd, &err_desc);
      return;
    }

  command = adm_command_find(cmd_code);
  EXA_ASSERT_VERBOSE(command, "Missing implementation of command #%d", cmd_code);

  connectlist[conn_id].uid = get_new_cmd_uid();
  retval = send_command_to_evmgr(connectlist[conn_id].uid,
                                 command,
				 &cluster_uuid, data, data_size);
  if (retval < 0)
    {
      if (retval == -EXA_ERR_ADM_BUSY)
        exalog_warning("Running command %s (request from %s) failed: %s",
                       adm_command_find(cmd_code)->msg,
                       cli_get_peername(connectlist[conn_id].uid),
                       exa_error_msg(retval));
      else
        exalog_error("Running command %s (request from %s) failed: %s (%d)",
                     adm_command_find(cmd_code)->msg,
		     cli_get_peername(connectlist[conn_id].uid),
                     exa_error_msg(retval), retval);

      set_error(&err_desc, retval, NULL);
      cli_command_end_complete(sock_fd, &err_desc);

      /* the command was not scheduled, reset the uid */
      connectlist[conn_id].uid = CMD_UID_INVALID;
    }

  os_free(data);
}

static void
check_internal_msg(void)
{
  struct timeval timeout = { .tv_sec = 0, .tv_usec = EXAMSG_TIMEOUT };
  static Examsg msg;
  command_end_t *end;
  int i, ret;

  ret = examsgWaitTimeout(cli_mh, &timeout);

  if (ret < 0 && ret != -ETIME)
    {
      exalog_error("Message wait failed %s (%d)",
	           exa_error_msg(ret), ret);
      return;
    }

  if (ret == -ETIME)
    return;

  ret = examsgRecv(cli_mh, NULL, &msg, sizeof(msg));
  if (ret == 0)
    return;

  EXA_ASSERT_VERBOSE(ret > 0, "Message receive failed: %s (%d)",
                     exa_error_msg(ret), ret);

  if (ret < 0)
    exalog_error("Message receive failed: %s (%d)",
	         exa_error_msg(ret), ret);

  /* The CLI server can only receive EXAMSG_ADM_CLUSTER_CMD_END messages for now */
  EXA_ASSERT(msg.any.type == EXAMSG_ADM_CLUSTER_CMD_END);

  end = (command_end_t *)msg.payload;
  for (i = 0; i < MAX_CONNECTION; i++)
    if (end->cuid == connectlist[i].uid)
      {
	cli_command_end_complete(connectlist[i].fd, &end->err_desc);
	connectlist[i].uid = CMD_UID_INVALID;
	break;
      }
  EXA_ASSERT(i < MAX_CONNECTION);
}

static void
check_tcp_connection(void)
{
  static struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };
  fd_set setSave = setSocks;
  int ret, conn_id;

  do
    ret = os_select(FD_SETSIZE, &setSave, NULL,  NULL, &timeout);
  while (ret == -EINTR);

  if (ret < 0)
    {
      /* FIXME should assert ? */
      exalog_debug("Select failed %m");
      return;
    }

  /* Check working sockets */
  for (conn_id = 0; conn_id < MAX_CONNECTION; ++conn_id)
    {
      int sock_fd = connectlist[conn_id].fd;
      if (sock_fd >= 0 && FD_ISSET(sock_fd, &setSave))
	handle_inputdata(conn_id, sock_fd);
    }

  /* Must be done at the end to make sure messages for current
   * working threads are processed first */
  if (FD_ISSET(listen_fd, &setSave))
    accept_new_client();
}

/*-------------------------------------------------------------------------*/
/** \brief Connection thread: wait on xml message and pass the command
 * to the work thread.
 *
 * \param[in] sock_xml: socket xml on which it receives commands.
 *
 */
/*-------------------------------------------------------------------------*/
static void
cli_server(void *data)
{
  int i;

  /* Initialize exalog */
  exalog_as(EXAMSG_ADMIND_ID);
  exalog_debug("cli_server: started");

  /* Initialization */
  FD_ZERO(&setSocks);
  FD_SET(listen_fd, &setSocks);

  for (i = 0; i < MAX_CONNECTION; i++)
    {
      connectlist[i].fd  = -1;
      /* A command cannot be CMD_UID_INVALID, so CMD_UID_INVALID means here
       * no command running */
      connectlist[i].uid = CMD_UID_INVALID;
    }

  while (!stop)
    {
      check_tcp_connection();
      check_internal_msg();
    }

  os_closesocket(listen_fd);

  os_net_cleanup();

  examsgDelMbox(cli_mh, EXAMSG_ADMIND_CLISERVER_ID);
  examsgExit(cli_mh);
}

int
cli_server_start(void)
{
  listen_fd = listen_socket_port(ADMIND_SOCKET_PORT);
  if (listen_fd < 0)
    return listen_fd;

  cli_mh = examsgInit(EXAMSG_ADMIND_CLISERVER_ID);
  if (!cli_mh)
    return -EINVAL;

  /* The mailbox needs to be able to receive command end messages from the
   * event manager; as there can be at most MAX_CONNECTION client connections
   * we can receive at the time at most 10 command end messages. */
  examsgAddMbox(cli_mh, EXAMSG_ADMIND_CLISERVER_ID, MAX_CONNECTION,
	        sizeof(command_end_t));

  stop = false;

  if (!exathread_create_named(&thr_xml_proto,
                              ADMIND_THREAD_STACK_SIZE+MIN_THREAD_STACK_SIZE,
                              &cli_server, NULL, "exa_adm_xml"))
      return -EXA_ERR_DEFAULT;

  return EXA_SUCCESS;
}

void cli_server_stop(void)
{
    stop = true;
    os_thread_join(thr_xml_proto);
}

char *cli_get_peername(unsigned int uid)
{
  int i;

  if (uid != CMD_UID_INVALID)
    for (i = 0; i < MAX_CONNECTION; i++)
      if (uid == connectlist[i].uid)
	return cli_peer_from_fd(connectlist[i].fd);

  return "Unknown Peer";
}

/*----------------------------------------------------------------------------*/
/** \brief Low level, non blocking socket write a result to the CLI/GUI
 *
 * \param msg_sock a socket id
 * \param result is the message to write
 *
 */
/*----------------------------------------------------------------------------*/

static void write_result(int msg_sock, const char *result)
{
  int skip = 0;

  if (result == NULL || msg_sock < 0)
    return;

  do
    {
      int rv;
      do
	rv = os_send(msg_sock, result + skip, strlen(result) - skip);
      while (rv == -EINTR);

      if (rv < 0 && rv != -EAGAIN)
	{
	  exalog_error("Error '%s' while sending result '%s'",
                       exa_error_msg(rv), result);
	  break;
	}

      if (rv > 0)
	skip += rv;

    } while (skip < strlen(result));
}

void cli_payload_str(unsigned int uid, const char *str)
{
  char *result;
  int i;

  if (uid == CMD_UID_INVALID)
    return;

  result = xml_payload_str_new(str);
  for (i = 0; i < MAX_CONNECTION; i++)
    if (uid == connectlist[i].uid)
      write_result(connectlist[i].fd, result);
  os_free(result);
}

void cli_write_inprogress(unsigned int uid, const char *src_node,
                          const char *desc, int err, const char *str)
{
  char text_buf[EXA_MAXSIZE_BUFFER + 1];
  cl_error_desc_t err_desc;
  int i;

  if (uid == CMD_UID_INVALID)
    return;

  set_error(&err_desc, err, str);

  xml_inprogress(text_buf, sizeof(text_buf), src_node, desc, &err_desc);

  for (i = 0; i < MAX_CONNECTION; i++)
    if (uid == connectlist[i].uid)
      write_result(connectlist[i].fd, text_buf);
}

static void
cli_command_end_complete(int sock_fd, const cl_error_desc_t *err_desc)
{
  char *result;

  result = xml_command_end(err_desc);

  write_result(sock_fd, result);

  os_free(result);
}

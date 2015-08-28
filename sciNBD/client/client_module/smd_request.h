#ifndef _smd_request_h
#define _smd_request_h
extern void smd_request(request_queue_t *q);
extern void abort_current_req(servernode_t *sn);
extern request_queue_t *smd_next_non_empty_queue(servernode_t *sn);
extern void smd_end_request(servernode_t *sn, int status);

#endif

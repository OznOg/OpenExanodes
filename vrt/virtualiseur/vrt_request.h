#ifndef _vrt_request_h
#define _vrt_request_h
//extern void vrt_request(request_queue_t *q);
extern int vrt_make_request(request_queue_t *q, int rw, struct buffer_head *bh);
#endif

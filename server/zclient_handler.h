#ifndef _Z_CLIENT_HANDLER_H__
#define _Z_CLIENT_HANDLER_H__

#include "zhandler.h"

class ZClientHandler : public ZHandler {
//  public:
// 	virtual int init() = 0;
// 	virtual int close() = 0;
// 	virtual int event(char *buf, uint32_t buf_len) = 0;
// 	virtual int onInnerMsg(ZInnerMsg *msg) = 0;

 public:
	evutil_socket_t fd_;
	struct event *read_event_;
};



#endif // _Z_CLIENT_HANDLER_H__



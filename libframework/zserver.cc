#include "zserver.h"

#include <assert.h>
// ::close
#include <unistd.h>
// inet_ntoa
#include <arpa/inet.h>

#include "zerrno.h"
#include "zdispatcher.h"

static void SOCKET_CALLBACK(evutil_socket_t fd, short events, void *arg)
{
	assert(arg);
	ZServer *p = (ZServer*)arg;
	p->event(fd, events);
}

int ZServer::sendMsg(ZInnerMsg *msg)
{
	printf("ZServer::sendMsg\n");
	return onInnerMsg(msg);
}

int ZServer::onInnerMsg(ZInnerMsg *msg)
{
	printf("ZServer::onInnerMsg\n");
	return 0;
}

int ZServer::init() {
	int rv;

	if (ZDispatcher::instance()->registerModule(this) != OK) {
		return FAIL;
	}
	
	fd_ = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd_ >= 0);

	evutil_make_socket_nonblocking(fd_);

#ifndef WIN32
	{
		int one = 1;
		setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	}
#endif // WIN32

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	// sin.sin_addr.s_addr = 0; // Listen IP
	inet_aton(ip_.c_str(), &sin.sin_addr);
	sin.sin_port = htons(port_);

	rv = bind(fd_, (struct sockaddr*) (&sin), sizeof(sin));
	if (rv < 0) {
		perror("bind");
		return FAIL;
	}

	rv = listen(fd_, 16);
	if (rv < 0) {
		perror("listen");
		return FAIL;
	}

	struct event* listen_event =
		event_new(base_, fd_, EV_READ|EV_PERSIST, SOCKET_CALLBACK, (void*)this);
	assert(listen_event);

	event_add(listen_event, NULL);

	// state_ = STATE_ACCEPTING;

	return OK;
}

void ZServer::close() {
	// XXX: use event_del to remove event
	::close(fd_);
	fd_ = -1;
}

void ZServer::event(evutil_socket_t fd, short events) {
	acceptClient(fd, events);
}

void ZServer::acceptClient(evutil_socket_t fd, short events) {
	printf("ZServer::accept()\n");

	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);
	int clifd = accept(fd, (struct sockaddr*) (&ss), &slen);
	if (clifd < 0) {           // XXX EAGAIN?
		perror("accept");
	} else if (clifd > FD_SETSIZE) {
		printf("Maximum size of fd has reached.\n");
		::close(clifd); // XXX evutil_closesocket
	} else {
		//
		struct sockaddr_in* addr = (struct sockaddr_in*)(&ss);
		unsigned short port = ntohs(addr->sin_port);
		printf("accepted connection from: %s:%u\n",
				inet_ntoa(addr->sin_addr), port);

		evutil_make_socket_nonblocking(clifd);

		onAccept(clifd, addr, port);
	}
}


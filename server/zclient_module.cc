#include "zclient_module.h"

#include <errno.h>
#include <assert.h>
// inet_addr
#include <arpa/inet.h>
#include <unistd.h>

#include "zerrno.h"


// TODO: server wait timeout
// static const struct timeval SERVER_WAIT_TIMEOUT = { 20, 0 };
static const struct timeval RETRY_INTERVAL = { 5, 0 };

static void SOCKET_CALLBACK(evutil_socket_t fd, short events, void *arg)
{
	// printf("SOCKET_CALLBACK\n");
	assert(arg);
	ZTask *task = (ZTask*)arg;
	task->event(fd, events);
}

int ZClientModule::init() {
	int rv = super_::init();
	if (rv != OK) {
		return rv;
	}

	rv = onDisconnected(-1, 0);
	if (rv != OK && rv != ERR_IO_PENDING) {
		return FAIL;
	}

	return OK;
}

// 0: success, connected
//-1: IO_PENDING
// this method should not change current state;
int ZClientModule::connect() {
	if (fd_ >= 0) {
		::close(fd_); // XXX: evutil_closesocket(fd_);
		fd_ = -1;
	}

	fd_ = socket(AF_INET, SOCK_STREAM, 0);
	assert(fd_ >= 0);

	evutil_make_socket_nonblocking(fd_);

#ifndef WIN32
	{
		int one = 1;
		if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
			perror("setsockopt");
			::close(fd_);
			fd_ = -1;
			return FAIL;
		}
	}
#endif // WIN32

	int rv;

	// // need bind or not?
	// if (false) {
	// 	rv = bind(fd, (struct sockaddr*) (&sin), sizeof(sin));
	// 	if (rv < 0) {
	// 		perror("bind");
	// 		return false;
	// 	}
	// }

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr("127.0.0.1");
	sin.sin_port = 1983;

	// connect(int socket, const struct sockaddr *address, socklen_t address_len);
	rv = ::connect(fd_, (struct sockaddr*) (&sin), sizeof(sin));
	if (rv < 0) {
		if (errno != EINPROGRESS) {
			perror("connect");
			printf("Can not initial connection\n");
			::close(fd_); // XXX: evutil_closesocket
			fd_ = -1;
			return FAIL;
		} else {
			return ERR_IO_PENDING;
		}
	}

	return OK;
}

// int ZClientModule::doLoop() {
// 	do {
// 	} while (rv 
// 	int rv = connect();
// 	switch (rv) {
// 		case OK:
// 			break;
// 		case ERR_IO_PENDING:
// 			break;
// 		case FAIL:
// 			break;
// 		default:
// 			assert(false);
// 			break;
// 	}
// }

void ZClientModule::close() {
	::close(fd_); // XXX: evutil_closesocket(fd_);
	fd_ = -1;
	state_ = STATE_FINISHED;
}

void ZClientModule::event(evutil_socket_t fd, short events) {
	printf("ZClientModule::event()\n");
	switch (state_) {
		case STATE_WAITING_FOR_CONNECT:
			onWaitingForConnect(fd, events);
			break;
		case STATE_CONNECTED:
			onConnected(fd, events);
			break;
		case STATE_DISCONNECTED:
			onDisconnected(fd, events);
			break;
		case STATE_FINISHED:
			// should never happen
			assert(false);
			break;
		default:
			close();
			break;
	}
}

int ZClientModule::onWaitingForConnect(evutil_socket_t fd, short events) {
	int val;
	socklen_t val_len = sizeof(val);
	int rv = getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &val_len);
	if (rv == 0 && val == 0) {
		printf("Connected\n");
		// event_free(write_event_);
		// write_event_ = NULL;
		read_event_ =
			event_new(base_, fd, EV_READ|EV_PERSIST, SOCKET_CALLBACK, (void*)this);
		event_add(read_event_, NULL);

		state_ = STATE_CONNECTED;
	} else {
		printf("Failed to connect\n");
		// event_free(read_event_);
		// read_event_ = NULL;
		// event_free(write_event_);
		// write_event_ = NULL;

		::close(fd_);
		fd_ = -1;
		state_ = STATE_DISCONNECTED;

		scheduleReconnect();
	}

	return OK;

}

void ZClientModule::scheduleReconnect() {
	struct event* ev = evtimer_new(base_, SOCKET_CALLBACK, this);
	event_add(ev, &RETRY_INTERVAL);
}

void ZClientModule::onConnected(evutil_socket_t fd, short events) {
	assert(fd >= 0);
	int rv = recv(fd, buf_, sizeof(buf_), 0);
	if (rv == 0) {
		printf("peer closed\n");
		state_ = STATE_DISCONNECTED;
		scheduleReconnect();
		// close();
		return;
	} else if (rv < 0) { // XXX EAGAIN
		perror("recv");
		printf("Failed to receive data from socket\n");
		::close(fd);
		state_ = STATE_DISCONNECTED;
		scheduleReconnect();
		return;
	}
	
	// == for DEBUGGING only ==
	if (rv >= (int)sizeof(buf_)) {
		buf_[sizeof(buf_) - 1] = 0x00;
	} else {
		buf_[rv] = 0x00;
	}
	printf("Received: %s\n", buf_);
	// == for DEBUGGING only ==
}

int ZClientModule::onDisconnected(evutil_socket_t fd, short events) {
	// state_ = STATE_DISCONNECTED;
	// return doLoop();
	int rv = connect();
	switch (rv) {
		case OK:
			{
				printf("Connected\n");
				state_ = STATE_CONNECTED;
				read_event_ =
					event_new(base_, fd_, EV_READ|EV_PERSIST, SOCKET_CALLBACK, (void*)this);
				event_add(read_event_, NULL);
				break;
			}
		case ERR_IO_PENDING:
			{
				printf("Waiting for response\n");
				state_ = STATE_WAITING_FOR_CONNECT;

				// one shot event
				struct event* ev =
					event_new(base_, fd_, EV_WRITE, SOCKET_CALLBACK, (void*)this);
				event_add(ev, NULL);
				break;
			}
		case FAIL:
			{
				printf("Failed to connect\n");
				state_ = STATE_DISCONNECTED;

				scheduleReconnect();

				break;
			}
		default:
			{
				rv = FAIL;
				break;
			}
	}

	return rv;
}

void ZClientModule::doTimeout() {
}

bool ZClientModule::isComplete() {
	return (state_ == STATE_FINISHED);
}

int ZClientModule::onInnerMsg(ZInnerMsg *msg) {
	printf("ZClientModule::onInnerMsg()\n");
	return 0;
}




#ifndef _Z_DISPATCHER_H__
#define _Z_DISPATCHER_H__

#include <list>

#include "zmodule.h"
#include "zsession.h"
#include "zsession_ctrl.h"

class ZDispatcher {
 private:
	ZDispatcher() {
			module_list_.clear();
		}

 public:
	static ZDispatcher* instance() {
		static ZDispatcher dispatcher;
		return &dispatcher;
	}
 public:
	int registerModule(ZModule *module);
	int sendMsg(ZInnerMsg *msg);
	void routine(time_t delta);

 private:
	ZModule* findModule(int moduleType, int moduleId);

	void processMsg(ZInnerGetDevListReq *msg);
	void processMsg(ZInnerGetDevListRsp *msg);
	void processMsg(ZInnerGetDevInfoReq *msg);
	void processMsg(ZInnerGetDevInfoRsp *msg);
	void processMsg(ZInnerSetDevInfoReq *msg);
	void processMsg(ZInnerSetDevInfoRsp *msg);

	void processInnerMsg(ZInnerMsg *msg);

	void consumeMsg();
	void checkTimeout(time_t delta);

 private:
	typedef std::list<ZModule*> MODULE_LIST_TYPE;
	typedef ZSessionCtrl<uint32_t, ZSession> SESSION_CTRL_TYPE;

 private:
	// TODO: use hash map instead of list
	MODULE_LIST_TYPE module_list_;
	std::list<ZInnerMsg*> msg_list_;
	SESSION_CTRL_TYPE session_ctrl_;
};

#endif



#include "zdispatcher.h"

#include "zerrno.h"

int ZDispatcher::sendDirect(ZInnerMsg *msg)
{
	ZInnerAddress dst_addr = msg->dst_addr_;

	// find module
	ZModule *module = findModule(dst_addr.module_type_);
	if (module == NULL) {
		printf("Target module [%d] is not found\n", dst_addr.module_type_);
		return FAIL;
	}

	module->sendMsg(msg);

	return OK;
}

// we handle the memory
int ZDispatcher::sendMsg(ZInnerMsg *msg)
{
	if (!msg->src_addr_.isValid()) {
		printf("Source address is invalid\n");
		// delete msg;
		// return -1;
	}

	msg_list_.push_back(msg);

	return 0;
}

int ZDispatcher::registerModule(ZModule *m)
{
	if (findModule(m->getType())) {
		printf("Duplicated module\n");
		return FAIL;
	}

	module_list_.push_back(m);

	// printf("Module(%d:%s), added\n",
	// 		m->getType(), moduleType2string(m->getType()));

	return OK;
}

ZModule* ZDispatcher::findModule(int moduleType)
{
	MODULE_LIST_TYPE::iterator iter = module_list_.begin();
	ZModule *m = NULL;

	for (; iter != module_list_.end(); ++iter) {
		m = *iter;
		if (m->getType() == moduleType) {
			return m;
		}
	}

	return NULL;
}

void ZDispatcher::checkTimeout(long delta)
{
	// printf("ZDispatcher::checkTimeout\n");

	ZSession *session;

	SESSION_CTRL_TYPE::iterator iter = session_ctrl_.begin();
	SESSION_CTRL_TYPE::iterator tmp_iter;

	while (iter != session_ctrl_.end()) {
		// printf("while loop\n");
		session = iter->second;
		session->doTimeout(delta);
		if (session->isComplete())	{
			// printf("delta: [%ld]\n", delta);
			// printf("timeout: [%ld]\n", session->getTimeout());
			tmp_iter = iter;
			++iter;
			session_ctrl_.erase(tmp_iter);
		} else {
			++iter;
		}
	}
}

void ZDispatcher::moduleRoutine(long delta)
{
	ZModule *module;
	MODULE_LIST_TYPE::iterator iter = module_list_.begin();
	for (; iter != module_list_.end(); ++iter) {
		module = *iter;
		module->routine(delta);
	}
}

void ZDispatcher::routine(long delta)
{
	// consumeMsg();

	checkTimeout(delta);

	moduleRoutine(delta);
}


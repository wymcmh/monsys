#include "zzigbee_handler.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <assert.h>

#include "libframework/zframework.h"

#include "zzigbee_session.h"

/////////////////////////////////////////////////////////////////////

int ZZigBeeHandler::init()
{
	printf("fd_: %d\n", fd_);
	return 0;
}

void ZZigBeeHandler::close()
{
}

int ZZigBeeHandler::onInnerMsg(ZInnerMsg *msg)
{
	printf("ZZigBeeHandler::onInnerMsg(%p)\n", msg);

	// printf("fd_: %d\n", fd_);
	// if (fd_ < 0) {
	// 	printf("Invalid fd, not connected?\n");
	// 	return -1;
	// }

	switch (msg->msg_type_) {
		case Z_ZB_GET_DEV_LIST_REQ:
			{
				return processMsg((ZInnerGetDevListReq*)msg);
			}
		case Z_ZB_GET_DEV_REQ:
			{
				return processMsg((ZInnerGetDevInfoReq*)msg);
			}
		case Z_ZB_SET_DEV_REQ:
			{
				return processMsg((ZInnerSetDevInfoReq*)msg);
			}
		default:
			printf("Unknown message type: %u", msg->msg_type_);
			return -1;
	}
}

void ZZigBeeHandler::routine(long delta)
{
	// printf("ZZigBeeHandler::routine()\n");

	ZSession *session;

	SESSION_CTRL_TYPE::iterator iter = session_ctrl_.begin();
	SESSION_CTRL_TYPE::iterator tmp_iter;
	// SESSION_CTRL_TYPE::iterator iter1;

	while (iter != session_ctrl_.end()) {
		session = iter->second;
		session->doTimeout(delta);
		if (session->isComplete()) {
			tmp_iter = iter;
			++iter;

			// XXX:
			// don't forget to erase another one first
			// iter1 = session_ctrl_1_.find(session->);

			session_ctrl_.erase(tmp_iter);

		} else {
			++iter;
		}
	}
}

void ZZigBeeHandler::onConnected()
{
	Z_LOG_D("ZZigBeeHandler::onConnected()");
}


int ZZigBeeHandler::onRead(char *buf, uint32_t buf_len)
{
	printf("ZZigBeeHandler::onRead()\n");

	ZZigBeeMsg hdr;
	int rv = hdr.decode(buf, buf_len);
	if (rv < 0) {
		printf("Failed to decode header\n");
		return -1;
	}

	// uint8_t msg_type = ZZigBeeMsg::getMsgType(buf, buf_len);
	// switch (msg_type) {
	switch (hdr.cmd_) {
		case Z_ID_ZB_REG_REQ:
			{
				printf("Z_ID_ZB_REG_REQ\n");
				ZZBRegReq msg;
				int rv = msg.decode(buf, buf_len);
				if (rv < 0) {
					printf("Failed to decode message\n");
				} else {
					printf("decoding success\n");
					processMsg(msg);
				}
				break;
			}
	case Z_ID_ZB_GET_RSP:
		{
			printf("Z_ID_ZB_GET_RSP\n");
			ZZBGetRsp msg;
			int rv = msg.decode(buf, buf_len);
			if (rv < 0) {
				printf("Failed to decode message\n");
			} else {
				printf("decoding success\n");
				processMsg(msg);
			}
			break;
		}
	case Z_ID_ZB_SET_RSP:
		{
			printf("Z_ID_ZB_SET_RSP\n");
			ZZBSetRsp msg;
			int rv = msg.decode(buf, buf_len);
			if (rv < 0) {
				printf("Failed to decode message\n");
			} else {
				printf("decoding success\n");
				processMsg(msg);
			}
			break;
		}
		case Z_ID_ZB_UPDATE_ID_REQ:
		{
			printf("Z_ID_ZB_UPDATE_ID_REQ\n");
			ZZBUpdateIdInfoReq msg;
			int rv = msg.decode(buf, buf_len);
			if (rv < 0) {
				printf("Failed to decode message\n");
			} else {
				printf("decoding success\n");
				processMsg(msg);
			}
			break;
		}
	default:
		{
			printf("Unknow message: %u\n", hdr.cmd_);
			break;
		}
	}

	return OK;
}

int ZZigBeeHandler::processMsg(ZZBRegReq &msg)
{
	printf("ZZigBeeHandler::processMsg(RegReq)\n");

	ZZBRegRsp rsp;
	// if (!dev_manager_.add(msg.mac_, msg.addr_, "dev-xxx", msg.id_count_)) {
	if (!dev_manager_.add(msg.mac_, msg.addr_, "dev-xxx", 1)) {
		printf("Failed to add device to device manager\n");
		rsp.status_ = -1;
	} else {
		printf("Added/Updated into device manager\n");
		rsp.status_ = 0;
	}

	rsp.addr_ = msg.addr_;

	int rv = rsp.encode(buf_, sizeof(buf_));
	if (rv < 0) {
		printf("failed to encode register response\n");
		return -1;
	}

	send(buf_, rv);

	// // TODO:
	// // 2. send query request(with item id zero (means all))
	// ZZBGetReq req;

	// req.items_.push_back(ITEM_ID_ALL);

	// processMsg(req);

	return 0;
}

int ZZigBeeHandler::processMsg(ZZBGetRsp &msg)
{
	printf("ZZigBeeHandler::processMsg(ZZBGetRsp)\n");

	printf("rsp item count: %ld\n", msg.items_.size());

	ZInnerGetDevInfoRsp *rsp = new ZInnerGetDevInfoRsp(addr_);
	ZItemPair pair;

	for (uint32_t i = 0; i < msg.items_.size(); ++i) {
		printf("items.id[%d]: 0x%02X\n", i, msg.items_[i].id);
		printf("items.val[%d]: 0x%02X\n", i, msg.items_[i].val);
		pair.id = msg.items_[i].id;
		pair.val = msg.items_[i].val;
		rsp->dev_infos_.push_back(pair);
	}

	// ZDispatcher::instance()->sendMsg(rsp);
	{
		// FIXME: should zigbee message have a requence or not?
		// if it doesn't, then we should have a queue for serial processing
		// so, add one
		ZZigBeeSession *session =
				(ZZigBeeSession*)session_ctrl_.findByKey2((msg.addr_ << 16) | 0x00);
		if (session == NULL) {
			printf("No session found\n");
			delete rsp;
			return -1;
		}

		// if ((session != session1)
		// 		|| ((session->extern_key_.u32 >> 16) != msg.addr_)) {
		// 		assert(false);
		// 	printf("two different session, how could it be possible...\n");
		// 	return FAIL;
		// }

		rsp->dst_addr_ = session->src_addr_;

		session_ctrl_.removeByKey1(session->getKey());
		// session_ctrl_1_.remove(msg.addr_);

		delete session;

		ZDispatcher::instance()->sendDirect(rsp);
	}

	return 0;
}

int ZZigBeeHandler::processMsg(ZZBSetRsp &msg)
{
	printf("rsp status: %d\n", msg.status_);

	ZInnerSetDevInfoRsp *rsp = new ZInnerSetDevInfoRsp(addr_);
	rsp->status_ = msg.status_;

	{
		// FIXME: should zigbee message have a requence or not?
		// if it doesn't, then we should have a queue for serial processing
		// so, add one
		ZZigBeeSession *session =
				(ZZigBeeSession*)session_ctrl_.findByKey2((msg.addr_ << 16) | 0x00);
		if (session == NULL) {
			printf("No session found\n");
			delete rsp;
			return -1;
		}

		// if ((session != session1)
		// 		|| ((session->extern_key_.u32 >> 16) != msg.addr_)) {
		// 		assert(false);
		// 	printf("two different session, how could it be possible...\n");
		// 	return FAIL;
		// }

		rsp->dst_addr_ = session->src_addr_;

		session_ctrl_.removeByKey1(session->getKey());
		// session_ctrl_1_.remove(msg.addr_);

		delete session;

		// ZDispatcher::instance()->sendDirect(rsp);
	}

	ZDispatcher::instance()->sendMsg(rsp);

	return 0;
}

void ZZigBeeHandler::printDevInfo()
{
	// for (int i = 1; i < DEV_LIST_LEN; ++i) {
	// 	if (zb_dev_list_[i].info.state == zb_dev_state_active) {
	// 		printf("idx: %d\n", i);
	// 	}
	// }
}

int ZZigBeeHandler::processMsg(ZInnerGetDevListReq *msg)
{
	printf("ZZigBeeHandler::processMsg(ZInnerGetDevListReq)\n");

	ZInnerGetDevListRsp *rsp = new ZInnerGetDevListRsp(addr_);
	rsp->dst_addr_ = msg->src_addr_;

	ZZBDevInfo *info = NULL;

	// --- for debugging only ---
	{
		char dev_name_buf[64];
		for (int i = 0; i < 5; ++i) {
			snprintf(dev_name_buf, sizeof(dev_name_buf), "dev-%02d", i);
			info = new ZZBDevInfo();
			info->addr_ = i;
			info->name_ = dev_name_buf;
			info->state_ = i;
			info->type_ = i;
			memset(&info->mac_, i, sizeof(info->mac_));
			rsp->info_list_.push_back(info);
		}
	}
	// --- for debugging only ---

	const ZZBDevManager::MAC_DEV_MAP_TYPE &dev_map = dev_manager_.getMacDevMap();
	ZZBDevManager::MAC_DEV_MAP_TYPE::const_iterator iter = dev_map.begin();
	for (; iter != dev_map.end(); ++iter) {
		printf("count\n");
		info = new ZZBDevInfo(*(iter->second));
		rsp->info_list_.push_back(info);
	}

	// ZDispatcher::instance()->sendMsg(rsp);
	ZDispatcher::instance()->sendDirect(rsp);

	return 0;
}

int ZZigBeeHandler::processMsg(ZInnerGetDevInfoReq *msg)
{
	printf("ZZigBeeHandler::processMsg(ZInnerGetDevInfoReq)\n");

	// check
	{
		ZSession *session = session_ctrl_.findByKey1(msg->seq_);
		if (session) {
			printf("Duplicated session: [%u]\n", msg->seq_);
			return FAIL;
		}
	}

	ZZBGetReq req;
	req.addr_ = msg->addr_;

	// // --- for debugging only ---
	// {
	// 	ZInnerGetDevInfoRsp *rsp = new ZInnerGetDevInfoRsp(addr_);
	// 	ZItemPair pair;

	// 	for (uint32_t i = 0; i < msg->item_ids_.size(); ++i)
	// 	{
	// 		pair.id = i;
	// 		pair.val = i * 11;
	// 		rsp->dev_infos_.push_back(pair);
	// 	}

	// 	ZDispatcher::instance()->sendMsg(rsp);
	// }
	// // --- for debugging only ---

	for (uint32_t i = 0; i < msg->item_ids_.size(); ++i) {
		req.items_.push_back(msg->item_ids_[i]);
	}

	int rv = req.encode(buf_, sizeof(buf_));
	if (rv < 0) {
		printf("Failed to encode GetReq\n");
		return -1;
	}

	send(buf_, rv);

	// save session
	{
		// TODO: get inner sequence from ZBGet
		ZZigBeeSession *session = new ZZigBeeSession();
		session->setKey(msg->seq_);
		session->src_addr_ = msg->src_addr_;
		session->dst_addr_ = msg->dst_addr_;
		session->extern_key_.u32 = (req.addr_ << 16) | 0x00;

		session_ctrl_.add(msg->seq_, (req.addr_ << 16 | 0x00), session);
		// session_ctrl_1_.add(msg->addr_, session);
	}

	return 0;
}

int ZZigBeeHandler::processMsg(ZInnerSetDevInfoReq *msg)
{
	// [ {"id":1, "val":4}, {"id":2, "val":5} ]
	printf("ZZigBeeHandler::processMsg(ZInnerSetDevInfoReq)\n");

	// printDevInfo();
	// check
	{
		ZSession *session = session_ctrl_.findByKey1(msg->seq_);
		if (session) {
			printf("Duplicated session: [%u]\n", msg->seq_);
			return FAIL;
		}
	}

	// // -- for debugging only --
	// ZInnerSetDevInfoRsp *rsp = new ZInnerSetDevInfoRsp(addr_);
	// rsp->status_ = 0;
	// ZDispatcher::instance()->sendMsg(rsp);
	// return 0;
	// // -- for debugging only --

	// // set all device
	// if (msg->addr_ == 0) {
	// 	printf("Trying to set all devices\n");
	// }
	ZZBDevInfo *dev_info = dev_manager_.find(msg->addr_);
	if (dev_info == NULL) {
		printf("wrong address\n");
		return -1;
	}

	ZZBSetReq req;
	req.addr_ = msg->addr_;
	for (size_t i = 0; i < msg->dev_vals_.size(); ++i) {
		req.items_.push_back(msg->dev_vals_[i]);
	}

	int rv = req.encode(buf_, sizeof(buf_));
	if (rv < 0) {
		printf("Failed to encode ZZBSetReq\n");
		return -1;
	}

	trace_bin(buf_, rv);

	rv = send(buf_, rv);
	if (rv <= 0) {
		perror("send");
		printf("Failed to send\n");
	} else {
		printf("write over.\n");
		// save session
		{
			// TODO: get inner sequence from ZBGet
			ZZigBeeSession *session = new ZZigBeeSession();
			session->setKey(msg->seq_);
			session->src_addr_ = msg->src_addr_;
			session->dst_addr_ = msg->dst_addr_;
			session->extern_key_.u32 = (req.addr_ << 16) | 0x00;

			session_ctrl_.add(msg->seq_, (req.addr_ << 16 | 0x00), session);
			// session_ctrl_1_.add(msg->addr_, session);
		}
	}

	return 0;
}

static void updateIdInfo(ZZBDevInfo &dev_info, zb_item_id_info_t &new_id_info)
{
	// zb_item_id_info_t &id_info = dev_info.id_info_list_[new_id_info.id_];

	// id_info.clone(new_id_info);

	// // if (dev_info.id_info_list[new_id_info.id]) {
	// // 	id_info = dev_info.id_info_list[new_id_info.id];
	// // } else {
	// // 	id_info = new zb_item_id_info_t;
	// // 	dev_info.id_info_list[new_id_info.id] = id_info;
	// // }

	// // id_info->clone(new_id_info);

	// id_info.print();
}

int ZZigBeeHandler::processMsg(ZZBUpdateIdInfoReq &msg)
{
	printf("ZZigBeeHandler::processMsg(UpdateIdInfoReq)\n");

	ZZBDevInfo *dev_info = dev_manager_.find(msg.addr_);
	if (dev_info == NULL) {
		printf("wrong address\n");
		return -1;
	}

	size_t id_list_len = msg.id_list_.size();
	for (size_t i = 0; i < id_list_len; ++i) {
		updateIdInfo(*dev_info, msg.id_list_[i]);
	}

	ZZBUpdateIdInfoRsp rsp;
	rsp.addr_ = msg.addr_;
	rsp.status_ = 0x00;

	int rv = rsp.encode(buf_, sizeof(buf_));
	if (rv < 0) {
		printf("Failed to encode rsp\n");
		return -1;
	}

	// trace_bin(buf_, rv);

	rv = send(buf_, rv);
	if (rv <= 0) {
		perror("send");
		printf("Failed to send\n");
	} else {
		printf("write over.\n");
	}

	return 0;
}



/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		exe_proto.h
 *	DESCRIPTION:	Prototype header file for exe.cpp
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */

#ifndef JRD_EXE_PROTO_H
#define JRD_EXE_PROTO_H

#include "../jrd/cmp_proto.h"
#include <atomic>

namespace Firebird::Jrd {
	class Request;
	class jrd_tra;
	class AssignmentNode;

	enum InternalRequest : USHORT;

	void EXE_assignment(thread_db*, const AssignmentNode*);
	void EXE_assignment(thread_db*, const ValueExprNode*, const ValueExprNode*);
	void EXE_assignment(thread_db* tdbb, const ValueExprNode* to, dsc* from_desc,
		const ValueExprNode* missing_node, const ValueExprNode* missing2_node);

	void EXE_execute_db_triggers(thread_db*, jrd_tra*, enum TriggerAction);
	void EXE_execute_ddl_triggers(thread_db* tdbb, jrd_tra* transaction,
		bool preTriggers, int action);
	void EXE_execute_triggers(thread_db*, const Triggers&, record_param*, record_param*,
		enum TriggerAction, StmtNode::WhichTrigger, int = 0);
	void EXE_execute_function(thread_db* tdbb, Request* request, jrd_tra* transaction,
		ULONG inMsgLength, UCHAR* inMsg, ULONG outMsgLength, UCHAR* outMsg);
	bool EXE_get_stack_trace(const Request* request, string& sTrace);

	const StmtNode* EXE_looper(thread_db* tdbb, Request* request,
		const StmtNode* in_node);
	void EXE_receive(thread_db*, Request*, USHORT, ULONG, void*, bool = false);
	void EXE_release(thread_db*, Request*);
	void EXE_send(thread_db*, Request*, USHORT, ULONG, const void*);
	void EXE_start(thread_db*, Request*, jrd_tra*);
	void EXE_unwind(thread_db*, Request*);

	class CachedRequestId
	{
	public:
		CachedRequestId()
			: id(generator++)
		{
			fb_assert(id <= MAX_USHORT);
		}

		CachedRequestId(const CachedRequestId&) = delete;
		CachedRequestId& operator=(const CachedRequestId&) = delete;

	public:
		USHORT getId() const noexcept
		{
			return id;
		}

	private:
		unsigned id;
		static inline std::atomic_uint generator;
	};

	// ASF: To make this class MT-safe in SS for v3, it should be AutoCacheRequest::release job to
	// inform CMP that the request is available for subsequent usage.
	class AutoCacheRequest
	{
	public:
		AutoCacheRequest(thread_db* tdbb, USHORT aId, InternalRequest aWhich)
			: id(aId),
			  which(aWhich),
			  request(tdbb->getDatabase()->findSystemRequest(tdbb, id, which))
		{
		}

		AutoCacheRequest(thread_db* tdbb, const CachedRequestId& cachedRequestId)
			: id(cachedRequestId.getId()),
			  which(CACHED_REQUESTS),
			  request(tdbb->getDatabase()->findSystemRequest(tdbb, id, which))
		{
		}

		AutoCacheRequest() noexcept
			: id(0),
			  which(NOT_REQUEST),
			  request(NULL)
		{
		}

		~AutoCacheRequest()
		{
			release();
		}

	public:
		void reset(thread_db* tdbb, USHORT aId, InternalRequest aWhich)
		{
			release();

			id = aId;
			which = aWhich;
			request = tdbb->getDatabase()->findSystemRequest(tdbb, id, which);
		}

		void reset(thread_db* tdbb, const CachedRequestId& cachedRequestId)
		{
			release();

			id = cachedRequestId.getId();
			which = CACHED_REQUESTS;
			request = tdbb->getDatabase()->findSystemRequest(tdbb, id, which);
		}

		void compile(thread_db* tdbb, const UCHAR* blr, ULONG blrLength)
		{
			if (request)
				return;

			cacheRequest(CMP_compile_request(tdbb, blr, blrLength, true));
		}

		Request* operator ->() noexcept
		{
			return request;
		}

		operator Request*() noexcept
		{
			return request;
		}

		bool operator !() const noexcept
		{
			return !request;
		}

	private:
		void release();
		void cacheRequest(Request* req);

	private:
		USHORT id;
		InternalRequest which;
		Request* request;
	};

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define AUTO_HANDLE(rq) static CachedRequestId TOKENPASTE2(cachedRq, __LINE__); AutoCacheRequest rq(tdbb, TOKENPASTE2(cachedRq, __LINE__))

	class AutoRequest
	{
	public:
		AutoRequest() noexcept
			: request(NULL)
		{
		}

		~AutoRequest()
		{
			release();
		}

	public:
		void reset()
		{
			release();
		}

		void compile(thread_db* tdbb, const UCHAR* blr, ULONG blrLength)
		{
			if (request)
				return;

			request = CMP_compile_request(tdbb, blr, blrLength, true);
			request->setUsed();
		}

		Request* operator ->() noexcept
		{
			return request;
		}

		operator Request*() noexcept
		{
			return request;
		}

		bool operator !() const noexcept
		{
			return !request;
		}

	private:
		void release();

	private:
		Request* request;
	};
}	// namespace Firebird::Jrd

#endif // JRD_EXE_PROTO_H

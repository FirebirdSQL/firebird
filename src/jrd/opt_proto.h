/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		opt_proto.h
 *	DESCRIPTION:	Prototype header file for opt.cpp
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

#ifndef JRD_OPT_PROTO_H
#define JRD_OPT_PROTO_H

#include "../jrd/jrd.h"
#include "../jrd/btr.h"
#include "../jrd/lls.h"

namespace Jrd {
	class Request;
	class jrd_rel;
	class RecordSource;
	struct index_desc;
	class CompilerScratch;
	class OptimizerBlk;
	class SortedStream;
	class SortNode;
	class MapNode;
}

Firebird::string OPT_get_plan(Jrd::thread_db* tdbb, const Jrd::Statement* statement, bool detailed);
Jrd::RecordSource* OPT_compile(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb,
	Jrd::RseNode* rse, Jrd::BoolExprNodeStack* parent_stack);
void OPT_compile_relation(Jrd::thread_db* tdbb, Jrd::jrd_rel* relation, Jrd::CompilerScratch* csb,
	StreamType stream, bool needIndices);
void OPT_gen_aggregate_distincts(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb, Jrd::MapNode* map);
Jrd::SortedStream* OPT_gen_sort(Jrd::thread_db* tdbb, Jrd::CompilerScratch* csb, const Jrd::StreamList& streams,
	const Jrd::StreamList* dbkey_streams, Jrd::RecordSource* prior_rsb, Jrd::SortNode* sort,
	bool refetch_flag, bool project_flag);

#endif // JRD_OPT_PROTO_H

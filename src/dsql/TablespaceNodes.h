/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Roman Simakov
 *  for the RedDatabase project.
 *
 *  Copyright (c) 2018 <roman.simakov@red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_TABLESPACE_NODES_H
#define DSQL_TABLESPACE_NODES_H

#include "../dsql/DdlNodes.h"
#include "../common/classes/array.h"

namespace Jrd {


class CreateAlterTablespaceNode : public DdlNode
{
public:
	CreateAlterTablespaceNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool),
		  name(pool, aName),
		  fileName(pool),
		  create(true),
		  alter(false),
		  offline(false),
		  readonly(false)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector <<
			Firebird::Arg::Gds(createAlterCode(create, alter,
					isc_dsql_create_ts_failed, isc_dsql_alter_ts_failed,
					isc_dsql_create_alter_ts_failed)) <<
				name;
	}

private:
	void executeCreate(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);
	bool executeAlter(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

public:
	MetaName name;
	Firebird::PathName fileName;
	bool create;
	bool alter;
	bool offline;
	bool readonly;
	bool createIfNotExistsOnly = false;
};


class DropTablespaceNode : public DdlNode
{
public:
	DropTablespaceNode(MemoryPool& pool, const MetaName& aName)
		: DdlNode(pool), name(pool, aName)
	{
	}

public:
	virtual Firebird::string internalPrint(NodePrinter& printer) const;
	virtual void checkPermission(thread_db* tdbb, jrd_tra* transaction);
	virtual void execute(thread_db* tdbb, DsqlCompilerScratch* dsqlScratch, jrd_tra* transaction);

protected:
	virtual void putErrorPrefix(Firebird::Arg::StatusVector& statusVector)
	{
		statusVector << Firebird::Arg::Gds(isc_dsql_drop_ts_failed) << name;
	}

public:
	MetaName name;
	bool silent = false;
	bool dropDependencies = false;
};

template <>
inline RecreateNode<CreateAlterTablespaceNode, DropTablespaceNode, isc_dsql_recreate_ts_failed>::
	RecreateNode(MemoryPool& p, CreateAlterTablespaceNode* aCreateNode)
		: DdlNode(p),
		  createNode(aCreateNode),
		  dropNode(p, createNode->name)
	{
		dropNode.silent = true;
	}

typedef RecreateNode<CreateAlterTablespaceNode, DropTablespaceNode, isc_dsql_recreate_ts_failed>
	RecreateTablespaceNode;

} // namespace

#endif // DSQL_TABLESPACE_NODES_H

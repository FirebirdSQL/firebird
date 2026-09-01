/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			DistributedTransaction.h
 *	DESCRIPTION:	DTC and distributed transaction (also known as 2PC).
 *
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
 *  The Original Code was created by Alex Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2011 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef YVALVE_DISTRIBUTED_TRANSACTION_H
#define YVALVE_DISTRIBUTED_TRANSACTION_H

#include "../yvalve/YObjects.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/array.h"
#include "../common/StatusHolder.h"

namespace Firebird::Why
{


class DtcStart : public DisposeIface<IDtcStartImpl<DtcStart, CheckStatusWrapper> >
{
public:
	DtcStart()
		: components(getPool())
	{ }

	struct Component
	{
		IAttachment* att;
		unsigned tpbLen;
		const unsigned char* tpb;
	};

	unsigned getCount(CheckStatusWrapper*)
	{
		return components.getCount();
	}

	const Component* getEntry(CheckStatusWrapper* status, unsigned pos)
	{
		return &components[pos];
	}

	// IDtcStart implementation
	void addAttachment(CheckStatusWrapper* status, IAttachment* att) override;
	void addWithTpb(CheckStatusWrapper* status, IAttachment* att,
		unsigned length, const unsigned char* tpb) override;
	YTransaction* start(CheckStatusWrapper* status) override;
	void dispose() override;

private:
	HalfStaticArray<Component, 16> components;
};

class Dtc : public AutoIface<IDtcImpl<Dtc, CheckStatusWrapper> >
{
public:
	// IDtc implementation
	YTransaction* join(CheckStatusWrapper* status,
		ITransaction* one, ITransaction* two);
	DtcStart* startBuilder(CheckStatusWrapper* status);
};


} // namespace Firebird::Why

#endif // YVALVE_DISTRIBUTED_TRANSACTION_H

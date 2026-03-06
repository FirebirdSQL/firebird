/*
 *	PROGRAM:		FB interfaces.
 *	MODULE:			BlrFromMessage.h
 *	DESCRIPTION:	New=>old message style converter.
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
 *  Copyright (c) 2013 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_REMOTE_CLIENT_BLR_FROM_MESSAGE
#define FB_REMOTE_CLIENT_BLR_FROM_MESSAGE

#include "firebird/Interface.h"
#include "../common/classes/BlrWriter.h"

namespace Firebird::Remote
{


class BlrFromMessage : public BlrWriter
{
public:
	BlrFromMessage(IMessageMetadata* metadata, unsigned dialect, unsigned protocol);

	unsigned getLength();
	const unsigned char* getBytes();
	unsigned getMsgLength();

	virtual bool isVersion4();

private:
	void buildBlr(IMessageMetadata* metadata);

	unsigned expectedMessageLength;
	unsigned dialect;
	unsigned protocol;
};


} // namespace Firebird::Remote

#endif // FB_REMOTE_CLIENT_BLR_FROM_MESSAGE

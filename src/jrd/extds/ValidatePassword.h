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
 *  The Original Code was created by Alexander Peshkov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2016 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef EDS_VALIDATEPASSWORD_H
#define EDS_VALIDATEPASSWORD_H

#include "../../common/classes/fb_string.h"
#include "../../common/classes/ClumpletWriter.h"


namespace Jrd
{
	class thread_db;
}

namespace EDS {

bool validatePassword(Jrd::thread_db* tdbb, const Firebird::PathName& file,
	Firebird::ClumpletWriter& dpb);

} // namespace EDS

#endif // EDS_VALIDATEPASSWORD_H

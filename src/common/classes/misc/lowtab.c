/*
 *	PROGRAM:	memory allocator
 *	MODULE:		lowtab.c
 *	DESCRIPTION:	Types c++ arrays, required for memory pool
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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include <stdio.h>


int main(int ac, char** av)
{
	int mode = ac < 2 ? 0 : ((*av[1]) - '0');

	int cur = 32;
	int prev = 0;
	const int dstep = 16;
	int limit = 1024;
	int slot = 0;

	while (cur <= limit)
	{
		if (mode == 0)
			printf ("\t%d, // %d\n", slot, cur);

		if (((cur - prev) * 10) / cur > 0)
		{
			if (mode == 1)
				printf ("\t%d, // %d\n", cur, slot);
			prev = cur;
			++slot;
		}

		cur += dstep;
	}

	if (mode == 1)
		printf ("\t%d, // %d\n", cur, slot);

	return 0;
}

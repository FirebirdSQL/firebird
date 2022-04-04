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
 *  Copyright (c) 2017 Alexander Peshkov <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ________________________________
 */

#include "firebird.h"

#include "../common/classes/auto.h"
#include "../common/classes/array.h"
#include "../common/utils_proto.h"

namespace Firebird {

	class Transliterate
	{
	public:
		virtual void transliterate(IStatus* status) = 0;
	};

	class BatchCompletionState final :
		public DisposeIface<IBatchCompletionStateImpl<BatchCompletionState, CheckStatusWrapper> >
	{
	public:
		BatchCompletionState(bool storeCounts, ULONG lim)
			: rare(getPool()),
			  reccount(0u),
			  detailedLimit(lim)
		{
			if (storeCounts)
				array = FB_NEW_POOL(getPool()) DenseArray(getPool());
		}

		~BatchCompletionState()
		{
			for (unsigned i = 0; i < rare.getCount() && rare[i].second; ++i)
				rare[i].second->dispose();
		}

		void regError(IStatus* errStatus, Transliterate* transliterate)
		{
			IStatus* newVector = nullptr;
			if (rare.getCount() < detailedLimit)
			{
				newVector = errStatus->clone();
				if (transliterate)
					transliterate->transliterate(newVector);
			}
			rare.add(StatusPair(reccount, newVector));

			regUpdate(IBatchCompletionState::EXECUTE_FAILED);
		}

		void regErrorAt(ULONG at, IStatus* errStatus)
		{
			IStatus* newVector = nullptr;
			if ((rare.getCount() < detailedLimit) && errStatus)
				newVector = errStatus->clone();
			rare.add(StatusPair(at, newVector));
		}

		void regUpdate(SLONG count)
		{
			if (array)
				array->push(count);

			++reccount;
		}

		void regSize(ULONG total)
		{
			reccount = total;
		}

		// IBatchCompletionState implementation
		unsigned getSize(CheckStatusWrapper*)
		{
			return reccount;
		}

		int getState(CheckStatusWrapper* status, unsigned pos)
		{
			try
			{
				checkRange(pos);

				if (array)
					return (*array)[pos];

				ULONG index = find(pos);
				return (index >= rare.getCount() || rare[index].first != pos) ?
					SUCCESS_NO_INFO : EXECUTE_FAILED;
			}
			catch (const Exception& ex)
			{
				ex.stuffException(status);
			}
			return 0;
		}

		unsigned findError(CheckStatusWrapper* status, unsigned pos)
		{
			try
			{
				ULONG index = find(pos);
				if (index < rare.getCount())
					return rare[index].first;
			}
			catch (const Exception& ex)
			{
				ex.stuffException(status);
			}
			return NO_MORE_ERRORS;
		}

		void getStatus(CheckStatusWrapper* status, IStatus* to, unsigned pos)
		{
			try
			{
				checkRange(pos);

				ULONG index = find(pos);
				if (index < rare.getCount() && rare[index].first == pos)
				{
					if (rare[index].second)
					{
						CheckStatusWrapper w(to);
						fb_utils::copyStatus(&w, rare[index].second);
						return;
					}
					(Arg::Gds(isc_batch_compl_detail) << Arg::Num(pos)).raise();
				}
			}
			catch (const Exception& ex)
			{
				ex.stuffException(status);
			}
		}

	private:
		typedef Pair<NonPooled<ULONG, IStatus*> > StatusPair;
		typedef Array<StatusPair> RarefiedArray;
		RarefiedArray rare;
		typedef Array<SLONG> DenseArray;
		AutoPtr<DenseArray> array;
		ULONG reccount, detailedLimit;

		ULONG find(ULONG recno) const
		{
			ULONG high = rare.getCount(), low = 0;
			while (high > low)
			{
				ULONG med = (high + low) / 2;
				if (recno > rare[med].first)
					low = med + 1;
				else
					high = med;
			}

			return low;
		}

		void checkRange(unsigned pos)
		{
			if (pos >= reccount)
				(Arg::Gds(isc_batch_compl_range) << Arg::Num(pos) << Arg::Num(reccount)).raise();
		}
	};
}

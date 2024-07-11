/*
 *	PROGRAM:		Firebird interface.
 *	MODULE:			firebird/Interface.h
 *	DESCRIPTION:	Base class for all FB interfaces / plugins.
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
 *  Copyright (c) 2010 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#ifndef FB_INTERFACE_H
#define FB_INTERFACE_H

#include "ibase.h"
#include <assert.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define CLOOP_CARG __cdecl
#endif

struct dsc;

namespace Firebird
{

// Performance counters for individual table
typedef int ntrace_relation_t;
struct TraceCounts
{
	// Per-table performance counters, must correspond to RuntimeStatistics::StatType
	// between RECORD_FIRST_ITEM and RECORD_LAST_ITEM
	enum RecordCounters
	{
		SEQ_READS = 0,
		IDX_READS,
		UPDATES,
		INSERTS,
		DELETES,
		BACKOUTS,
		PURGES,
		EXPUNGES,
		LOCKS,
		WAITS,
		CONFLICTS,
		BACKVERSION_READS,
		FRAGMENT_READS,
		RPT_READS,
		IMGC
	};

	ntrace_relation_t	trc_relation_id;	// Relation ID
	const char*			trc_relation_name;	// Relation name
	const ISC_INT64*	trc_counters;	    // Pointer to allow easy addition of new counters
};

// Performance statistics for operation
struct PerformanceInfo
{
	// IO performance counters, must correspond to RuntimeStatistics::StatType
	// between PAGE_FETCHES and (not including) RECORD_FIRST_ITEM
	enum PageCounters
	{
		FETCHES = 0,
		READS,
		MARKS,
		WRITES
	};

	ISC_INT64 pin_time;				// Total operation time in milliseconds
	ISC_INT64* pin_counters;		// Pointer to allow easy addition of new counters

	size_t pin_count;				// Number of relations involved in analysis
	struct TraceCounts* pin_tables; // Pointer to array with table stats

	ISC_INT64 pin_records_fetched;	// records fetched from statement/procedure
};

inline const intptr_t* stubError()
{
	static const intptr_t codes[] = {
		isc_arg_gds, isc_random,
		isc_arg_string, (intptr_t) "Unrecognized exception in Status interface",
		isc_arg_end
	};

	return codes;
}

} // namespace Firebird

#ifndef FB_UsedInYValve
#define FB_UsedInYValve false
#endif

#include "IdlFbInterfaces.h"

namespace Firebird
{
	class FbException
	{
	public:
		FbException(IStatus* aStatus, const ISC_STATUS* vector)
		{
			aStatus->setErrors(vector);
			status = aStatus->clone();
		}

		FbException(IStatus* aStatus)
			: status(aStatus->clone())
		{
		}

		FbException(const FbException& copy)
			: status(copy.status->clone())
		{
		}

		FbException& operator =(const FbException& copy)
		{
			status->dispose();
			status = copy.status->clone();
			return *this;
		}

		virtual ~FbException()
		{
			status->dispose();
		}

	public:
		static void check(ISC_STATUS code, IStatus* status, const ISC_STATUS* vector)
		{
			if (code != 0 && vector[1])
				throw FbException(status, vector);
		}

	public:
		IStatus* getStatus() const
		{
			return status;
		}

	private:
		IStatus* status;
	};

	template <typename T>
	class BaseStatusWrapper : public IStatusImpl<T, T>
	{
	public:
		BaseStatusWrapper(IStatus* aStatus)
			: status(aStatus),
			  dirty(false)
		{
		}

	public:
		static void catchException(IStatus* status)
		{
			if (!status)
				return;

			try
			{
				throw;
			}
			catch (const FbException& e)
			{
				status->setErrors(e.getStatus()->getErrors());
			}
			catch (...)
			{
				ISC_STATUS statusVector[] = {
					isc_arg_gds, isc_random,
					isc_arg_string, (ISC_STATUS) "Unrecognized C++ exception",
					isc_arg_end};
				status->setErrors(statusVector);
			}
		}

		static void clearException(BaseStatusWrapper* status)
		{
			status->clearException();
		}

		void clearException()
		{
			if (dirty)
			{
				dirty = false;
				status->init();
			}
		}

		bool isDirty() const
		{
			return dirty;
		}

		bool hasData() const
		{
			return getState() & IStatus::STATE_ERRORS;
		}

		bool isEmpty() const
		{
			return !hasData();
		}

		bool isSuccess() const
		{
			return isEmpty();
		}

		static void setVersionError(IStatus* status, const char* interfaceName,
			unsigned currentVersion, unsigned expectedVersion)
		{
			intptr_t codes[] = {
				isc_arg_gds,
				isc_interface_version_too_old,
				isc_arg_number,
				(intptr_t) expectedVersion,
				isc_arg_number,
				(intptr_t) currentVersion,
				isc_arg_string,
				(intptr_t) interfaceName,
				isc_arg_end
			};

			status->setErrors(codes);
		}

	public:
		virtual void dispose()
		{
			// Disposes only the delegated status. Let the user destroy this instance.
			status->dispose();
		}

		virtual void init()
		{
			clearException();
		}

		virtual unsigned getState() const
		{
			return dirty ? status->getState() : 0;
		}

		virtual void setErrors2(unsigned length, const intptr_t* value)
		{
			dirty = true;
			status->setErrors2(length, value);
		}

		virtual void setWarnings2(unsigned length, const intptr_t* value)
		{
			dirty = true;
			status->setWarnings2(length, value);
		}

		virtual void setErrors(const intptr_t* value)
		{
			dirty = true;
			status->setErrors(value);
		}

		virtual void setWarnings(const intptr_t* value)
		{
			dirty = true;
			status->setWarnings(value);
		}

		virtual const intptr_t* getErrors() const
		{
			return dirty ? status->getErrors() : cleanStatus();
		}

		virtual const intptr_t* getWarnings() const
		{
			return dirty ? status->getWarnings() : cleanStatus();
		}

		virtual IStatus* clone() const
		{
			return status->clone();
		}

	protected:
		IStatus* status;
		bool dirty;

		static const intptr_t* cleanStatus()
		{
			static intptr_t clean[3] = {1, 0, 0};
			return clean;
		}
	};

	class CheckStatusWrapper : public BaseStatusWrapper<CheckStatusWrapper>
	{
	public:
		CheckStatusWrapper(IStatus* aStatus)
			: BaseStatusWrapper(aStatus)
		{
		}

	public:
		static void checkException(CheckStatusWrapper* /* status */)
		{
		}
	};

	class ThrowStatusWrapper : public BaseStatusWrapper<ThrowStatusWrapper>
	{
	public:
		ThrowStatusWrapper(IStatus* aStatus)
			: BaseStatusWrapper(aStatus)
		{
		}

	public:
		static void checkException(ThrowStatusWrapper* status)
		{
			if (status->dirty && (status->getState() & IStatus::STATE_ERRORS))
				throw FbException(status->status);
		}
	};

#ifdef FB_API_VER	// internal hack
	class Helper
	{
	public:
		template <typename StatusType>
		static isc_db_handle getIscDbHandle(StatusType* status, IAttachment* attachment)
		{
			if (!attachment)
				return 0;

			ISC_STATUS_ARRAY statusVector = {0};
			isc_db_handle handle = 0;

			fb_get_database_handle(statusVector, &handle, attachment);

			if (!handle)
			{
				status->setErrors(statusVector);
				StatusType::checkException(status);
			}

			return handle;
		}

		template <typename StatusType>
		static isc_db_handle getIscDbHandle(StatusType* status, IExternalContext* context)
		{
			IAttachment* attachment = context->getAttachment(status);

			if (!attachment)
				return 0;

			try
			{
				isc_db_handle handle = getIscDbHandle(status, attachment);
				attachment->release();
				return handle;
			}
			catch (...)
			{
				attachment->release();
				throw;
			}
		}

		template <typename StatusType>
		static isc_tr_handle getIscTrHandle(StatusType* status, ITransaction* transaction)
		{
			if (!transaction)
				return 0;

			ISC_STATUS_ARRAY statusVector = {0};
			isc_tr_handle handle = 0;

			fb_get_transaction_handle(statusVector, &handle, transaction);

			if (!handle)
			{
				status->setErrors(statusVector);
				StatusType::checkException(status);
			}

			return handle;
		}

		template <typename StatusType>
		static isc_tr_handle getIscTrHandle(StatusType* status, IExternalContext* context)
		{
			ITransaction* transaction = context->getTransaction(status);

			if (!transaction)
				return 0;

			try
			{
				isc_tr_handle handle = getIscTrHandle(status, transaction);
				transaction->release();
				return handle;
			}
			catch (...)
			{
				transaction->release();
				throw;
			}
		}
	};
#endif	// FB_API_VER

	// Additional API function.
	// Should be used only in non-plugin modules.
	// All plugins including providers should use passed at init time interface instead.
	extern "C" IMaster* ISC_EXPORT fb_get_master_interface();

} // namespace Firebird

#define FB_PLUGIN_ENTRY_POINT firebird_plugin
#define FB_UDR_PLUGIN_ENTRY_POINT firebird_udr_plugin

#endif // FB_INTERFACE_H

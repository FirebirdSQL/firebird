/*
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

#ifndef JRD_SAVEPOINT_H
#define JRD_SAVEPOINT_H

#include "../common/classes/File.h"
#include "../common/classes/MetaName.h"
#include "../jrd/Record.h"
#include "../jrd/RecordNumber.h"

namespace Jrd
{
	class jrd_tra;

	// Verb actions

	class UndoItem
	{
	public:
		static const SINT64& generate(const void* /*sender*/, const UndoItem& item)
		{
			return item.m_number;
		}

		UndoItem()
			: m_number(0), m_offset(0), m_format(NULL)
		{}

		UndoItem(RecordNumber recordNumber)
			: m_number(recordNumber.getValue()), m_offset(0), m_format(NULL)
		{}

		UndoItem(jrd_tra* transaction, RecordNumber recordNumber, const Record* record);

		Record* setupRecord(jrd_tra* transaction) const;
		void release(jrd_tra* transaction);

		void clear()
		{
			m_format = NULL;
		}

		bool hasData() const
		{
			return (m_format != NULL);
		}

		bool isEmpty() const
		{
			return (m_format == NULL);
		}

	private:
		SINT64 m_number;
		offset_t m_offset;
		const Format* m_format;
	};

	typedef Firebird::BePlusTree<UndoItem, SINT64, MemoryPool, UndoItem> UndoItemTree;

	class VerbAction
	{
	public:
		VerbAction()
			: vct_next(NULL), vct_relation(NULL), vct_records(NULL), vct_undo(NULL)
		{}

		~VerbAction()
		{
			delete vct_records;
			delete vct_undo;
		}

		VerbAction* 	vct_next;		// Next action within verb
		jrd_rel*		vct_relation;	// Relation involved
		RecordBitmap*	vct_records;	// Record involved
		UndoItemTree*	vct_undo;		// Data for undo records

		void mergeTo(thread_db* tdbb, jrd_tra* transaction, VerbAction* nextAction);
		void undo(thread_db* tdbb, jrd_tra* transaction);
		void garbageCollectIdxLite(thread_db* tdbb, jrd_tra* transaction, SINT64 recordNumber,
								   VerbAction* nextAction, Record* goingRecord);

	private:
		void release(jrd_tra* transaction);
	};

	// Savepoint class

	class Savepoint
	{
		// Maximum size in bytes of transaction-level savepoint data.
		// When transaction-level savepoint gets past this size we drop it and use GC
		// mechanisms to clean out changes done in transaction
		static const U_IPTR SIZE_THRESHOLD = 1024 * 32;

		// Savepoint flags
		static const USHORT SAV_root		= 1;	// transaction-level savepoint
		static const USHORT SAV_force_dfw	= 2;	// DFW is present even if savepoint is empty
		static const USHORT SAV_replicated	= 4;	// savepoint has already been replicated

	public:
		explicit Savepoint(jrd_tra* transaction)
			: m_transaction(transaction), m_number(0), m_flags(0), m_count(0),
			  m_next(NULL), m_actions(NULL), m_freeActions(NULL)
		{}

		~Savepoint()
		{
			while (m_actions)
			{
				VerbAction* next = m_actions->vct_next;
				delete m_actions;
				m_actions = next;
			}

			while (m_freeActions)
			{
				VerbAction* next = m_freeActions->vct_next;
				delete m_freeActions;
				m_freeActions = next;
			}
		}

		void init(SavNumber number, bool root, Savepoint* next)
		{
			m_number = number;
			m_flags |= root ? SAV_root : 0;
			m_next = next;
			fb_assert(m_next != this);
		}

		VerbAction* getAction(const jrd_rel* relation) const
		{
			// Find and return (if exists) action that belongs to the given relation

			for (VerbAction* action = m_actions; action; action = action->vct_next)
			{
				if (action->vct_relation == relation)
					return action;
			}

			return NULL;
		}

		Savepoint* getNext() const
		{
			return m_next;
		}

		SavNumber getNumber() const
		{
			return m_number;
		}

		const Firebird::MetaName& getName() const
		{
			return m_name;
		}

		void setName(const Firebird::MetaName& name)
		{
			m_name = name;
		}

		bool isSystem() const
		{
			return m_name.isEmpty();
		}

		bool isRoot() const
		{
			return (m_flags & SAV_root);
		}

		bool isReplicated() const
		{
			return (m_flags & SAV_replicated);
		}

		bool isChanging() const
		{
			return (m_count != 0);
		}

		bool hasChanges() const
		{
			return (m_actions != NULL);
		}

		void forceDeferredWork()
		{
			m_flags |= SAV_force_dfw;
		}

		void markAsReplicated()
		{
			m_flags |= SAV_replicated;
		}

		Savepoint* moveToStack(Savepoint*& target)
		{
			// Relink savepoint to the top of the provided savepoint stack.
			// Return the former "next" pointer to the caller.

			Savepoint* const next = m_next;
			m_next = target;
			fb_assert(m_next != this);
			target = this;
			return next;
		}

		VerbAction* createAction(jrd_rel* relation);

		void cleanupTempData();

		Savepoint* rollback(thread_db* tdbb, Savepoint* prior = NULL);
		Savepoint* rollforward(thread_db* tdbb, Savepoint* prior = NULL);

		static void destroy(Savepoint*& savepoint)
		{
			while (savepoint)
			{
				Savepoint* const next = savepoint->m_next;
				delete savepoint;
				savepoint = next;
			}
		}

		static void mergeStacks(Savepoint*& target, Savepoint*& source)
		{
			// Given two savepoint stacks, merge them together.
			// The source stack becomes empty after that.

			while (source)
				source = source->moveToStack(target);
		}

		class Iterator
		{
		public:
			explicit Iterator(Savepoint* savepoint)
				: m_savepoint(savepoint)
			{}

			Iterator& operator++()
			{
				if (m_savepoint)
					m_savepoint = m_savepoint->m_next;

				return *this;
			}

			Savepoint* operator*() const
			{
				return m_savepoint;
			}

		private:
			Iterator(const Iterator&);
			Iterator& operator=(const Iterator&);

			Savepoint* m_savepoint;
		};

		class ChangeMarker
		{
		public:
			explicit ChangeMarker(Savepoint* savepoint)
				: m_savepoint(savepoint)
			{
				if (m_savepoint)
					++m_savepoint->m_count;
			}

			~ChangeMarker()
			{
				if (m_savepoint)
					--m_savepoint->m_count;
			}

		private:
			ChangeMarker(const ChangeMarker&);
			ChangeMarker& operator=(const ChangeMarker&);

			Savepoint* const m_savepoint;
		};

	private:
		// Prohibit unwanted creation/copying
		Savepoint(const Savepoint&);
		Savepoint& operator=(const Savepoint&);

		bool isLarge() const;
		Savepoint* release(Savepoint* prior = NULL);

		jrd_tra* const m_transaction; 	// transaction this savepoint belongs to
		SavNumber m_number;				// savepoint number
		USHORT m_flags;					// misc flags
		USHORT m_count;					// active verb count
		Firebird::MetaName m_name; 		// savepoint name
		Savepoint* m_next;				// next savepoint in the list


		VerbAction* m_actions;			// verb action list
		VerbAction* m_freeActions;		// free verb actions
	};


	// Starts a savepoint and rollback it in destructor if release() is not called

	class AutoSavePoint
	{
	public:
		AutoSavePoint(thread_db* tdbb, jrd_tra* trans);
		~AutoSavePoint();

		void release()
		{
			m_released = true;
		}

	private:
		thread_db* const m_tdbb;
		jrd_tra* const m_transaction;
		bool m_released;
	};

	class StableCursorSavePoint
	{
	public:
		StableCursorSavePoint(thread_db* tdbb, jrd_tra* trans, bool start);

		~StableCursorSavePoint()
		{
			release();
		}

		void release();

	private:
		thread_db* const m_tdbb;
		jrd_tra* const m_transaction;
		SavNumber m_number;
	};

} // namespace

#endif // JRD_SAVEPOINT_H

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
 *  The Original Code was created by Dmitry Yemanov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2009 Dmitry Yemanov <dimitr@firebirdsql.org>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JRD_RECORD_SOURCE_H
#define JRD_RECORD_SOURCE_H

#include "../common/classes/array.h"
#include "../common/classes/objects_array.h"
#include "../common/classes/NestConst.h"
#include "../jrd/RecordSourceNodes.h"
#include "../jrd/req.h"
#include "../jrd/rse.h"
#include "firebird/impl/inf_pub.h"
#include "../jrd/evl_proto.h"

namespace Jrd
{
	class thread_db;
	class jrd_req;
	class jrd_prc;
	class AggNode;
	class BoolExprNode;
	class Sort;
	class CompilerScratch;
	class RecordBuffer;
	class BtrPageGCLock;
	struct index_desc;
	struct record_param;
	struct temporary_key;
	struct win;
	class BaseBufferedStream;
	class BufferedStream;

	enum JoinType { INNER_JOIN, OUTER_JOIN, SEMI_JOIN, ANTI_JOIN };

	// Abstract base class

	class RecordSource
	{
	public:
		virtual void open(thread_db* tdbb) const = 0;
		virtual void close(thread_db* tdbb) const = 0;

		virtual bool getRecord(thread_db* tdbb) const = 0;
		virtual bool refetchRecord(thread_db* tdbb) const = 0;
		virtual bool lockRecord(thread_db* tdbb) const = 0;

		virtual void print(thread_db* tdbb, Firebird::string& plan,
						   bool detailed, unsigned level) const = 0;

		virtual void markRecursive() = 0;
		virtual void invalidateRecords(jrd_req* request) const = 0;

		virtual void findUsedStreams(StreamList& streams, bool expandAll = false) const = 0;
		virtual void nullRecords(thread_db* tdbb) const = 0;

		virtual void setAnyBoolean(BoolExprNode* /*anyBoolean*/, bool /*ansiAny*/, bool /*ansiNot*/)
		{
			fb_assert(false);
		}

		virtual ~RecordSource();

		static bool rejectDuplicate(const UCHAR* /*data1*/, const UCHAR* /*data2*/, void* /*userArg*/)
		{
			return true;
		}

	protected:
		// Generic impure block
		struct Impure
		{
			ULONG irsb_flags;
		};

		static const ULONG irsb_open = 1;
		static const ULONG irsb_first = 2;
		static const ULONG irsb_joined = 4;
		static const ULONG irsb_mustread = 8;
		static const ULONG irsb_singular_processed = 16;

		RecordSource()
			: m_impure(0), m_recursive(false)
		{}

		static Firebird::string printName(thread_db* tdbb, const Firebird::string& name, bool quote = true);
		static Firebird::string printName(thread_db* tdbb, const Firebird::string& name,
										  const Firebird::string& alias);

		static Firebird::string printIndent(unsigned level);
		static void printInversion(thread_db* tdbb, const InversionNode* inversion,
								   Firebird::string& plan, bool detailed,
								   unsigned level, bool navigation = false);

		static void saveRecord(thread_db* tdbb, record_param* rpb);
		static void restoreRecord(thread_db* tdbb, record_param* rpb);

		ULONG m_impure;
		bool m_recursive;
	};


	// Helper class implementing some common methods

	class RecordStream : public RecordSource
	{
	public:
		RecordStream(CompilerScratch* csb, StreamType stream, const Format* format = NULL);

		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	protected:
		const StreamType m_stream;
		const Format* const m_format;
	};


	// Primary (table scan) access methods

	class FullTableScan : public RecordStream
	{
		struct Impure : public RecordSource::Impure
		{
			RecordNumber irsb_lower;
			RecordNumber irsb_upper;
		};

	public:
		FullTableScan(CompilerScratch* csb, const Firebird::string& alias,
					  StreamType stream, jrd_rel* relation,
					  const Firebird::Array<DbKeyRangeNode*>& dbkeyRanges);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

	private:
		const Firebird::string m_alias;
		jrd_rel* const m_relation;
		Firebird::Array<DbKeyRangeNode*> m_dbkeyRanges;
	};

	class BitmapTableScan : public RecordStream
	{
		struct Impure : public RecordSource::Impure
		{
			RecordBitmap** irsb_bitmap;
		};

	public:
		BitmapTableScan(CompilerScratch* csb, const Firebird::string& alias,
						StreamType stream, jrd_rel* relation, InversionNode* inversion);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

	private:
		const Firebird::string m_alias;
		jrd_rel* const m_relation;
		NestConst<InversionNode> const m_inversion;
	};

	class IndexTableScan : public RecordStream
	{
		struct Impure : public RecordSource::Impure
		{
			RecordNumber irsb_nav_number;				// last record number
			ULONG irsb_nav_page;						// index page number
			SLONG irsb_nav_incarnation;					// buffer/page incarnation counter
			RecordBitmap** irsb_nav_bitmap;				// bitmap for inversion tree
			RecordBitmap* irsb_nav_records_visited;		// bitmap of records already retrieved
			BtrPageGCLock* irsb_nav_btr_gc_lock;		// lock to prevent removal of currently walked index page
			USHORT irsb_nav_offset;						// page offset of current index node
			USHORT irsb_nav_upper_length;				// length of upper key value
			USHORT irsb_nav_length;						// length of expanded key
			UCHAR irsb_nav_data[1];						// expanded key, upper bound, and index desc
		};

	public:
		IndexTableScan(CompilerScratch* csb, const Firebird::string& alias,
					   StreamType stream, jrd_rel* relation,
					   InversionNode* index, USHORT keyLength);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void setInversion(InversionNode* inversion, BoolExprNode* condition)
		{
			fb_assert(!m_inversion && !m_condition);
			m_inversion = inversion;
			m_condition = condition;
		}

	private:
		int compareKeys(const index_desc*, const UCHAR*, USHORT, const temporary_key*, USHORT) const;
		bool findSavedNode(thread_db* tdbb, Impure* impure, win* window, UCHAR**) const;
		UCHAR* getPosition(thread_db* tdbb, Impure* impure, win* window) const;
		UCHAR* openStream(thread_db* tdbb, Impure* impure, win* window) const;
		void setPage(thread_db* tdbb, Impure* impure, win* window) const;
		void setPosition(thread_db* tdbb, Impure* impure, record_param*,
						 win* window, const UCHAR*, const temporary_key&) const;
		bool setupBitmaps(thread_db* tdbb, Impure* impure) const;

		const Firebird::string m_alias;
		jrd_rel* const m_relation;
		NestConst<InversionNode> const m_index;
		NestConst<InversionNode> m_inversion;
		NestConst<BoolExprNode> m_condition;
		const FB_SIZE_T m_length;
		FB_SIZE_T m_offset;
	};

	class ExternalTableScan : public RecordStream
	{
		struct Impure : public RecordSource::Impure
		{
			FB_UINT64 irsb_position;
		};

	public:
		ExternalTableScan(CompilerScratch* csb, const Firebird::string& alias,
						  StreamType stream, jrd_rel* relation);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

	private:
		jrd_rel* const m_relation;
		const Firebird::string m_alias;
	};

	class VirtualTableScan : public RecordStream
	{
	public:
		VirtualTableScan(CompilerScratch* csb, const Firebird::string& alias,
						 StreamType stream, jrd_rel* relation);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

	protected:
		virtual const Format* getFormat(thread_db* tdbb, jrd_rel* relation) const = 0;
		virtual bool retrieveRecord(thread_db* tdbb, jrd_rel* relation,
									FB_UINT64 position, Record* record) const = 0;

	private:
		jrd_rel* const m_relation;
		const Firebird::string m_alias;
	};

	class ProcedureScan : public RecordStream
	{
		struct Impure : public RecordSource::Impure
		{
			jrd_req* irsb_req_handle;
			UCHAR* irsb_message;
		};

	public:
		ProcedureScan(CompilerScratch* csb, const Firebird::string& alias, StreamType stream,
					  const jrd_prc* procedure, const ValueListNode* sourceList,
					  const ValueListNode* targetList, MessageNode* message);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

	private:
		void assignParams(thread_db* tdbb, const dsc* from_desc, const dsc* flag_desc,
						  const UCHAR* msg, const dsc* to_desc, SSHORT to_id, Record* record) const;

		const Firebird::string m_alias;
		const jrd_prc* const m_procedure;
		const ValueListNode* m_sourceList;
		const ValueListNode* m_targetList;
		NestConst<MessageNode> const m_message;
	};


	// Filtering (one -> one) access methods

	class SingularStream : public RecordSource
	{
	public:
		SingularStream(CompilerScratch* csb, RecordSource* next);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		void doGetRecord(thread_db* tdbb) const;

		NestConst<RecordSource> m_next;
		StreamList m_streams;
	};

	class LockedStream : public RecordSource
	{
	public:
		LockedStream(CompilerScratch* csb, RecordSource* next);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		NestConst<RecordSource> m_next;
	};

	class FirstRowsStream : public RecordSource
	{
		struct Impure : public RecordSource::Impure
		{
			SINT64 irsb_count;
		};

	public:
		FirstRowsStream(CompilerScratch* csb, RecordSource* next, ValueExprNode* value);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

		void setAnyBoolean(BoolExprNode* anyBoolean, bool ansiAny, bool ansiNot) override
		{
			m_next->setAnyBoolean(anyBoolean, ansiAny, ansiNot);
		}

	private:
		NestConst<RecordSource> m_next;
		NestConst<ValueExprNode> const m_value;
	};

	class SkipRowsStream : public RecordSource
	{
		struct Impure : public RecordSource::Impure
		{
			SINT64 irsb_count;
		};

	public:
		SkipRowsStream(CompilerScratch* csb, RecordSource* next, ValueExprNode* value);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

		void setAnyBoolean(BoolExprNode* anyBoolean, bool ansiAny, bool ansiNot) override
		{
			m_next->setAnyBoolean(anyBoolean, ansiAny, ansiNot);
		}

	private:
		NestConst<RecordSource> m_next;
		NestConst<ValueExprNode> const m_value;
	};

	class FilteredStream : public RecordSource
	{
	public:
		FilteredStream(CompilerScratch* csb, RecordSource* next, BoolExprNode* boolean);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

		void setAnyBoolean(BoolExprNode* anyBoolean, bool ansiAny, bool ansiNot) override
		{
			fb_assert(!m_anyBoolean);
			m_anyBoolean = anyBoolean;

			m_ansiAny = ansiAny;
			m_ansiAll = !ansiAny;
			m_ansiNot = ansiNot;
		}

	private:
		bool evaluateBoolean(thread_db* tdbb) const;

		NestConst<RecordSource> m_next;
		NestConst<BoolExprNode> const m_boolean;
		NestConst<BoolExprNode> m_anyBoolean;
		bool m_ansiAny;
		bool m_ansiAll;
		bool m_ansiNot;
	};

	class SortedStream : public RecordSource
	{
		struct Impure : public RecordSource::Impure
		{
			Sort* irsb_sort;
		};

	public:
		static const USHORT FLAG_PROJECT	= 0x1;	// sort is really a project
		static const USHORT FLAG_UNIQUE		= 0x2;	// sorts using unique key - for distinct and group by
		static const USHORT FLAG_KEY_VARY	= 0x4;	// sort key contains varying length string(s)
		static const USHORT FLAG_REFETCH	= 0x8;	// refetch data after sorting

		// Special values for SortMap::Item::fieldId.
		static const SSHORT ID_DBKEY		= -1;	// dbkey value
		static const SSHORT ID_DBKEY_VALID	= -2;	// dbkey valid flag
		static const SSHORT ID_TRANS 		= -3;	// transaction id of record

		// Sort map block
		class SortMap : public Firebird::PermanentStorage
		{
		public:
			struct Item
			{
				void clear()
				{
					desc.clear();
					flagOffset = fieldId = 0;
					stream = 0;
					node = NULL;
				}

				StreamType stream;			// stream for field id
				dsc desc;					// relative descriptor
				ULONG flagOffset;			// offset of missing flag
				SSHORT fieldId;				// id for field (or ID constants)
				NestConst<ValueExprNode> node;	// expression node
			};

			explicit SortMap(MemoryPool& p)
				: PermanentStorage(p),
				  length(0),
				  keyLength(0),
				  flags(0),
				  keyItems(p),
				  items(p)
			{
			}

			ULONG length;			// sort record length
			ULONG keyLength;		// key length
			USHORT flags;			// misc sort flags
			Firebird::Array<sort_key_def> keyItems;	// address of key descriptors
			Firebird::Array<Item> items;
		};

		SortedStream(CompilerScratch* csb, RecordSource* next, SortMap* map);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

		void setAnyBoolean(BoolExprNode* anyBoolean, bool ansiAny, bool ansiNot) override
		{
			m_next->setAnyBoolean(anyBoolean, ansiAny, ansiNot);
		}

		ULONG getLength() const
		{
			return m_map->length;
		}

		ULONG getKeyLength() const
		{
			return m_map->keyLength;
		}

		bool compareKeys(const UCHAR* p, const UCHAR* q) const;

		UCHAR* getData(thread_db* tdbb) const;
		void mapData(thread_db* tdbb, jrd_req* request, UCHAR* data) const;

	private:
		Sort* init(thread_db* tdbb) const;

		NestConst<RecordSource> m_next;
		const SortMap* const m_map;
	};

	// Make moves in a window without going out of partition boundaries.
	class SlidingWindow
	{
	public:
		SlidingWindow(thread_db* aTdbb, const BaseBufferedStream* aStream, jrd_req* request,
			FB_UINT64 aPartitionStart, FB_UINT64 aPartitionEnd,
			FB_UINT64 aFrameStart, FB_UINT64 aFrameEnd);
		~SlidingWindow();

		FB_UINT64 getPartitionStart() const
		{
			return partitionStart;
		}

		FB_UINT64 getPartitionEnd() const
		{
			return partitionEnd;
		}

		FB_UINT64 getPartitionSize() const
		{
			return partitionEnd - partitionStart + 1;
		}

		FB_UINT64 getFrameStart() const
		{
			return frameStart;
		}

		FB_UINT64 getFrameEnd() const
		{
			return frameEnd;
		}

		FB_UINT64 getFrameSize() const
		{
			return frameEnd - frameStart + 1;
		}

		FB_UINT64 getRecordPosition() const
		{
			return savedPosition;
		}

		bool moveWithinPartition(SINT64 delta);
		bool moveWithinFrame(SINT64 delta);

	private:
		thread_db* tdbb;
		const BaseBufferedStream* const stream;
		FB_UINT64 partitionStart;
		FB_UINT64 partitionEnd;
		FB_UINT64 frameStart;
		FB_UINT64 frameEnd;
		FB_UINT64 savedPosition;
		bool moved;
	};

	template <typename ThisType, typename NextType>
	class BaseAggWinStream : public RecordStream
	{
	protected:
		enum State
		{
			STATE_EOF,			// We processed everything now process EOF
			STATE_FETCHED,		// Values are pending from a prior fetch
			STATE_GROUPING		// Entering EVL group before fetching the first record
		};

		struct Impure : public RecordSource::Impure
		{
			impure_value* groupValues;
			State state;
		};

		struct DummyAdjustFunctor
		{
			void operator ()(thread_db* /*tdbb*/, impure_value* /*target*/)
			{
			}
		};

	public:
		BaseAggWinStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
			const NestValueArray* group, MapNode* groupMap, bool oneRowWhenEmpty, NextType* next);

	public:
		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;

	protected:
		Impure* getImpure(jrd_req* request) const
		{
			return request->getImpure<typename ThisType::Impure>(m_impure);
		}

		bool evaluateGroup(thread_db* tdbb) const;

		void aggInit(thread_db* tdbb, jrd_req* request, const MapNode* map) const;
		bool aggPass(thread_db* tdbb, jrd_req* request,
			const NestValueArray& sourceList, const NestValueArray& targetList) const;
		void aggExecute(thread_db* tdbb, jrd_req* request,
			const NestValueArray& sourceList, const NestValueArray& targetList) const;
		void aggFinish(thread_db* tdbb, jrd_req* request, const MapNode* map) const;

		// Cache the values of a group/order in the impure.
		template <typename AdjustFunctor>
		void cacheValues(thread_db* tdbb, jrd_req* request,
			const NestValueArray* group, impure_value* values,
			AdjustFunctor adjustFunctor) const
		{
			if (!group)
				return;

			Impure* const impure = getImpure(request);

			for (const NestConst<ValueExprNode>* ptrValue = group->begin(), *endValue = group->end();
				 ptrValue != endValue;
				 ++ptrValue)
			{
				const ValueExprNode* from = *ptrValue;
				impure_value* target = &values[ptrValue - group->begin()];

				dsc* desc = EVL_expr(tdbb, request, from);

				if (request->req_flags & req_null)
					target->vlu_desc.dsc_address = NULL;
				else
				{
					EVL_make_value(tdbb, desc, target);
					adjustFunctor(tdbb, target);
				}
			}
		}

		int lookForChange(thread_db* tdbb, jrd_req* request,
			const NestValueArray* group, const SortNode* sort, impure_value* values) const;

	private:
		bool getNextRecord(thread_db* tdbb, jrd_req* request) const;

	protected:
		NestConst<NextType> m_next;
		const NestValueArray* const m_group;
		NestConst<MapNode> m_groupMap;
		bool m_oneRowWhenEmpty;
	};

	class AggregatedStream : public BaseAggWinStream<AggregatedStream, RecordSource>
	{
	public:
		AggregatedStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
			const NestValueArray* group, MapNode* map, RecordSource* next);

	public:
		void print(thread_db* tdbb, Firebird::string& plan, bool detailed, unsigned level) const;
		bool getRecord(thread_db* tdbb) const;
	};

	class WindowedStream : public RecordSource
	{
	public:
		class WindowStream : public BaseAggWinStream<WindowStream, BaseBufferedStream>
		{
		private:
			struct AdjustFunctor
			{
				AdjustFunctor(const ArithmeticNode* aArithNode, const dsc* aOffsetDesc)
					: arithNode(aArithNode),
					  offsetDesc(aOffsetDesc)
				{
				}

				void operator ()(thread_db* tdbb, impure_value* target)
				{
					ArithmeticNode::add2(tdbb, offsetDesc, target, arithNode, arithNode->blrOp);
				}

				const ArithmeticNode* arithNode;
				const dsc* offsetDesc;
			};

			struct Block
			{
				SINT64 startPosition;
				SINT64 endPosition;

				void invalidate()
				{
					startPosition = endPosition = MIN_SINT64;
				}

				bool isValid() const
				{
					return !(startPosition == MIN_SINT64 && endPosition == MIN_SINT64);
				}
			};

		public:
			struct Impure : public BaseAggWinStream::Impure
			{
				impure_value* orderValues;
				SINT64 partitionPending, rangePending;
				Block partitionBlock, windowBlock;
				impure_value_ex startOffset, endOffset;
			};

		public:
			WindowStream(thread_db* tdbb, CompilerScratch* csb, StreamType stream,
				const NestValueArray* group, BaseBufferedStream* next,
				SortNode* order, MapNode* windowMap,
				WindowClause::FrameExtent* frameExtent,
				WindowClause::Exclusion exclusion);

		public:
			void open(thread_db* tdbb) const;
			void close(thread_db* tdbb) const;

			bool getRecord(thread_db* tdbb) const;

			void print(thread_db* tdbb, Firebird::string& plan, bool detailed, unsigned level) const;
			void findUsedStreams(StreamList& streams, bool expandAll = false) const;
			void nullRecords(thread_db* tdbb) const;

		protected:
			Impure* getImpure(jrd_req* request) const
			{
				return request->getImpure<Impure>(m_impure);
			}

		private:
			const void getFrameValue(thread_db* tdbb, jrd_req* request,
				const WindowClause::Frame* frame, impure_value_ex* impureValue) const;

			SINT64 locateFrameRange(thread_db* tdbb, jrd_req* request, Impure* impure,
				const WindowClause::Frame* frame, const dsc* offsetDesc, SINT64 position) const;

		private:
			NestConst<SortNode> m_order;
			const MapNode* m_windowMap;
			NestConst<WindowClause::FrameExtent> m_frameExtent;
			Firebird::Array<NestConst<ArithmeticNode> > m_arithNodes;
			NestValueArray m_aggSources, m_aggTargets;
			NestValueArray m_winPassSources, m_winPassTargets;
			WindowClause::Exclusion m_exclusion;
			UCHAR m_invariantOffsets;	// 0x1 | 0x2 bitmask
		};

	public:
		WindowedStream(thread_db* tdbb, CompilerScratch* csb,
			Firebird::ObjectsArray<WindowSourceNode::Window>& windows, RecordSource* next);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
			bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		NestConst<BufferedStream> m_next;
		NestConst<RecordSource> m_joinedStream;
	};

	// Abstract class for different implementations of buffered streams.
	class BaseBufferedStream : public RecordSource
	{
	public:
		virtual void locate(thread_db* tdbb, FB_UINT64 position) const = 0;
		virtual FB_UINT64 getCount(thread_db* tdbb) const = 0;
		virtual FB_UINT64 getPosition(jrd_req* request) const = 0;
	};

	class BufferedStream : public BaseBufferedStream
	{
		struct FieldMap
		{
			static const UCHAR REGULAR_FIELD = 1;
			static const UCHAR TRANSACTION_ID = 2;
			static const UCHAR DBKEY_NUMBER = 3;
			static const UCHAR DBKEY_VALID = 4;

			FieldMap() : map_stream(0), map_id(0), map_type(0)
			{}

			FieldMap(UCHAR type, StreamType stream, ULONG id)
				: map_stream(stream), map_id(id), map_type(type)
			{}

			StreamType map_stream;
			USHORT map_id;
			UCHAR map_type;
		};

		struct Impure : public RecordSource::Impure
		{
			RecordBuffer* irsb_buffer;
			FB_UINT64 irsb_position;
		};

	public:
		BufferedStream(CompilerScratch* csb, RecordSource* next);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

		void locate(thread_db* tdbb, FB_UINT64 position) const override;
		FB_UINT64 getCount(thread_db* tdbb) const override;

		FB_UINT64 getPosition(jrd_req* request) const override
		{
			Impure* const impure = request->getImpure<Impure>(m_impure);
			return impure->irsb_position;
		}

	private:
		NestConst<RecordSource> m_next;
		Firebird::HalfStaticArray<FieldMap, OPT_STATIC_ITEMS> m_map;
		const Format* m_format;
	};

	// Multiplexing (many -> one) access methods

	class NestedLoopJoin : public RecordSource
	{
	public:
		NestedLoopJoin(CompilerScratch* csb, FB_SIZE_T count, RecordSource* const* args);
		NestedLoopJoin(CompilerScratch* csb, RecordSource* outer, RecordSource* inner,
					   BoolExprNode* boolean, JoinType joinType);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		bool fetchRecord(thread_db*, FB_SIZE_T) const;

		const JoinType m_joinType;
		Firebird::Array<NestConst<RecordSource> > m_args;
		NestConst<BoolExprNode> const m_boolean;
	};

	class FullOuterJoin : public RecordSource
	{
	public:
		FullOuterJoin(CompilerScratch* csb, RecordSource* arg1, RecordSource* arg2);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		NestConst<RecordSource> m_arg1;
		NestConst<RecordSource> m_arg2;
	};

	class HashJoin : public RecordSource
	{
		class HashTable;

		struct SubStream
		{
			union
			{
				RecordSource* source;
				BufferedStream* buffer;
			};

			NestValueArray* keys;
			ULONG* keyLengths;
			ULONG totalKeyLength;
		};

		struct Impure : public RecordSource::Impure
		{
			HashTable* irsb_hash_table;
			UCHAR* irsb_leader_buffer;
			ULONG irsb_leader_hash;
		};

	public:
		HashJoin(thread_db* tdbb, CompilerScratch* csb, FB_SIZE_T count,
				 RecordSource* const* args, NestValueArray* const* keys);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		ULONG computeHash(thread_db* tdbb, jrd_req* request,
						  const SubStream& sub, UCHAR* buffer) const;
		bool fetchRecord(thread_db* tdbb, Impure* impure, FB_SIZE_T stream) const;

		SubStream m_leader;
		Firebird::Array<SubStream> m_args;
	};

	class MergeJoin : public RecordSource
	{
		struct MergeFile
		{
			TempSpace* mfb_space;				// merge file uses SORT I/O routines
			ULONG mfb_equal_records;			// equality group cardinality
			ULONG mfb_record_size;				// matches sort map length
			ULONG mfb_current_block;			// current merge block in buffer
			ULONG mfb_block_size;				// merge block I/O size
			ULONG mfb_blocking_factor;			// merge equality records per block
			UCHAR* mfb_block_data;				// merge block I/O buffer
		};

		struct Impure : public RecordSource::Impure
		{
			// CVC: should this value exist for compatibility? It's not used.
			USHORT irsb_mrg_count;				// next stream in group
			struct irsb_mrg_repeat
			{
				SLONG irsb_mrg_equal;			// queue of equal records
				SLONG irsb_mrg_equal_end;		// end of the equal queue
				SLONG irsb_mrg_equal_current;	// last fetched record from equal queue
				SLONG irsb_mrg_last_fetched;	// first sort merge record of next group
				SSHORT irsb_mrg_order;			// logical merge order by substream
				MergeFile irsb_mrg_file;		// merge equivalence file
			} irsb_mrg_rpt[1];
		};

		static const FB_SIZE_T MERGE_BLOCK_SIZE = 65536;

	public:
		MergeJoin(CompilerScratch* csb, FB_SIZE_T count,
				  SortedStream* const* args,
				  const NestValueArray* const* keys);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		int compare(thread_db* tdbb, const NestValueArray* node1,
			const NestValueArray* node2) const;
		UCHAR* getData(thread_db* tdbb, MergeFile* mfb, SLONG record) const;
		SLONG getRecord(thread_db* tdbb, FB_SIZE_T index) const;
		bool fetchRecord(thread_db* tdbb, FB_SIZE_T index) const;

		Firebird::Array<NestConst<SortedStream> > m_args;
		Firebird::Array<const NestValueArray*> m_keys;
	};

	class Union : public RecordStream
	{
		struct Impure : public RecordSource::Impure
		{
			USHORT irsb_count;
		};

	public:
		Union(CompilerScratch* csb, StreamType stream,
			  FB_SIZE_T argCount, RecordSource* const* args, NestConst<MapNode>* maps,
			  FB_SIZE_T streamCount, const StreamType* streams);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;
		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;

	private:
		Firebird::Array<NestConst<RecordSource> > m_args;
		Firebird::Array<NestConst<MapNode> > m_maps;
		StreamList m_streams;
	};

	class RecursiveStream : public RecordStream
	{
		static const FB_SIZE_T MAX_RECURSE_LEVEL = 1024;

		enum Mode { ROOT, RECURSE };

		struct Impure: public RecordSource::Impure
		{
			USHORT irsb_level;
			Mode irsb_mode;
			UCHAR* irsb_stack;
			UCHAR* irsb_data;
		};

	public:
		RecursiveStream(CompilerScratch* csb, StreamType stream, StreamType mapStream,
					    RecordSource* root, RecordSource* inner,
					    const MapNode* rootMap, const MapNode* innerMap,
					    FB_SIZE_T streamCount, const StreamType* innerStreams,
					    ULONG saveOffset);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;
		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;

	private:
		void cleanupLevel(jrd_req* request, Impure* impure) const;

		const StreamType m_mapStream;
		NestConst<RecordSource> m_root;
		NestConst<RecordSource> m_inner;
		const MapNode* const m_rootMap;
		const MapNode* const m_innerMap;
		StreamList m_innerStreams;
		const ULONG m_saveOffset;
		FB_SIZE_T m_saveSize;
	};

	class ConditionalStream : public RecordSource
	{
		struct Impure : public RecordSource::Impure
		{
			const RecordSource* irsb_next;
		};

	public:
		ConditionalStream(CompilerScratch* csb, RecordSource* first, RecordSource* second,
						  BoolExprNode* boolean);

		void open(thread_db* tdbb) const override;
		void close(thread_db* tdbb) const override;

		bool getRecord(thread_db* tdbb) const override;
		bool refetchRecord(thread_db* tdbb) const override;
		bool lockRecord(thread_db* tdbb) const override;

		void print(thread_db* tdbb, Firebird::string& plan,
				   bool detailed, unsigned level) const override;

		void markRecursive() override;
		void invalidateRecords(jrd_req* request) const override;

		void findUsedStreams(StreamList& streams, bool expandAll = false) const override;
		void nullRecords(thread_db* tdbb) const override;

	private:
		NestConst<RecordSource> m_first;
		NestConst<RecordSource> m_second;
		NestConst<BoolExprNode> const m_boolean;
	};

} // namespace

#endif // JRD_RECORD_SOURCE_H

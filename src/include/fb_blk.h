#ifndef INCLUDE_FB_BLK
#define INCLUDE_FB_BLK

#include "../common/classes/alloc.h"
#ifdef DEBUG_GDS_ALLOC
#include <source_location>
#endif

enum BlockType
{
	type_unknown = 0,

	type_vec,
	type_dbb,
	type_bcb,
	type_bdb,
	type_pre,
	type_lck,
	type_fil,
	type_pgc,
	type_rel,
	type_fmt,
	type_vcl,
	type_req,
	type_tra,
	type_nod,
	type_csb,
	type_rec,
	type_rsb,
	type_dfw,
	type_tfb,
	type_str,
	type_blb,
	type_irb,
	type_fld,
	type_ext,
	type_att,
	type_sym,
	type_irl,
	type_sdw,
	type_blf,
	type_arr,
	type_map,
	type_prm,
	type_idb,
	type_tpc,
	type_svc,
	type_lwt,
	type_srpb,
	type_opt,
	type_prf,
	type_ctl,
	type_Events,
	type_user_data,

	type_PageSpace,
	type_PageManager,

	dsql_type_ctx,
	dsql_type_map,
	dsql_type_req,
	dsql_type_dbb,
	dsql_type_rel,
	dsql_type_fld,
	dsql_type_fil,
	dsql_type_nod,
	dsql_type_str,
	dsql_type_tra,
	dsql_type_udf,
	dsql_type_prc,
	dsql_type_intlsym,
	dsql_type_imp_join,
	dsql_type_tab_func,

	alice_type_tdr,
	alice_type_str,

	rem_type_rdb,
	rem_type_rrq,
	rem_type_rtr,
	rem_type_rbl,
	rem_type_rsr,
	rem_type_rev
};


template<BlockType BLOCK_TYPE>
class TypedHandle
{
public:
	TypedHandle() : blockType(BLOCK_TYPE) {}

	TypedHandle<BLOCK_TYPE>& operator= (const TypedHandle<BLOCK_TYPE>& from)
	{
		blockType = from.blockType;
		return *this;
	}

	BlockType getType() const
	{
		return blockType;
	}

	bool checkHandle() const
	{
		const TypedHandle* const thisPointer = this;	// avoid warning
		if (!thisPointer)
			return false;

		return (blockType == BLOCK_TYPE);
	}

private:
	BlockType blockType;
};

template<BlockType BLOCK_TYPE = type_unknown>
class pool_alloc : public TypedHandle<BLOCK_TYPE>
{
public:
	void* operator new(size_t s, MemoryPool& p ALLOC_PARAMS)
		{ return p.calloc(s ALLOC_PASS_ARGS); }
	void* operator new[](size_t s, MemoryPool& p ALLOC_PARAMS)
		{ return p.calloc(s ALLOC_PASS_ARGS); }

	void operator delete(void* mem, MemoryPool& p)
	{
		if (mem)
			p.deallocate(mem);
	}

	void operator delete[](void* mem, MemoryPool& p)
	{
		if (mem)
			p.deallocate(mem);
	}

	void operator delete(void* mem)
	{
		if (mem)
			MemoryPool::globalFree(mem);
	}

	void operator delete[](void* mem)
	{
		if (mem)
			MemoryPool::globalFree(mem);
	}

private:
	// These operators are off-limits
	void* operator new(size_t);
	void* operator new[](size_t);
};

template<typename RPT, BlockType BLOCK_TYPE = type_unknown>
class pool_alloc_rpt : public TypedHandle<BLOCK_TYPE>
{
public:
	typedef RPT blk_repeat_type;

	void* operator new(size_t s, MemoryPool& p, size_t rpt ALLOC_PARAMS)
		{ return p.calloc(s + sizeof(RPT) * rpt ALLOC_PASS_ARGS); }

	void operator delete(void* mem, MemoryPool& p)
	{
		if (mem)
			p.deallocate(mem);
	}

	void operator delete(void* mem)
	{
		if (mem)
			MemoryPool::globalFree(mem);
	}

private:
	// These operations are not supported on static repeat-base objects
	void* operator new[](size_t /*s*/, MemoryPool& /*p*/);
	void operator delete[](void* /*mem*/, MemoryPool& /*p*/);
	void operator delete[](void* /*mem*/);

	// These operators are off-limits
	void* operator new(size_t);
	void* operator new[](size_t);
};

#endif	// INCLUDE_FB_BLK

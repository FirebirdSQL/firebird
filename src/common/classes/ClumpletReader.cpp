/*
 *	PROGRAM:	Client/Server Common Code
 *	MODULE:		ClumpletReader.cpp
 *	DESCRIPTION:	Secure handling of clumplet buffers
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
 *  The Original Code was created by Nickolay Samofatov
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2004 Nickolay Samofatov <nickolay@broadviewsoftware.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */

#include "firebird.h"

#include "../common/classes/ClumpletReader.h"
#include "fb_exception.h"

#include "ibase.h"
#include "firebird/Interface.h"

#ifdef DEBUG_CLUMPLETS
#include "../yvalve/gds_proto.h"
#include <ctype.h>

namespace Firebird {

class ClumpletDump : public ClumpletReader
{
public:
	ClumpletDump(Kind k, const UCHAR* buffer, FB_SIZE_T buffLen)
		: ClumpletReader(k, buffer, buffLen)
	{ }
	static string hexString(const UCHAR* b, FB_SIZE_T len)
	{
		string t1, t2;
		for (; len > 0; --len, ++b)
		{
			if (isprint(*b))
				t2 += *b;
			else
			{
				t1.printf("<%02x>", *b);
				t2 += t1;
			}
		}
		return t2;
	}
protected:
	virtual void usage_mistake(const char* what) const
	{
		fatal_exception::raiseFmt("Internal error when using clumplet API: %s", what);
	}
	virtual void invalid_structure(const char* what, const int data) const
	{
		fatal_exception::raiseFmt("Invalid clumplet buffer structure: %s (%d)", what, data);
	}
};

void ClumpletReader::dump() const
{
	static int dmp = 0;
	gds__log("*** DUMP ***");
	if (dmp)
	{
		// Avoid infinite recursion during dump
		gds__log("recursion");
		return;
	}
	dmp++;

	try {
		ClumpletDump d(kind, getBuffer(), getBufferLength());
		int t = (kind == SpbStart || kind == UnTagged || kind == WideUnTagged || kind == SpbResponse) ? -1 : d.getBufferTag();
		gds__log("Tag=%d Offset=%d Length=%d Eof=%d\n", t, getCurOffset(), getBufferLength(), isEof());
		for (d.rewind(); !(d.isEof()); d.moveNext())
		{
			gds__log("Clump %d at offset %d: %s", d.getClumpTag(), d.getCurOffset(),
				ClumpletDump::hexString(d.getBytes(), d.getClumpLength()).c_str());
		}
	}
	catch (const fatal_exception& x)
	{
		gds__log("Fatal exception during clumplet dump: %s", x.what());
		FB_SIZE_T l = getBufferLength() - getCurOffset();
		const UCHAR *p = getBuffer() + getCurOffset();
		gds__log("Plain dump starting with offset %d: %s", getCurOffset(),
			ClumpletDump::hexString(p, l).c_str());
	}
	dmp--;
}

}
#endif //DEBUG_CLUMPLETS

namespace Firebird {

ClumpletReader::ClumpletReader(Kind k, const UCHAR* buffer, FB_SIZE_T buffLen) :
	kind(k), static_buffer(buffer), static_buffer_end(buffer + buffLen)
{
	rewind();	// this will set cur_offset and spbState
}

ClumpletReader::ClumpletReader(MemoryPool& pool, Kind k, const UCHAR* buffer, FB_SIZE_T buffLen) :
	AutoStorage(pool), kind(k), static_buffer(buffer), static_buffer_end(buffer + buffLen)
{
	rewind();	// this will set cur_offset and spbState
}

ClumpletReader::ClumpletReader(MemoryPool& pool, const ClumpletReader& from) :
	AutoStorage(pool), kind(from.kind),
	static_buffer(from.getBuffer()), static_buffer_end(from.getBufferEnd())
{
	rewind();	// this will set cur_offset and spbState
}

ClumpletReader::ClumpletReader(const ClumpletReader& from) :
	AutoStorage(), kind(from.kind),
	static_buffer(from.getBuffer()), static_buffer_end(from.getBufferEnd())
{
	rewind();	// this will set cur_offset and spbState
}

ClumpletReader::ClumpletReader(MemoryPool& pool, const KindList* kl,
							   const UCHAR* buffer, FB_SIZE_T buffLen, FPTR_VOID raise) :
	AutoStorage(pool), kind(kl->kind), static_buffer(buffer), static_buffer_end(buffer + buffLen)
{
	create(kl, buffLen, raise);
}

ClumpletReader::ClumpletReader(const KindList* kl, const UCHAR* buffer, FB_SIZE_T buffLen, FPTR_VOID raise) :
	kind(kl->kind), static_buffer(buffer), static_buffer_end(buffer + buffLen)
{
	create(kl, buffLen, raise);
}

void ClumpletReader::create(const KindList* kl, FB_SIZE_T buffLen, FPTR_VOID raise)
{
	cur_offset = 0;

	if (buffLen)
	{
		while (kl->kind != EndOfList)
		{
			kind = kl->kind;
			if (getBufferTag() == kl->tag)
			{
				break;
			}
			++kl;
		}

		if (kl->kind == EndOfList)
		{
			if (raise)
			{
				raise();
			}
			invalid_structure("Unknown tag value - missing in the list of possible");
		}
	}

	rewind();	// this will set cur_offset and spbState
}

const UCHAR* ClumpletReader::getBuffer() const
{
	return static_buffer;
}

const UCHAR* ClumpletReader::getBufferEnd() const
{
	return static_buffer_end;
}

void ClumpletReader::usage_mistake(const char* what) const
{
#ifdef DEBUG_CLUMPLETS
	dump();
#endif
	fatal_exception::raiseFmt("Internal error when using clumplet API: %s", what);
}

void ClumpletReader::invalid_structure(const char* what, const int data) const
{
#ifdef DEBUG_CLUMPLETS
	dump();
#endif
	fatal_exception::raiseFmt("Invalid clumplet buffer structure: %s (%d)", what, data);
}

bool ClumpletReader::isTagged() const
{
	switch (kind)
	{
	case Tpb:
	case Tagged:
	case WideTagged:
	case SpbAttach:
		return true;
	}

	return false;
}

UCHAR ClumpletReader::getBufferTag() const
{
	const UCHAR* const buffer_end = getBufferEnd();
	const UCHAR* buffer_start = getBuffer();

	switch (kind)
	{
	case Tpb:
	case Tagged:
	case WideTagged:
		if (buffer_end - buffer_start == 0)
		{
			invalid_structure("empty buffer");
			return 0;
		}
		return buffer_start[0];
	case SpbStart:
	case UnTagged:
	case WideUnTagged:
	case SpbSendItems:
	case SpbReceiveItems:
	case SpbResponse:
		usage_mistake("buffer is not tagged");
		return 0;
	case SpbAttach:
		if (buffer_end - buffer_start == 0)
		{
			invalid_structure("empty buffer");
			return 0;
		}
		switch (buffer_start[0])
		{
		case isc_spb_version1:
			// This is old SPB format, it's almost like DPB -
			// buffer's tag is the first byte.
			return buffer_start[0];
		case isc_spb_version:
			// Buffer's tag is the second byte
			if (buffer_end - buffer_start == 1)
			{
				invalid_structure("buffer too short", 1);
				return 0;
			}
			return buffer_start[1];
		case isc_spb_version3:
			// This is wide SPB attach format - buffer's tag is the first byte.
			return buffer_start[0];
		default:
			invalid_structure("spb in service attach should begin with isc_spb_version1 or isc_spb_version", buffer_start[0]);
			return 0;
		}
	default:
		fb_assert(false);
		return 0;
	}
}

ClumpletReader::ClumpletType ClumpletReader::getClumpletType(UCHAR tag) const
{
	switch (kind)
	{
	case Tagged:
	case UnTagged:
	case SpbAttach:
		return TraditionalDpb;
	case WideTagged:
	case WideUnTagged:
		return Wide;
	case Tpb:
		switch (tag)
		{
        case isc_tpb_lock_write:
        case isc_tpb_lock_read:
		case isc_tpb_lock_timeout:
		case isc_tpb_at_snapshot_number:
			return TraditionalDpb;
		}
		return SingleTpb;
	case SpbSendItems:
		switch (tag)
		{
		case isc_info_svc_auth_block:
			return Wide;
		case isc_info_end:
		case isc_info_truncated:
		case isc_info_error:
		case isc_info_data_not_ready:
		case isc_info_length:
		case isc_info_flag_end:
			return SingleTpb;
		}
		return StringSpb;
	case SpbReceiveItems:
		return SingleTpb;
	case SpbStart:
		switch(tag)
		{
		case isc_spb_auth_block:
		case isc_spb_trusted_auth:
		case isc_spb_auth_plugin_name:
		case isc_spb_auth_plugin_list:
			return Wide;
		}
		switch (spbState)
		{
		case 0:
			return SingleTpb;
		case isc_action_svc_backup:
		case isc_action_svc_restore:
			switch (tag)
			{
			case isc_spb_bkp_file:
			case isc_spb_dbname:
			case isc_spb_res_fix_fss_data:
			case isc_spb_res_fix_fss_metadata:
			case isc_spb_bkp_stat:
			case isc_spb_bkp_skip_data:
			case isc_spb_bkp_include_data:
			case isc_spb_bkp_keyholder:
			case isc_spb_bkp_keyname:
			case isc_spb_bkp_crypt:
				return StringSpb;
			case isc_spb_bkp_factor:
			case isc_spb_bkp_length:
			case isc_spb_res_length:
			case isc_spb_res_buffers:
			case isc_spb_res_page_size:
			case isc_spb_options:
			case isc_spb_verbint:
				return IntSpb;
			case isc_spb_verbose:
				return SingleTpb;
			case isc_spb_res_access_mode:
				return ByteSpb;
			}
			invalid_structure("unknown parameter for backup/restore", tag);
			break;
		case isc_action_svc_repair:
			switch (tag)
			{
			case isc_spb_dbname:
				return StringSpb;
			case isc_spb_options:
			case isc_spb_rpr_commit_trans:
			case isc_spb_rpr_rollback_trans:
			case isc_spb_rpr_recover_two_phase:
				return IntSpb;
			case isc_spb_rpr_commit_trans_64:
			case isc_spb_rpr_rollback_trans_64:
			case isc_spb_rpr_recover_two_phase_64:
				return BigIntSpb;
			}
			invalid_structure("unknown parameter for repair", tag);
			break;
		case isc_action_svc_add_user:
		case isc_action_svc_delete_user:
		case isc_action_svc_modify_user:
		case isc_action_svc_display_user:
		case isc_action_svc_display_user_adm:
		case isc_action_svc_set_mapping:
		case isc_action_svc_drop_mapping:
			switch (tag)
			{
			case isc_spb_dbname:
			case isc_spb_sql_role_name:
			case isc_spb_sec_username:
			case isc_spb_sec_password:
			case isc_spb_sec_groupname:
			case isc_spb_sec_firstname:
			case isc_spb_sec_middlename:
			case isc_spb_sec_lastname:
				return StringSpb;
			case isc_spb_sec_userid:
			case isc_spb_sec_groupid:
			case isc_spb_sec_admin:
				return IntSpb;
			}
			invalid_structure("unknown parameter for security database operation", tag);
			break;
		case isc_action_svc_properties:
			switch (tag)
			{
			case isc_spb_dbname:
				return StringSpb;
			case isc_spb_prp_page_buffers:
			case isc_spb_prp_sweep_interval:
			case isc_spb_prp_shutdown_db:
			case isc_spb_prp_deny_new_attachments:
			case isc_spb_prp_deny_new_transactions:
			case isc_spb_prp_set_sql_dialect:
			case isc_spb_options:
			case isc_spb_prp_force_shutdown:
			case isc_spb_prp_attachments_shutdown:
			case isc_spb_prp_transactions_shutdown:
				return IntSpb;
			case isc_spb_prp_reserve_space:
			case isc_spb_prp_write_mode:
			case isc_spb_prp_access_mode:
			case isc_spb_prp_shutdown_mode:
			case isc_spb_prp_online_mode:
				return ByteSpb;
			}
			invalid_structure("unknown parameter for setting database properties", tag);
			break;
//		case isc_action_svc_add_license:
//		case isc_action_svc_remove_license:
		case isc_action_svc_db_stats:
			switch (tag)
			{
			case isc_spb_dbname:
			case isc_spb_command_line:
			case isc_spb_sts_table:
				return StringSpb;
			case isc_spb_options:
				return IntSpb;
			}
			invalid_structure("unknown parameter for getting statistics", tag);
			break;
		case isc_action_svc_get_ib_log:
			invalid_structure("unknown parameter for getting log", tag);
			break;
		case isc_action_svc_nbak:
		case isc_action_svc_nrest:
			switch (tag)
			{
			case isc_spb_nbk_file:
			case isc_spb_nbk_direct:
			case isc_spb_dbname:
			case isc_spb_nbk_guid:
				return StringSpb;
			case isc_spb_nbk_level:
			case isc_spb_options:
				return IntSpb;
			}
			invalid_structure("unknown parameter for nbackup", tag);
			break;
		case isc_action_svc_trace_start:
		case isc_action_svc_trace_stop:
		case isc_action_svc_trace_suspend:
		case isc_action_svc_trace_resume:
			switch (tag)
			{
			case isc_spb_trc_cfg:
			case isc_spb_trc_name:
				return StringSpb;
			case isc_spb_trc_id:
				return IntSpb;
			}
			break;
		case isc_action_svc_validate:
			switch (tag)
			{
			case isc_spb_val_tab_incl:
			case isc_spb_val_tab_excl:
			case isc_spb_val_idx_incl:
			case isc_spb_val_idx_excl:
			case isc_spb_dbname:
				return StringSpb;
			case isc_spb_val_lock_timeout:
				return IntSpb;
			}
			break;
		}
		invalid_structure("wrong spb state", spbState);
		break;
	case SpbResponse:
		switch(tag)
		{
		case isc_info_svc_version:
		case isc_spb_num_att:
		case isc_spb_num_db:
		case isc_spb_multi_tra_id:
		case isc_spb_single_tra_id:
		case isc_spb_tra_id:
		case isc_info_svc_stdin:
		case isc_info_svc_capabilities:
			return IntSpb;
		case isc_spb_multi_tra_id_64:
		case isc_spb_single_tra_id_64:
		case isc_spb_tra_id_64:
			return BigIntSpb;
		case isc_info_svc_server_version:
		case isc_info_svc_implementation:
		case isc_info_svc_get_env_msg:
		case isc_info_svc_get_env:
		case isc_info_svc_get_env_lock:
		case isc_info_svc_user_dbpath:
		case isc_spb_dbname:
		case isc_spb_tra_host_site:
		case isc_spb_tra_remote_site:
		case isc_spb_tra_db_path:
		case isc_info_svc_get_users:
		case isc_info_svc_line:
		case isc_info_svc_to_eof:
			return StringSpb;
		case isc_info_end:
		case isc_info_svc_svr_db_info:
		case isc_info_svc_limbo_trans:
		case isc_info_flag_end:
		case isc_info_truncated:
		case isc_info_svc_timeout:
		case isc_info_data_not_ready:
			return SingleTpb;
		case isc_spb_tra_state:
		case isc_spb_tra_advise:
			return ByteSpb;
		}
		invalid_structure("unrecognized service response tag", tag);
		break;
	}
	invalid_structure("unknown clumplet kind", kind);
	return SingleTpb;
}

void ClumpletReader::adjustSpbState()
{
	switch (kind)
	{
	case SpbStart:
		if (spbState == 0 &&							// Just started with service start block ...
			getClumpletSize(true, true, true) == 1)		// and this is action_XXX clumplet
		{
			spbState = getClumpTag();
		}
		break;
	default:
		break;
	}
}

FB_SIZE_T ClumpletReader::getClumpletSize(bool wTag, bool wLength, bool wData) const
{
	const UCHAR* clumplet = getBuffer() + cur_offset;
	const UCHAR* const buffer_end = getBufferEnd();

	// Check for EOF
	if (clumplet >= buffer_end)
	{
		usage_mistake("read past EOF");
		return 0;
	}

	FB_SIZE_T rc = wTag ? 1 : 0;
	FB_SIZE_T lengthSize = 0;
	FB_SIZE_T dataSize = 0;

	ClumpletType t = getClumpletType(clumplet[0]);
	switch (t)
	{

	// This form allows clumplets of virtually any size
	case Wide:
		// Check did we receive length component for clumplet
		if (buffer_end - clumplet < 5)
		{
			invalid_structure("buffer end before end of clumplet - no length component", buffer_end - clumplet);
			return rc;
		}
		lengthSize = 4;
		dataSize = clumplet[4];
		dataSize <<= 8;
		dataSize += clumplet[3];
		dataSize <<= 8;
		dataSize += clumplet[2];
		dataSize <<= 8;
		dataSize += clumplet[1];
		break;

	// This is the most widely used form
	case TraditionalDpb:
		// Check did we receive length component for clumplet
		if (buffer_end - clumplet < 2)
		{
			invalid_structure("buffer end before end of clumplet - no length component", buffer_end - clumplet);
			return rc;
		}
		lengthSize = 1;
		dataSize = clumplet[1];
		break;

	// Almost all TPB parameters are single bytes
	case SingleTpb:
		break;

	// Used in SPB for long strings
	case StringSpb:
		// Check did we receive length component for clumplet
		if (buffer_end - clumplet < 3)
		{
			invalid_structure("buffer end before end of clumplet - no length component", buffer_end - clumplet);
			return rc;
		}
		lengthSize = 2;
		dataSize = clumplet[2];
		dataSize <<= 8;
		dataSize += clumplet[1];
		break;

	// Used in SPB for 4-byte integers
	case IntSpb:
		dataSize = 4;
		break;

	// Used in SPB for 8-byte integers
	case BigIntSpb:
		dataSize = 8;
		break;

	// Used in SPB for single byte
	case ByteSpb:
		dataSize = 1;
		break;

	default:
		invalid_structure("unknown clumplet type", t);
	}

	const FB_SIZE_T total = 1 + lengthSize + dataSize;
	if (clumplet + total > buffer_end)
	{
		invalid_structure("buffer end before end of clumplet - clumplet too long", total);
		FB_SIZE_T delta = total - (buffer_end - clumplet);
		if (delta > dataSize)
			dataSize = 0;
		else
			dataSize -= delta;
	}

	if (wLength) {
		rc += lengthSize;
	}
	if (wData) {
		rc += dataSize;
	}
	return rc;
}

void ClumpletReader::moveNext()
{
	if (isEof())
		return;		// no need to raise useless exceptions
	FB_SIZE_T cs = getClumpletSize(true, true, true);
	adjustSpbState();
	cur_offset += cs;
}

void ClumpletReader::rewind()
{
	if (! getBuffer())
	{
		cur_offset = 0;
		spbState = 0;
		return;
	}
	switch (kind)
	{
	case UnTagged:
	case WideUnTagged:
	case SpbStart:
	case SpbSendItems:
	case SpbReceiveItems:
	case SpbResponse:
		cur_offset = 0;
		break;
	default:
		if (kind == SpbAttach && getBufferLength() > 0 && getBuffer()[0] != isc_spb_version1)
			cur_offset = 2;
		else
			cur_offset = 1;
	}
	spbState = 0;
}

bool ClumpletReader::find(UCHAR tag)
{
	const FB_SIZE_T co = getCurOffset();
	for (rewind(); !isEof(); moveNext())
	{
		if (tag == getClumpTag())
		{
			return true;
		}
	}
	setCurOffset(co);
	return false;
}

bool ClumpletReader::next(UCHAR tag)
{
	if (!isEof())
	{
		const FB_SIZE_T co = getCurOffset();
		if (tag == getClumpTag())
		{
			moveNext();
		}
		for (; !isEof(); moveNext())
		{
			if (tag == getClumpTag())
			{
				return true;
			}
		}
		setCurOffset(co);
	}
	return false;
}

// Methods which work with currently selected clumplet
UCHAR ClumpletReader::getClumpTag() const
{
	const UCHAR* clumplet = getBuffer() + cur_offset;
	const UCHAR* const buffer_end = getBufferEnd();

	// Check for EOF
	if (clumplet >= buffer_end)
	{
		usage_mistake("read past EOF");
		return 0;
	}

	return clumplet[0];
}

FB_SIZE_T ClumpletReader::getClumpLength() const
{
	return getClumpletSize(false, false, true);
}

const UCHAR* ClumpletReader::getBytes() const
{
	return getBuffer() + cur_offset + getClumpletSize(true, true, false);
}

SINT64 ClumpletReader::fromVaxInteger(const UCHAR* ptr, FB_SIZE_T length)
{
	// We can't handle numbers bigger than int64. Some cases use length == 0.
	fb_assert(ptr && length >= 0 && length < 9);

	// This code is taken from isc_portable_integer()
	// Be ready to parse buffer made by hand instead of ClumpletWriter
	if (!ptr || length <= 0 || length > 8)
		return 0;

	SINT64 value = 0;
	int shift = 0;

	while (--length > 0)
	{
		value += ((SINT64) *ptr++) << shift;
		shift += 8;
	}

	value += ((SINT64)(SCHAR) *ptr) << shift;

	return value;
}

SLONG ClumpletReader::getInt() const
{
	const FB_SIZE_T length = getClumpLength();

	if (length > 4)
	{
		invalid_structure("length of integer exceeds 4 bytes", length);
		return 0;
	}

	return fromVaxInteger(getBytes(), length);
}

double ClumpletReader::getDouble() const
{
	const FB_SIZE_T length = getClumpLength();
	if (length != sizeof(double))
	{
		invalid_structure("length of double must be equal 8 bytes", length);
		return 0;
	}

	// based on XDR code
	union {
		double temp_double;
		SLONG temp_long[2];
	} temp;

	fb_assert(sizeof(double) == sizeof(temp));

	const UCHAR* ptr = getBytes();
	temp.temp_long[FB_LONG_DOUBLE_FIRST] = fromVaxInteger(ptr, sizeof(SLONG));
	temp.temp_long[FB_LONG_DOUBLE_SECOND] = fromVaxInteger(ptr + sizeof(SLONG), sizeof(SLONG));

	return temp.temp_double;
}

ISC_TIMESTAMP ClumpletReader::getTimeStamp() const
{
	ISC_TIMESTAMP value;

	const FB_SIZE_T length = getClumpLength();
	if (length != sizeof(ISC_TIMESTAMP))
	{
		invalid_structure("length of ISC_TIMESTAMP must be equal 8 bytes", length);
		value.timestamp_date = 0;
		value.timestamp_time = 0;
		return value;
	}

	const UCHAR* ptr = getBytes();
	value.timestamp_date = fromVaxInteger(ptr, sizeof(SLONG));
	value.timestamp_time = fromVaxInteger(ptr + sizeof(SLONG), sizeof(SLONG));
	return value;
}

SINT64 ClumpletReader::getBigInt() const
{
	const FB_SIZE_T length = getClumpLength();

	if (length > 8)
	{
		invalid_structure("length of BigInt exceeds 8 bytes", length);
		return 0;
	}

	return fromVaxInteger(getBytes(), length);
}

string& ClumpletReader::getString(string& str) const
{
	const UCHAR* ptr = getBytes();
	const FB_SIZE_T length = getClumpLength();
	str.assign(reinterpret_cast<const char*>(ptr), length);
	str.recalculate_length();
	if (str.length() + 1 < length)
	{
		invalid_structure("string length doesn't match with clumplet", str.length() + 1);
	}
	return str;
}

PathName& ClumpletReader::getPath(PathName& str) const
{
	const UCHAR* ptr = getBytes();
	const FB_SIZE_T length = getClumpLength();
	str.assign(reinterpret_cast<const char*>(ptr), length);
	str.recalculate_length();
	if (str.length() + 1 < length)
	{
		invalid_structure("path length doesn't match with clumplet", str.length() + 1);
	}
	return str;
}

void ClumpletReader::getData(UCharBuffer& data) const
{
	data.assign(getBytes(), getClumpLength());
}

bool ClumpletReader::getBoolean() const
{
	const UCHAR* ptr = getBytes();
	const FB_SIZE_T length = getClumpLength();
	if (length > 1)
	{
		invalid_structure("length of boolean exceeds 1 byte", length);
		return false;
	}
	return length && ptr[0];
}

ClumpletReader::SingleClumplet ClumpletReader::getClumplet() const
{
	SingleClumplet rc;
	rc.tag = getClumpTag();
	rc.size = getClumpletSize(false, false, true);
	rc.data = getBytes();
	return rc;
}

const ClumpletReader::KindList ClumpletReader::dpbList[] = {
	{ClumpletReader::Tagged, isc_dpb_version1},
	{ClumpletReader::WideTagged, isc_dpb_version2},
	{ClumpletReader::EndOfList, 0}
};

const ClumpletReader::KindList ClumpletReader::spbList[] = {
	{ClumpletReader::SpbAttach, isc_spb_current_version},
	{ClumpletReader::SpbAttach, isc_spb_version1},
	{ClumpletReader::WideTagged, isc_spb_version3},
	{ClumpletReader::EndOfList, 0}
};

AuthReader::AuthReader(const AuthBlock& authBlock)
	: ClumpletReader(ClumpletReader::WideUnTagged, authBlock.begin(), authBlock.getCount())
{
	rewind();
}

AuthReader::AuthReader(MemoryPool& pool, const AuthBlock& authBlock)
	: ClumpletReader(pool, ClumpletReader::WideUnTagged, authBlock.begin(), authBlock.getCount())
{
	rewind();
}

static inline void erase(NoCaseString& s)
{
	s.erase();
}

static inline void set(NoCaseString& s, const ClumpletReader& rdr)
{
	s.assign(rdr.getBytes(), rdr.getClumpLength());
}

bool AuthReader::getInfo(Info& info)
{
	if (isEof())
	{
		return false;
	}

	erase(info.type);
	erase(info.name);
	erase(info.plugin);
	erase(info.secDb);
	erase(info.origPlug);

	ClumpletReader internal(WideUnTagged, getBytes(), getClumpLength());
	for (internal.rewind(); !internal.isEof(); internal.moveNext())
	{
		switch(internal.getClumpTag())
		{
		case AUTH_TYPE:
			set(info.type, internal);
			break;
		case AUTH_NAME:
			set(info.name, internal);
			break;
		case AUTH_PLUGIN:
			set(info.plugin, internal);
			break;
		case AUTH_SECURE_DB:
			set(info.secDb, internal);
			break;
		case AUTH_ORIG_PLUG:
			set(info.origPlug, internal);
			break;
		default:
			break;
		}
	}

	return true;
}

#ifdef AUTH_BLOCK_DEBUG
void dumpAuthBlock(const char* text, ClumpletReader* pb, unsigned char param)
{
	fprintf(stderr, "AuthBlock in %s:", text);
	if (pb->find(param))
	{
		Firebird::AuthReader::AuthBlock tmp;
		tmp.assign(pb->getBytes(), pb->getClumpLength());
		Firebird::AuthReader rdr(tmp);
		string name, plugin;
		PathName secureDb;
		bool x = false;
		while (rdr.getInfo(&name, &plugin, &secureDb))
		{
			fprintf(stderr, " %s::%s::%s", name.c_str(), plugin.c_str(), secureDb.c_str());
			x = true;
			rdr.moveNext();
		}
		fprintf(stderr, "%s\n", x ? "" : " <empty>");
	}
	else
	{
		fprintf(stderr, " <missing>\n");
	}
}
#endif

} // namespace

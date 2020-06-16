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
 *  The Original Code was created by Vlad Khorsun
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2007 Vlad Khorsun <hvlad@users.sourceforge.net>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef EXTDS_ISC_H
#define EXTDS_ISC_H


#include "ExtDS.h"
#include "fb_api_proto.h"
#include "../jrd/status.h"


namespace EDS {

class IscProvider : public Provider
{
public:
	friend class EngineCallbackGuard;

	explicit IscProvider(const char* prvName) :
		Provider(prvName),
		m_api_loaded(false)
	{
		memset(&m_api, 0, sizeof(m_api));
	}

	virtual void initialize()
	{
		if (!m_api_loaded)
			loadAPI();
	}

	virtual void getRemoteError(const Jrd::FbStatusVector* status, Firebird::string& err) const;

protected:
	ISC_STATUS notImplemented(Jrd::FbStatusVector*) const;
	virtual void loadAPI();
	virtual Connection* doCreateConnection();

	FirebirdApiPointers m_api;
	bool m_api_loaded;

public:
	virtual ISC_STATUS ISC_EXPORT isc_attach_database(Jrd::FbStatusVector*,
										  short,
										  const char*,
										  isc_db_handle*,
										  short,
										  const char*);

	virtual ISC_STATUS ISC_EXPORT isc_array_gen_sdl(Jrd::FbStatusVector*,
										const ISC_ARRAY_DESC*,
										short*,
										char*,
										short*);

	virtual ISC_STATUS ISC_EXPORT isc_array_get_slice(Jrd::FbStatusVector*,
										  isc_db_handle*,
										  isc_tr_handle*,
										  ISC_QUAD*,
										  const ISC_ARRAY_DESC*,
										  void*,
										  ISC_LONG*);

	virtual ISC_STATUS ISC_EXPORT isc_array_lookup_bounds(Jrd::FbStatusVector*,
											  isc_db_handle*,
											  isc_tr_handle*,
											  const char*,
											  const char*,
											  ISC_ARRAY_DESC*);

	virtual ISC_STATUS ISC_EXPORT isc_array_lookup_desc(Jrd::FbStatusVector*,
											isc_db_handle*,
											isc_tr_handle*,
											const char*,
											const char*,
											ISC_ARRAY_DESC*);

	virtual ISC_STATUS ISC_EXPORT isc_array_set_desc(Jrd::FbStatusVector*,
										 const char*,
										 const char*,
										 const short*,
										 const short*,
										 const short*,
										 ISC_ARRAY_DESC*);

	virtual ISC_STATUS ISC_EXPORT isc_array_put_slice(Jrd::FbStatusVector*,
										  isc_db_handle*,
										  isc_tr_handle*,
										  ISC_QUAD*,
										  const ISC_ARRAY_DESC*,
										  void*,
										  ISC_LONG*);

	virtual void ISC_EXPORT isc_blob_default_desc(ISC_BLOB_DESC*,
									  const unsigned char*,
									  const unsigned char*);

	virtual ISC_STATUS ISC_EXPORT isc_blob_gen_bpb(Jrd::FbStatusVector*,
									   const ISC_BLOB_DESC*,
									   const ISC_BLOB_DESC*,
									   unsigned short,
									   unsigned char*,
									   unsigned short*);

	virtual ISC_STATUS ISC_EXPORT isc_blob_info(Jrd::FbStatusVector*,
									isc_blob_handle*,
									short,
									const char*,
									short,
									char*);

	virtual ISC_STATUS ISC_EXPORT isc_blob_lookup_desc(Jrd::FbStatusVector*,
										   isc_db_handle*,
										   isc_tr_handle*,
										   const unsigned char*,
										   const unsigned char*,
										   ISC_BLOB_DESC*,
										   unsigned char*);

	virtual ISC_STATUS ISC_EXPORT isc_blob_set_desc(Jrd::FbStatusVector*,
										const unsigned char*,
										const unsigned char*,
										short,
										short,
										short,
										ISC_BLOB_DESC*);

	virtual ISC_STATUS ISC_EXPORT isc_cancel_blob(Jrd::FbStatusVector*,
									  isc_blob_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_cancel_events(Jrd::FbStatusVector*,
										isc_db_handle*,
										ISC_LONG*);

	virtual ISC_STATUS ISC_EXPORT isc_close_blob(Jrd::FbStatusVector*,
									 isc_blob_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_commit_retaining(Jrd::FbStatusVector*,
										   isc_tr_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_commit_transaction(Jrd::FbStatusVector*,
											 isc_tr_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_create_blob(Jrd::FbStatusVector*,
									  isc_db_handle*,
									  isc_tr_handle*,
									  isc_blob_handle*,
									  ISC_QUAD*);

	virtual ISC_STATUS ISC_EXPORT isc_create_blob2(Jrd::FbStatusVector*,
									   isc_db_handle*,
									   isc_tr_handle*,
									   isc_blob_handle*,
									   ISC_QUAD*,
									   short,
									   const char*);

	virtual ISC_STATUS ISC_EXPORT isc_create_database(Jrd::FbStatusVector*,
										  short,
										  const char*,
										  isc_db_handle*,
										  short,
										  const char*,
										  short);

	virtual ISC_STATUS ISC_EXPORT isc_database_info(Jrd::FbStatusVector*,
										isc_db_handle*,
										short,
										const unsigned char*,
										short,
										char*);

	virtual void ISC_EXPORT isc_decode_date(const ISC_QUAD*,
								void*);

	virtual void ISC_EXPORT isc_decode_sql_date(const ISC_DATE*,
									void*);

	virtual void ISC_EXPORT isc_decode_sql_time(const ISC_TIME*,
									void*);

	virtual void ISC_EXPORT isc_decode_timestamp(const ISC_TIMESTAMP*,
									 void*);

	virtual ISC_STATUS ISC_EXPORT isc_detach_database(Jrd::FbStatusVector*,
										  isc_db_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_drop_database(Jrd::FbStatusVector*,
										isc_db_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_allocate_statement(Jrd::FbStatusVector*,
												  isc_db_handle*,
												  isc_stmt_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_alloc_statement2(Jrd::FbStatusVector*,
												isc_db_handle*,
												isc_stmt_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_describe(Jrd::FbStatusVector*,
										isc_stmt_handle*,
										unsigned short,
										XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_describe_bind(Jrd::FbStatusVector*,
											 isc_stmt_handle*,
											 unsigned short,
											 XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_exec_immed2(Jrd::FbStatusVector*,
										   isc_db_handle*,
										   isc_tr_handle*,
										   unsigned short,
										   const char*,
										   unsigned short,
										   const XSQLDA*,
										   const XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_execute(Jrd::FbStatusVector*,
									   isc_tr_handle*,
									   isc_stmt_handle*,
									   unsigned short,
									   const XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_execute2(Jrd::FbStatusVector*,
										isc_tr_handle*,
										isc_stmt_handle*,
										unsigned short,
										const XSQLDA*,
										const XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_execute_immediate(Jrd::FbStatusVector*,
												 isc_db_handle*,
												 isc_tr_handle*,
												 unsigned short,
												 const char*,
												 unsigned short,
												 const XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_fetch(Jrd::FbStatusVector*,
									 isc_stmt_handle*,
									 unsigned short,
									 const XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_finish(isc_db_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_free_statement(Jrd::FbStatusVector*,
											  isc_stmt_handle*,
											  unsigned short);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_insert(Jrd::FbStatusVector*,
									  isc_stmt_handle*,
									  unsigned short,
									  XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_prepare(Jrd::FbStatusVector*,
									   isc_tr_handle*,
									   isc_stmt_handle*,
									   unsigned short,
									   const char*,
									   unsigned short,
									   XSQLDA*);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_set_cursor_name(Jrd::FbStatusVector*,
											   isc_stmt_handle*,
											   const char*,
											   unsigned short);

	virtual ISC_STATUS ISC_EXPORT isc_dsql_sql_info(Jrd::FbStatusVector*,
										isc_stmt_handle*,
										short,
										const char*,
										short,
										char*);

	virtual void ISC_EXPORT isc_encode_date(const void*,
								ISC_QUAD*);

	virtual void ISC_EXPORT isc_encode_sql_date(const void*,
									ISC_DATE*);

	virtual void ISC_EXPORT isc_encode_sql_time(const void*,
									ISC_TIME*);

	virtual void ISC_EXPORT isc_encode_timestamp(const void*,
									 ISC_TIMESTAMP*);

	virtual ISC_LONG ISC_EXPORT_VARARG isc_event_block(char**,
										   char**,
										   unsigned short, ...);

	virtual void ISC_EXPORT isc_event_counts(ISC_ULONG*,
								 short,
								 char*,
								 const char*);

	// 17 May 2001 - isc_expand_dpb is DEPRECATED
	virtual void ISC_EXPORT_VARARG isc_expand_dpb(char**,
									  short*, ...);

	virtual int ISC_EXPORT isc_modify_dpb(char**,
							  short*,
							  unsigned short,
							  const char*,
							  short);

	virtual ISC_LONG ISC_EXPORT isc_free(char*);

	virtual ISC_STATUS ISC_EXPORT isc_get_segment(Jrd::FbStatusVector*,
									  isc_blob_handle*,
									  unsigned short*,
									  unsigned short,
									  char*);

	virtual ISC_STATUS ISC_EXPORT isc_get_slice(Jrd::FbStatusVector*,
									isc_db_handle*,
									isc_tr_handle*,
									ISC_QUAD*,
									short,
									const char*,
									short,
									const ISC_LONG*,
									ISC_LONG,
									void*,
									ISC_LONG*);

	virtual ISC_STATUS ISC_EXPORT isc_interprete(char*,
									 const Jrd::FbStatusVector**);

	virtual ISC_STATUS ISC_EXPORT isc_open_blob(Jrd::FbStatusVector*,
									isc_db_handle*,
									isc_tr_handle*,
									isc_blob_handle*,
									ISC_QUAD*);

	virtual ISC_STATUS ISC_EXPORT isc_open_blob2(Jrd::FbStatusVector*,
									 isc_db_handle*,
									 isc_tr_handle*,
									 isc_blob_handle*,
									 ISC_QUAD*,
									 ISC_USHORT,
									 const ISC_UCHAR*);

	virtual ISC_STATUS ISC_EXPORT isc_prepare_transaction2(Jrd::FbStatusVector*,
											   isc_tr_handle*,
											   ISC_USHORT,
											   const ISC_UCHAR*);

	virtual void ISC_EXPORT isc_print_sqlerror(ISC_SHORT,
								   const Jrd::FbStatusVector*);

	virtual ISC_STATUS ISC_EXPORT isc_print_status(const Jrd::FbStatusVector*);

	virtual ISC_STATUS ISC_EXPORT isc_put_segment(Jrd::FbStatusVector*,
									  isc_blob_handle*,
									  unsigned short,
									  const char*);

	virtual ISC_STATUS ISC_EXPORT isc_put_slice(Jrd::FbStatusVector*,
									isc_db_handle*,
									isc_tr_handle*,
									ISC_QUAD*,
									short,
									const char*,
									short,
									const ISC_LONG*,
									ISC_LONG,
									void*);

	virtual ISC_STATUS ISC_EXPORT isc_que_events(Jrd::FbStatusVector*,
									 isc_db_handle*,
									 ISC_LONG*,
									 ISC_USHORT,
									 const ISC_UCHAR*,
									 isc_callback,
									 void*);

	virtual ISC_STATUS ISC_EXPORT isc_rollback_retaining(Jrd::FbStatusVector*,
											 isc_tr_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_rollback_transaction(Jrd::FbStatusVector*,
											   isc_tr_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_start_multiple(Jrd::FbStatusVector*,
										 isc_tr_handle*,
										 short,
										 void*);

	virtual ISC_STATUS ISC_EXPORT_VARARG isc_start_transaction(Jrd::FbStatusVector*,
												   isc_tr_handle*,
												   short, ...);

	virtual ISC_STATUS ISC_EXPORT_VARARG isc_reconnect_transaction(Jrd::FbStatusVector*,
                                                   isc_db_handle*,
                                                   isc_tr_handle*,
                                                   short,
                                                   const char*);

	virtual ISC_LONG ISC_EXPORT isc_sqlcode(const Jrd::FbStatusVector*);

	virtual void ISC_EXPORT isc_sql_interprete(short,
								   char*,
								   short);

	virtual ISC_STATUS ISC_EXPORT isc_transaction_info(Jrd::FbStatusVector*,
										   isc_tr_handle*,
										   short,
										   const char*,
										   short,
										   char*);

	virtual ISC_STATUS ISC_EXPORT isc_transact_request(Jrd::FbStatusVector*,
										   isc_db_handle*,
										   isc_tr_handle*,
										   unsigned short,
										   char*,
										   unsigned short,
										   char*,
										   unsigned short,
										   char*);

	virtual ISC_LONG ISC_EXPORT isc_vax_integer(const char*,
									short);

	virtual ISC_INT64 ISC_EXPORT isc_portable_integer(const unsigned char*,
										  short);

	virtual ISC_STATUS ISC_EXPORT isc_seek_blob(Jrd::FbStatusVector*,
									isc_blob_handle*,
									short,
									ISC_LONG,
									ISC_LONG*);



	virtual ISC_STATUS ISC_EXPORT isc_service_attach(Jrd::FbStatusVector*,
										 unsigned short,
										 const char*,
										 isc_svc_handle*,
										 unsigned short,
										 const char*);

	virtual ISC_STATUS ISC_EXPORT isc_service_detach(Jrd::FbStatusVector*,
										 isc_svc_handle*);

	virtual ISC_STATUS ISC_EXPORT isc_service_query(Jrd::FbStatusVector*,
										isc_svc_handle*,
										isc_resv_handle*,
										unsigned short,
										const char*,
										unsigned short,
										const char*,
										unsigned short,
										char*);

	virtual ISC_STATUS ISC_EXPORT isc_service_start(Jrd::FbStatusVector*,
										isc_svc_handle*,
										isc_resv_handle*,
										unsigned short,
										const char*);

	virtual ISC_STATUS ISC_EXPORT fb_cancel_operation(Jrd::FbStatusVector*,
											isc_db_handle*,
											USHORT);

	virtual ISC_STATUS ISC_EXPORT fb_database_crypt_callback(Jrd::FbStatusVector*,
											void*);

	virtual ISC_STATUS API_ROUTINE fb_dsql_set_timeout(Jrd::FbStatusVector*,
										isc_stmt_handle*,
										ULONG);
};


class FBProvider : public IscProvider
{
public:
	explicit FBProvider(const char* prvName) :
		IscProvider(prvName)
	{
	    // Assume that winsspi auth plugin is enabled in configuration
		m_flags = prvTrustedAuth;
	}

protected:
	virtual void loadAPI();
};


class IscConnection : public Connection
{
	friend class IscProvider;

protected:
	explicit IscConnection(IscProvider& prov);
	virtual ~IscConnection();

public:
	FB_API_HANDLE& getAPIHandle() { return m_handle; }

	virtual void attach(Jrd::thread_db* tdbb);

	virtual bool cancelExecution(bool forced);
	virtual bool resetSession();

	virtual bool isAvailable(Jrd::thread_db* tdbb, TraScope traScope) const;

	virtual bool isConnected() const { return (m_handle != 0); }

	virtual bool validate(Jrd::thread_db* tdbb);

	virtual Blob* createBlob();

protected:
	virtual Transaction* doCreateTransaction();
	virtual Statement* doCreateStatement();
	virtual void doDetach(Jrd::thread_db* tdbb);

	IscProvider& m_iscProvider;
	FB_API_HANDLE m_handle;
};


class IscTransaction : public Transaction
{
	friend class IscConnection;

public:
	FB_API_HANDLE& getAPIHandle() { return m_handle; }

protected:
	explicit IscTransaction(IscConnection& conn) :
	  Transaction(conn),
	  m_iscProvider(*(IscProvider*) conn.getProvider()),
	  m_iscConnection(conn),
	  m_handle(0)
	{}

	virtual ~IscTransaction() {}

	virtual void generateTPB(Jrd::thread_db* tdbb, Firebird::ClumpletWriter& tpb,
		TraModes traMode, bool readOnly, bool wait, int lockTimeout) const;

	virtual void doStart(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, Firebird::ClumpletWriter& tpb);
	virtual void doPrepare(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, int info_len, const char* info);
	virtual void doCommit(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain);
	virtual void doRollback(Jrd::FbStatusVector* status, Jrd::thread_db* tdbb, bool retain);

	IscProvider& m_iscProvider;
	IscConnection& m_iscConnection;
	FB_API_HANDLE m_handle;
};


class IscStatement : public Statement
{
	friend class IscConnection;

public:
	FB_API_HANDLE& getAPIHandle() { return m_handle; }

protected:
	explicit IscStatement(IscConnection& conn);
	virtual ~IscStatement();

protected:
	virtual void doPrepare(Jrd::thread_db* tdbb, const Firebird::string& sql);
	virtual void doSetTimeout(Jrd::thread_db* tdbb, unsigned int timeout);
	virtual void doExecute(Jrd::thread_db* tdbb);
	virtual void doOpen(Jrd::thread_db* tdbb);
	virtual bool doFetch(Jrd::thread_db* tdbb);
	virtual void doClose(Jrd::thread_db* tdbb, bool drop);

	virtual void doSetInParams(Jrd::thread_db* tdbb, unsigned int count,
		const Jrd::MetaName* const* names, const NestConst<Jrd::ValueExprNode>* params);

	IscTransaction* getIscTransaction() { return (IscTransaction*) m_transaction; }

	IscProvider& m_iscProvider;
	IscConnection& m_iscConnection;
	FB_API_HANDLE m_handle;
	XSQLDA	*m_in_xsqlda;
	XSQLDA	*m_out_xsqlda;
};

class IscBlob : public Blob
{
	friend class IscConnection;
protected:
	explicit IscBlob(IscConnection& conn);

public:
	~IscBlob();

public:
	virtual void open(Jrd::thread_db* tdbb, Transaction& tran, const dsc& desc,
		const Firebird::UCharBuffer* bpb);
	virtual void create(Jrd::thread_db* tdbb, Transaction& tran, dsc& desc,
		const Firebird::UCharBuffer* bpb);
	virtual USHORT read(Jrd::thread_db* tdbb, UCHAR* buff, USHORT len);
	virtual void write(Jrd::thread_db* tdbb, const UCHAR* buff, USHORT len);
	virtual void close(Jrd::thread_db* tdbb);
	virtual void cancel(Jrd::thread_db* tdbb);

private:
	IscProvider& m_iscProvider;
	IscConnection& m_iscConnection;
	FB_API_HANDLE m_handle;
	ISC_QUAD m_blob_id;
};

} // namespace EDS

#endif // EXTDS_ISC_H

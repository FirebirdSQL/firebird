	FbException = class(Exception)
	public
		constructor create(status: IStatus); virtual;
		destructor Destroy(); override;

		function getStatus: IStatus;

		class procedure checkException(status: IStatus);
		class procedure catchException(status: IStatus; e: Exception);
		class procedure setVersionError(status: IStatus; interfaceName: AnsiString;
			currentVersion, expectedVersion: NativeInt);

	private
		status: IStatus;
	end;

	ISC_DATE = Integer;
	ISC_TIME = Integer;
	ISC_QUAD = array [1..2] of Integer;
	FB_DEC16 = array [1..1] of Int64;
	FB_DEC34 = array [1..2] of Int64;
	FB_I128 = array [1..2] of Int64;

	isc_tr_handle = ^Integer;
	isc_stmt_handle = ^Integer;

	ISC_USHORT = word;		{ 16 bit unsigned }
	ISC_SHORT = smallint;	{ 16 bit signed }

	ISC_TIME_TZ = record
		utc_time: ISC_TIME;
		time_zone: ISC_USHORT;
	end;

	ISC_TIME_TZ_EX = record
		utc_time: ISC_TIME;
		time_zone: ISC_USHORT;
		ext_offset: ISC_SHORT;
	end;

	ISC_TIMESTAMP = record
		timestamp_date: ISC_DATE;
		timestamp_time: ISC_TIME;
	end;

	ISC_TIMESTAMP_TZ = record
		utc_timestamp: ISC_TIMESTAMP;
		time_zone: ISC_USHORT;
	end;

	ISC_TIMESTAMP_TZ_EX = record
		utc_timestamp: ISC_TIMESTAMP;
		time_zone: ISC_USHORT;
		ext_offset: ISC_SHORT;
	end;

	ntrace_relation_t = Integer;
	TraceCounts = Record
		trc_relation_id		: ntrace_relation_t;
		trc_relation_name	: PAnsiChar;
		trc_counters		: ^Int64;
	end;
	TraceCountsPtr = ^TraceCounts;
	PerformanceInfo = Record
		pin_time			: Int64;
		pin_counters		: ^Int64;
		pin_count			: NativeUInt;
		pin_tables			: TraceCountsPtr;
		pin_records_fetched	: Int64;
	end;

	Dsc = Record
		dsc_dtype, dsc_scale: Byte;
		dsc_length, dsc_sub_type, dsc_flags: Int16;
		dsc_address: ^Byte;
	end;

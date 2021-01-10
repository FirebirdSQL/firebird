/*
 *
 *  Sample replication plugin
 *
 */

#include <algorithm>
#include <atomic>
#define __USE_MINGW_ANSI_STDIO 1
#include <stdio.h>
#include <time.h>

#include "firebird/Interface.h"

using namespace Firebird;

#define WriteLog(file, ...) fprintf(file, __VA_ARGS__), fflush(file)

class ReplPlugin : public IReplicatedSessionImpl<ReplPlugin, CheckStatusWrapper>
{
public:
	ReplPlugin(IPluginConfig* config);
	virtual ~ReplPlugin();

	// IReferenceCounted implementation
	void addRef() override;
	int release() override;

	// IPluginBase implementation
	void setOwner(IReferenceCounted* r) override;
	IReferenceCounted* getOwner() override;

	// IReplicatedSession implementation
	IStatus* getStatus() override;
	void setAttachment(IAttachment* attachment) override;
	IReplicatedTransaction* startTransaction(ITransaction* transaction, ISC_INT64 number) override;
	FB_BOOLEAN cleanupTransaction(ISC_INT64 number) override;
	FB_BOOLEAN setSequence(const char* name, ISC_INT64 value) override;

private:
	friend class ReplTransaction;
	IAttachment* att = nullptr;
	FILE* log = nullptr;
	IStatus* status = nullptr;
	std::atomic_int refCounter;
	IReferenceCounted* owner;

	void dumpInfo(const unsigned char* buffer, size_t length);
};

class ReplTransaction: public IReplicatedTransactionImpl<ReplTransaction, CheckStatusWrapper>
{
public:
	ReplTransaction(ReplPlugin* session, ITransaction* transaction, ISC_INT64 number);
	~ReplTransaction();

	// IDisposable implementation
	void dispose() override;

	// IReplicatedTransaction implementation
	FB_BOOLEAN prepare() override;
	FB_BOOLEAN commit() override;
	FB_BOOLEAN rollback() override;
	FB_BOOLEAN startSavepoint() override;
	FB_BOOLEAN releaseSavepoint() override;
	FB_BOOLEAN rollbackSavepoint() override;
	FB_BOOLEAN insertRecord(const char* name, IReplicatedRecord* record) override;
	FB_BOOLEAN updateRecord(const char* name, IReplicatedRecord* orgRecord, IReplicatedRecord* newRecord) override;
	FB_BOOLEAN deleteRecord(const char* name, IReplicatedRecord* record) override;
	FB_BOOLEAN executeSql(const char* sql) override;
	FB_BOOLEAN executeSqlIntl(unsigned charset, const char* sql) override;

private:
	ReplPlugin* parent;
	ITransaction* trans;

	bool dumpData(IReplicatedRecord* record);
};

IMaster* master = nullptr;

class PluginModule : public IPluginModuleImpl<PluginModule, CheckStatusWrapper>
{
public:
	void doClean() override {}
	void threadDetach() override {}

} module;

class Factory : public IPluginFactoryImpl<Factory, CheckStatusWrapper>
{
public:
	IPluginBase* createPlugin(CheckStatusWrapper* status, IPluginConfig* factoryParameter)
	{
		IPluginBase* p = new ReplPlugin(factoryParameter);
		p->addRef();
		return p;
	}
} factory;

extern "C"
{
#if defined(__WIN32__)
	void __declspec(dllexport) FB_PLUGIN_ENTRY_POINT(IMaster* m);
#else
	void FB_PLUGIN_ENTRY_POINT(IMaster* m)
		__attribute__((visibility("default")));
#endif // __WIN32__

	void FB_PLUGIN_ENTRY_POINT(IMaster* m)
	{
		master = m;
		IPluginManager* pm = m->getPluginManager();
		pm->registerModule(&module);
		pm->registerPluginFactory(IPluginManager::TYPE_REPLICATOR, "fbSampleReplicator", &factory);
	}
}

static std::atomic_int logCounter;

static const ISC_STATUS err[] = { isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS)"Intolerable integer value", isc_arg_end };
static const ISC_STATUS wrn[] = { isc_arg_gds, isc_random, isc_arg_string, (ISC_STATUS)"Just a warning", isc_arg_end };

ReplPlugin::ReplPlugin(IPluginConfig* conf)
{
	char fn[100];
	sprintf(fn, "session_%08x_%d.log", (unsigned)time(nullptr), logCounter++);
	log = fopen(fn, "w");
	WriteLog(log, "%p\tReplicatedSession constructed\n", this);
	status = master->getStatus();
}

ReplPlugin::~ReplPlugin()
{
	if (log != nullptr)
	{
		WriteLog(log, "%p\tReplicatedSession destructed\n", this);
		fclose(log);
	}
	if (att != nullptr)
		att->release();
	if (status != nullptr)
		status->dispose();
}

void ReplPlugin::addRef()
{
	WriteLog(log, "%p\taddRef() to %d\n", this, ++refCounter);
}

int ReplPlugin::release()
{
	WriteLog(log, "%p\trelease at %d\n", this, refCounter.load());
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}
	return 1;
}

void ReplPlugin::setOwner(IReferenceCounted* r)
{
	WriteLog(log, "%p\tsetOwner(%p)\n", this, r);
	owner = r;
}

IReferenceCounted* ReplPlugin::getOwner()
{
	WriteLog(log, "%p\tgetOwner()\n", this);
	return owner;
}

IStatus* ReplPlugin::getStatus()
{
	WriteLog(log, "%p\tgetStatus()\n", this);
	return status;
}

void ReplPlugin::dumpInfo(const unsigned char* buffer, size_t length)
{
	const unsigned char* p = buffer;
	while (p < buffer + length)
	{
		unsigned char item = *p++;

		// Handle terminating items fist
		if (item == isc_info_end)
		{
			return;
		}
		if (item == isc_info_truncated)
		{
			WriteLog(log, "\t\tDatabase info truncated\n");
			return;
		}

		// Now data items
		const unsigned len = p[0] | p[1] << 8;

		p += 2;
		switch (item)
		{
		case fb_info_db_guid:
			{
				WriteLog(log, "\t\tDatabase GUID = %.*s\n", len, p);
				break;
			}
		case isc_info_error:
			{
				unsigned err = p[1];
				for (unsigned i = 1; i < std::min(len, 4U); i++)
				{
					err |= p[i + 1] << (8 * i);
				}
				WriteLog(log, "\t\tDatabase info error %u for item %d\n", err, p[0]);
				return;
			}
		default:
			WriteLog(log, "\t\tUnexpected info item %d\n", item);
			break;
		}
		p += len;
	}
	WriteLog(log, "\t\tSuspicious exit from info parse loop\n");
}

void ReplPlugin::setAttachment(IAttachment* attachment)
{
	WriteLog(log, "%p\tAssigned attachment %p\n", this, attachment);
	att = attachment;
	att->addRef();
	CheckStatusWrapper ExtStatus(status);
	const unsigned char items[] = { fb_info_db_guid };
	unsigned char response[80];
	att->getInfo(&ExtStatus, sizeof(items), items, sizeof(response), response);
	if (status->getState() == 0)
	{
		dumpInfo(response, sizeof(response));
	}
}

IReplicatedTransaction* ReplPlugin::startTransaction(ITransaction* transaction, ISC_INT64 number)
{
	WriteLog(log, "%p\tstartTransaction(%p, %lld)\n", this, transaction, number);
	return new ReplTransaction(this, transaction, number);
}

FB_BOOLEAN ReplPlugin::cleanupTransaction(ISC_INT64 number)
{
	WriteLog(log, "%p\tcleanupTransaction(%lld)\n", this, number);
	return FB_TRUE;
}

FB_BOOLEAN ReplPlugin::setSequence(const char* name, ISC_INT64 value)
{
	WriteLog(log, "%p\tsetSequence(%s, %lld)\n", this, name, value);
	return FB_TRUE;
}

ReplTransaction::ReplTransaction(ReplPlugin* session, ITransaction* transaction, ISC_INT64 number):
	parent(session), trans(transaction)
{
	parent->addRef(); // Lock parent from disappearing
	trans->addRef();
	WriteLog(parent->log, "%p\tTransaction started\n", this);
}

ReplTransaction::~ReplTransaction()
{
	WriteLog(parent->log, "%p\tTransaction destructed\n", this);
	trans->release();
	parent->release();
}

void ReplTransaction::dispose()
{
	WriteLog(parent->log, "%p\tdispose()\n", this);
	delete this;
}

FB_BOOLEAN ReplTransaction::prepare()
{
	WriteLog(parent->log, "%p\tprepare()\n", this);
	return FB_TRUE;
}

FB_BOOLEAN ReplTransaction::commit()
{
	WriteLog(parent->log, "%p\tcommit()\n", this);
	return FB_TRUE;
}

FB_BOOLEAN ReplTransaction::rollback()
{
	WriteLog(parent->log, "%p\trollback()\n", this);
	parent->status->setWarnings(wrn);
	return FB_FALSE;
}

FB_BOOLEAN ReplTransaction::startSavepoint()
{
	WriteLog(parent->log, "%p\tstartSavepoint()\n", this);
	return FB_TRUE;
}

FB_BOOLEAN ReplTransaction::releaseSavepoint()
{
	WriteLog(parent->log, "%p\treleaseSavepoint()\n", this);
	return FB_TRUE;
}

FB_BOOLEAN ReplTransaction::rollbackSavepoint()
{
	WriteLog(parent->log, "%p\trollbackSavepoint()\n", this);
	return FB_TRUE;
}

bool ReplTransaction::dumpData(IReplicatedRecord* record)
{
	for (unsigned i = 0; i < record->getCount(); i++)
	{
		IReplicatedField* field = record->getField(i);
		if (field == nullptr)
		{
			WriteLog(parent->log, "\t\tNO FIELD %u FOUND\n", i);
			continue;
		}
		unsigned fieldType = field->getType();

		WriteLog(parent->log, "\tfield %u (%s), type %u:\n", i, field->getName(), fieldType);

		const void* fieldData = field->getData();
		if (fieldData == nullptr)
		{
			WriteLog(parent->log, "\t\tNULL\n");
		}
		else
		{
			switch (fieldType)
			{
			case SQL_TEXT:
				{
					unsigned length = field->getLength();
					unsigned charSet = field->getCharSet();

					if (charSet == 1) // OCTETS
					{
						WriteLog(parent->log, "\t\tBINARY data length %u: ", length);
						const unsigned char* data = reinterpret_cast<const unsigned char*>(fieldData);
						for (unsigned j = 0; j < length; j++)
							WriteLog(parent->log, "%02u", *data++);
						WriteLog(parent->log, "\n");
					}
					else
						WriteLog(parent->log, "\t\tTEXT with charset %u, length %u: \"%.*s\"\n", charSet, length, length, reinterpret_cast<const char*>(fieldData));

					break;
				}
			case SQL_VARYING:
				{
					unsigned charSet = field->getCharSet();
					const paramvary* data = static_cast<const paramvary*>(fieldData);

					if (charSet == 1) // OCTETS
					{
						fprintf(parent->log, "\t\tVARBINARY data length %u: ", data->vary_length);
						for (unsigned j = 0; j < data->vary_length; j++)
							fprintf(parent->log, "%02u", data->vary_string[j]);
						WriteLog(parent->log, "\n");
					}
					else
						WriteLog(parent->log, "\t\tVARCHAR with charset %u, length %u: \"%.*s\"\n", charSet, data->vary_length, data->vary_length, data->vary_string);

					break;
				}
			case SQL_SHORT:
				{
					WriteLog(parent->log, "\t\tSMALLINT with scale %u: %d\n", field->getScale(), *reinterpret_cast<const int16_t*>(fieldData));
					break;
				}
			case SQL_LONG:
				{
					int value = *reinterpret_cast<const int32_t*>(fieldData);
					WriteLog(parent->log, "\t\tINTEGER with scale %u: %d\n", field->getScale(), value);
					if (value == 666)
						throw value;
					break;
				}
			case SQL_ARRAY:
				{
					WriteLog(parent->log, "\t\tARRAY\n");
					break;
				}
			case SQL_BLOB:
				{
					unsigned subType = field->getSubType();
					WriteLog(parent->log, "\t\tBLOB subtype %u:\n", subType);

					unsigned charSet = 1;
					if (subType == 1)
					{
						charSet = field->getCharSet();
					}

					CheckStatusWrapper ExtStatus(parent->status);
					ISC_QUAD blobId = *reinterpret_cast<const ISC_QUAD*>(fieldData);
					IBlob* blob(parent->att->openBlob(&ExtStatus, trans, &blobId, 0, nullptr));
					if (ExtStatus.getState() & IStatus::STATE_ERRORS)
						return false;

					char buffer[USHRT_MAX];
					do
					{
						unsigned length;
						int ret = blob->getSegment(&ExtStatus, sizeof(buffer), buffer, &length);
						if (ret == IStatus::RESULT_ERROR)
						{
							blob->release();
							return false;
						}
						if (ret == IStatus::RESULT_NO_DATA)
							break;
						if (length > 0)
						{
							fprintf(parent->log, "\t\t - segment of length %u: ", length);
							if (subType != 1 || charSet == 1)
							{
								for (unsigned j = 0; j < length; j++)
									fprintf(parent->log, "%02u", buffer[j]);
								WriteLog(parent->log, "\n");
							}
							else
								WriteLog(parent->log, "(charset %u) \"%.*s\"\n", charSet, length, buffer);
						}
					} while (true);

					blob->close(&ExtStatus);
					blob->release();
					if (ExtStatus.getState() & IStatus::STATE_ERRORS)
						return false;

					break;
				}
			case SQL_FLOAT:
				{
					WriteLog(parent->log, "\t\tFLOAT: %f\n", *reinterpret_cast<const float*>(fieldData));
					break;
				}
			case SQL_DOUBLE:
				{
					WriteLog(parent->log, "\t\tDOUBLE: %f\n", *reinterpret_cast<const double*>(fieldData));
					break;
				}
			case SQL_TYPE_DATE:
				{
					ISC_DATE value = *reinterpret_cast<const ISC_DATE*>(fieldData);
					IUtil* utl = master->getUtilInterface();
					unsigned year, month, day;
					utl->decodeDate(value, &year, &month, &day);
					WriteLog(parent->log, "\t\tDATE: %04u-%02u-%02u\n", year, month, day);
					break;
				}
			case SQL_TYPE_TIME:
				{
					ISC_TIME value = *reinterpret_cast<const ISC_TIME*>(fieldData);
					IUtil* utl = master->getUtilInterface();
					unsigned hours, minutes, seconds, fractions;
					utl->decodeTime(value, &hours, &minutes, &seconds, &fractions);
					WriteLog(parent->log, "\t\tTIME: %02u:%02u:%02u.%04u\n", hours, minutes, seconds, fractions);
					break;
				}
			case SQL_TIMESTAMP:
				{
					ISC_TIMESTAMP value = *reinterpret_cast<const ISC_TIMESTAMP*>(fieldData);
					IUtil* utl = master->getUtilInterface();
					unsigned year, month, day, hours, minutes, seconds, fractions;
					utl->decodeDate(value.timestamp_date, &year, &month, &day);
					utl->decodeTime(value.timestamp_time, &hours, &minutes, &seconds, &fractions);
					WriteLog(parent->log, "\t\tTIMESTAMP: %04u-%02u-%02u %02u:%02u:%02u.%04u\n", year, month, day, hours, minutes, seconds, fractions);
					break;
				}
			case SQL_INT64:
				{
					WriteLog(parent->log, "\t\tBIGINT with scale %u: %lld\n", field->getScale(), *reinterpret_cast<const int64_t*>(fieldData));
					break;
				}
			case SQL_BOOLEAN:
				{
					WriteLog(parent->log, "\t\tBOOLEAN: %s\n",
							*reinterpret_cast<const FB_BOOLEAN*>(fieldData) == FB_TRUE ?
							"true" : "false");
					break;
				}
			case SQL_INT128:
				{
					char buffer[50];
					unsigned scale = field->getScale();
					CheckStatusWrapper ExtStatus(parent->status);
					master->getUtilInterface()->getInt128(&ExtStatus)->toString(&ExtStatus,
															reinterpret_cast<const FB_I128*>(fieldData), scale,
															sizeof(buffer), buffer);
					WriteLog(parent->log, "\t\tINT128 with scale %u: %s\n", scale, buffer);
					break;
				}
			default:
				{
					WriteLog(parent->log, "\t\twhatever\n");
				}
			}
		}
	}
	return true;
}

FB_BOOLEAN ReplTransaction::insertRecord(const char* name, IReplicatedRecord* record)
{
	WriteLog(parent->log, "%p\tInsert record into %s\n", this, name);
	try
	{
		return dumpData(record) ? FB_TRUE : FB_FALSE;
	}
	catch (const int)
	{
		parent->status->setErrors(err);
		return FB_FALSE;
	}
}

FB_BOOLEAN ReplTransaction::updateRecord(const char* name, IReplicatedRecord* orgRecord, IReplicatedRecord* newRecord)
{
	WriteLog(parent->log, "%p\tUpdate %s\nOldData:\n", this, name);
	try
	{
		if (!dumpData(orgRecord))
			return FB_FALSE;

		WriteLog(parent->log, "NewData:\n");
		return dumpData(newRecord) ? FB_TRUE : FB_FALSE;
	}
	catch (const int)
	{
		parent->status->setErrors(err);
		return FB_FALSE;
	}
}

FB_BOOLEAN ReplTransaction::deleteRecord(const char* name, IReplicatedRecord* record)
{
	WriteLog(parent->log, "%p\tDelete from %s\n", this, name);
	try
	{
		return dumpData(record) ? FB_TRUE : FB_FALSE;
	}
	catch (const int)
	{
		parent->status->setErrors(err);
		return FB_FALSE;
	}
}

FB_BOOLEAN ReplTransaction::executeSql(const char* sql)
{
	WriteLog(parent->log, "%p\tExecuteSql(%s)\n", this, sql);
	return FB_TRUE;
}

FB_BOOLEAN ReplTransaction::executeSqlIntl(unsigned charset, const char* sql)
{
	WriteLog(parent->log, "%p\tExecuteSqlIntl(%u, %s)\n", this, charset, sql);
	return FB_TRUE;
}

/*
 *	PROGRAM:		Firebird authentication
 *	MODULE:			AuthSspi.cpp
 *	DESCRIPTION:	Windows trusted authentication
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
 *  Copyright (c) 2006 Alex Peshkov <peshkoff at mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 *
 */
#include "AuthSspi.h"

#ifdef TRUSTED_AUTH

#include "../common/classes/ClumpletReader.h"
#include "firebird/Interface.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/isc_f_proto.h"

using namespace Firebird;

namespace
{
	Firebird::SimpleFactory<Auth::WinSspiClient> clientFactory;
	Firebird::SimpleFactory<Auth::WinSspiServer> serverFactory;

	const char* plugName = "Win_Sspi";

	void makeDesc(SecBufferDesc& d, SecBuffer& b, FB_SIZE_T len, void* p)
	{
		b.BufferType = SECBUFFER_TOKEN;
		b.cbBuffer = len;
		b.pvBuffer = len ? p : 0;
		d.ulVersion = SECBUFFER_VERSION;
		d.cBuffers = 1;
		d.pBuffers = &b;
	}

	template<typename ToType>
		ToType getProc(HINSTANCE lib, const char* entry)
	{
		FARPROC rc = GetProcAddress(lib, entry);
		if (! rc)
		{
			LongJump::raise();
		}
		return (ToType)rc;
	}
}

namespace Auth {

HINSTANCE AuthSspi::library = 0;

bool AuthSspi::initEntries()
{
	if (! library)
	{
		library = LoadLibrary("secur32.dll");
	}
	if (! library)
	{
		return false;
	}

	try
	{
		fAcquireCredentialsHandle = getProc<ACQUIRE_CREDENTIALS_HANDLE_FN_A>
			(library, "AcquireCredentialsHandleA");
		fDeleteSecurityContext = getProc<DELETE_SECURITY_CONTEXT_FN>
			(library, "DeleteSecurityContext");
		fFreeCredentialsHandle = getProc<FREE_CREDENTIALS_HANDLE_FN>
			(library, "FreeCredentialsHandle");
		fQueryContextAttributes = getProc<QUERY_CONTEXT_ATTRIBUTES_FN_A>
			(library, "QueryContextAttributesA");
		fFreeContextBuffer = getProc<FREE_CONTEXT_BUFFER_FN>
			(library, "FreeContextBuffer");
		fInitializeSecurityContext = getProc<INITIALIZE_SECURITY_CONTEXT_FN_A>
			(library, "InitializeSecurityContextA");
		fAcceptSecurityContext = getProc<ACCEPT_SECURITY_CONTEXT_FN>
			(library, "AcceptSecurityContext");
	}
	catch (const LongJump&)
	{
		return false;
	}
	return true;
}

AuthSspi::AuthSspi()
	: hasContext(false), ctName(*getDefaultMemoryPool()), wheel(false),
	  groupNames(*getDefaultMemoryPool()), sessionKey(*getDefaultMemoryPool())
{
	TimeStamp timeOut;
	hasCredentials = initEntries() && (fAcquireCredentialsHandle(0, "NTLM",
					SECPKG_CRED_BOTH, 0, 0, 0, 0,
					&secHndl, &timeOut) == SEC_E_OK);
}

AuthSspi::~AuthSspi()
{
	if (hasContext)
	{
		fDeleteSecurityContext(&ctxtHndl);
	}
	if (hasCredentials)
	{
		fFreeCredentialsHandle(&secHndl);
	}
}

const AuthSspi::Key* AuthSspi::getKey() const
{
	if (sessionKey.hasData())
		return &sessionKey;
	return NULL;
}

bool AuthSspi::checkAdminPrivilege()
{
	// Query access token from security context
	SecPkgContext_AccessToken spc;
	spc.AccessToken = 0;
	if (fQueryContextAttributes(&ctxtHndl, SECPKG_ATTR_ACCESS_TOKEN, &spc) != SEC_E_OK)
	{
		return false;
	}

	// Query required buffer size
	DWORD token_len = 0;
	GetTokenInformation(spc.AccessToken, TokenGroups, 0, 0, &token_len);

	// Query actual group information
	Array<char> buffer;
	TOKEN_GROUPS *ptg = (TOKEN_GROUPS *)buffer.getBuffer(token_len);
	if (!GetTokenInformation(spc.AccessToken, TokenGroups, ptg, token_len, &token_len))
		return false;

	// Create a System Identifier for the Admin group.
	SID_IDENTIFIER_AUTHORITY system_sid_authority = {SECURITY_NT_AUTHORITY};
	PSID domain_admin_sid, local_admin_sid;

	if (!AllocateAndInitializeSid(&system_sid_authority, 2,
				SECURITY_BUILTIN_DOMAIN_RID,
				DOMAIN_GROUP_RID_ADMINS,
				0, 0, 0, 0, 0, 0, &domain_admin_sid))
	{
		return false;
	}

	if (!AllocateAndInitializeSid(&system_sid_authority, 2,
				SECURITY_BUILTIN_DOMAIN_RID,
				DOMAIN_ALIAS_RID_ADMINS,
				0, 0, 0, 0, 0, 0, &local_admin_sid))
	{
		FreeSid(domain_admin_sid);
		return false;
	}

	bool matched = false;
	char groupName[256];
	char domainName[256];
	DWORD dwAcctName = 1, dwDomainName = 1;
	SID_NAME_USE snu = SidTypeUnknown;

	groupNames.clear();

	// Finally we'll iterate through the list of groups for this access
	// token looking for a match against the SID we created above.
	for (DWORD i = 0; i < ptg->GroupCount; i++)
	{
		// consider denied ACE with Administrator SID
		if ((ptg->Groups[i].Attributes & SE_GROUP_ENABLED) &&
			!(ptg->Groups[i].Attributes & SE_GROUP_USE_FOR_DENY_ONLY))
		{
			DWORD dwSize = 256;
			if (LookupAccountSid(NULL, ptg->Groups[i].Sid, groupName, &dwSize, domainName, &dwSize, &snu) &&
				domainName[0] && strcmp(domainName, "NT AUTHORITY"))
			{
				string sumName = domainName;
				sumName += '\\';
				sumName += groupName;
				groupNames.add(sumName);

				sumName = groupName;
				FB_SIZE_T dummy;
				if (!groupNames.find(sumName, dummy))
					groupNames.add(sumName);
			}

			if (EqualSid(ptg->Groups[i].Sid, domain_admin_sid) ||
				EqualSid(ptg->Groups[i].Sid, local_admin_sid))
			{
				matched = true;
			}
		}
	}

	FreeSid(domain_admin_sid);
	FreeSid(local_admin_sid);
	return matched;
}

bool AuthSspi::request(AuthSspi::DataHolder& data)
{
	if (! hasCredentials)
	{
		data.clear();
		return false;
	}

	TimeStamp timeOut;

	char s[BUFSIZE];
	SecBuffer outputBuffer, inputBuffer;
	SecBufferDesc outputDesc, inputDesc;
	makeDesc(outputDesc, outputBuffer, sizeof(s), s);
	makeDesc(inputDesc, inputBuffer, data.getCount(), data.begin());

	ULONG fContextAttr = 0;

	SECURITY_STATUS x = fInitializeSecurityContext(
		&secHndl, hasContext ? &ctxtHndl : 0, 0, 0, 0, SECURITY_NATIVE_DREP,
		hasContext ? &inputDesc : 0, 0, &ctxtHndl, &outputDesc, &fContextAttr, &timeOut);

	SecPkgContext_SessionKey key;
	switch (x)
	{
	case SEC_E_OK:
		if (fQueryContextAttributes(&ctxtHndl, SECPKG_ATTR_SESSION_KEY, &key) == SEC_E_OK)
		{
			sessionKey.assign(key.SessionKey, key.SessionKeyLength);
		}
		fDeleteSecurityContext(&ctxtHndl);
		hasContext = false;
		break;

	case SEC_I_CONTINUE_NEEDED:
		hasContext = true;
		break;

	default:
		if (hasContext)
		{
			fDeleteSecurityContext(&ctxtHndl);
		}
		hasContext = false;
		data.clear();
		return false;
	}

	if (outputBuffer.cbBuffer)
	{
		memcpy(data.getBuffer(outputBuffer.cbBuffer),
			   outputBuffer.pvBuffer, outputBuffer.cbBuffer);
	}
	else
	{
		data.clear();
	}

	return true;
}

bool AuthSspi::accept(AuthSspi::DataHolder& data)
{
	if (! hasCredentials)
	{
		data.clear();
		return false;
	}

	TimeStamp timeOut;

	char s[BUFSIZE];
	SecBuffer outputBuffer, inputBuffer;
	SecBufferDesc outputDesc, inputDesc;
	makeDesc(outputDesc, outputBuffer, sizeof(s), s);
	makeDesc(inputDesc, inputBuffer, data.getCount(), data.begin());

	ULONG fContextAttr = 0;
	SecPkgContext_Names name;
	SecPkgContext_SessionKey key;
	SECURITY_STATUS x = fAcceptSecurityContext(
		&secHndl, hasContext ? &ctxtHndl : 0, &inputDesc, 0,
		SECURITY_NATIVE_DREP, &ctxtHndl, &outputDesc,
		&fContextAttr, &timeOut);

	switch (x)
	{
	case SEC_E_OK:
		if (fQueryContextAttributes(&ctxtHndl, SECPKG_ATTR_NAMES, &name) == SEC_E_OK)
		{
			ctName = name.sUserName;
			ctName.upper();
			fFreeContextBuffer(name.sUserName);
			wheel = checkAdminPrivilege();
		}

		if (fQueryContextAttributes(&ctxtHndl, SECPKG_ATTR_SESSION_KEY, &key) == SEC_E_OK)
			sessionKey.assign(key.SessionKey, key.SessionKeyLength);

		fDeleteSecurityContext(&ctxtHndl);
		hasContext = false;
		break;

	case SEC_I_CONTINUE_NEEDED:
		hasContext = true;
		break;

	default:
		if (hasContext)
		{
			fDeleteSecurityContext(&ctxtHndl);
		}
		hasContext = false;
		data.clear();
		return false;
	}

	if (outputBuffer.cbBuffer)
	{
		memcpy(data.getBuffer(outputBuffer.cbBuffer),
			   outputBuffer.pvBuffer, outputBuffer.cbBuffer);
	}
	else
	{
		data.clear();
	}

	return true;
}

bool AuthSspi::getLogin(string& login, bool& wh, GroupsList& grNames)
{
	wh = false;
	if (ctName.hasData())
	{
		login = ctName;
		ctName.erase();
		wh = wheel;
		wheel = false;
		grNames = groupNames;
		groupNames.clear();

		return true;
	}
	return false;
}


WinSspiServer::WinSspiServer(Firebird::IPluginConfig*)
	: sspiData(getPool())
{ }

int WinSspiServer::authenticate(Firebird::CheckStatusWrapper* status,
								IServerBlock* sBlock,
								IWriter* writerInterface)
{
	try
	{
		const bool wasActive = sspi.isActive();

		sspiData.clear();
		unsigned int length;
		const unsigned char* bytes = sBlock->getData(&length);
		sspiData.add(bytes, length);

		if (!sspi.accept(sspiData))
			return AUTH_CONTINUE;

		if (wasActive && !sspi.isActive())
		{
			bool wheel = false;
			string login;
			AuthSspi::GroupsList grNames;
			sspi.getLogin(login, wheel, grNames);
			ISC_systemToUtf8(login);

			// publish user name obtained during SSPI handshake
			writerInterface->add(status, login.c_str());
			if (status->getState() & IStatus::STATE_ERRORS)
				return AUTH_FAILED;

			// is it suser account?
			if (wheel)
			{
				writerInterface->add(status, FB_DOMAIN_ANY_RID_ADMINS);
				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;

				writerInterface->setType(status, FB_PREDEFINED_GROUP);

				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;
			}

			// walk groups to which login belongs and list them using writerInterface
			Firebird::string grName;

			for (unsigned n = 0; n < grNames.getCount(); ++n)
			{
				grName = grNames[n];
				ISC_systemToUtf8(grName);
				writerInterface->add(status, grName.c_str());

				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;

				writerInterface->setType(status, "Group");

				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;
			}

			// set wire crypt key
			const UCharBuffer* key = sspi.getKey();
			if (key)
			{
				ICryptKey* cKey = sBlock->newKey(status);

				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;

				cKey->setSymmetric(status, "Symmetric", key->getCount(), key->begin());

				if (status->getState() & IStatus::STATE_ERRORS)
					return AUTH_FAILED;
			}

			return AUTH_SUCCESS;
		}

		sBlock->putData(status, sspiData.getCount(), sspiData.begin());
	}
	catch (const Firebird::Exception& ex)
	{
		ex.stuffException(status);
		return AUTH_FAILED;
	}

	return AUTH_MORE_DATA;
}

int WinSspiServer::release()
{
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}

	return 1;
}


WinSspiClient::WinSspiClient(Firebird::IPluginConfig*)
	: sspiData(getPool()), keySet(false)
{ }

int WinSspiClient::authenticate(Firebird::CheckStatusWrapper* status,
								IClientBlock* cBlock)
{
	try
	{
		if (cBlock->getLogin())
		{
			// user specified login - we should not continue with trusted-like auth
			return AUTH_CONTINUE;
		}

		sspiData.clear();
		unsigned int length;
		const unsigned char* bytes = cBlock->getData(&length);
		sspiData.add(bytes, length);

		if (!sspi.request(sspiData))
			return AUTH_CONTINUE;

		cBlock->putData(status, sspiData.getCount(), sspiData.begin());
		if (status->getState() & IStatus::STATE_ERRORS)
			return AUTH_FAILED;

		// set wire crypt key
		const UCharBuffer* key = sspi.getKey();

		if (key && !keySet)
		{
			ICryptKey* cKey = cBlock->newKey(status);

			if (status->getState() & IStatus::STATE_ERRORS)
				return AUTH_FAILED;

			cKey->setSymmetric(status, "Symmetric", key->getCount(), key->begin());

			if (status->getState() & IStatus::STATE_ERRORS)
				return AUTH_FAILED;

			keySet = true;
		}
	}
	catch (const Firebird::Exception& ex)
	{
		ex.stuffException(status);
		return AUTH_FAILED;
	}

	return AUTH_MORE_DATA;
}

int WinSspiClient::release()
{
	if (--refCounter == 0)
	{
		delete this;
		return 0;
	}

	return 1;
}


void registerTrustedClient(Firebird::IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(Firebird::IPluginManager::TYPE_AUTH_CLIENT, plugName, &clientFactory);
}

void registerTrustedServer(Firebird::IPluginManager* iPlugin)
{
	iPlugin->registerPluginFactory(Firebird::IPluginManager::TYPE_AUTH_SERVER, plugName, &serverFactory);
}

} // namespace Auth

#endif // TRUSTED_AUTH

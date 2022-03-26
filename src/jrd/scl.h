/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		scl.h
 *	DESCRIPTION:	Security class definitions
 *
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

#ifndef JRD_SCL_H
#define JRD_SCL_H

#include "../jrd/MetaName.h"
#include "../common/classes/tree.h"
#include "../common/security.h"
#include "../jrd/obj.h"
#include "../jrd/SystemPrivileges.h"

namespace Firebird {
class ClumpletWriter;
}

namespace Jrd {

class thread_db;

const size_t ACL_BLOB_BUFFER_SIZE = MAX_USHORT; // used to read/write acl blob

// Security class definition

class SecurityClass
{
public:
	typedef ULONG flags_t;
	enum BlobAccessCheck { BA_UNKNOWN, BA_SUCCESS, BA_FAILURE };

	SecurityClass(Firebird::MemoryPool &pool, const MetaName& name, const MetaName& userName)
		: scl_flags(0), sclClassUser(pool, MetaNamePair(name, userName)), scl_blb_access(BA_UNKNOWN)
	{}

	flags_t scl_flags;			// Access permissions
	const MetaNamePair sclClassUser;
	BlobAccessCheck scl_blb_access;

	static const MetaNamePair& generate(const void*, const SecurityClass* item)
	{
		return item->sclClassUser;
	}
};

typedef Firebird::BePlusTree<
	SecurityClass*,
	MetaNamePair,
	Firebird::MemoryPool,
	SecurityClass
> SecurityClassList;


const SecurityClass::flags_t SCL_select			= 1;		// SELECT access
const SecurityClass::flags_t SCL_drop			= 2;		// DROP access
const SecurityClass::flags_t SCL_control		= 4;		// Control access
const SecurityClass::flags_t SCL_exists			= 8;		// At least ACL exists
const SecurityClass::flags_t SCL_alter			= 16;		// ALTER access
const SecurityClass::flags_t SCL_corrupt		= 32;		// ACL does look too good
const SecurityClass::flags_t SCL_insert			= 64;		// INSERT access
const SecurityClass::flags_t SCL_delete			= 128;		// DELETE access
const SecurityClass::flags_t SCL_update			= 256;		// UPDATE access
const SecurityClass::flags_t SCL_references		= 512;		// REFERENCES access
const SecurityClass::flags_t SCL_execute		= 1024;		// EXECUTE access
const SecurityClass::flags_t SCL_usage			= 2048;		// USAGE access
const SecurityClass::flags_t SCL_create			= 4096;

const SecurityClass::flags_t SCL_SELECT_ANY	= SCL_select | SCL_references;
const SecurityClass::flags_t SCL_ACCESS_ANY	= SCL_insert | SCL_update | SCL_delete |
											  SCL_execute | SCL_usage | SCL_SELECT_ANY;
const SecurityClass::flags_t SCL_MODIFY_ANY	= SCL_create | SCL_alter | SCL_control | SCL_drop;


// information about the user

const USHORT USR_mapdown	= 1;		// Mapping failed when getting context
const USHORT USR_newrole	= 2;		// usr_granted_roles array needs refresh
const USHORT USR_sysdba		= 4;		// User detected as SYSDBA

class UserId
{
public:
	// Arbitrary size bitmask
	template <unsigned N>
	class Bits
	{
		static const unsigned shift = 3;
		static const unsigned bitmask = (1 << shift) - 1;

		static const unsigned L = (N >> shift) + (N & bitmask ? 1 : 0);

	public:
		static const unsigned BYTES_COUNT = L;

		Bits()
		{
			clearAll();
		}

		Bits(const Bits& b)
		{
			assign(b);
		}

		Bits& operator=(const Bits& b)
		{
			assign(b);
			return *this;
		}

		Bits& set(unsigned i)
		{
			fb_assert(i < N);
			if (i < N)
				data[index(i)] |= mask(i);
			return *this;
		}

		Bits& setAll()
		{
			memset(data, ~0, sizeof data);
			return *this;
		}

		Bits& clear(unsigned i)
		{
			fb_assert(i < N);
			if (i < N)
				data[index(i)] &= ~mask(i);
			return *this;
		}

		Bits& clearAll()
		{
			memset(data, 0, sizeof data);
			return *this;
		}

		bool test(unsigned int i) const
		{
			fb_assert(i < N);
			if (i >= N)
				return false;
			return data[index(i)] & mask(i);
		}

		void load(const void* from)
		{
			memcpy(data, from, sizeof data);
		}

		void store(void* to) const
		{
			memcpy(to, data, sizeof data);
		}

		Bits& operator|=(const Bits& b)
		{
			for (unsigned n = 0; n < L; ++n)
				data[n] |= b.data[n];
			return *this;
		}

	private:
		UCHAR data[L];

		void assign(const Bits& b)
		{
			memcpy(data, b.data, sizeof data);
		}

		static unsigned index(unsigned i)
		{
			return i >> shift;
		}

		static UCHAR mask(unsigned i)
		{
			return 1U << (i & bitmask);
		}
	};

	typedef Bits<maxSystemPrivilege> Privileges;

private:
	Firebird::MetaString	usr_user_name;		// User name
	Firebird::MetaString	usr_sql_role_name;	// Role name
	mutable Firebird::SortedArray<MetaName> usr_granted_roles; // Granted roles list
	Firebird::MetaString	usr_trusted_role;	// Trusted role if set
	Firebird::MetaString	usr_init_role;		// Initial role, assigned at sclInit()

public:
	Firebird::string	usr_project_name;	// Project name
	Firebird::string	usr_org_name;		// Organization name
	Firebird::string	usr_auth_method;	// Authentication method

private:
	mutable Privileges	usr_privileges;		// Privileges granted to user by default

public:
	Auth::AuthenticationBlock usr_auth_block;	// Authentication block after mapping
	USHORT				usr_user_id;		// User id
	USHORT				usr_group_id;		// Group id

private:
	mutable USHORT		usr_flags;			// Misc. crud

public:
	UserId()
		: usr_user_id(0), usr_group_id(0), usr_flags(0)
	{}

	UserId(Firebird::MemoryPool& p, const UserId& ui)
		: usr_user_name(p, ui.usr_user_name),
		  usr_sql_role_name(p, ui.usr_sql_role_name),
		  usr_granted_roles(p),
		  usr_trusted_role(p, ui.usr_trusted_role),
		  usr_init_role(p, ui.usr_init_role),
		  usr_project_name(p, ui.usr_project_name),
		  usr_org_name(p, ui.usr_org_name),
		  usr_auth_method(p, ui.usr_auth_method),
		  usr_privileges(ui.usr_privileges),
		  usr_auth_block(p),
		  usr_user_id(ui.usr_user_id),
		  usr_group_id(ui.usr_group_id),
		  usr_flags(ui.usr_flags)
	{
		usr_auth_block.assign(ui.usr_auth_block);
		if (!testFlag(USR_newrole))
			usr_granted_roles = ui.usr_granted_roles;
	}

	UserId(Firebird::MemoryPool& p)
		: usr_user_name(p),
		  usr_sql_role_name(p),
		  usr_granted_roles(p),
		  usr_trusted_role(p),
		  usr_init_role(p),
		  usr_project_name(p),
		  usr_org_name(p),
		  usr_auth_method(p),
		  usr_auth_block(p)
	{
	}

	UserId(const UserId& ui)
		: usr_user_name(ui.usr_user_name),
		  usr_sql_role_name(ui.usr_sql_role_name),
		  usr_trusted_role(ui.usr_trusted_role),
		  usr_init_role(ui.usr_init_role),
		  usr_project_name(ui.usr_project_name),
		  usr_org_name(ui.usr_org_name),
		  usr_auth_method(ui.usr_auth_method),
		  usr_privileges(ui.usr_privileges),
		  usr_user_id(ui.usr_user_id),
		  usr_group_id(ui.usr_group_id),
		  usr_flags(ui.usr_flags)
	{
		usr_auth_block.assign(ui.usr_auth_block);
		if (!testFlag(USR_newrole))
			usr_granted_roles = ui.usr_granted_roles;
	}

	UserId& operator=(const UserId& ui)
	{
		usr_user_name = ui.usr_user_name;
		usr_sql_role_name = ui.usr_sql_role_name;
		usr_trusted_role = ui.usr_trusted_role;
		usr_init_role = ui.usr_init_role;
		usr_project_name = ui.usr_project_name;
		usr_org_name = ui.usr_org_name;
		usr_privileges = ui.usr_privileges;
		usr_auth_method = ui.usr_auth_method;
		usr_user_id = ui.usr_user_id;
		usr_group_id = ui.usr_group_id;
		usr_flags = ui.usr_flags;
		usr_auth_block.assign(ui.usr_auth_block);

		if (!testFlag(USR_newrole))
			usr_granted_roles = ui.usr_granted_roles;

		return *this;
	}

	void populateDpb(Firebird::ClumpletWriter& dpb, bool embeddedSupport);

	bool locksmith(thread_db* tdbb, ULONG sp) const
	{
		if (testFlag(USR_newrole))
			findGrantedRoles(tdbb);
		return usr_privileges.test(sp);
	}

	void sclInit(thread_db* tdbb, bool create);

	void setUserName(const Firebird::MetaString& userName)
	{
		if (userName != usr_user_name)
		{
			usr_flags |= USR_newrole;
			usr_user_name = userName;
		}
	}

	const Firebird::MetaString& getUserName() const
	{
		return usr_user_name;
	}

	void setTrustedRole(const Firebird::MetaString& roleName)
	{
		usr_trusted_role = roleName;
	}

	const Firebird::MetaString& getTrustedRole() const
	{
		return usr_trusted_role;
	}

	void setSqlRole(const Firebird::MetaString& roleName)
	{
		if (roleName != usr_sql_role_name)
		{
			usr_flags |= USR_newrole;
			usr_sql_role_name = roleName;
		}
	}

	const Firebird::MetaString& getSqlRole() const
	{
		return usr_sql_role_name;
	}

	void setRoleTrusted();

	// Restore initial role, returns true if role was actually changed
	bool resetRole()
	{
		setSqlRole(usr_init_role);
		return (usr_flags & USR_newrole);
	}

	bool roleInUse(thread_db* tdbb, const MetaName& role) const
	{
		if (testFlag(USR_newrole))
			findGrantedRoles(tdbb);
		return usr_granted_roles.exist(role);
	}

	const auto& getGrantedRoles(thread_db* tdbb) const
	{
		if (testFlag(USR_newrole))
			findGrantedRoles(tdbb);
		return usr_granted_roles;
	}

	void makeRoleName(const int dialect)
	{
		makeRoleName(usr_sql_role_name, dialect);
		makeRoleName(usr_trusted_role, dialect);
	}

	bool testFlag(USHORT mask) const
	{
		return usr_flags & mask;
	}

	void setFlag(USHORT mask)
	{
		usr_flags |= mask;
	}

	static void makeRoleName(Firebird::MetaString& role, const int dialect);

private:
	void findGrantedRoles(thread_db* tdbb) const;
};


} //namespace Jrd

#endif // JRD_SCL_H

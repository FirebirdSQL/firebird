/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/***************** gpre version WI-T6.0.0.179-dev Firebird 6.0 Initial **********************/
/*
 *	PROGRAM:	Interactive SQL utility
 *	MODULE:		show.epp
 *	DESCRIPTION:	Display routines
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
 * Revision 1.2  2000/11/19 07:02:49  fsg
 * Change in show.epp to use CHARACTER_LENGTH instead of FIELD_LENGTH in
 * SHOW PROCEDURE
 *
 * 19-May-2001 Claudio Valderrama.
 * Change to be in sync with extract.e: BLOB is not returned
 * by value but by descriptor.
 * 2001.09.21 Claudio Valderrama: Show correct mechanism for UDF parameters.
 * 2001.10.01 Claudio Valderrama: SHOW GRANTS works without any argument, too.
 *   Metadata extraction is slightly faster if SHOW_grants() knows the obj_type.
 *   Keyword USER is written when the grantee is a user and since the engine
 *   now supports GRANT...to ROLE role_name, ROLE is written when the grantee is
 *   indeed a role. When the grantee is a group, it's shown, too.
 * 2003.02.04 Dmitry Yemanov: support for universal triggers
 */

#include "firebird.h"
#include <stdio.h>
#include "../jrd/license.h"
#include <string.h>

#include "../yvalve/gds_proto.h"
#include "ibase.h"
#include "../isql/isql.h"
#include "../jrd/intl.h"
#include "../common/intlobj_new.h"
#include "../common/classes/AlignedBuffer.h"
#include "../common/classes/ClumpletReader.h"
#include "../isql/isql_proto.h"
#include "../isql/show_proto.h"
#include "../isql/iutils_proto.h"
#include "../jrd/obj.h"
#include "../jrd/ods.h"
#include "../isql/extra_proto.h"
#include "../common/utils_proto.h"
#include "../jrd/constants.h"
#include "../common/classes/UserBlob.h"
#include "../common/classes/VaryStr.h"
#include "../common/classes/ImplementHelper.h"
#include "../common/classes/MetaString.h"
#include "../common/TimeZoneUtil.h"
#include "../isql/OptionsBase.h"

#include <firebird/Interface.h>

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

using Firebird::string;
using Firebird::MetaString;
using MsgFormat::SafeArg;


//DATABASE DB = EXTERN COMPILETIME "yachts.lnk";
/*DATABASE DB = EXTERN COMPILETIME "yachts.lnk" RUNTIME isqlGlob.global_Db_name;*/
/**** GDS Preprocessor Definitions ****/
#ifndef JRD_IBASE_H
#include <ibase.h>
#endif
#include <firebird/Interface.h>
#define CAST_CONST_MSG(A) (reinterpret_cast<const unsigned char*>(A))
#define CAST_MSG(A) (reinterpret_cast<unsigned char*>(A))

#ifndef fbBlobNull
static const ISC_QUAD
   fbBlobNull = {0, 0};	/* initializer for blobs */
#endif
#ifndef DB
extern Firebird::IAttachment*
   DB;		/* database handle */

#endif
#ifndef fbTrans
extern Firebird::ITransaction*
   fbTrans;		/* default transaction handle */
#endif
#ifndef fbMaster
extern Firebird::IMaster* fbMaster;		/* master interface */
#endif
#ifndef fbProvider
extern Firebird::IProvider* fbProvider;		/* provider interface */
#endif
#ifndef fbStatus
extern Firebird::CheckStatusWrapper fbStatusObj;	/* status vector */
extern Firebird::CheckStatusWrapper* fbStatus;	/* status vector */
#endif
#ifndef fbStatus2
extern Firebird::CheckStatusWrapper fbStatus2Obj;	/* status vector */
extern Firebird::CheckStatusWrapper* fbStatus2;	/* status vector */
#endif
#ifndef fbIStatus
extern int fbIStatus;	/* last completion code */
#endif
static Firebird::IRequest*
   fb_0 = 0;		/* request handle */

static const unsigned
   fb_1l = 247;
static const unsigned char
   fb_1 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 15,0, 
	    blr_quad, 0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_int64, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 12,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_or, 
			      blr_eql, 
				 blr_fid, 0, 1,0, 
				 blr_parameter, 0, 0,0, 
			      blr_eql, 
				 blr_fid, 0, 0,0, 
				 blr_parameter, 0, 1,0, 
			   blr_or, 
			      blr_eql, 
				 blr_fid, 0, 8,0, 
				 blr_literal, blr_long, 0, 0,0,0,0,
			      blr_missing, 
				 blr_fid, 0, 8,0, 
		     blr_sort, 4, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
			blr_ascending, 
			   blr_fid, 0, 2,0, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter2, 1, 0,0, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 13,0, 
			   blr_parameter2, 1, 1,0, 6,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter, 1, 7,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 11,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 12,0, 10,0, 
			blr_assignment, 
			   blr_fid, 0, 14,0, 
			   blr_parameter2, 1, 13,0, 9,0, 
			blr_assignment, 
			   blr_fid, 0, 12,0, 
			   blr_parameter2, 1, 14,0, 5,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 3,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1 */

static Firebird::IRequest*
   fb_21 = 0;		/* request handle */

static const unsigned
   fb_22l = 149;
static const unsigned char
   fb_22 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 7,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_rid, 12,0, 0, 
		  blr_rid, 6,0, 1, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 8,0, 
			blr_eql, 
			   blr_fid, 0, 1,0, 
			   blr_fid, 1, 8,0, 
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 1,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 10,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter2, 0, 6,0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_22 */

static Firebird::IRequest*
   fb_31 = 0;		/* request handle */

static const unsigned
   fb_32l = 120;
static const unsigned char
   fb_32 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 5,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 12,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 8,0, 
			blr_missing, 
			   blr_fid, 0, 1,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 10,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_32 */

static Firebird::IRequest*
   fb_39 = 0;		/* request handle */

static const unsigned
   fb_40l = 125;
static const unsigned char
   fb_40 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 22,0, 0, 
		     blr_rid, 24,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 2,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 1,0, 
				 blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
			      blr_eql, 
				 blr_fid, 0, 0,0, 
				 blr_fid, 1, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_40 */

static Firebird::IRequest*
   fb_47 = 0;		/* request handle */

static const unsigned
   fb_48l = 96;
static const unsigned char
   fb_48 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 4,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter2, 1, 2,0, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_48 */

static Firebird::IRequest*
   fb_56 = 0;		/* request handle */

static const unsigned
   fb_57l = 143;
static const unsigned char
   fb_57 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 7,0, 
	    blr_cstring2, 0,0, 12,0, 
	    blr_cstring2, 0,0, 12,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 22,0, 0, 
		     blr_rid, 23,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_fid, 1, 1,0, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 4,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 1, 3,0, 
			   blr_parameter2, 1, 1,0, 4,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 6,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_57 */

static Firebird::IRequest*
   fb_68 = 0;		/* request handle */

static const unsigned
   fb_69l = 119;
static const unsigned char
   fb_69 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_cstring2, 0,0, 12,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 22,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 0, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_69 */

static Firebird::IRequest*
   fb_77 = 0;		/* request handle */

static const unsigned
   fb_78l = 91;
static const unsigned char
   fb_78 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_parameter, 0, 0,0, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_78 */

static Firebird::IRequest*
   fb_85 = 0;		/* request handle */

static const unsigned
   fb_86l = 385;
static const unsigned char
   fb_86 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 34,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 5,0, 0, 
		     blr_rid, 2,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 2,0, 
			      blr_fid, 1, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 6,0, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 3,0, 
			   blr_parameter2, 1, 0,0, 6,0, 
			blr_assignment, 
			   blr_fid, 1, 7,0, 
			   blr_parameter2, 1, 1,0, 7,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter2, 1, 2,0, 8,0, 
			blr_assignment, 
			   blr_fid, 1, 5,0, 
			   blr_parameter2, 1, 3,0, 25,0, 
			blr_assignment, 
			   blr_fid, 1, 4,0, 
			   blr_parameter2, 1, 4,0, 26,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 20,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 1, 23,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 0, 16,0, 
			   blr_parameter, 1, 13,0, 
			blr_assignment, 
			   blr_fid, 1, 17,0, 
			   blr_parameter, 1, 14,0, 
			blr_assignment, 
			   blr_fid, 1, 25,0, 
			   blr_parameter2, 1, 16,0, 15,0, 
			blr_assignment, 
			   blr_fid, 0, 18,0, 
			   blr_parameter2, 1, 18,0, 17,0, 
			blr_assignment, 
			   blr_fid, 1, 26,0, 
			   blr_parameter2, 1, 20,0, 19,0, 
			blr_assignment, 
			   blr_fid, 1, 9,0, 
			   blr_parameter, 1, 21,0, 
			blr_assignment, 
			   blr_fid, 1, 27,0, 
			   blr_parameter, 1, 22,0, 
			blr_assignment, 
			   blr_fid, 1, 11,0, 
			   blr_parameter, 1, 23,0, 
			blr_assignment, 
			   blr_fid, 1, 10,0, 
			   blr_parameter, 1, 24,0, 
			blr_assignment, 
			   blr_fid, 1, 22,0, 
			   blr_parameter2, 1, 28,0, 27,0, 
			blr_assignment, 
			   blr_fid, 1, 15,0, 
			   blr_parameter, 1, 29,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 30,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 31,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter2, 1, 32,0, 11,0, 
			blr_assignment, 
			   blr_fid, 0, 19,0, 
			   blr_parameter2, 1, 33,0, 10,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 5,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_86 */

static Firebird::IRequest*
   fb_124 = 0;		/* request handle */

static const unsigned
   fb_125l = 116;
static const unsigned char
   fb_125 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 7,0, 
	    blr_quad, 0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_bool, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter2, 1, 1,0, 4,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter2, 1, 6,0, 5,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_125 */

static Firebird::IRequest*
   fb_136 = 0;		/* request handle */

static const unsigned
   fb_137l = 106;
static const unsigned char
   fb_137 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 30,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 5,0, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_137 */

static Firebird::IRequest*
   fb_144 = 0;		/* request handle */

static const unsigned
   fb_145l = 106;
static const unsigned char
   fb_145 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 20,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 4,0, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_145 */

static Firebird::IRequest*
   fb_152 = 0;		/* request handle */

static const unsigned
   fb_153l = 106;
static const unsigned char
   fb_153 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 42,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 4,0, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_153 */

static Firebird::IRequest*
   fb_160 = 0;		/* request handle */

static const unsigned
   fb_161l = 112;
static const unsigned char
   fb_161 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 14,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 16,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 0,0, 
				 blr_parameter, 0, 0,0, 
			      blr_missing, 
				 blr_fid, 0, 9,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_161 */

static Firebird::IRequest*
   fb_168 = 0;		/* request handle */

static const unsigned
   fb_169l = 112;
static const unsigned char
   fb_169 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 26,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 7,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 0,0, 
				 blr_parameter, 0, 0,0, 
			      blr_missing, 
				 blr_fid, 0, 16,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_169 */

static Firebird::IRequest*
   fb_176 = 0;		/* request handle */

static const unsigned
   fb_177l = 125;
static const unsigned char
   fb_177 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 5,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 14,0, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 6,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_177 */

static Firebird::IRequest*
   fb_185 = 0;		/* request handle */

static const unsigned
   fb_186l = 121;
static const unsigned char
   fb_186 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 6,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 14,0, 
			      blr_fid, 1, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter2, 1, 1,0, 3,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_186 */

static Firebird::IRequest*
   fb_195 = 0;		/* request handle */

static const unsigned
   fb_196l = 121;
static const unsigned char
   fb_196 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 6,0, 0, 
		     blr_rid, 9,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 9,0, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter2, 1, 1,0, 3,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_196 */

static Firebird::IRequest*
   fb_205 = 0;		/* request handle */

static const unsigned
   fb_206l = 87;
static const unsigned char
   fb_206 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_rid, 1,0, 0, 
		  blr_rid, 9,0, 1, 
		  blr_boolean, 
		     blr_eql, 
			blr_fid, 1, 0,0, 
			blr_fid, 0, 2,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 1, 1,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 1, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_206 */

static Firebird::IRequest*
   fb_211 = 0;		/* request handle */

static const unsigned
   fb_212l = 147;
static const unsigned char
   fb_212 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 18,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 7,0, 
			      blr_parameter, 0, 2,0, 
			   blr_and, 
			      blr_or, 
				 blr_eql, 
				    blr_fid, 0, 6,0, 
				    blr_parameter, 0, 1,0, 
				 blr_eql, 
				    blr_fid, 0, 6,0, 
				    blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 4,0, 
				    blr_parameter, 0, 3,0, 
				 blr_eql, 
				    blr_fid, 0, 2,0, 
				    blr_literal, blr_text, 1,0, 'M',
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_212 */

static Firebird::IRequest*
   fb_222 = 0;		/* request handle */

static const unsigned
   fb_223l = 89;
static const unsigned char
   fb_223 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 31,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_223 */

static Firebird::IRequest*
   fb_229 = 0;		/* request handle */

static const unsigned
   fb_230l = 91;
static const unsigned char
   fb_230 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 31,0, 0, 
		  blr_boolean, 
		     blr_not, 
			blr_missing, 
			   blr_fid, 0, 0,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_230 */

static Firebird::IRequest*
   fb_236 = 0;		/* request handle */

static const unsigned
   fb_237l = 126;
static const unsigned char
   fb_237 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 11,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_literal, blr_text, 21,0, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E','S',
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_add, 
				 blr_multiply, 
				    blr_parameter, 0, 1,0, 
				    blr_literal, blr_long, 0, 8,0,0,0,
				 blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_237 */

static Firebird::IRequest*
   fb_244 = 0;		/* request handle */

static const unsigned
   fb_245l = 87;
static const unsigned char
   fb_245 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_text2, 1,0, 8,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 31,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_245 */

static Firebird::IRequest*
   fb_252 = 0;		/* request handle */

static const unsigned
   fb_253l = 122;
static const unsigned char
   fb_253 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 52,0, 0, 
		     blr_rid, 51,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
		     blr_sort, 2, 
			blr_descending, 
			   blr_fid, 1, 2,0, 
			blr_descending, 
			   blr_fid, 1, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 1, 3,0, 
			   blr_parameter2, 1, 2,0, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_253 */

static Firebird::IRequest*
   fb_261 = 0;		/* request handle */

static const unsigned
   fb_262l = 84;
static const unsigned char
   fb_262 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 51,0, 0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_262 */

static Firebird::IRequest*
   fb_268 = 0;		/* request handle */

static const unsigned
   fb_269l = 111;
static const unsigned char
   fb_269 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 51,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter2, 1, 2,0, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter2, 1, 4,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_269 */

static Firebird::IRequest*
   fb_279 = 0;		/* request handle */

static const unsigned
   fb_280l = 97;
static const unsigned char
   fb_280 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 5,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 18,0, 
			   blr_parameter2, 1, 2,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_280 */

static Firebird::IRequest*
   fb_288 = 0;		/* request handle */

static const unsigned
   fb_289l = 346;
static const unsigned char
   fb_289 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 27,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 27,0, 0, 
		     blr_rid, 2,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_equiv, 
				 blr_fid, 0, 14,0, 
				 blr_value_if, 
				    blr_eql, 
				       blr_parameter, 0, 1,0, 
				       blr_literal, blr_text, 0,0, 
				    blr_null, 
				    blr_parameter, 0, 1,0, 
			      blr_eql, 
				 blr_fid, 0, 4,0, 
				 blr_fid, 1, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
			blr_ascending, 
			   blr_fid, 0, 2,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 7,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 8,0, 
			   blr_parameter2, 1, 1,0, 19,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 26,0, 
			   blr_parameter2, 1, 5,0, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 24,0, 
			   blr_parameter2, 1, 7,0, 6,0, 
			blr_assignment, 
			   blr_fid, 1, 9,0, 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_fid, 1, 27,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 1, 11,0, 
			   blr_parameter, 1, 10,0, 
			blr_assignment, 
			   blr_fid, 1, 10,0, 
			   blr_parameter, 1, 11,0, 
			blr_assignment, 
			   blr_fid, 1, 15,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 1, 25,0, 
			   blr_parameter2, 1, 14,0, 13,0, 
			blr_assignment, 
			   blr_fid, 0, 9,0, 
			   blr_parameter2, 1, 18,0, 17,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter2, 1, 21,0, 20,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 22,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 23,0, 
			blr_assignment, 
			   blr_fid, 0, 13,0, 
			   blr_parameter2, 1, 24,0, 16,0, 
			blr_assignment, 
			   blr_fid, 0, 12,0, 
			   blr_parameter2, 1, 25,0, 15,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 26,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_289 */

static Firebird::IRequest*
   fb_321 = 0;		/* request handle */

static const unsigned
   fb_322l = 168;
static const unsigned char
   fb_322 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 10,0, 
	    blr_quad, 0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 26,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_equiv, 
			      blr_fid, 0, 16,0, 
			      blr_value_if, 
				 blr_eql, 
				    blr_parameter, 0, 1,0, 
				    blr_literal, blr_text, 0,0, 
				 blr_null, 
				 blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 15,0, 
			   blr_parameter2, 1, 1,0, 6,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 18,0, 
			   blr_parameter2, 1, 7,0, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 14,0, 
			   blr_parameter2, 1, 8,0, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 9,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_322 */

static Firebird::IRequest*
   fb_337 = 0;		/* request handle */

static const unsigned
   fb_338l = 114;
static const unsigned char
   fb_338 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 13,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_parameter, 0, 0,0, 
			   blr_fid, 0, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 4,0, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
		     blr_project, 2, 
			blr_fid, 0, 4,0, 
			blr_fid, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_338 */

static Firebird::IRequest*
   fb_345 = 0;		/* request handle */

static const unsigned
   fb_346l = 120;
static const unsigned char
   fb_346 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 8,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 26,0, 0, 
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 16,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 12,0, 
			blr_parameter2, 0, 3,0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 10,0, 
			blr_parameter2, 0, 5,0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 16,0, 
			blr_parameter2, 0, 6,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 7,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_346 */

static Firebird::IRequest*
   fb_356 = 0;		/* request handle */

static const unsigned
   fb_357l = 164;
static const unsigned char
   fb_357 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 12,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 5,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,3, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 46,0, 0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 7,0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter2, 0, 8,0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter, 0, 9,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 10,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 11,0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_357 */

static Firebird::IRequest*
   fb_371 = 0;		/* request handle */

static const unsigned
   fb_372l = 183;
static const unsigned char
   fb_372 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 12,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 5,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,3, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 45,0, 0, 
		  blr_boolean, 
		     blr_or, 
			blr_neq, 
			   blr_fid, 0, 8,0, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			blr_missing, 
			   blr_fid, 0, 8,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 7,0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter2, 0, 8,0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter, 0, 9,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 10,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 11,0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_372 */

static Firebird::IRequest*
   fb_386 = 0;		/* request handle */

static const unsigned
   fb_387l = 140;
static const unsigned char
   fb_387 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 10,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 42,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter2, 1, 0,0, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 1,0, 6,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter2, 1, 4,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_fid, 0, 8,0, 
			   blr_parameter2, 1, 9,0, 7,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_387 */

static Firebird::IRequest*
   fb_401 = 0;		/* request handle */

static const unsigned
   fb_402l = 84;
static const unsigned char
   fb_402 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 42,0, 0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_402 */

static Firebird::IRequest*
   fb_408 = 0;		/* request handle */

static const unsigned
   fb_409l = 87;
static const unsigned char
   fb_409 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 4,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 14,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 14,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_409 */

static Firebird::IRequest*
   fb_415 = 0;		/* request handle */

static const unsigned
   fb_416l = 182;
static const unsigned char
   fb_416 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 10,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_rid, 4,0, 0, 
		  blr_rid, 6,0, 1, 
		  blr_boolean, 
		     blr_and, 
			blr_eql, 
			   blr_fid, 1, 8,0, 
			   blr_fid, 0, 1,0, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 1, 4,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 1, 4,0, 
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 1,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 11,0, 
			blr_parameter2, 0, 0,0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 10,0, 
			blr_parameter2, 0, 1,0, 4,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter, 0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 8,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 9,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_416 */

static Firebird::IRequest*
   fb_428 = 0;		/* request handle */

static const unsigned
   fb_429l = 87;
static const unsigned char
   fb_429 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 4,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 14,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 14,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_429 */

static Firebird::IRequest*
   fb_435 = 0;		/* request handle */

static const unsigned
   fb_436l = 180;
static const unsigned char
   fb_436 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 11,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 4,0, 0, 
		     blr_boolean, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 1,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter2, 1, 1,0, 4,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 6,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter2, 1, 8,0, 7,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 10,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_436 */

static Firebird::IRequest*
   fb_452 = 0;		/* request handle */

static const unsigned
   fb_453l = 95;
static const unsigned char
   fb_453 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_int64, 0, 
	    blr_long, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 20,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_453 */

static Firebird::IRequest*
   fb_461 = 0;		/* request handle */

static const unsigned
   fb_462l = 84;
static const unsigned char
   fb_462 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 20,0, 0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_462 */

static Firebird::IRequest*
   fb_468 = 0;		/* request handle */

static const unsigned
   fb_469l = 124;
static const unsigned char
   fb_469 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 15,0, 0, 
		     blr_rid, 28,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 4,0, 
			      blr_fid, 0, 7,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 0,0, 
				 blr_parameter, 0, 1,0, 
			      blr_and, 
				 blr_missing, 
				    blr_fid, 0, 10,0, 
				 blr_eql, 
				    blr_fid, 0, 1,0, 
				    blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 1, 8,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_469 */

static Firebird::IRequest*
   fb_477 = 0;		/* request handle */

static const unsigned
   fb_478l = 253;
static const unsigned char
   fb_478 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 14,0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 14,0, 0, 
		     blr_rid, 15,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_fid, 1, 0,0, 
			   blr_and, 
			      blr_missing, 
				 blr_fid, 1, 10,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 0,0, 
				    blr_parameter, 0, 0,0, 
				 blr_and, 
				    blr_missing, 
				       blr_fid, 0, 9,0, 
				    blr_not, 
				       blr_missing, 
					  blr_fid, 0, 4,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 1, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 5,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 1, 4,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 6,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 1, 8,0, 
			   blr_parameter2, 1, 7,0, 6,0, 
			blr_assignment, 
			   blr_fid, 1, 3,0, 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 10,0, 
			blr_assignment, 
			   blr_fid, 1, 2,0, 
			   blr_parameter, 1, 11,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 13,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_478 */

static Firebird::IRequest*
   fb_496 = 0;		/* request handle */

static const unsigned
   fb_497l = 97;
static const unsigned char
   fb_497 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 5,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 18,0, 
			   blr_parameter2, 1, 2,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_497 */

static Firebird::IRequest*
   fb_505 = 0;		/* request handle */

static const unsigned
   fb_506l = 341;
static const unsigned char
   fb_506 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 27,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 15,0, 0, 
		     blr_rid, 2,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_parameter, 0, 0,0, 
			      blr_fid, 0, 0,0, 
			   blr_and, 
			      blr_equiv, 
				 blr_fid, 0, 10,0, 
				 blr_value_if, 
				    blr_eql, 
				       blr_parameter, 0, 1,0, 
				       blr_literal, blr_text, 0,0, 
				    blr_null, 
				    blr_parameter, 0, 1,0, 
			      blr_eql, 
				 blr_fid, 0, 12,0, 
				 blr_fid, 1, 0,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 7,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 14,0, 
			   blr_parameter2, 1, 1,0, 19,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 25,0, 
			   blr_parameter2, 1, 5,0, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 15,0, 
			   blr_parameter2, 1, 7,0, 6,0, 
			blr_assignment, 
			   blr_fid, 1, 26,0, 
			   blr_parameter2, 1, 9,0, 8,0, 
			blr_assignment, 
			   blr_fid, 1, 24,0, 
			   blr_parameter2, 1, 11,0, 10,0, 
			blr_assignment, 
			   blr_fid, 1, 9,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 1, 27,0, 
			   blr_parameter, 1, 13,0, 
			blr_assignment, 
			   blr_fid, 1, 11,0, 
			   blr_parameter, 1, 14,0, 
			blr_assignment, 
			   blr_fid, 1, 10,0, 
			   blr_parameter, 1, 15,0, 
			blr_assignment, 
			   blr_fid, 1, 15,0, 
			   blr_parameter, 1, 16,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter2, 1, 21,0, 20,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 22,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 23,0, 
			blr_assignment, 
			   blr_fid, 0, 19,0, 
			   blr_parameter2, 1, 24,0, 18,0, 
			blr_assignment, 
			   blr_fid, 0, 18,0, 
			   blr_parameter2, 1, 25,0, 17,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 26,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_506 */

static Firebird::IRequest*
   fb_538 = 0;		/* request handle */

static const unsigned
   fb_539l = 194;
static const unsigned char
   fb_539 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 13,0, 
	    blr_quad, 0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 14,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_equiv, 
			      blr_fid, 0, 9,0, 
			      blr_value_if, 
				 blr_eql, 
				    blr_parameter, 0, 1,0, 
				    blr_literal, blr_text, 0,0, 
				 blr_null, 
				 blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter2, 1, 0,0, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter2, 1, 1,0, 7,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 19,0, 
			   blr_parameter2, 1, 9,0, 8,0, 
			blr_assignment, 
			   blr_fid, 0, 20,0, 
			   blr_parameter2, 1, 10,0, 6,0, 
			blr_assignment, 
			   blr_fid, 0, 8,0, 
			   blr_parameter2, 1, 11,0, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 12,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_539 */

static Firebird::IRequest*
   fb_557 = 0;		/* request handle */

static const unsigned
   fb_558l = 111;
static const unsigned char
   fb_558 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 14,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_equiv, 
			      blr_fid, 0, 9,0, 
			      blr_value_if, 
				 blr_eql, 
				    blr_parameter, 0, 1,0, 
				    blr_literal, blr_text, 0,0, 
				 blr_null, 
				 blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_558 */

static Firebird::IRequest*
   fb_566 = 0;		/* request handle */

static const unsigned
   fb_567l = 114;
static const unsigned char
   fb_567 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 13,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_parameter, 0, 0,0, 
			   blr_fid, 0, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 4,0, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
		     blr_project, 2, 
			blr_fid, 0, 4,0, 
			blr_fid, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_567 */

static Firebird::IRequest*
   fb_574 = 0;		/* request handle */

static const unsigned
   fb_575l = 105;
static const unsigned char
   fb_575 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 14,0, 0, 
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 9,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 14,0, 
			blr_parameter2, 0, 3,0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter2, 0, 4,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_575 */

static Firebird::IRequest*
   fb_583 = 0;		/* request handle */

static const unsigned
   fb_584l = 99;
static const unsigned char
   fb_584 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 14,0, 0, 
		  blr_boolean, 
		     blr_eql, 
			blr_fid, 0, 7,0, 
			blr_literal, blr_long, 0, 1,0,0,0,
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 9,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_584 */

static Firebird::IRequest*
   fb_589 = 0;		/* request handle */

static const unsigned
   fb_590l = 131;
static const unsigned char
   fb_590 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 16,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_590 */

static Firebird::IRequest*
   fb_600 = 0;		/* request handle */

static const unsigned
   fb_601l = 69;
static const unsigned char
   fb_601 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 16,0, 0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_601 */

static Firebird::IRequest*
   fb_605 = 0;		/* request handle */

static const unsigned
   fb_606l = 116;
static const unsigned char
   fb_606 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 13,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_parameter, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 1,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_606 */

static Firebird::IRequest*
   fb_614 = 0;		/* request handle */

static const unsigned
   fb_615l = 87;
static const unsigned char
   fb_615 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_cstring2, 0,0, 0,4, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 30,0, 0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 0,0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_615 */

static Firebird::IRequest*
   fb_621 = 0;		/* request handle */

static const unsigned
   fb_622l = 207;
static const unsigned char
   fb_622 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 16,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 2,0, 0, 
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 23,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 25,0, 
			   blr_parameter2, 1, 5,0, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 26,0, 
			   blr_parameter2, 1, 7,0, 6,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_fid, 0, 9,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 0, 27,0, 
			   blr_parameter, 1, 10,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 11,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 0, 22,0, 
			   blr_parameter2, 1, 14,0, 13,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 15,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_622 */

static Firebird::IRequest*
   fb_642 = 0;		/* request handle */

static const unsigned
   fb_643l = 117;
static const unsigned char
   fb_643 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 2,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_matching2, 
			      blr_fid, 0, 0,0, 
			      blr_literal, blr_text, 5,0, 'R','D','B','$',43,
			      blr_literal, blr_text, 15,0, 43,61,91,'0',45,'9',93,91,'0',45,'9',93,42,32,42,
			blr_neq, 
			   blr_fid, 0, 15,0, 
			   blr_literal, blr_long, 0, 1,0,0,0,
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_643 */

static Firebird::IRequest*
   fb_647 = 0;		/* request handle */

static const unsigned
   fb_648l = 194;
static const unsigned char
   fb_648 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 8,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 3, 
		     blr_rid, 27,0, 0, 
		     blr_rid, 2,0, 1, 
		     blr_rid, 13,0, 2, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 4,0, 
				 blr_fid, 1, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 2, 0,0, 
				    blr_fid, 0, 4,0, 
				 blr_eql, 
				    blr_fid, 2, 3,0, 
				    blr_parameter, 0, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
			blr_ascending, 
			   blr_fid, 0, 2,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 4,0, 
			   blr_parameter2, 1, 0,0, 4,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 2, 4,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 6,0, 
			blr_assignment, 
			   blr_fid, 2, 1,0, 
			   blr_parameter, 1, 7,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_648 */

static Firebird::IRequest*
   fb_661 = 0;		/* request handle */

static const unsigned
   fb_662l = 178;
static const unsigned char
   fb_662 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 7,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 3, 
		     blr_rid, 5,0, 0, 
		     blr_rid, 2,0, 1, 
		     blr_rid, 13,0, 2, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 2,0, 
				 blr_fid, 1, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 2, 0,0, 
				    blr_fid, 0, 2,0, 
				 blr_eql, 
				    blr_fid, 2, 3,0, 
				    blr_parameter, 0, 0,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 6,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 4,0, 
			   blr_parameter2, 1, 0,0, 3,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 2, 4,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 2, 1,0, 
			   blr_parameter, 1, 6,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_662 */

static Firebird::IRequest*
   fb_674 = 0;		/* request handle */

static const unsigned
   fb_675l = 94;
static const unsigned char
   fb_675 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_missing, 
				 blr_fid, 0, 0,0, 
			      blr_or, 
				 blr_missing, 
				    blr_fid, 0, 5,0, 
				 blr_eql, 
				    blr_fid, 0, 5,0, 
				    blr_literal, blr_long, 0, 8,0,0,0,
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_675 */

static Firebird::IRequest*
   fb_680 = 0;		/* request handle */

static const unsigned
   fb_681l = 120;
static const unsigned char
   fb_681 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 13,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 1,0, 
			   blr_eql, 
			      blr_fid, 0, 3,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter2, 1, 4,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_681 */

static Firebird::IRequest*
   fb_691 = 0;		/* request handle */

static const unsigned
   fb_692l = 120;
static const unsigned char
   fb_692 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 13,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 1,0, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter2, 1, 4,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_692 */

static Firebird::IRequest*
   fb_702 = 0;		/* request handle */

static const unsigned
   fb_703l = 63;
static const unsigned char
   fb_703 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 51,0, 0, 
		  blr_boolean, 
		     blr_gtr, 
			blr_fid, 0, 3,0, 
			blr_literal, blr_long, 0, 0,0,0,0,
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_703 */

static Firebird::IRequest*
   fb_706 = 0;		/* request handle */

static const unsigned
   fb_707l = 79;
static const unsigned char
   fb_707 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 5,0, 
	    blr_long, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_bool, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 1,0, 0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter2, 0, 0,0, 3,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter2, 0, 4,0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_707 */

static Firebird::IRequest*
   fb_714 = 0;		/* request handle */

static const unsigned
   fb_715l = 92;
static const unsigned char
   fb_715 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_rid, 1,0, 0, 
		  blr_rid, 28,0, 1, 
		  blr_boolean, 
		     blr_eql, 
			blr_fid, 1, 0,0, 
			blr_fid, 0, 3,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 1, 3,0, 
			blr_parameter2, 0, 3,0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_715 */

static Firebird::IRequest*
   fb_721 = 0;		/* request handle */

static const unsigned
   fb_722l = 151;
static const unsigned char
   fb_722 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 12,0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_long, 0, 
	    blr_long, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 10,0, 0, 
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 5,0, 
		     blr_ascending, 
			blr_fid, 0, 1,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter2, 0, 0,0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 1,0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter2, 0, 2,0, 9,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter2, 0, 8,0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter2, 0, 11,0, 10,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 3,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_722 */

static Firebird::IRequest*
   fb_736 = 0;		/* request handle */

static const unsigned
   fb_737l = 88;
static const unsigned char
   fb_737 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 6,0, 0, 
		  blr_boolean, 
		     blr_eql, 
			blr_fid, 0, 8,0, 
			blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 13,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_737 */

static Firebird::IRequest*
   fb_742 = 0;		/* request handle */

static const unsigned
   fb_743l = 87;
static const unsigned char
   fb_743 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 46,0, 0, 
		  blr_boolean, 
		     blr_not, 
			blr_missing, 
			   blr_fid, 0, 8,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_743 */

static Firebird::IRequest*
   fb_748 = 0;		/* request handle */

static const unsigned
   fb_749l = 106;
static const unsigned char
   fb_749 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 45,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 9,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 8,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_749 */

static Firebird::IRequest*
   fb_754 = 0;		/* request handle */

static const unsigned
   fb_755l = 106;
static const unsigned char
   fb_755 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 42,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 7,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 6,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 6,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_755 */

static Firebird::IRequest*
   fb_760 = 0;		/* request handle */

static const unsigned
   fb_761l = 106;
static const unsigned char
   fb_761 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 29,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 5,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_761 */

static Firebird::IRequest*
   fb_766 = 0;		/* request handle */

static const unsigned
   fb_767l = 106;
static const unsigned char
   fb_767 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 28,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 6,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 5,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 5,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_767 */

static Firebird::IRequest*
   fb_772 = 0;		/* request handle */

static const unsigned
   fb_773l = 106;
static const unsigned char
   fb_773 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 42,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 7,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 6,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 6,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_773 */

static Firebird::IRequest*
   fb_778 = 0;		/* request handle */

static const unsigned
   fb_779l = 106;
static const unsigned char
   fb_779 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 31,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 2,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 3,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 3,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_779 */

static Firebird::IRequest*
   fb_784 = 0;		/* request handle */

static const unsigned
   fb_785l = 106;
static const unsigned char
   fb_785 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 4,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 4,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 9,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 9,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_785 */

static Firebird::IRequest*
   fb_790 = 0;		/* request handle */

static const unsigned
   fb_791l = 106;
static const unsigned char
   fb_791 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 20,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 3,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 2,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 2,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_791 */

static Firebird::IRequest*
   fb_796 = 0;		/* request handle */

static const unsigned
   fb_797l = 106;
static const unsigned char
   fb_797 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 30,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 3,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_797 */

static Firebird::IRequest*
   fb_802 = 0;		/* request handle */

static const unsigned
   fb_803l = 106;
static const unsigned char
   fb_803 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 16,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 1,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 6,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 6,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_803 */

static Firebird::IRequest*
   fb_808 = 0;		/* request handle */

static const unsigned
   fb_809l = 158;
static const unsigned char
   fb_809 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 15,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_equiv, 
				 blr_fid, 0, 10,0, 
				 blr_value_if, 
				    blr_eql, 
				       blr_parameter, 0, 1,0, 
				       blr_literal, blr_text, 0,0, 
				    blr_null, 
				    blr_parameter, 0, 1,0, 
			      blr_not, 
				 blr_missing, 
				    blr_fid, 0, 21,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 21,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter2, 1, 3,0, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 5,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_809 */

static Firebird::IRequest*
   fb_820 = 0;		/* request handle */

static const unsigned
   fb_821l = 122;
static const unsigned char
   fb_821 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 5,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 14,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 3,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 7,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 7,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter2, 0, 3,0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_821 */

static Firebird::IRequest*
   fb_828 = 0;		/* request handle */

static const unsigned
   fb_829l = 106;
static const unsigned char
   fb_829 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 12,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 6,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 8,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 8,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_829 */

static Firebird::IRequest*
   fb_834 = 0;		/* request handle */

static const unsigned
   fb_835l = 135;
static const unsigned char
   fb_835 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 27,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_equiv, 
				 blr_fid, 0, 14,0, 
				 blr_value_if, 
				    blr_eql, 
				       blr_parameter, 0, 1,0, 
				       blr_literal, blr_text, 0,0, 
				    blr_null, 
				    blr_parameter, 0, 1,0, 
			      blr_not, 
				 blr_missing, 
				    blr_fid, 0, 5,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
			blr_ascending, 
			   blr_fid, 0, 2,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_835 */

static Firebird::IRequest*
   fb_843 = 0;		/* request handle */

static const unsigned
   fb_844l = 119;
static const unsigned char
   fb_844 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 6,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 26,0, 0, 
		  blr_boolean, 
		     blr_or, 
			blr_eql, 
			   blr_fid, 0, 10,0, 
			   blr_literal, blr_long, 0, 0,0,0,0,
			blr_missing, 
			   blr_fid, 0, 10,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter2, 0, 0,0, 3,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 16,0, 
			blr_parameter2, 0, 4,0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_844 */

static Firebird::IRequest*
   fb_852 = 0;		/* request handle */

static const unsigned
   fb_853l = 106;
static const unsigned char
   fb_853 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 5,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 11,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 6,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_853 */

static Firebird::IRequest*
   fb_860 = 0;		/* request handle */

static const unsigned
   fb_861l = 110;
static const unsigned char
   fb_861 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 6,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 0,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 8,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 0,0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_861 */

static Firebird::IRequest*
   fb_867 = 0;		/* request handle */

static const unsigned
   fb_868l = 106;
static const unsigned char
   fb_868 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 5,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 11,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 6,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_868 */

static Firebird::IRequest*
   fb_875 = 0;		/* request handle */

static const unsigned
   fb_876l = 109;
static const unsigned char
   fb_876 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 4,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 6,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_missing, 
			   blr_fid, 0, 0,0, 
			blr_or, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 0,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 8,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter2, 0, 0,0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_876 */

static Firebird::IRequest*
   fb_882 = 0;		/* request handle */

static const unsigned
   fb_883l = 141;
static const unsigned char
   fb_883 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 2,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_matching2, 
			      blr_fid, 0, 0,0, 
			      blr_literal, blr_text, 5,0, 'R','D','B','$',43,
			      blr_literal, blr_text, 15,0, 43,61,91,'0',45,'9',93,91,'0',45,'9',93,42,32,42,
			blr_and, 
			   blr_or, 
			      blr_eql, 
				 blr_fid, 0, 15,0, 
				 blr_literal, blr_long, 0, 0,0,0,0,
			      blr_missing, 
				 blr_fid, 0, 15,0, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 14,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 14,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_883 */

static Firebird::IRequest*
   fb_888 = 0;		/* request handle */

static const unsigned
   fb_889l = 76;
static const unsigned char
   fb_889 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 1,0, 0, 
		  blr_first, 
		     blr_literal, blr_long, 0, 1,0,0,0,
		  blr_boolean, 
		     blr_not, 
			blr_missing, 
			   blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_889 */

static Firebird::IRequest*
   fb_893 = 0;		/* request handle */

static const unsigned
   fb_894l = 156;
static const unsigned char
   fb_894 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 11,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_rid, 29,0, 0, 
		  blr_rid, 28,0, 1, 
		  blr_boolean, 
		     blr_eql, 
			blr_fid, 1, 4,0, 
			blr_fid, 0, 2,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter2, 0, 0,0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter2, 0, 4,0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter2, 0, 7,0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 8,0, 
		     blr_assignment, 
			blr_fid, 1, 0,0, 
			blr_parameter, 0, 9,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 10,0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_894 */

static Firebird::IRequest*
   fb_907 = 0;		/* request handle */

static const unsigned
   fb_908l = 142;
static const unsigned char
   fb_908 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 12,0, 0, 
		     blr_rid, 24,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 3,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 0,0, 
				 blr_fid, 1, 1,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 8,0, 
				    blr_parameter, 0, 0,0, 
				 blr_eql, 
				    blr_fid, 0, 1,0, 
				    blr_parameter, 0, 1,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 1, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_908 */

static Firebird::IRequest*
   fb_917 = 0;		/* request handle */

static const unsigned
   fb_918l = 130;
static const unsigned char
   fb_918 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 4,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 29,0, 0, 
		     blr_rid, 28,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 2,0, 
			      blr_fid, 1, 4,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 1,0, 
				 blr_parameter, 0, 1,0, 
			      blr_eql, 
				 blr_fid, 1, 4,0, 
				 blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 3,0, 
			   blr_parameter, 1, 3,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_918 */

static Firebird::IRequest*
   fb_927 = 0;		/* request handle */

static const unsigned
   fb_928l = 93;
static const unsigned char
   fb_928 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 4,0, 
			      blr_parameter, 0, 0,0, 
			   blr_missing, 
			      blr_fid, 0, 0,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 8,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_928 */

static Firebird::IRequest*
   fb_934 = 0;		/* request handle */

static const unsigned
   fb_935l = 76;
static const unsigned char
   fb_935 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 6,0, 0, 
		  blr_boolean, 
		     blr_not, 
			blr_missing, 
			   blr_fid, 0, 0,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 8,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_935 */

static Firebird::IRequest*
   fb_939 = 0;		/* request handle */

static const unsigned
   fb_940l = 77;
static const unsigned char
   fb_940 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_940 */

static Firebird::IRequest*
   fb_945 = 0;		/* request handle */

static const unsigned
   fb_946l = 77;
static const unsigned char
   fb_946 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 42,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_946 */

static Firebird::IRequest*
   fb_951 = 0;		/* request handle */

static const unsigned
   fb_952l = 77;
static const unsigned char
   fb_952 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 29,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_952 */

static Firebird::IRequest*
   fb_957 = 0;		/* request handle */

static const unsigned
   fb_958l = 77;
static const unsigned char
   fb_958 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 28,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_958 */

static Firebird::IRequest*
   fb_963 = 0;		/* request handle */

static const unsigned
   fb_964l = 77;
static const unsigned char
   fb_964 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 2,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_964 */

static Firebird::IRequest*
   fb_969 = 0;		/* request handle */

static const unsigned
   fb_970l = 77;
static const unsigned char
   fb_970 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 30,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_970 */

static Firebird::IRequest*
   fb_975 = 0;		/* request handle */

static const unsigned
   fb_976l = 77;
static const unsigned char
   fb_976 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 20,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_976 */

static Firebird::IRequest*
   fb_981 = 0;		/* request handle */

static const unsigned
   fb_982l = 83;
static const unsigned char
   fb_982 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 14,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_missing, 
			      blr_fid, 0, 9,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_982 */

static Firebird::IRequest*
   fb_987 = 0;		/* request handle */

static const unsigned
   fb_988l = 77;
static const unsigned char
   fb_988 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 31,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_988 */

static Firebird::IRequest*
   fb_993 = 0;		/* request handle */

static const unsigned
   fb_994l = 83;
static const unsigned char
   fb_994 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 26,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_missing, 
			      blr_fid, 0, 16,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_994 */

static Firebird::IRequest*
   fb_999 = 0;		/* request handle */

static const unsigned
   fb_1000l = 77;
static const unsigned char
   fb_1000 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1000 */

static Firebird::IRequest*
   fb_1005 = 0;		/* request handle */

static const unsigned
   fb_1006l = 77;
static const unsigned char
   fb_1006 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1006 */

static Firebird::IRequest*
   fb_1011 = 0;		/* request handle */

static const unsigned
   fb_1012l = 185;
static const unsigned char
   fb_1012 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 7,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 18,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 2,0, 
			      blr_and, 
				 blr_or, 
				    blr_eql, 
				       blr_fid, 0, 6,0, 
				       blr_parameter, 0, 1,0, 
				    blr_eql, 
				       blr_fid, 0, 6,0, 
				       blr_parameter, 0, 0,0, 
				 blr_eql, 
				    blr_fid, 0, 2,0, 
				    blr_literal, blr_text, 1,0, 'M',
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 4,0, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter2, 1, 4,0, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 6,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1012 */

static Firebird::IRequest*
   fb_1025 = 0;		/* request handle */

static const unsigned
   fb_1026l = 85;
static const unsigned char
   fb_1026 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 48,0, 0, 
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 1,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1026 */

static Firebird::IRequest*
   fb_1031 = 0;		/* request handle */

static const unsigned
   fb_1032l = 175;
static const unsigned char
   fb_1032 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 8,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring2, 0,0, 7,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 18,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_geq, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 0,0, 
			      blr_eql, 
				 blr_fid, 0, 4,0, 
				 blr_parameter, 0, 1,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 6,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 7,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1032 */

static Firebird::IRequest*
   fb_1045 = 0;		/* request handle */

static const unsigned
   fb_1046l = 193;
static const unsigned char
   fb_1046 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 18,0, 0, 
		     blr_rid, 30,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 4,0, 
				    blr_parameter, 0, 1,0, 
				 blr_and, 
				    blr_eql, 
				       blr_fid, 1, 0,0, 
				       blr_parameter, 0, 2,0, 
				    blr_and, 
				       blr_eql, 
					  blr_fid, 0, 2,0, 
					  blr_literal, blr_text, 1,0, 'G',
				       blr_neq, 
					  blr_fid, 1, 6,0, 
					  blr_fid, 0, 0,0, 
		     blr_sort, 3, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 5,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 5,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1046 */

static Firebird::IRequest*
   fb_1058 = 0;		/* request handle */

static const unsigned
   fb_1059l = 77;
static const unsigned char
   fb_1059 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 30,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1059 */

static Firebird::IRequest*
   fb_1064 = 0;		/* request handle */

static const unsigned
   fb_1065l = 193;
static const unsigned char
   fb_1065 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 18,0, 0, 
		     blr_rid, 20,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 4,0, 
				    blr_parameter, 0, 1,0, 
				 blr_and, 
				    blr_eql, 
				       blr_fid, 1, 0,0, 
				       blr_parameter, 0, 2,0, 
				    blr_and, 
				       blr_eql, 
					  blr_fid, 0, 2,0, 
					  blr_literal, blr_text, 1,0, 'G',
				       blr_neq, 
					  blr_fid, 1, 5,0, 
					  blr_fid, 0, 0,0, 
		     blr_sort, 3, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 5,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 5,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1065 */

static Firebird::IRequest*
   fb_1077 = 0;		/* request handle */

static const unsigned
   fb_1078l = 77;
static const unsigned char
   fb_1078 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 20,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1078 */

static Firebird::IRequest*
   fb_1083 = 0;		/* request handle */

static const unsigned
   fb_1084l = 199;
static const unsigned char
   fb_1084 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 18,0, 0, 
		     blr_rid, 14,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 4,0, 
				    blr_parameter, 0, 1,0, 
				 blr_and, 
				    blr_eql, 
				       blr_fid, 1, 0,0, 
				       blr_parameter, 0, 2,0, 
				    blr_and, 
				       blr_missing, 
					  blr_fid, 1, 9,0, 
				       blr_and, 
					  blr_eql, 
					     blr_fid, 0, 2,0, 
					     blr_literal, blr_text, 1,0, 'X',
					  blr_neq, 
					     blr_fid, 1, 17,0, 
					     blr_fid, 0, 0,0, 
		     blr_sort, 3, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 5,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 5,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1084 */

static Firebird::IRequest*
   fb_1096 = 0;		/* request handle */

static const unsigned
   fb_1097l = 83;
static const unsigned char
   fb_1097 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 14,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_missing, 
			      blr_fid, 0, 9,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1097 */

static Firebird::IRequest*
   fb_1102 = 0;		/* request handle */

static const unsigned
   fb_1103l = 193;
static const unsigned char
   fb_1103 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 18,0, 0, 
		     blr_rid, 42,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 4,0, 
				    blr_parameter, 0, 1,0, 
				 blr_and, 
				    blr_eql, 
				       blr_fid, 1, 0,0, 
				       blr_parameter, 0, 2,0, 
				    blr_and, 
				       blr_eql, 
					  blr_fid, 0, 2,0, 
					  blr_literal, blr_text, 1,0, 'X',
				       blr_neq, 
					  blr_fid, 1, 5,0, 
					  blr_fid, 0, 0,0, 
		     blr_sort, 3, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 5,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 5,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1103 */

static Firebird::IRequest*
   fb_1115 = 0;		/* request handle */

static const unsigned
   fb_1116l = 77;
static const unsigned char
   fb_1116 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 42,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1116 */

static Firebird::IRequest*
   fb_1121 = 0;		/* request handle */

static const unsigned
   fb_1122l = 197;
static const unsigned char
   fb_1122 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 8,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 4,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 18,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 2,0, 
			      blr_and, 
				 blr_or, 
				    blr_eql, 
				       blr_fid, 0, 6,0, 
				       blr_parameter, 0, 1,0, 
				    blr_eql, 
				       blr_fid, 0, 6,0, 
				       blr_parameter, 0, 0,0, 
				 blr_and, 
				    blr_eql, 
				       blr_fid, 0, 4,0, 
				       blr_parameter, 0, 3,0, 
				    blr_eql, 
				       blr_fid, 0, 2,0, 
				       blr_literal, blr_text, 1,0, 'M',
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter2, 1, 6,0, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 7,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1122 */

static Firebird::IRequest*
   fb_1137 = 0;		/* request handle */

static const unsigned
   fb_1138l = 77;
static const unsigned char
   fb_1138 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 31,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1138 */

static Firebird::IRequest*
   fb_1143 = 0;		/* request handle */

static const unsigned
   fb_1144l = 199;
static const unsigned char
   fb_1144 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 6,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 18,0, 0, 
		     blr_rid, 26,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 7,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 4,0, 
				    blr_parameter, 0, 1,0, 
				 blr_and, 
				    blr_eql, 
				       blr_fid, 1, 0,0, 
				       blr_parameter, 0, 2,0, 
				    blr_and, 
				       blr_eql, 
					  blr_fid, 0, 2,0, 
					  blr_literal, blr_text, 1,0, 'X',
				       blr_and, 
					  blr_neq, 
					     blr_fid, 1, 8,0, 
					     blr_fid, 0, 0,0, 
					  blr_missing, 
					     blr_fid, 1, 16,0, 
		     blr_sort, 3, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 5,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 5,0, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1144 */

static Firebird::IRequest*
   fb_1156 = 0;		/* request handle */

static const unsigned
   fb_1157l = 83;
static const unsigned char
   fb_1157 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 26,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_missing, 
			      blr_fid, 0, 16,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1157 */

static Firebird::IRequest*
   fb_1162 = 0;		/* request handle */

static const unsigned
   fb_1163l = 211;
static const unsigned char
   fb_1163 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 9,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring2, 0,0, 7,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 18,0, 0, 
		     blr_rid, 6,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 4,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 1, 8,0, 
				    blr_parameter, 0, 1,0, 
				 blr_and, 
				    blr_neq, 
				       blr_fid, 0, 2,0, 
				       blr_literal, blr_text, 1,0, 'M',
				    blr_neq, 
				       blr_fid, 1, 13,0, 
				       blr_fid, 0, 0,0, 
		     blr_sort, 3, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
			blr_ascending, 
			   blr_fid, 0, 5,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter2, 1, 4,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter2, 1, 6,0, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 7,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 8,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1163 */

static Firebird::IRequest*
   fb_1177 = 0;		/* request handle */

static const unsigned
   fb_1178l = 77;
static const unsigned char
   fb_1178 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 6,0, 0, 
		     blr_first, 
			blr_literal, blr_long, 0, 1,0,0,0,
		     blr_boolean, 
			blr_eql, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1178 */

static Firebird::IRequest*
   fb_1183 = 0;		/* request handle */

static const unsigned
   fb_1184l = 111;
static const unsigned char
   fb_1184 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 11,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_literal, blr_text, 21,0, 'R','D','B','$','S','Y','S','T','E','M','_','P','R','I','V','I','L','E','G','E','S',
			   blr_eql, 
			      blr_fid, 0, 1,0, 
			      blr_parameter, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1184 */

static Firebird::IRequest*
   fb_1190 = 0;		/* request handle */

static const unsigned
   fb_1191l = 88;
static const unsigned char
   fb_1191 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 6,0, 0, 
		  blr_boolean, 
		     blr_eql, 
			blr_fid, 0, 8,0, 
			blr_literal, blr_text, 12,0, 'R','D','B','$','D','A','T','A','B','A','S','E',
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 13,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_1191 */





// This enumeration tell comment-related routines how to behave: for SHOW,
// we present names and text as they are. For EXTRACT, we need to make a valid
// script and escape double quotes in identifiers and single quotes in strings.
enum commentMode {cmmShow, cmmExtract};


static void remove_delimited_double_quotes(TEXT*);
static void make_priv_string(USHORT, char*, bool);
static processing_state show_all_tables(SSHORT);
static void show_charsets(SSHORT char_set_id, SSHORT collation);
static processing_state show_check(const SCHAR*);
static processing_state show_collations(const SCHAR*, SSHORT sys_flag, const char* msg = 0, bool compact = false);
static void show_comment(const char* objtype, char* packageName, char* name1, char* name2,
	ISC_QUAD* blobfld, const commentMode showextract, const char* banner);
static processing_state show_comments(const commentMode showextract, const char* banner);
static void show_db();
static processing_state show_dependencies(const char* object);
static processing_state show_dependencies(const char* object, int obj_type);
static processing_state show_dialect();
static processing_state show_domains(const SCHAR*);
static processing_state show_exceptions(const SCHAR*);
static processing_state show_filters(const SCHAR*);
static processing_state show_functions(const SCHAR*, bool);
static processing_state show_sys_functions(const char* msg);
static processing_state show_func_legacy(const MetaString&);
static processing_state show_func(const MetaString&, const MetaString&);
static processing_state show_generators(const SCHAR*);
static void show_index(SCHAR*, SCHAR*, const SSHORT, const SSHORT, const SSHORT);
static processing_state show_indices(const SCHAR* const*);
static processing_state show_proc(const SCHAR*, bool, bool, const char* msg = nullptr);
static processing_state show_packages(const SCHAR* package_name, bool, const SCHAR* = NULL);
static processing_state show_publications(const SCHAR* pub_name, bool, const SCHAR* = NULL);
static void show_pub_table(const SCHAR* table_name);
static processing_state show_role(const SCHAR*, bool, const char* msg = NULL);
static processing_state show_secclass(const char* object, const char* opt);
static processing_state show_table(const SCHAR*, bool);
static processing_state show_trigger(const SCHAR*, bool, bool);
static processing_state show_users();
static processing_state show_users12();
static void parse_package(const char* procname, MetaString& package, MetaString& procedure);

const char* const spaces = "                                ";
static TEXT Print_buffer[512];
static TEXT SQL_identifier[BUFFER_LENGTH256];
static bool reReadDbOwner = true;


namespace {

// Used to make sure that local calls to print stuff go to isqlGlob.Out
// and not to stdout if IUtil::version gets called

class VersionCallback :
	public Firebird::AutoIface<Firebird::IVersionCallbackImpl<VersionCallback, Firebird::CheckStatusWrapper> >
{
public:
	// IVersionCallback implementation
	void callback(Firebird::CheckStatusWrapper*, const char* text)
	{
		isqlGlob.printf("%s%s", text, NEWLINE);
	}
};

} // anonymous namespace


static SINT64 ISQL_vax_integer(const UCHAR* bytes, USHORT length)
{
	return isc_portable_integer(bytes, length);
}


// Initialize types

// Keep this array in sync with obj.h in jrd.
static const SCHAR* Object_types[] =
{
	"Table",
	"View",
	"Trigger",
	"Computed column",
	"Validation",
	"Procedure",
	"Expression index",
	"Exception",
	"User",
	"Domain",
	"Index",
	"Count [error if used]",
	"User group",
	"SQL role",
	"Generator",
	"User defined function",
	"Blob filter",	// impossible to have explicit dependencies on it, but...
	"Collation"
};



const SCHAR* Trigger_prefix_types[] =
{
	"BEFORE",			// keyword
	"AFTER"				// keyword
};

const SCHAR* Trigger_suffix_types[] =
{
	"",
	"INSERT",			// keyword
	"UPDATE",			// keyword
	"DELETE"			// keyword
};

const SCHAR* Db_trigger_types[] =
{
	"CONNECT",				// keyword
	"DISCONNECT",			// keyword
	"TRANSACTION START",	// keyword
	"TRANSACTION COMMIT",	// keyword
	"TRANSACTION ROLLBACK"	// keyword
};


enum priv_flag {
	priv_UNKNOWN	= 1,
	priv_SELECT	= 2,
	priv_INSERT	= 4,
	priv_UPDATE	= 8,
	priv_DELETE	= 16,
	priv_EXECUTE	= 32,
	priv_REFERENCES	= 64,
	priv_CREATE = 128,
	priv_ALTER = 256,
	priv_DROP = 512
};


static const struct
{
	USHORT priv_flag;
	const char* priv_string;
} privs[] =
{
	{ priv_DELETE, "DELETE"},			// keyword
	{ priv_EXECUTE, "EXECUTE"},			// keyword
	{ priv_INSERT, "INSERT"},			// keyword
	{ priv_SELECT, "SELECT"},			// keyword
	{ priv_UPDATE, "UPDATE"},			// keyword
	{ priv_REFERENCES, "REFERENCES"},	// keyword
	{ priv_CREATE, "CREATE"},			// keyword
	{ priv_ALTER, "ALTER"},				// keyword
	{ priv_DROP, "DROP"},				// keyword
	{ 0, NULL}
};

// strlen of each element above, + strlen(", ") for separators

const int MAX_PRIV_LIST = (6 + 2 + 7 + 2 + 6 + 2 + 6 + 2 + 6 + 2 + 10 + 2 + 6 + 2 + 5 + 2 + 4 + 1);

const int PRIV_UPDATE_POS = 4;
const int PRIV_REFERENCES_POS = 5;

const int RELATION_PRIV_ALL = priv_SELECT | priv_INSERT | priv_UPDATE | priv_DELETE | priv_REFERENCES;

struct PrivilegeFields
{
	const char* privilege;
	string fields;
};

// Added support to display FORCED WRITES status. - PR 27-NOV-2001
// Added support to display transaction info when next_transaction id is fixed.
// Added support to display ODS version. CVC 26-Aug-2004.
static const UCHAR db_items[] =
{
	isc_info_page_size,
	isc_info_db_size_in_pages,
	fb_info_pages_used,
	fb_info_pages_free,
	isc_info_sweep_interval,
	isc_info_limbo,
	isc_info_forced_writes,
	isc_info_oldest_transaction,
	isc_info_oldest_active,
	isc_info_oldest_snapshot,
	isc_info_next_transaction,
	isc_info_ods_version,
	isc_info_ods_minor_version,
#ifdef DEV_BUILD
	isc_info_db_id,
#endif
	fb_info_crypt_state,
	fb_info_protocol_version,
	fb_info_wire_crypt,
	fb_info_creation_timestamp_tz,
	isc_info_creation_date,
	fb_info_replica_mode,
	isc_info_end
};


/* BPB to force transliteration of any shown system blobs from
 * Database character set (CS_METADATA) to process character set
 * (CS_dynamic).
 * This same BPB is safe to use for both V3 & V4 db's - as
 * a V3 db will ignore the source_ & target_interp values.
 */
static const UCHAR metadata_text_bpb[] =
{
	isc_bpb_version1,
	isc_bpb_source_type, 1, isc_blob_text,
	isc_bpb_target_type, 1, isc_blob_text,
	isc_bpb_source_interp, 1, CS_METADATA,
	isc_bpb_target_interp, 1, CS_dynamic
};

// trigger action helpers

inline int TRIGGER_ACTION_PREFIX(int value)
{
	return (value + 1) & 1;
}

inline int TRIGGER_ACTION_SUFFIX(int value, int slot)
{
	return ((value + 1) >> (slot * 2 - 1)) & 3;
}

const string SHOW_trigger_action(SINT64 type)
{
	switch (type & TRIGGER_TYPE_MASK)
	{
	case TRIGGER_TYPE_DML:
		{
			char buffer[256];
			int prefix = TRIGGER_ACTION_PREFIX(type);
			strcpy(buffer, Trigger_prefix_types[prefix]);
			int suffix = TRIGGER_ACTION_SUFFIX(type, 1);
			strcat(buffer, " ");
			strcat(buffer, Trigger_suffix_types[suffix]);
			if ( (suffix = TRIGGER_ACTION_SUFFIX(type, 2)) )
			{
				strcat(buffer, " OR ");
				strcat(buffer, Trigger_suffix_types[suffix]);
			}
			if ( (suffix = TRIGGER_ACTION_SUFFIX(type, 3)) )
			{
				strcat(buffer, " OR ");
				strcat(buffer, Trigger_suffix_types[suffix]);
			}
			return buffer;
		}

	case TRIGGER_TYPE_DB:
		return string("ON ") + Db_trigger_types[type & ~TRIGGER_TYPE_DB];

	case TRIGGER_TYPE_DDL:
		{
			bool first = true;
			string buffer = Trigger_prefix_types[type & 1];

			if ((type & DDL_TRIGGER_ANY) == DDL_TRIGGER_ANY)
				buffer += " ANY DDL STATEMENT";
			else
			{
				for (SINT64 pos = 1; pos < 64; ++pos)
				{
					if (((1LL << pos) & TRIGGER_TYPE_MASK) || (type & (1LL << pos)) == 0)
						continue;

					if (first)
						first = false;
					else
						buffer += " OR";

					buffer += " ";

					if (pos < FB_NELEM(DDL_TRIGGER_ACTION_NAMES))
					{
						buffer += string(DDL_TRIGGER_ACTION_NAMES[pos][0]) + " " +
							DDL_TRIGGER_ACTION_NAMES[pos][1];
					}
					else
						buffer += "<unknown>";
				}
			}

			return buffer;
		}

	default:
		fb_assert(false);
		return "";
	}
}


// *************************
// S H O W _ c o m m e n t s
// *************************
// Extract database comments. Since it's called by the script extraction
// routine, it does nothing for ODS < 11, unless the force option is true.
void SHOW_comments(bool force)
{
	if (isqlGlob.major_ods >= ODS_VERSION11 || force)
	{
		char banner[BUFFER_LENGTH128];
		fb_utils::snprintf(banner, sizeof(banner), "%s/* Comments for database objects. */%s", NEWLINE, NEWLINE);
		show_comments(cmmExtract, banner);
	}
}


bool SHOW_dbb_parameters(Firebird::IAttachment* db_handle,
						 std::string& info_buf,
						 const UCHAR* db_itemsL,
						 unsigned item_length,
						 bool translate,
						 const char* separator)
{
/**************************************
 *
 *	S H O W _ d b b _ p a r a m e t e r s
 *
 **************************************
 *
 * Functional description
 *	Show db_info on this database
 *
 *	Arguments:
 *	    db_handle -- database handle
 *	    info_buf -- info_bufput file pointer
 *	    db_itemsL -- list of db_info items to process
 *
 **************************************/
	UCHAR buffer[BUFFER_LENGTH400];
	TEXT msg[MSG_LENGTH];

	Firebird::AutoPtr<Firebird::IStatus, Firebird::SimpleDispose>
		status_vector(fbMaster->getStatus());
	Firebird::CheckStatusWrapper statusWrapper(status_vector);
	db_handle->getInfo(&statusWrapper, item_length, db_itemsL, sizeof(buffer), buffer);
	if (ISQL_errmsg(status_vector))
		return false;

	bool crdatePrinted = false;

	info_buf.clear();
	SCHAR* info = new SCHAR[info_buf.length() + 1];
	strcpy(info, info_buf.c_str());


	for (Firebird::ClumpletReader p(Firebird::ClumpletReader::InfoResponse, buffer, sizeof(buffer)); !p.isEof(); p.moveNext())
	{
		UCHAR item = p.getClumpTag();
		SINT64 value_out = 0;

		/*
		 * This is not the best solution but it fixes the lack of <LF> characters
		 * in Windows ISQL.  This will need to remain until we modify the messages
		 * to remove the '\n' (on the PC its '\n\r').
		 * CVC: WISQL is dead. I've restored the missing newlines for isql.
		 */
		switch (item)
		{
		case isc_info_end:
			break;

		case isc_info_page_size:
			value_out = p.getInt();
			sprintf(info, "PAGE_SIZE %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_db_size_in_pages:
			value_out = p.getInt();
			if (translate)
			{
				IUTILS_msg_get(NUMBER_PAGES, msg, SafeArg() << value_out);
				sprintf(info, "%s%s", msg, separator);
			}
			else
				sprintf(info, "Number of DB pages allocated = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case fb_info_pages_used:
			value_out = p.getInt();
			if (translate)
			{
				IUTILS_msg_get(NUMBER_USED_PAGES, msg, SafeArg() << value_out);
				sprintf(info, "%s%s", msg, separator);
			}
			else
				sprintf(info, "Number of DB pages used = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case fb_info_pages_free:
			value_out = p.getInt();
			if (translate)
			{
				IUTILS_msg_get(NUMBER_FREE_PAGES, msg, SafeArg() << value_out);
				sprintf(info, "%s%s", msg, separator);
			}
			else
				sprintf(info, "Number of DB pages free = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case fb_info_crypt_state:
			value_out = p.getInt();
			if (translate)
			{
				Firebird::string s;
				if (value_out & fb_info_crypt_encrypted)
					IUTILS_msg_get(DATABASE_CRYPTED, msg);
				else
					IUTILS_msg_get(DATABASE_NOT_CRYPTED, msg);

				s = msg;

				if (value_out & fb_info_crypt_process)
				{
					s += ", ";
					IUTILS_msg_get(DATABASE_CRYPT_PROCESS, msg);
					s += msg;
				}

				sprintf(info, "%s%s", s.c_str(), separator);
			}
			else
			{
				sprintf(info, "DB %sencrypted%s%s",
					(value_out & fb_info_crypt_encrypted ? "" : "not "),
					(value_out & fb_info_crypt_process ? ", crypt thread not complete" : ""),
					separator);
			}
			break;

		case isc_info_sweep_interval:
			value_out = p.getInt();
			if (translate)
			{
				IUTILS_msg_get(SWEEP_INTERV, msg, SafeArg() << value_out);
				sprintf(info, "%s%s", msg, separator);
			}
			else
				sprintf(info, "Sweep interval = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_forced_writes:
			value_out = p.getInt();
			sprintf (info, "Forced Writes are %s%s", (value_out == 1 ? "ON" : "OFF"), separator);
			break;

		case isc_info_oldest_transaction :
			value_out = p.getInt();
			sprintf(info, "Transaction - oldest = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_oldest_active :
			value_out = p.getInt();
			sprintf(info, "Transaction - oldest active = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_oldest_snapshot :
			value_out = p.getInt();
			sprintf(info, "Transaction - oldest snapshot = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_next_transaction :
			value_out = p.getInt();
			sprintf (info, "Transaction - Next = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_base_level:
			value_out = p.getInt();
			if (translate)
			{
				IUTILS_msg_get(BASE_LEVEL, msg, SafeArg() << value_out);
				sprintf(info, "%s%s", msg, separator);
			}
			else
				sprintf(info, "Base level = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_limbo:
			value_out = p.getInt();
			if (translate)
			{
				IUTILS_msg_get(LIMBO, msg, SafeArg() << value_out);
				sprintf(info, "%s%s", msg, separator);
			}
			else
				sprintf(info, "Transaction in limbo = %" SQUADFORMAT"%s", value_out, separator);
			break;

		case isc_info_ods_version:
			isqlGlob.major_ods = p.getInt();
			break;
		case isc_info_ods_minor_version:
			value_out = p.getInt();
			sprintf(info, "ODS = %" SLONGFORMAT".%" SQUADFORMAT"%s",
					(SLONG) isqlGlob.major_ods, value_out, separator);
			break;

		case fb_info_wire_crypt:
			if (p.getClumpLength())
				sprintf (info, "Wire crypt plugin: %.*s%s", p.getClumpLength(), p.getBytes(), separator);
			break;

		case fb_info_protocol_version:
			value_out = p.getInt();
			if (value_out)
				sprintf(info, "Protocol version = %" SQUADFORMAT"%s", value_out, separator);
			else
				sprintf(info, "Embedded connection%s", separator);
			break;

		case isc_info_creation_date:
			if (p.getClumpLength() == sizeof(ISC_TIMESTAMP) && !crdatePrinted)
			{
				ISC_TIMESTAMP ts;

				const UCHAR* t = p.getBytes();
				ts.timestamp_date = ISQL_vax_integer(t, sizeof(ISC_DATE));
				t += sizeof(ISC_DATE);
				ts.timestamp_time = ISQL_vax_integer(t, sizeof(ISC_TIME));

				struct tm time;
				isc_decode_timestamp(&ts, &time);

				sprintf(info, "Creation date: %s %d, %d %d:%02d:%02d%s",
					FB_SHORT_MONTHS[time.tm_mon], time.tm_mday, time.tm_year + 1900,
					time.tm_hour, time.tm_min, time.tm_sec, separator);
			}
			break;

		case fb_info_creation_timestamp_tz:
			if (p.getClumpLength() == sizeof(ISC_TIMESTAMP))
			{
				ISC_TIMESTAMP_TZ tsz;

				const UCHAR* t = p.getBytes();
				tsz.utc_timestamp.timestamp_date = ISQL_vax_integer(t, sizeof(ISC_DATE));
				t += sizeof(ISC_DATE);
				tsz.utc_timestamp.timestamp_time = ISQL_vax_integer(t, sizeof(ISC_TIME));
				t += sizeof(ISC_TIME);
				tsz.time_zone = ISQL_vax_integer(t, sizeof(ULONG));

				unsigned year, month, day, hours, minutes, seconds, fractions;
				char timeZone[Firebird::TimeZoneUtil::MAX_SIZE];

				Firebird::UtilInterfacePtr()->decodeTimeStampTz(fbStatus, &tsz,
					&year, &month, &day, &hours, &minutes, &seconds, &fractions, sizeof(timeZone), timeZone);

				if (ISQL_errmsg(fbStatus))
					break;

				sprintf(info, "Creation date: %s %d, %d %d:%02d:%02d %s%s",
					FB_SHORT_MONTHS[month - 1], day, year,
					hours, minutes, seconds, timeZone, separator);

				crdatePrinted = true;
			}
			break;

#ifdef DEV_BUILD
		case isc_info_db_id:
			{
				// Will print with garbage for now.
				//It's sprintf(info, "DB/Host = %.*s", length, d);
				const UCHAR* s = p.getBytes();
				const UCHAR* end = s + p.getClumpLength();
				++s; // Skip useless indicator.
				int len = *s++;
				printf("DB = %.*s\n", len, s);
				s += len;
				while (s < end)
				{
					len = *s++;
					printf("Host = %.*s\n", len, s);
					s += len;
				}
			}
			break;
#endif

		case fb_info_replica_mode:
			{
				value_out = p.getInt();
				const char* mode =
					(value_out == fb_info_replica_none) ? "NONE" :
					(value_out == fb_info_replica_read_only) ? "READ_ONLY" :
					(value_out == fb_info_replica_read_write) ? "READ_WRITE" :
					"unknown";
				sprintf(info, "Replica mode: %s%s", mode, separator);
			}
			break;

		case isc_info_truncated:
			return info > info_buf; // If we got some items, we are (partially) successful.
		}
		info += strlen(info);
	}

	return info > info_buf;
}


processing_state SHOW_grants(const SCHAR* object, const SCHAR* terminator, ObjectType obj_type)
{
/**************************************
 *
 *	S H O W _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Placeholder for SHOW_grants2 without additional message.
 *
 **************************************/
	return SHOW_grants2 (object, terminator, obj_type, NULL, false);
}


void SHOW_read_owner()
{
/**************************************
 *
 *	S H O W _ r e a d _ o w n e r
 *
 **************************************
 *
 * Functional description
 *	Make granted_by() re-read db owner name.
 *
 **************************************/
	reReadDbOwner = true;
}


static const char* granted_by(char* buffer, const char* grantor, bool nullGrantor)
{
   struct fb_1192_struct {
          short fb_1193;	/* fbUtility */
          short fb_1194;	/* gds__null_flag */
          char  fb_1195 [253];	/* RDB$OWNER_NAME */
   } fb_1192;
/**************************************
 *
 *	g r a n t e d _ b y
 *
 **************************************
 *
 * Functional description
 *	Output message only if not granted by DB owner.
 *
 **************************************/
	static /*BASED_ON RDB$RELATIONS.RDB$OWNER_NAME owner;*/
	       char
		  owner[253];


	if (reReadDbOwner)
	{
		// Get the owner name
		strcpy(owner, DBA_USER_NAME);

		/*FOR REL IN RDB$RELATIONS WITH
			REL.RDB$RELATION_NAME = "RDB$DATABASE"*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_1190 && fbTrans && DB)
		      fb_1190 = DB->compileRequest(fbStatus, sizeof(fb_1191), fb_1191);
		   if (fbTrans && fb_1190)
		      fb_1190->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1190->release(); fb_1190 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_1190->receive (fbStatus, 0, 0, 257, CAST_MSG(&fb_1192));
		   if (!fb_1192.fb_1193 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if (!/*REL.RDB$OWNER_NAME.NULL*/
			     fb_1192.fb_1194)
				strcpy(owner, /*REL.RDB$OWNER_NAME*/
					      fb_1192.fb_1195);
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			buffer[0] = '\0';
			return "";
		/*END_ERROR;*/
		   }
		}

		fb_utils::exact_name(owner);
		reReadDbOwner = false;
	}

 	strcpy(buffer, grantor);
	fb_utils::exact_name(buffer);
	if ((!strcmp(buffer, owner)) || nullGrantor)
		buffer[0] = '\0';
	else
	{
		strcpy(buffer, " GRANTED BY ");
		strcat(buffer, grantor);
		fb_utils::exact_name(buffer);
	}

	return buffer;
}


static void set_grantee(int user_type, const char* SQL_identifier, char* user_string)
{
   struct fb_1187_struct {
          short fb_1188;	/* fbUtility */
          char  fb_1189 [253];	/* RDB$TYPE_NAME */
   } fb_1187;
   struct fb_1185_struct {
          short fb_1186;	/* RDB$TYPE */
   } fb_1185;
	switch (user_type)
	{
	case obj_view:
		sprintf(user_string, "VIEW %s", SQL_identifier);
		break;
	case obj_trigger:
		sprintf(user_string, "TRIGGER %s", SQL_identifier);
		break;
	case obj_procedure:
		sprintf(user_string, "PROCEDURE %s", SQL_identifier);
		break;
	case obj_udf:
		sprintf(user_string, "FUNCTION %s", SQL_identifier);
		break;
	case obj_user:
		if (strcmp(SQL_identifier, "PUBLIC"))
			sprintf(user_string, "USER %s", SQL_identifier);
		else
			strcpy(user_string, SQL_identifier);
		break;
	case obj_user_group:
		sprintf(user_string, "GROUP %s", SQL_identifier);
		break;
	case obj_sql_role:
		sprintf(user_string, "ROLE %s", SQL_identifier);
		break;
	case obj_package_header:
		sprintf(user_string, "PACKAGE %s", SQL_identifier);
		break;
	case obj_privilege:
		/*FOR T IN RDB$TYPES
			WITH T.RDB$FIELD_NAME EQ 'RDB$SYSTEM_PRIVILEGES' AND
				 T.RDB$TYPE EQ atoi(SQL_identifier)*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_1183 && DB)
		      fb_1183 = DB->compileRequest(fbStatus, sizeof(fb_1184), fb_1184);
		   fb_1185.fb_1186 = atoi(SQL_identifier);
		   fb_1183->startAndSend(fbStatus, fbTrans, 0, 0, 2, CAST_CONST_MSG(&fb_1185));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1183->release(); fb_1183 = NULL; }
		   else break;
		   }
		while (1)
		   {
		   fb_1183->receive (fbStatus, 0, 1, 255, CAST_MSG(&fb_1187));
		   if (!fb_1187.fb_1188) break;
		{
			sprintf(user_string, "SYSTEM PRIVILEGE %s", fb_utils::exact_name(/*T.RDB$TYPE_NAME*/
											 fb_1187.fb_1189));
		}
		/*END_FOR*/
		   }
		}
		break;
	default:
		strcpy(user_string, SQL_identifier);
		break;
	}
}


static USHORT convert_privilege_to_code(char privilege)
{
	switch (privilege)
	{
	case 'S':
		return priv_SELECT;
	case 'I':
		return priv_INSERT;
	case 'U':
		return priv_UPDATE;
	case 'D':
		return priv_DELETE;
	case 'R':
		return priv_REFERENCES;
	case 'X':
	case 'G':
		// Execute should not be here
		return 0;
	}

	return priv_UNKNOWN;
}


static void make_privilege_string_with_fields(USHORT priv_flags,
											  const PrivilegeFields* priv_fields,
											  int priv_fields_count,
											  char* priv_string,
											  string& out_result)
{
	out_result = "";

	if (priv_flags)
	{
		make_priv_string(priv_flags, priv_string, false);
		out_result.assign(priv_string);
	}

	for (int i = 0; i < priv_fields_count; i++)
	{
		if (priv_fields[i].fields.isEmpty())
			continue;

		if (!out_result.isEmpty())
			out_result += ", ";
		out_result += priv_fields[i].privilege + (" (" + priv_fields[i].fields + ")");
	}
}


processing_state SHOW_grants2 (const SCHAR* object,
				  const SCHAR* terminator,
				  ObjectType obj_type,
				  const TEXT* optional_msg,
				  bool mangle)
{
   struct fb_1027_struct {
          short fb_1028;	/* fbUtility */
          short fb_1029;	/* SEC$USER_TYPE */
          char  fb_1030 [253];	/* SEC$USER */
   } fb_1027;
   struct fb_1036_struct {
          short fb_1037;	/* fbUtility */
          short fb_1038;	/* gds__null_flag */
          short fb_1039;	/* RDB$GRANT_OPTION */
          short fb_1040;	/* RDB$USER_TYPE */
          short fb_1041;	/* RDB$OBJECT_TYPE */
          char  fb_1042 [253];	/* RDB$USER */
          char  fb_1043 [7];	/* RDB$PRIVILEGE */
          char  fb_1044 [253];	/* RDB$GRANTOR */
   } fb_1036;
   struct fb_1033_struct {
          short fb_1034;	/* RDB$OBJECT_TYPE */
          char  fb_1035 [253];	/* RDB$RELATION_NAME */
   } fb_1033;
   struct fb_1051_struct {
          short fb_1052;	/* fbUtility */
          short fb_1053;	/* gds__null_flag */
          short fb_1054;	/* RDB$GRANT_OPTION */
          short fb_1055;	/* RDB$USER_TYPE */
          char  fb_1056 [253];	/* RDB$USER */
          char  fb_1057 [253];	/* RDB$GRANTOR */
   } fb_1051;
   struct fb_1047_struct {
          short fb_1048;	/* RDB$OBJECT_TYPE */
          char  fb_1049 [253];	/* RDB$RELATION_NAME */
          char  fb_1050 [253];	/* RDB$EXCEPTION_NAME */
   } fb_1047;
   struct fb_1062_struct {
          short fb_1063;	/* fbUtility */
   } fb_1062;
   struct fb_1060_struct {
          char  fb_1061 [253];	/* RDB$EXCEPTION_NAME */
   } fb_1060;
   struct fb_1070_struct {
          short fb_1071;	/* fbUtility */
          short fb_1072;	/* gds__null_flag */
          short fb_1073;	/* RDB$GRANT_OPTION */
          short fb_1074;	/* RDB$USER_TYPE */
          char  fb_1075 [253];	/* RDB$USER */
          char  fb_1076 [253];	/* RDB$GRANTOR */
   } fb_1070;
   struct fb_1066_struct {
          short fb_1067;	/* RDB$OBJECT_TYPE */
          char  fb_1068 [253];	/* RDB$RELATION_NAME */
          char  fb_1069 [253];	/* RDB$GENERATOR_NAME */
   } fb_1066;
   struct fb_1081_struct {
          short fb_1082;	/* fbUtility */
   } fb_1081;
   struct fb_1079_struct {
          char  fb_1080 [253];	/* RDB$GENERATOR_NAME */
   } fb_1079;
   struct fb_1089_struct {
          short fb_1090;	/* fbUtility */
          short fb_1091;	/* gds__null_flag */
          short fb_1092;	/* RDB$GRANT_OPTION */
          short fb_1093;	/* RDB$USER_TYPE */
          char  fb_1094 [253];	/* RDB$USER */
          char  fb_1095 [253];	/* RDB$GRANTOR */
   } fb_1089;
   struct fb_1085_struct {
          short fb_1086;	/* RDB$OBJECT_TYPE */
          char  fb_1087 [253];	/* RDB$RELATION_NAME */
          char  fb_1088 [253];	/* RDB$FUNCTION_NAME */
   } fb_1085;
   struct fb_1100_struct {
          short fb_1101;	/* fbUtility */
   } fb_1100;
   struct fb_1098_struct {
          char  fb_1099 [253];	/* RDB$FUNCTION_NAME */
   } fb_1098;
   struct fb_1108_struct {
          short fb_1109;	/* fbUtility */
          short fb_1110;	/* gds__null_flag */
          short fb_1111;	/* RDB$GRANT_OPTION */
          short fb_1112;	/* RDB$USER_TYPE */
          char  fb_1113 [253];	/* RDB$USER */
          char  fb_1114 [253];	/* RDB$GRANTOR */
   } fb_1108;
   struct fb_1104_struct {
          short fb_1105;	/* RDB$OBJECT_TYPE */
          char  fb_1106 [253];	/* RDB$RELATION_NAME */
          char  fb_1107 [253];	/* RDB$PACKAGE_NAME */
   } fb_1104;
   struct fb_1119_struct {
          short fb_1120;	/* fbUtility */
   } fb_1119;
   struct fb_1117_struct {
          char  fb_1118 [253];	/* RDB$PACKAGE_NAME */
   } fb_1117;
   struct fb_1128_struct {
          short fb_1129;	/* fbUtility */
          short fb_1130;	/* gds__null_flag */
          short fb_1131;	/* gds__null_flag */
          short fb_1132;	/* RDB$GRANT_OPTION */
          char  fb_1133 [253];	/* RDB$RELATION_NAME */
          char  fb_1134 [253];	/* RDB$USER */
          char  fb_1135 [253];	/* RDB$FIELD_NAME */
          char  fb_1136 [253];	/* RDB$GRANTOR */
   } fb_1128;
   struct fb_1123_struct {
          short fb_1124;	/* RDB$USER_TYPE */
          short fb_1125;	/* RDB$USER_TYPE */
          short fb_1126;	/* RDB$OBJECT_TYPE */
          char  fb_1127 [253];	/* RDB$RELATION_NAME */
   } fb_1123;
   struct fb_1141_struct {
          short fb_1142;	/* fbUtility */
   } fb_1141;
   struct fb_1139_struct {
          char  fb_1140 [253];	/* RDB$ROLE_NAME */
   } fb_1139;
   struct fb_1149_struct {
          short fb_1150;	/* fbUtility */
          short fb_1151;	/* gds__null_flag */
          short fb_1152;	/* RDB$GRANT_OPTION */
          short fb_1153;	/* RDB$USER_TYPE */
          char  fb_1154 [253];	/* RDB$USER */
          char  fb_1155 [253];	/* RDB$GRANTOR */
   } fb_1149;
   struct fb_1145_struct {
          short fb_1146;	/* RDB$OBJECT_TYPE */
          char  fb_1147 [253];	/* RDB$RELATION_NAME */
          char  fb_1148 [253];	/* RDB$PROCEDURE_NAME */
   } fb_1145;
   struct fb_1160_struct {
          short fb_1161;	/* fbUtility */
   } fb_1160;
   struct fb_1158_struct {
          char  fb_1159 [253];	/* RDB$PROCEDURE_NAME */
   } fb_1158;
   struct fb_1167_struct {
          short fb_1168;	/* fbUtility */
          short fb_1169;	/* RDB$USER_TYPE */
          short fb_1170;	/* gds__null_flag */
          short fb_1171;	/* gds__null_flag */
          short fb_1172;	/* RDB$GRANT_OPTION */
          char  fb_1173 [253];	/* RDB$USER */
          char  fb_1174 [253];	/* RDB$FIELD_NAME */
          char  fb_1175 [253];	/* RDB$GRANTOR */
          char  fb_1176 [7];	/* RDB$PRIVILEGE */
   } fb_1167;
   struct fb_1164_struct {
          char  fb_1165 [253];	/* RDB$RELATION_NAME */
          char  fb_1166 [253];	/* RDB$RELATION_NAME */
   } fb_1164;
   struct fb_1181_struct {
          short fb_1182;	/* fbUtility */
   } fb_1181;
   struct fb_1179_struct {
          char  fb_1180 [253];	/* RDB$RELATION_NAME */
   } fb_1179;
/**************************************
 *
 *	S H O W _ g r a n t s 2
 *
 **************************************
 *
 * Functional description
 *	Show grants for given object name
 *	This function is also called by extract for privileges.
 *  	It must extract granted privileges on tables/views to users,
 *		- these may be compound, so put them on the same line.
 *	Grant execute privilege on procedures to users
 *	Grant various privilegs to procedures.
 *	All privileges may have the with_grant option set.
 *	The optional msg is to display a customized message. When the
 *	new feature of all grants is used, there's no way to print the
 *	header message after this routine, so it should be printed here.
 *
 **************************************/
	/*BASED_ON RDB$USER_PRIVILEGES.RDB$USER prev_user;*/
	char
	   prev_user[253];

	/*BASED_ON RDB$USER_PRIVILEGES.RDB$GRANT_OPTION prev_grant_option;*/
	short
	   prev_grant_option;

	/*BASED_ON RDB$USER_PRIVILEGES.RDB$GRANTOR prev_grantor;*/
	char
	   prev_grantor[253];

	SCHAR buf_grantor[sizeof(prev_grantor) + 20];
	SCHAR user_string[QUOTED_NAME_SIZE + 20];
	SCHAR obj_string[QUOTED_NAME_SIZE + 20];

	PrivilegeFields priv_fields[] =
	{
		{privs[PRIV_UPDATE_POS].priv_string, ""},
		{privs[PRIV_REFERENCES_POS].priv_string, ""}
	};
	const int priv_fields_count = FB_NELEM(priv_fields);

	bool first = true;

	if (!*object)
		return ps_ERR;

	// Query against user_privileges instead of looking at rdb$security_classes

	prev_grant_option = -1;
	prev_user[0] = '\0';
	prev_grantor[0] = '\0';
	char priv_string[MAX_PRIV_LIST] = "";
	string col_string;
	char with_option[19] = "";
	USHORT priv_flags = 0;

	if (obj_type == obj_relation || obj_type == obj_any)
	{
		// Find the user specified relation and show its privileges

		/*FOR FIRST 1 R IN RDB$RELATIONS WITH R.RDB$RELATION_NAME EQ object*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_1177 && fbTrans && DB)
		      fb_1177 = DB->compileRequest(fbStatus, sizeof(fb_1178), fb_1178);
		   isc_vtov ((const char*) object, (char*) fb_1179.fb_1180, 253);
		   if (fbTrans && fb_1177)
		      fb_1177->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1179));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1177->release(); fb_1177 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_1177->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1181));
		   if (!fb_1181.fb_1182 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// This query only finds tables, eliminating owner privileges

			/*FOR PRV IN RDB$USER_PRIVILEGES CROSS
				REL IN RDB$RELATIONS WITH
				PRV.RDB$GRANTOR NOT MISSING		AND
				PRV.RDB$RELATION_NAME EQ object AND
				REL.RDB$RELATION_NAME EQ object AND
				PRV.RDB$PRIVILEGE     NE 'M'    AND
				REL.RDB$OWNER_NAME    NE PRV.RDB$USER
				SORTED BY PRV.RDB$USER, PRV.RDB$GRANT_OPTION, PRV.RDB$FIELD_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1162 && fbTrans && DB)
			      fb_1162 = DB->compileRequest(fbStatus, sizeof(fb_1163), fb_1163);
			   isc_vtov ((const char*) object, (char*) fb_1164.fb_1165, 253);
			   isc_vtov ((const char*) object, (char*) fb_1164.fb_1166, 253);
			   if (fbTrans && fb_1162)
			      fb_1162->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_1164));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1162->release(); fb_1162 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1162->receive (fbStatus, 0, 1, 776, CAST_MSG(&fb_1167));
			   if (!fb_1167.fb_1168 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;;

				fb_utils::exact_name(/*PRV.RDB$USER*/
						     fb_1167.fb_1173);

				// Sometimes grant options are null, sometimes 0.  Both same

				if (/*PRV.RDB$GRANT_OPTION.NULL*/
				    fb_1167.fb_1171)
					/*PRV.RDB$GRANT_OPTION*/
					fb_1167.fb_1172 = 0;

				if (/*PRV.RDB$FIELD_NAME.NULL*/
				    fb_1167.fb_1170)
					/*PRV.RDB$FIELD_NAME*/
					fb_1167.fb_1174[0] = '\0';

				// Print a new grant statement for each new user or change of option

				if (prev_grant_option != -1 &&
					(strcmp(prev_user, /*PRV.RDB$USER*/
							   fb_1167.fb_1173) ||
					prev_grant_option != /*PRV.RDB$GRANT_OPTION*/
							     fb_1167.fb_1172 ||
					strcmp(prev_grantor, /*PRV.RDB$GRANTOR*/
							     fb_1167.fb_1175)))
				{
					if (first && optional_msg)
						isqlGlob.prints(optional_msg);
					first = false;

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					if (prev_grant_option)
						strcpy(with_option, " WITH GRANT OPTION");

					string combine_privileges = "";
					make_privilege_string_with_fields(priv_flags, priv_fields, priv_fields_count,
						priv_string, combine_privileges);

					isqlGlob.printf("GRANT %s ON %s TO %s%s%s%s%s",
						combine_privileges.c_str(),
						SQL_identifier,
						user_string, with_option, granted_by(buf_grantor, prev_grantor, false),
						terminator, NEWLINE);

					// re-initialize
					priv_flags = 0;
					with_option[0] = '\0';
					priv_string[0] = '\0';
					col_string = "";
					for (int i = 0; i < priv_fields_count; i++)
						priv_fields[i].fields = "";
				}

				// At each row, store this value for the next compare of contol break

				strcpy (prev_user, /*PRV.RDB$USER*/
						   fb_1167.fb_1173);
				prev_grant_option = /*PRV.RDB$GRANT_OPTION*/
						    fb_1167.fb_1172;
				strcpy (prev_grantor, /*PRV.RDB$GRANTOR*/
						      fb_1167.fb_1175);

				switch (/*PRV.RDB$USER_TYPE*/
					fb_1167.fb_1169)
				{
				case obj_relation:
				case obj_view:
				case obj_trigger:
				case obj_procedure:
				case obj_udf:
				case obj_sql_role:
				case obj_package_header:
				case obj_user: // Users can be machine\user and need quoting
					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
								   fb_1167.fb_1173, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1167.fb_1173);
					break;
				default:
					strcpy(SQL_identifier, /*PRV.RDB$USER*/
							       fb_1167.fb_1173);
					break;
				}

				set_grantee(/*PRV.RDB$USER_TYPE*/
					    fb_1167.fb_1169, SQL_identifier, user_string);

				// Only the first character is used for permissions
				const char c = /*PRV.RDB$PRIVILEGE*/
					       fb_1167.fb_1176[0];

				// Column level privileges for update and references only
				if (/*PRV.RDB$FIELD_NAME.NULL*/
				    fb_1167.fb_1170)
				{
					col_string = "";
					priv_flags |= convert_privilege_to_code(c);
					continue;
				}
				else
				{
					fb_utils::exact_name(/*PRV.RDB$FIELD_NAME*/
							     fb_1167.fb_1174);
					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					{
						IUTILS_copy_SQL_id(/*PRV.RDB$FIELD_NAME*/
								   fb_1167.fb_1174, SQL_identifier, DBL_QUOTE);
						col_string = SQL_identifier;
					}
					else
					{
						col_string = /*PRV.RDB$FIELD_NAME*/
							     fb_1167.fb_1174;
					}
				}

				for (int i = 0; i < priv_fields_count; i++)
				{
					if (c == priv_fields[i].privilege[0])
					{
						priv_fields[i].fields += priv_fields[i].fields.isEmpty() ? col_string
							: ", " + col_string;
						break;
					}
				}

			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg (fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

			// Print last case if there was anything to print

			if (prev_grant_option != -1)
			{
				if (first && optional_msg)
					isqlGlob.prints(optional_msg);
				first = false;

				if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
				else
					strcpy(SQL_identifier, object);

				if (prev_grant_option)
					strcpy(with_option, " WITH GRANT OPTION");

				string combine_privileges;
				make_privilege_string_with_fields(priv_flags, priv_fields, priv_fields_count,
					priv_string, combine_privileges);

				isqlGlob.printf("GRANT %s ON %s TO %s%s%s%s%s",
					combine_privileges.c_str(),
					SQL_identifier,
					user_string, with_option, granted_by(buf_grantor, prev_grantor, false),
					terminator, NEWLINE);
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (!first)
			return (SKIP);
	}

	// No relation called "object" was found, try procedure "object"
	if (obj_type == obj_procedure || obj_type == obj_any)
	{
		/*FOR FIRST 1 P IN RDB$PROCEDURES WITH
			P.RDB$PROCEDURE_NAME EQ object AND
			P.RDB$PACKAGE_NAME MISSING*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_1156 && fbTrans && DB)
		      fb_1156 = DB->compileRequest(fbStatus, sizeof(fb_1157), fb_1157);
		   isc_vtov ((const char*) object, (char*) fb_1158.fb_1159, 253);
		   if (fbTrans && fb_1156)
		      fb_1156->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1158));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1156->release(); fb_1156 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_1156->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1160));
		   if (!fb_1160.fb_1161 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// Part two is for stored procedures only

			/*FOR PRV IN RDB$USER_PRIVILEGES CROSS
				PRC IN RDB$PROCEDURES WITH
				PRV.RDB$GRANTOR NOT MISSING AND
				PRV.RDB$OBJECT_TYPE = obj_procedure AND
				PRV.RDB$RELATION_NAME EQ object AND
				PRC.RDB$PROCEDURE_NAME EQ object AND
				PRV.RDB$PRIVILEGE EQ 'X' AND
				PRC.RDB$OWNER_NAME NE PRV.RDB$USER AND
				PRC.RDB$PACKAGE_NAME MISSING
				SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1143 && fbTrans && DB)
			      fb_1143 = DB->compileRequest(fbStatus, sizeof(fb_1144), fb_1144);
			   fb_1145.fb_1146 = obj_procedure;
			   isc_vtov ((const char*) object, (char*) fb_1145.fb_1147, 253);
			   isc_vtov ((const char*) object, (char*) fb_1145.fb_1148, 253);
			   if (fbTrans && fb_1143)
			      fb_1143->startAndSend(fbStatus, fbTrans, 0, 0, 508, CAST_CONST_MSG(&fb_1145));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1143->release(); fb_1143 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1143->receive (fbStatus, 0, 1, 514, CAST_MSG(&fb_1149));
			   if (!fb_1149.fb_1150 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				if (first && optional_msg)
					isqlGlob.prints(optional_msg);

				first = false;
				fb_utils::exact_name(/*PRV.RDB$USER*/
						     fb_1149.fb_1154);

				switch (/*PRV.RDB$USER_TYPE*/
					fb_1149.fb_1153)
				{
				case obj_relation:
				case obj_view:
				case obj_trigger:
				case obj_procedure:
				case obj_udf:
				case obj_sql_role:
				case obj_package_header:
				case obj_user:
					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
								   fb_1149.fb_1154, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1149.fb_1154);
					break;
				default:
					strcpy(SQL_identifier, /*PRV.RDB$USER*/
							       fb_1149.fb_1154);
					break;
				}

				set_grantee(/*PRV.RDB$USER_TYPE*/
					    fb_1149.fb_1153, SQL_identifier, user_string);

				if (/*PRV.RDB$GRANT_OPTION*/
				    fb_1149.fb_1152)
					strcpy(with_option, " WITH GRANT OPTION");
				else
					with_option[0] = '\0';

				if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
				else
					strcpy(SQL_identifier, object);

				isqlGlob.printf("GRANT EXECUTE ON PROCEDURE %s TO %s%s%s%s%s",
						 SQL_identifier, user_string, with_option,
						 granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
									 fb_1149.fb_1155, /*PRV.RDB$GRANTOR.NULL*/
  fb_1149.fb_1151), terminator, NEWLINE);

			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR*/
			   }
			}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (!first)
			return (SKIP);
	}

	// No procedure called "object" was found, try role "object"
	SCHAR role_name[BUFFER_LENGTH256];

	if (obj_type == obj_sql_role || obj_type == obj_any)
	{
		// No procedure called "object" was found, try role "object"
		// CVC: This code could be superseded by SHOW_grant_roles() below
		// with the sole difference of the sort fields.
		// This part is only used by SHOW GRANT <object> command
		// Metadata extraction and SHOW GRANT with no param uses SHOW_grant_roles.

		/*FOR FIRST 1 R IN RDB$ROLES WITH R.RDB$ROLE_NAME EQ object*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_1137 && fbTrans && DB)
		      fb_1137 = DB->compileRequest(fbStatus, sizeof(fb_1138), fb_1138);
		   isc_vtov ((const char*) object, (char*) fb_1139.fb_1140, 253);
		   if (fbTrans && fb_1137)
		      fb_1137->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1139));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1137->release(); fb_1137 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_1137->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1141));
		   if (!fb_1141.fb_1142 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			/*FOR PRV IN RDB$USER_PRIVILEGES WITH
				PRV.RDB$GRANTOR NOT MISSING           AND
				PRV.RDB$OBJECT_TYPE   EQ obj_sql_role AND
				(PRV.RDB$USER_TYPE    EQ obj_user      OR
				 PRV.RDB$USER_TYPE    EQ obj_sql_role) AND
				PRV.RDB$RELATION_NAME EQ object       AND
				PRV.RDB$PRIVILEGE     EQ 'M'
				SORTED BY PRV.RDB$USER*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1121 && fbTrans && DB)
			      fb_1121 = DB->compileRequest(fbStatus, sizeof(fb_1122), fb_1122);
			   fb_1123.fb_1124 = obj_sql_role;
			   fb_1123.fb_1125 = obj_user;
			   fb_1123.fb_1126 = obj_sql_role;
			   isc_vtov ((const char*) object, (char*) fb_1123.fb_1127, 253);
			   if (fbTrans && fb_1121)
			      fb_1121->startAndSend(fbStatus, fbTrans, 0, 0, 259, CAST_CONST_MSG(&fb_1123));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1121->release(); fb_1121 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1121->receive (fbStatus, 0, 1, 1020, CAST_MSG(&fb_1128));
			   if (!fb_1128.fb_1129 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				fb_utils::exact_name(/*PRV.RDB$RELATION_NAME*/
						     fb_1128.fb_1133);
				strcpy (role_name, /*PRV.RDB$RELATION_NAME*/
						   fb_1128.fb_1133);
				if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(role_name, SQL_identifier, DBL_QUOTE);
				else
					strcpy(SQL_identifier, role_name);

				fb_utils::exact_name(/*PRV.RDB$USER*/
						     fb_1128.fb_1134);
				strcpy(user_string, /*PRV.RDB$USER*/
						    fb_1128.fb_1134);

				if (/*PRV.RDB$GRANT_OPTION*/
				    fb_1128.fb_1132)
					strcpy(with_option, " WITH ADMIN OPTION");
				else
					with_option[0] = '\0';

				const char* default_option = "";
				if (!/*PRV.RDB$FIELD_NAME.NULL*/
				     fb_1128.fb_1131 && /*PRV.RDB$FIELD_NAME*/
    fb_1128.fb_1135[0] == 'D')
					default_option = " DEFAULT";

				fb_utils::snprintf(Print_buffer, sizeof(Print_buffer), "GRANT%s %s TO %s%s%s%s%s",
						default_option, SQL_identifier,
						user_string, with_option, granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
												  fb_1128.fb_1136, /*PRV.RDB$GRANTOR.NULL*/
  fb_1128.fb_1130),
						terminator, NEWLINE);

				if (first && optional_msg)
					isqlGlob.prints(optional_msg);

				first = false;
				isqlGlob.prints(Print_buffer);

			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (!first)
			return (SKIP);
	}

	if (obj_type == obj_package_header || obj_type == obj_any)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			/*FOR FIRST 1 P IN RDB$PACKAGES WITH P.RDB$PACKAGE_NAME EQ object*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1115 && fbTrans && DB)
			      fb_1115 = DB->compileRequest(fbStatus, sizeof(fb_1116), fb_1116);
			   isc_vtov ((const char*) object, (char*) fb_1117.fb_1118, 253);
			   if (fbTrans && fb_1115)
			      fb_1115->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1117));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1115->release(); fb_1115 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1115->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1119));
			   if (!fb_1119.fb_1120 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
				/*FOR PRV IN RDB$USER_PRIVILEGES CROSS
					PACK IN RDB$PACKAGES WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_package_header AND
					PRV.RDB$RELATION_NAME EQ object AND
					PACK.RDB$PACKAGE_NAME EQ object AND
					PRV.RDB$PRIVILEGE EQ 'X' AND
					PACK.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_1102 && fbTrans && DB)
				      fb_1102 = DB->compileRequest(fbStatus, sizeof(fb_1103), fb_1103);
				   fb_1104.fb_1105 = obj_package_header;
				   isc_vtov ((const char*) object, (char*) fb_1104.fb_1106, 253);
				   isc_vtov ((const char*) object, (char*) fb_1104.fb_1107, 253);
				   if (fbTrans && fb_1102)
				      fb_1102->startAndSend(fbStatus, fbTrans, 0, 0, 508, CAST_CONST_MSG(&fb_1104));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1102->release(); fb_1102 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_1102->receive (fbStatus, 0, 1, 514, CAST_MSG(&fb_1108));
				   if (!fb_1108.fb_1109 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(/*PRV.RDB$USER*/
							     fb_1108.fb_1113);

					switch (/*PRV.RDB$USER_TYPE*/
						fb_1108.fb_1112)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
									   fb_1108.fb_1113, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, /*PRV.RDB$USER*/
									       fb_1108.fb_1113);
						break;
					default:
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1108.fb_1113);
						break;
					}

					set_grantee(/*PRV.RDB$USER_TYPE*/
						    fb_1108.fb_1112, SQL_identifier, user_string);

					if (/*PRV.RDB$GRANT_OPTION*/
					    fb_1108.fb_1111)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT EXECUTE ON PACKAGE %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
										 fb_1108.fb_1114, /*PRV.RDB$GRANTOR.NULL*/
  fb_1108.fb_1110),
							 terminator, NEWLINE);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				/*END_ERROR*/
				   }
				}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

			if (!first)
				return (SKIP);
		}
	}

	if (obj_type == obj_udf || obj_type == obj_any)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			/*FOR FIRST 1 F IN RDB$FUNCTIONS
			WITH F.RDB$FUNCTION_NAME EQ object AND
				 F.RDB$PACKAGE_NAME MISSING*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1096 && fbTrans && DB)
			      fb_1096 = DB->compileRequest(fbStatus, sizeof(fb_1097), fb_1097);
			   isc_vtov ((const char*) object, (char*) fb_1098.fb_1099, 253);
			   if (fbTrans && fb_1096)
			      fb_1096->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1098));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1096->release(); fb_1096 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1096->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1100));
			   if (!fb_1100.fb_1101 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				/*FOR PRV IN RDB$USER_PRIVILEGES CROSS
					FUN IN RDB$FUNCTIONS WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_udf AND
					PRV.RDB$RELATION_NAME EQ object AND
					FUN.RDB$FUNCTION_NAME EQ object AND
					FUN.RDB$PACKAGE_NAME MISSING AND
					PRV.RDB$PRIVILEGE EQ 'X' AND
					FUN.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_1083 && fbTrans && DB)
				      fb_1083 = DB->compileRequest(fbStatus, sizeof(fb_1084), fb_1084);
				   fb_1085.fb_1086 = obj_udf;
				   isc_vtov ((const char*) object, (char*) fb_1085.fb_1087, 253);
				   isc_vtov ((const char*) object, (char*) fb_1085.fb_1088, 253);
				   if (fbTrans && fb_1083)
				      fb_1083->startAndSend(fbStatus, fbTrans, 0, 0, 508, CAST_CONST_MSG(&fb_1085));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1083->release(); fb_1083 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_1083->receive (fbStatus, 0, 1, 514, CAST_MSG(&fb_1089));
				   if (!fb_1089.fb_1090 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(/*PRV.RDB$USER*/
							     fb_1089.fb_1094);

					switch (/*PRV.RDB$USER_TYPE*/
						fb_1089.fb_1093)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
									   fb_1089.fb_1094, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, /*PRV.RDB$USER*/
									       fb_1089.fb_1094);
						break;
					default:
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1089.fb_1094);
						break;
					}

					set_grantee(/*PRV.RDB$USER_TYPE*/
						    fb_1089.fb_1093, SQL_identifier, user_string);

					if (/*PRV.RDB$GRANT_OPTION*/
					    fb_1089.fb_1092)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT EXECUTE ON FUNCTION %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
										 fb_1089.fb_1095, /*PRV.RDB$GRANTOR.NULL*/
  fb_1089.fb_1091),
							 terminator, NEWLINE);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				/*END_ERROR*/
				   }
				}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

			if (!first)
				return (SKIP);
		}
	}

	if (obj_type == obj_generator || obj_type == obj_any)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			/*FOR FIRST 1 G IN RDB$GENERATORS WITH G.RDB$GENERATOR_NAME EQ object*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1077 && fbTrans && DB)
			      fb_1077 = DB->compileRequest(fbStatus, sizeof(fb_1078), fb_1078);
			   isc_vtov ((const char*) object, (char*) fb_1079.fb_1080, 253);
			   if (fbTrans && fb_1077)
			      fb_1077->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1079));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1077->release(); fb_1077 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1077->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1081));
			   if (!fb_1081.fb_1082 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
				/*FOR PRV IN RDB$USER_PRIVILEGES CROSS
					GEN IN RDB$GENERATORS WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_generator AND
					PRV.RDB$RELATION_NAME EQ object AND
					GEN.RDB$GENERATOR_NAME EQ object AND
					PRV.RDB$PRIVILEGE EQ 'G' AND
					GEN.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_1064 && fbTrans && DB)
				      fb_1064 = DB->compileRequest(fbStatus, sizeof(fb_1065), fb_1065);
				   fb_1066.fb_1067 = obj_generator;
				   isc_vtov ((const char*) object, (char*) fb_1066.fb_1068, 253);
				   isc_vtov ((const char*) object, (char*) fb_1066.fb_1069, 253);
				   if (fbTrans && fb_1064)
				      fb_1064->startAndSend(fbStatus, fbTrans, 0, 0, 508, CAST_CONST_MSG(&fb_1066));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1064->release(); fb_1064 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_1064->receive (fbStatus, 0, 1, 514, CAST_MSG(&fb_1070));
				   if (!fb_1070.fb_1071 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(/*PRV.RDB$USER*/
							     fb_1070.fb_1075);

					switch (/*PRV.RDB$USER_TYPE*/
						fb_1070.fb_1074)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
									   fb_1070.fb_1075, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, /*PRV.RDB$USER*/
									       fb_1070.fb_1075);
						break;
					default:
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1070.fb_1075);
						break;
					}

					set_grantee(/*PRV.RDB$USER_TYPE*/
						    fb_1070.fb_1074, SQL_identifier, user_string);

					if (/*PRV.RDB$GRANT_OPTION*/
					    fb_1070.fb_1073)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT USAGE ON SEQUENCE %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
										 fb_1070.fb_1076, /*PRV.RDB$GRANTOR.NULL*/
  fb_1070.fb_1072),
							 terminator, NEWLINE);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				/*END_ERROR*/
				   }
				}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

			if (!first)
				return (SKIP);
		}
	}

	if (obj_type == obj_exception || obj_type == obj_any)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			/*FOR FIRST 1 E IN RDB$EXCEPTIONS WITH E.RDB$EXCEPTION_NAME EQ object*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1058 && fbTrans && DB)
			      fb_1058 = DB->compileRequest(fbStatus, sizeof(fb_1059), fb_1059);
			   isc_vtov ((const char*) object, (char*) fb_1060.fb_1061, 253);
			   if (fbTrans && fb_1058)
			      fb_1058->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1060));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1058->release(); fb_1058 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1058->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1062));
			   if (!fb_1062.fb_1063 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
				/*FOR PRV IN RDB$USER_PRIVILEGES CROSS
					XCP IN RDB$EXCEPTIONS WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_exception AND
					PRV.RDB$RELATION_NAME EQ object AND
					XCP.RDB$EXCEPTION_NAME EQ object AND
					PRV.RDB$PRIVILEGE EQ 'G' AND
					XCP.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_1045 && fbTrans && DB)
				      fb_1045 = DB->compileRequest(fbStatus, sizeof(fb_1046), fb_1046);
				   fb_1047.fb_1048 = obj_exception;
				   isc_vtov ((const char*) object, (char*) fb_1047.fb_1049, 253);
				   isc_vtov ((const char*) object, (char*) fb_1047.fb_1050, 253);
				   if (fbTrans && fb_1045)
				      fb_1045->startAndSend(fbStatus, fbTrans, 0, 0, 508, CAST_CONST_MSG(&fb_1047));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1045->release(); fb_1045 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_1045->receive (fbStatus, 0, 1, 514, CAST_MSG(&fb_1051));
				   if (!fb_1051.fb_1052 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(/*PRV.RDB$USER*/
							     fb_1051.fb_1056);

					switch (/*PRV.RDB$USER_TYPE*/
						fb_1051.fb_1055)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
									   fb_1051.fb_1056, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, /*PRV.RDB$USER*/
									       fb_1051.fb_1056);
						break;
					default:
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1051.fb_1056);
						break;
					}

					set_grantee(/*PRV.RDB$USER_TYPE*/
						    fb_1051.fb_1055, SQL_identifier, user_string);

					if (/*PRV.RDB$GRANT_OPTION*/
					    fb_1051.fb_1054)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT USAGE ON EXCEPTION %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
										 fb_1051.fb_1057, /*PRV.RDB$GRANTOR.NULL*/
  fb_1051.fb_1053),
							 terminator, NEWLINE);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				/*END_ERROR*/
				   }
				}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

			if (!first)
				return (SKIP);
		}
	}

	/***
	if (obj_type == obj_field || obj_type == 255)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			FOR FIRST 1 F IN RDB$FIELDS WITH F.RDB$FIELD_NAME EQ object
				FOR PRV IN RDB$USER_PRIVILEGES CROSS
					FLD IN RDB$FIELDS WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_field AND
					PRV.RDB$RELATION_NAME EQ object AND
					FLD.RDB$FIELD_NAME EQ object AND
					PRV.RDB$PRIVILEGE EQ 'G' AND
					FLD.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(PRV.RDB$USER);

					switch (PRV.RDB$USER_TYPE)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(PRV.RDB$USER, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, PRV.RDB$USER);
						break;
					default:
						strcpy(SQL_identifier, PRV.RDB$USER);
						break;
					}

					set_grantee(PRV.RDB$USER_TYPE, SQL_identifier, user_string);

					if (PRV.RDB$GRANT_OPTION)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT USAGE ON DOMAIN %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, PRV.RDB$GRANTOR, PRV.RDB$GRANTOR.NULL),
							 terminator, NEWLINE);

				END_FOR
				ON_ERROR
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				END_ERROR
			END_FOR
			ON_ERROR
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			END_ERROR;

			if (!first)
				return (SKIP);
		}
	}

	if (obj_type == obj_charset || obj_type == 255)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			FOR FIRST 1 C IN RDB$CHARACTER_SETS WITH C.RDB$CHARACTER_SET_NAME EQ object
				FOR PRV IN RDB$USER_PRIVILEGES CROSS
					CS IN RDB$CHARACTER_SETS WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_charset AND
					PRV.RDB$RELATION_NAME EQ object AND
					CS.RDB$CHARACTER_SET_NAME EQ object AND
					PRV.RDB$PRIVILEGE EQ 'G' AND
					CS.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(PRV.RDB$USER);

					switch (PRV.RDB$USER_TYPE)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(PRV.RDB$USER, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, PRV.RDB$USER);
						break;
					default:
						strcpy(SQL_identifier, PRV.RDB$USER);
						break;
					}

					set_grantee(PRV.RDB$USER_TYPE, SQL_identifier, user_string);

					if (PRV.RDB$GRANT_OPTION)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT USAGE ON CHARACTER SET %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, PRV.RDB$GRANTOR, PRV.RDB$GRANTOR.NULL),
							 terminator, NEWLINE);

				END_FOR
				ON_ERROR
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				END_ERROR
			END_FOR
			ON_ERROR
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			END_ERROR;

			if (!first)
				return (SKIP);
		}
	}

	if (obj_type == obj_collation || obj_type == 255)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			FOR FIRST 1 C IN RDB$COLLATIONS WITH C.RDB$COLLATION_NAME EQ object
				FOR PRV IN RDB$USER_PRIVILEGES CROSS
					COL IN RDB$COLLATIONS WITH
					PRV.RDB$GRANTOR NOT MISSING AND
					PRV.RDB$OBJECT_TYPE = obj_collation AND
					PRV.RDB$RELATION_NAME EQ object AND
					COL.RDB$COLLATION_NAME EQ object AND
					PRV.RDB$PRIVILEGE EQ 'G' AND
					COL.RDB$OWNER_NAME NE PRV.RDB$USER
					SORTED BY PRV.RDB$USER, PRV.RDB$FIELD_NAME, PRV.RDB$GRANT_OPTION

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(PRV.RDB$USER);

					switch (PRV.RDB$USER_TYPE)
					{
					case obj_relation:
					case obj_view:
					case obj_trigger:
					case obj_procedure:
					case obj_udf:
					case obj_sql_role:
					case obj_package_header:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(PRV.RDB$USER, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, PRV.RDB$USER);
						break;
					default:
						strcpy(SQL_identifier, PRV.RDB$USER);
						break;
					}

					set_grantee(PRV.RDB$USER_TYPE, SQL_identifier, user_string);

					if (PRV.RDB$GRANT_OPTION)
						strcpy(with_option, " WITH GRANT OPTION");
					else
						with_option[0] = '\0';

					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(object, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, object);

					isqlGlob.printf("GRANT USAGE ON COLLATION %s TO %s%s%s%s%s",
							 SQL_identifier, user_string, with_option,
							 granted_by(buf_grantor, PRV.RDB$GRANTOR, PRV.RDB$GRANTOR.NULL),
							 terminator, NEWLINE);

				END_FOR
				ON_ERROR
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				END_ERROR
			END_FOR
			ON_ERROR
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			END_ERROR;

			if (!first)
				return (SKIP);
		}
	}
	***/

	if (isDdlObject(obj_type) || obj_type == obj_any)
	{
		if (isqlGlob.major_ods >= ODS_VERSION12)
		{
			/*FOR PRV IN RDB$USER_PRIVILEGES WITH
				PRV.RDB$GRANTOR NOT MISSING AND
				PRV.RDB$OBJECT_TYPE >= obj_database AND
				PRV.RDB$RELATION_NAME EQ object
				SORTED BY PRV.RDB$USER, PRV.RDB$GRANT_OPTION*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_1031 && fbTrans && DB)
			      fb_1031 = DB->compileRequest(fbStatus, sizeof(fb_1032), fb_1032);
			   fb_1033.fb_1034 = obj_database;
			   isc_vtov ((const char*) object, (char*) fb_1033.fb_1035, 253);
			   if (fbTrans && fb_1031)
			      fb_1031->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_1033));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1031->release(); fb_1031 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_1031->receive (fbStatus, 0, 1, 523, CAST_MSG(&fb_1036));
			   if (!fb_1036.fb_1037 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				// Double check if the object is DDL one.
				if (!isDdlObject(/*PRV.RDB$OBJECT_TYPE*/
						 fb_1036.fb_1041))
					continue;

				if (first && optional_msg)
					isqlGlob.prints(optional_msg);

				// re-initialize strings
				priv_string[0] = '\0';
				with_option[0] = '\0';
				priv_flags = 0;

				first = false;
				fb_utils::exact_name(/*PRV.RDB$USER*/
						     fb_1036.fb_1042);

				// Only the first character is used for permissions

				const char c = /*PRV.RDB$PRIVILEGE*/
					       fb_1036.fb_1043[0];

				switch (c)
				{
				case 'C':
					priv_flags |= priv_CREATE;
					break;
				case 'L':
					priv_flags |= priv_ALTER;
					break;
				case 'O':
					priv_flags |= priv_DROP;
					break;
				default:
					priv_flags |= priv_UNKNOWN;
					break;
				}

				make_priv_string(priv_flags, priv_string, (/*PRV.RDB$OBJECT_TYPE*/
									   fb_1036.fb_1041 != obj_database));

				switch (/*PRV.RDB$USER_TYPE*/
					fb_1036.fb_1040)
				{
				case obj_relation:
				case obj_view:
				case obj_trigger:
				case obj_procedure:
				case obj_udf:
				case obj_sql_role:
				case obj_package_header:
				case obj_user:
					if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
						IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
								   fb_1036.fb_1042, SQL_identifier, DBL_QUOTE);
					else
						strcpy(SQL_identifier, /*PRV.RDB$USER*/
								       fb_1036.fb_1042);
					break;
				default:
					strcpy(SQL_identifier, /*PRV.RDB$USER*/
							       fb_1036.fb_1042);
					break;
				}

				set_grantee(/*PRV.RDB$USER_TYPE*/
					    fb_1036.fb_1040, SQL_identifier, user_string);

				strcpy(obj_string, getDdlObjectName(/*PRV.RDB$OBJECT_TYPE*/
								    fb_1036.fb_1041));

				if (/*PRV.RDB$GRANT_OPTION*/
				    fb_1036.fb_1039)
					strcpy(with_option, " WITH GRANT OPTION");
				else
					with_option[0] = '\0';

				isqlGlob.printf("GRANT %s %s TO %s%s%s%s%s",
						 priv_string, obj_string, user_string, with_option,
						 granted_by(buf_grantor, /*PRV.RDB$GRANTOR*/
									 fb_1036.fb_1044, /*PRV.RDB$GRANTOR.NULL*/
  fb_1036.fb_1038),
						 terminator, NEWLINE);

			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR*/
			   }
			}

			if (obj_type == obj_database || obj_type == obj_any)
			{
				/*FOR PRV IN SEC$DB_CREATORS
					SORTED BY PRV.SEC$USER_TYPE, PRV.SEC$USER*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_1025 && fbTrans && DB)
				      fb_1025 = DB->compileRequest(fbStatus, sizeof(fb_1026), fb_1026);
				   if (fbTrans && fb_1025)
				      fb_1025->start(fbStatus, fbTrans, 0);
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1025->release(); fb_1025 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_1025->receive (fbStatus, 0, 0, 257, CAST_MSG(&fb_1027));
				   if (!fb_1027.fb_1028 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					if (first && optional_msg)
						isqlGlob.prints(optional_msg);

					first = false;
					fb_utils::exact_name(/*PRV.SEC$USER*/
							     fb_1027.fb_1030);

					switch (/*PRV.SEC$USER_TYPE*/
						fb_1027.fb_1029)
					{
					case obj_sql_role:
					case obj_user:
						if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
							IUTILS_copy_SQL_id(/*PRV.SEC$USER*/
									   fb_1027.fb_1030, SQL_identifier, DBL_QUOTE);
						else
							strcpy(SQL_identifier, /*PRV.SEC$USER*/
									       fb_1027.fb_1030);
						break;
					default:
						fb_assert(false);
						strcpy(SQL_identifier, /*PRV.SEC$USER*/
								       fb_1027.fb_1030);
						break;
					}

					set_grantee(/*PRV.SEC$USER_TYPE*/
						    fb_1027.fb_1029, SQL_identifier, user_string);

					isqlGlob.printf("GRANT CREATE DATABASE TO %s%s%s",
						user_string, terminator, NEWLINE);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				/*END_ERROR*/
				   }
				}
			}

			if (!first)
				return (SKIP);
		}
	}

	return OBJECT_NOT_FOUND;
}


void SHOW_grant_roles(const SCHAR* terminator, bool* first)
{
/**************************************
 *
 *	S H O W _ g r a n t _ r o l e s
 *
 **************************************
 *
 * Functional description
 *	Placeholder for SHOW_grant_roles2 without additional message.
 *
 **************************************/
	SHOW_grant_roles2 (terminator, first, 0, false);
}

void SHOW_grant_roles2 (const SCHAR* terminator,
						bool* first,
						const TEXT* optional_msg,
						bool mangle)
{
   struct fb_1017_struct {
          short fb_1018;	/* fbUtility */
          short fb_1019;	/* gds__null_flag */
          short fb_1020;	/* RDB$GRANT_OPTION */
          char  fb_1021 [253];	/* RDB$USER */
          char  fb_1022 [253];	/* RDB$FIELD_NAME */
          char  fb_1023 [253];	/* RDB$RELATION_NAME */
          char  fb_1024 [253];	/* RDB$GRANTOR */
   } fb_1017;
   struct fb_1013_struct {
          short fb_1014;	/* RDB$USER_TYPE */
          short fb_1015;	/* RDB$USER_TYPE */
          short fb_1016;	/* RDB$OBJECT_TYPE */
   } fb_1013;
/**************************************
 *
 *	S H O W _ g r a n t _ r o l e s
 *
 **************************************
 *
 * Functional description
 *	Show grants for each role name
 *	This function is also called by extract for privileges.
 *	All membership privilege may have the with_admin or/and default options set.
 *
 **************************************/
	TEXT SQL_identifier2[BUFFER_LENGTH256];
	/*BASED_ON RDB$USER_PRIVILEGES.RDB$GRANTOR dummy;*/
	char
	   dummy[253];
	// used to declare buf_grantor
	FB_UNUSED_VAR(dummy); // Silence compiler warning about unused variable
	SCHAR buf_grantor[sizeof(dummy) + 20];

	// process role "object"

	/*FOR PRV IN RDB$USER_PRIVILEGES WITH
		PRV.RDB$GRANTOR NOT MISSING           AND
		PRV.RDB$OBJECT_TYPE   EQ obj_sql_role AND
		(PRV.RDB$USER_TYPE    EQ obj_user      OR
		 PRV.RDB$USER_TYPE     EQ obj_sql_role) AND
		PRV.RDB$PRIVILEGE     EQ 'M'
		SORTED BY PRV.RDB$RELATION_NAME, PRV.RDB$USER*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_1011 && fbTrans && DB)
	      fb_1011 = DB->compileRequest(fbStatus, sizeof(fb_1012), fb_1012);
	   fb_1013.fb_1014 = obj_sql_role;
	   fb_1013.fb_1015 = obj_user;
	   fb_1013.fb_1016 = obj_sql_role;
	   if (fbTrans && fb_1011)
	      fb_1011->startAndSend(fbStatus, fbTrans, 0, 0, 6, CAST_CONST_MSG(&fb_1013));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1011->release(); fb_1011 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_1011->receive (fbStatus, 0, 1, 1018, CAST_MSG(&fb_1017));
	   if (!fb_1017.fb_1018 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			if (*first && optional_msg) {
				isqlGlob.prints(optional_msg);
			}
			*first = false;
		}

		const char* user_string = fb_utils::exact_name(/*PRV.RDB$USER*/
							       fb_1017.fb_1021);

		const char* with_option = "";
		if (/*PRV.RDB$GRANT_OPTION*/
		    fb_1017.fb_1020)
			with_option = " WITH ADMIN OPTION";
		const char* default_option = "";
		if (!/*PRV.RDB$FIELD_NAME.NULL*/
		     fb_1017.fb_1019 && /*PRV.RDB$FIELD_NAME*/
    fb_1017.fb_1022[0] == 'D')
			default_option = " DEFAULT";

		const char* role = fb_utils::exact_name(/*PRV.RDB$RELATION_NAME*/
							fb_1017.fb_1023);
		const char* grantor = fb_utils::exact_name(/*PRV.RDB$GRANTOR*/
							   fb_1017.fb_1024);
		if (mangle && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*PRV.RDB$RELATION_NAME*/
					    fb_1017.fb_1023, SQL_identifier, DBL_QUOTE);
			role = SQL_identifier;
			IUTILS_copy_SQL_id(/*PRV.RDB$USER*/
					   fb_1017.fb_1021, SQL_identifier2, DBL_QUOTE);
			user_string = SQL_identifier2;
		}
		isqlGlob.printf("GRANT%s %s TO %s%s%s%s%s", default_option, role,
			user_string, with_option, granted_by(buf_grantor, grantor, false), terminator, NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
	/*END_ERROR;*/
	   }
	}

}


void SHOW_print_metadata_text_blob(FILE* fp, ISC_QUAD* blobid, bool escape_squote, bool avoid_end_in_single_line_comment)
{
/**************************************
 *
 *	S H O W _ p r i n t _ m e t a d a t a _ t e x t _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Print a Blob that is known to be metadata text.
 *	The last param says whether single quotes should be escaped (duplicating them).
 *
 **************************************/

	// Don't bother with null blobs
	if (UserBlob::blobIsNull(*blobid))
		return;

	Firebird::IBlob* blob = DB->openBlob(fbStatus, fbTrans, blobid,
		sizeof(metadata_text_bpb), metadata_text_bpb);
	if (ISQL_errmsg(fbStatus))
		return;

	string fullText;
	SCHAR buffer[BUFFER_LENGTH512];
	bool endedWithCr = false;

	while (true)
	{
		unsigned int length;
		int cc = blob->getSegment(fbStatus, sizeof(buffer) - 1, buffer, &length);
		if (cc == Firebird::IStatus::RESULT_NO_DATA || cc == Firebird::IStatus::RESULT_ERROR)
			break;

		// ASF: In Windows, \n characters are printed as \r\n in text mode.
		// If the original string has \r\n, they're printed as \r\r\n, resulting
		// in mixed/wrong line endings. So here, we filter any \r present just
		// before \n.
		bool prevEndedWithCr = endedWithCr;

		if ((endedWithCr = length != 0 && buffer[length - 1] == '\r'))
			--length;

		buffer[length] = 0;

		for (SCHAR* p = buffer; p < buffer + length - 1; ++p)
		{
			if (p[0] == '\r' && p[1] == '\n')
			{
				memmove(p, p + 1, length - (p - buffer));
				--length;
			}
		}

		if (prevEndedWithCr && buffer[0] != '\n')
		{
			fputc('\r', fp);
			if (avoid_end_in_single_line_comment)
				fullText += "\r";
		}

		if (escape_squote)
		{
			for (const UCHAR* p = (UCHAR*) buffer; *p; ++p)
			{
				if (*p == SINGLE_QUOTE)
				{
					fputc(*p, fp);
					if (avoid_end_in_single_line_comment)
						fullText += *p;
				}

				fputc(*p, fp);
				if (avoid_end_in_single_line_comment)
					fullText += *p;
			}
			fflush(fp);
		}
		else
		{
			IUTILS_printf(fp, buffer);
			if (avoid_end_in_single_line_comment)
				fullText += buffer;
		}
	}

	if (endedWithCr)
	{
		fputc('\r', fp);
		if (avoid_end_in_single_line_comment)
			fullText += "\r";
	}

	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		ISQL_errmsg(fbStatus);

	blob->close(fbStatus);

	if (avoid_end_in_single_line_comment && ISQL_statement_ends_in_comment(fullText.c_str()))
		fputc('\n', fp);
}


processing_state SHOW_metadata(const SCHAR* const* cmd, SCHAR** lcmd)
{
   struct fb_943_struct {
          short fb_944;	/* fbUtility */
   } fb_943;
   struct fb_941_struct {
          char  fb_942 [253];	/* RDB$RELATION_NAME */
   } fb_941;
   struct fb_949_struct {
          short fb_950;	/* fbUtility */
   } fb_949;
   struct fb_947_struct {
          char  fb_948 [253];	/* RDB$PACKAGE_NAME */
   } fb_947;
   struct fb_955_struct {
          short fb_956;	/* fbUtility */
   } fb_955;
   struct fb_953_struct {
          char  fb_954 [253];	/* RDB$COLLATION_NAME */
   } fb_953;
   struct fb_961_struct {
          short fb_962;	/* fbUtility */
   } fb_961;
   struct fb_959_struct {
          char  fb_960 [253];	/* RDB$CHARACTER_SET_NAME */
   } fb_959;
   struct fb_967_struct {
          short fb_968;	/* fbUtility */
   } fb_967;
   struct fb_965_struct {
          char  fb_966 [253];	/* RDB$FIELD_NAME */
   } fb_965;
   struct fb_973_struct {
          short fb_974;	/* fbUtility */
   } fb_973;
   struct fb_971_struct {
          char  fb_972 [253];	/* RDB$EXCEPTION_NAME */
   } fb_971;
   struct fb_979_struct {
          short fb_980;	/* fbUtility */
   } fb_979;
   struct fb_977_struct {
          char  fb_978 [253];	/* RDB$GENERATOR_NAME */
   } fb_977;
   struct fb_985_struct {
          short fb_986;	/* fbUtility */
   } fb_985;
   struct fb_983_struct {
          char  fb_984 [253];	/* RDB$FUNCTION_NAME */
   } fb_983;
   struct fb_991_struct {
          short fb_992;	/* fbUtility */
   } fb_991;
   struct fb_989_struct {
          char  fb_990 [253];	/* RDB$ROLE_NAME */
   } fb_989;
   struct fb_997_struct {
          short fb_998;	/* fbUtility */
   } fb_997;
   struct fb_995_struct {
          char  fb_996 [253];	/* RDB$PROCEDURE_NAME */
   } fb_995;
   struct fb_1003_struct {
          short fb_1004;	/* fbUtility */
   } fb_1003;
   struct fb_1001_struct {
          char  fb_1002 [253];	/* RDB$RELATION_NAME */
   } fb_1001;
   struct fb_1009_struct {
          short fb_1010;	/* fbUtility */
   } fb_1009;
   struct fb_1007_struct {
          char  fb_1008 [253];	/* RDB$RELATION_NAME */
   } fb_1007;
/**************************************
 *
 *	S H O W _ m e t a d a t a
 *
 **************************************
 *
 * Functional description
 *	If somebody presses the show ..., come here to
 *	interpret the desired command.
 *	Paramters:
 *	cmd -- Array of words for the command
 *
 **************************************/

	class ShowOptions : public OptionsBase
	{
	public:
		enum show_commands
		{
			role, table, view, system, index, domain, exception,
			filter, function, generator, grant, procedure, trigger,
			check, database, comment, dependency, collation, security_class,
			users, package, publication, schema, map, wrong
		};
		ShowOptions(const optionsMap* inmap, size_t insize, int wrongval)
			: OptionsBase(inmap, insize, wrongval)
		{}
	};

	static const ShowOptions::optionsMap options[] =
	{
		//{role, "ROLE"},
		{ShowOptions::role, "ROLES", 4},
		//{table, "TABLE"},
		{ShowOptions::table, "TABLES", 5},
		//{view, "VIEW"},
		{ShowOptions::view, "VIEWS", 4},
		//{system, "SYS"},
		{ShowOptions::system, "SYSTEM", 3},
		//{index, "IND"},
		{ShowOptions::index, "INDEXES", 3},
		{ShowOptions::index, "INDICES", 0},
		//{domain, "DOMAIN"},
		{ShowOptions::domain, "DOMAINS", 6},
		//{exception, "EXCEPTION"},
		{ShowOptions::exception, "EXCEPTIONS", 5},
		//{filter, "FILTER"},
		{ShowOptions::filter, "FILTERS", 6},
		//{function, "FUNCTION"},
		{ShowOptions::function, "FUNCTIONS", 4},
		//{generator, "GEN"},
		//{generator, "GENERATOR"},
		{ShowOptions::generator, "GENERATORS", 3},
		//{generator, "SEQ"},
		//{generator, "SEQUENCE"},
		{ShowOptions::generator, "SEQUENCES", 3},
		//{grant, "GRANT"},
		{ShowOptions::grant, "GRANTS", 5},
		//{procedure, "PROC"},
		//{procedure, "PROCEDURE"},
		{ShowOptions::procedure, "PROCEDURES", 4},
		//{trigger, "TRIG"},
		//{trigger, "TRIGGER"},
		{ShowOptions::trigger, "TRIGGERS", 4},
		//{check, "CHECK"},
		{ShowOptions::check, "CHECKS", 5},
		{ShowOptions::database, "DB", 0},
		{ShowOptions::database, "DATABASE", 0},
		//{comment, "COMMENT"},
		{ShowOptions::comment, "COMMENTS", 7},
		{ShowOptions::dependency, "DEPENDENCY", 5},
		{ShowOptions::dependency, "DEPENDENCIES", 5},
		{ShowOptions::collation, "COLLATES", 7},
		{ShowOptions::collation, "COLLATIONS", 9},
		{ShowOptions::security_class, "SECURITY CLASSES", 12},
		{ShowOptions::security_class, "SECCLASSES", 6},
		{ShowOptions::users, "USERS", 0},
		{ShowOptions::package, "PACKAGES", 4},
		{ShowOptions::schema, "SCHEMAS", 4},
		{ShowOptions::map, "MAPPING", 3},
		{ShowOptions::publication, "PUBLICATIONS", 3}
	};

	const ShowOptions showoptions(options, FB_NELEM(options), ShowOptions::wrong);


	// Can't show nothing, return an error

	if (!cmd[1] || !*cmd[1])
	{
		TEXT msg_string[MSG_LENGTH];
		IUTILS_msg_get(VALID_OPTIONS, msg_string);
		isqlGlob.printf("%s\n", msg_string);
		showoptions.showCommands(isqlGlob.Out);
		return ps_ERR;
	}

	processing_state ret = SKIP;
	// Only show version and show sql dialect work if there is no db attached
	bool handled = true;
	if ((!strcmp(cmd[1], "VERSION")) || (!strcmp(cmd[1], "VER")))
	{
		TEXT msg_string[MSG_LENGTH];
		IUTILS_msg_get(VERSION, msg_string, SafeArg() << FB_VERSION);
		isqlGlob.printf("%s%s", msg_string, NEWLINE);
		isqlGlob.printf("Server version:%s", NEWLINE);
		VersionCallback callback;
		Firebird::UtilInterfacePtr()->getFbVersion(fbStatus, DB, &callback);
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		{
			IUTILS_msg_get(CANNOT_GET_SRV_VER, msg_string);
			STDERROUT(msg_string);
		}
	}
	else if (!strcmp(cmd[1], "SQL"))
	{
		if (!strcmp(cmd[2], "DIALECT"))
			ret = show_dialect();
		else
			ret = ps_ERR;
	}
	else
	{
		handled = false;
		if (!ISQL_dbcheck())
			ret = ps_ERR;
	}

	if (ret == ps_ERR || handled)
		return ret;

	TEXT SQL_id_for_grant[BUFFER_LENGTH256];
	int key = 0;

	switch (showoptions.getCommand(cmd[1]))
	{
	case ShowOptions::role:
		if (isqlGlob.major_ods >= ODS_VERSION9)
		{
			if (*cmd[2])
			{
				if (*cmd[2] == '"')
				{
					remove_delimited_double_quotes(lcmd[2]);
					ret = show_role(lcmd[2], false);
				}
				else
				{
					ret = show_role(cmd[2], false);
				}

				if (ret == OBJECT_NOT_FOUND)
					key = NO_ROLE;
			}
			else
			{
				ret = show_role(NULL, false);
				if (ret == OBJECT_NOT_FOUND)
					key = NO_ROLES;
			}
		}
		else
		{
			ret = OBJECT_NOT_FOUND;
			key = NO_ROLES;
		}
		break;

	case ShowOptions::table:
		if (*cmd[2])
		{
			if (*cmd[2] == '"')
			{
				remove_delimited_double_quotes(lcmd[2]);
				ret = show_table(lcmd[2], false);
			}
			else
			{
				ret = show_table(cmd[2], false);
			}

			if (ret == OBJECT_NOT_FOUND)
				key = NO_TABLE;
		}
		else
		{
			ret = show_all_tables(0);
			if (ret == OBJECT_NOT_FOUND)
				key = NO_TABLES;
		}
		break;

	case ShowOptions::view:
		if (*cmd[2])
		{
			if (*cmd[2] == '"')
			{
				remove_delimited_double_quotes(lcmd[2]);
				ret = show_table(lcmd[2], true);
			}
			else
			{
				ret = show_table(cmd[2], true);
			}

			if (ret == OBJECT_NOT_FOUND)
				key = NO_VIEW;
		}
		else
		{
			ret = show_all_tables(-1);
			if (ret == OBJECT_NOT_FOUND)
				key = NO_VIEWS;
		}
		break;

	case ShowOptions::system:
		if (*cmd[2])
		{
			switch (showoptions.getCommand(cmd[2]))
			{
			case ShowOptions::collation:
				show_collations("", 1);
				break;

			case ShowOptions::function:
				show_sys_functions(NULL);
				break;

			case ShowOptions::table:
				show_all_tables(1);
				break;

			case ShowOptions::role:
				show_role(NULL, true);
				break;

			case ShowOptions::procedure:
				show_proc(NULL, false, true);
				break;

			case ShowOptions::package:
				show_packages(NULL, true);
				break;

			case ShowOptions::publication:
				show_publications(NULL, true);
				break;

			default:
				return ps_ERR;
			}
		}
		else
		{
			TEXT msg[MSG_LENGTH];
			IUTILS_msg_get(MSG_TABLES, msg);
			isqlGlob.printf("%s%s", msg, NEWLINE);
			show_all_tables(1);
			IUTILS_msg_get(MSG_FUNCTIONS, msg);
			show_sys_functions(msg);
			IUTILS_msg_get(MSG_PROCEDURES, msg);
			show_proc(NULL, false, true, msg);
			IUTILS_msg_get(MSG_PACKAGES, msg);
			show_packages(NULL, true, msg);
			IUTILS_msg_get(MSG_COLLATIONS, msg);
			show_collations("", 1, msg, true);
			IUTILS_msg_get(MSG_ROLES, msg);
			show_role(NULL, true, msg);
			IUTILS_msg_get(MSG_PUBLICATIONS, msg);
			show_publications(NULL, true, msg);
		}
		break;

	case ShowOptions::index:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_indices(lcmd);
		}
		else
		{
			ret = show_indices(cmd);
		}

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
			{
				/*FOR FIRST 1 R IN RDB$RELATIONS
					WITH R.RDB$RELATION_NAME EQ cmd[2]*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_1005 && fbTrans && DB)
				      fb_1005 = DB->compileRequest(fbStatus, sizeof(fb_1006), fb_1006);
				   isc_vtov ((const char*) cmd[2], (char*) fb_1007.fb_1008, 253);
				   if (fbTrans && fb_1005)
				      fb_1005->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1007));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_1005->release(); fb_1005 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_1005->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1009));
				   if (!fb_1009.fb_1010 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					key = NO_INDICES_ON_REL;
				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					// Ignore any error
				/*END_ERROR;*/
				   }
				}
				if (!key)
					key = NO_REL_OR_INDEX;
			}
			else
			{
				key = NO_INDICES;
			}
		}
		break;

	case ShowOptions::domain:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_domains(lcmd[2]);
		}
		else
			ret = show_domains(cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_DOMAIN;
			else
				key = NO_DOMAINS;
		}
		break;

	case ShowOptions::exception:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_exceptions(lcmd[2]);
		}
		else
			ret = show_exceptions(cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_EXCEPTION;
			else
				key = NO_EXCEPTIONS;
		}
		break;

	case ShowOptions::filter:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_filters(lcmd[2]);
		}
		else
			ret = show_filters(cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_FILTER;
			else
				key = NO_FILTERS;
		}
		break;

	case ShowOptions::function:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_functions(lcmd[2], true);
		}
		else
			ret = show_functions(cmd[2], false);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_FUNCTION;
			else
				key = NO_FUNCTIONS;
		}
		break;

	case ShowOptions::generator:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_generators(lcmd[2]);
		}
		else
			ret = show_generators(cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_GEN;
			else
				key = NO_GENS;
		}
		break;

	case ShowOptions::grant:
		if (*cmd[2])
		{
			if (*cmd[2] == '"')
			{
				remove_delimited_double_quotes(lcmd[2]);
				strcpy(SQL_id_for_grant, lcmd[2]);
			}
			else
				strcpy(SQL_id_for_grant, cmd[2]);
			ret = SHOW_grants(SQL_id_for_grant, "", obj_any);
		}
		else
		{
			strcpy(SQL_id_for_grant, cmd[2]);
			ret = EXTRACT_list_grants("");
		}

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
			{
				/*FOR FIRST 1 R IN RDB$RELATIONS
					WITH R.RDB$RELATION_NAME EQ SQL_id_for_grant*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_999 && fbTrans && DB)
				      fb_999 = DB->compileRequest(fbStatus, sizeof(fb_1000), fb_1000);
				   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_1001.fb_1002, 253);
				   if (fbTrans && fb_999)
				      fb_999->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_1001));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_999->release(); fb_999 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_999->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_1003));
				   if (!fb_1003.fb_1004 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					key = NO_GRANT_ON_REL;
				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					// Ignore any error
				/*END_ERROR;*/
				   }
				}
				if (!key)
				{
					/*FOR FIRST 1 P IN RDB$PROCEDURES
						WITH P.RDB$PROCEDURE_NAME EQ SQL_id_for_grant AND
							 P.RDB$PACKAGE_NAME MISSING*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_993 && fbTrans && DB)
					      fb_993 = DB->compileRequest(fbStatus, sizeof(fb_994), fb_994);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_995.fb_996, 253);
					   if (fbTrans && fb_993)
					      fb_993->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_995));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_993->release(); fb_993 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_993->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_997));
					   if (!fb_997.fb_998 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_PROC;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 R IN RDB$ROLES
						WITH R.RDB$ROLE_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_987 && fbTrans && DB)
					      fb_987 = DB->compileRequest(fbStatus, sizeof(fb_988), fb_988);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_989.fb_990, 253);
					   if (fbTrans && fb_987)
					      fb_987->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_989));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_987->release(); fb_987 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_987->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_991));
					   if (!fb_991.fb_992 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_ROL;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 F IN RDB$FUNCTIONS
						WITH F.RDB$FUNCTION_NAME EQ SQL_id_for_grant AND
							 F.RDB$PACKAGE_NAME MISSING*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_981 && fbTrans && DB)
					      fb_981 = DB->compileRequest(fbStatus, sizeof(fb_982), fb_982);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_983.fb_984, 253);
					   if (fbTrans && fb_981)
					      fb_981->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_983));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_981->release(); fb_981 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_981->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_985));
					   if (!fb_985.fb_986 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_FUN;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 G IN RDB$GENERATORS
						WITH G.RDB$GENERATOR_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_975 && fbTrans && DB)
					      fb_975 = DB->compileRequest(fbStatus, sizeof(fb_976), fb_976);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_977.fb_978, 253);
					   if (fbTrans && fb_975)
					      fb_975->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_977));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_975->release(); fb_975 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_975->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_979));
					   if (!fb_979.fb_980 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_GEN;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 E IN RDB$EXCEPTIONS
						WITH E.RDB$EXCEPTION_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_969 && fbTrans && DB)
					      fb_969 = DB->compileRequest(fbStatus, sizeof(fb_970), fb_970);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_971.fb_972, 253);
					   if (fbTrans && fb_969)
					      fb_969->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_971));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_969->release(); fb_969 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_969->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_973));
					   if (!fb_973.fb_974 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_XCP;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 F IN RDB$FIELDS
						WITH F.RDB$FIELD_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_963 && fbTrans && DB)
					      fb_963 = DB->compileRequest(fbStatus, sizeof(fb_964), fb_964);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_965.fb_966, 253);
					   if (fbTrans && fb_963)
					      fb_963->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_965));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_963->release(); fb_963 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_963->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_967));
					   if (!fb_967.fb_968 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_FLD;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 CS IN RDB$CHARACTER_SETS
						WITH CS.RDB$CHARACTER_SET_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_957 && fbTrans && DB)
					      fb_957 = DB->compileRequest(fbStatus, sizeof(fb_958), fb_958);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_959.fb_960, 253);
					   if (fbTrans && fb_957)
					      fb_957->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_959));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_957->release(); fb_957 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_957->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_961));
					   if (!fb_961.fb_962 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_CS;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
				{
					/*FOR FIRST 1 C IN RDB$COLLATIONS
						WITH C.RDB$COLLATION_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_951 && fbTrans && DB)
					      fb_951 = DB->compileRequest(fbStatus, sizeof(fb_952), fb_952);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_953.fb_954, 253);
					   if (fbTrans && fb_951)
					      fb_951->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_953));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_951->release(); fb_951 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_951->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_955));
					   if (!fb_955.fb_956 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_COLL;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key && isqlGlob.major_ods >= ODS_VERSION12)
				{
					/*FOR FIRST 1 P IN RDB$PACKAGES
						WITH P.RDB$PACKAGE_NAME EQ SQL_id_for_grant*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_945 && fbTrans && DB)
					      fb_945 = DB->compileRequest(fbStatus, sizeof(fb_946), fb_946);
					   isc_vtov ((const char*) SQL_id_for_grant, (char*) fb_947.fb_948, 253);
					   if (fbTrans && fb_945)
					      fb_945->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_947));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_945->release(); fb_945 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_945->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_949));
					   if (!fb_949.fb_950 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

						key = NO_GRANT_ON_PKG;
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						// Ignore any error
					/*END_ERROR;*/
					   }
					}
				}
				if (!key)
					key = NO_OBJECT;
			}
			else {
				key = NO_GRANT_ON_ANY;
			}
		}
		break;

	case ShowOptions::procedure:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_proc(lcmd[2], true, false);
		}
		else
			ret = show_proc(cmd[2], false, false);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_PROC;
			else
				key = NO_PROCS;
		}
		break;

	case ShowOptions::trigger:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_trigger(lcmd[2], true, true);
		}
		else
			ret = show_trigger(cmd[2], true, true);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
			{
				/*
				FOR FIRST 1 R IN RDB$RELATIONS
					WITH R.RDB$RELATION_NAME EQ cmd[2]

					key = NO_TRIGGERS_ON_REL;
				END_FOR
				ON_ERROR
					// Ignore any error
				END_ERROR;
				if (!key)
					key = NO_REL_OR_TRIGGER;
				*/
				key = NO_TRIGGER;
			}
			else
				key = NO_TRIGGERS;
		}
		break;

	case ShowOptions::check:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_check(lcmd[2]);
		}
		else
			ret = show_check(cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
			{
				/*FOR FIRST 1 R IN RDB$RELATIONS
					WITH R.RDB$RELATION_NAME EQ cmd[2]*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_939 && fbTrans && DB)
				      fb_939 = DB->compileRequest(fbStatus, sizeof(fb_940), fb_940);
				   isc_vtov ((const char*) cmd[2], (char*) fb_941.fb_942, 253);
				   if (fbTrans && fb_939)
				      fb_939->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_941));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_939->release(); fb_939 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_939->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_943));
				   if (!fb_943.fb_944 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					key = NO_CHECKS_ON_REL;
				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					// Ignore any error
				/*END_ERROR;*/
				   }
				}
			}
			if (!key)
				key = NO_TABLE;
		}
		break;

	case ShowOptions::database:
		show_db();
		break;

	case ShowOptions::comment:
		ret = show_comments(cmmShow, 0);
		if (ret == OBJECT_NOT_FOUND)
			key = NO_COMMENTS;
		break;

	case ShowOptions::collation:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_collations(lcmd[2], -1);
		}
		else
			ret = show_collations(cmd[2], 0);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_COLLATION;
			else
				key = NO_COLLATIONS;
		}
		break;

	case ShowOptions::dependency:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_dependencies(lcmd[2]);
		}
		else
			ret = show_dependencies(cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
			key = NO_DEPENDENCIES;
		break;

	case ShowOptions::security_class:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_secclass(lcmd[2], cmd[3]);
		}
		else
			ret = show_secclass(cmd[2], cmd[3]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (!strcmp(cmd[2], "*"))
				key = NO_DB_WIDE_SECCLASS;
			else
				key = NO_SECCLASS;
		}
		break;

	case ShowOptions::users:
		ret = show_users();
		if (ret == OBJECT_NOT_FOUND) // It seems impossible, but...
			key = NO_CONNECTED_USERS;
		break;

	case ShowOptions::package:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_packages(lcmd[2], false);
		}
		else
			ret = show_packages(cmd[2], false);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_PACKAGE;
			else
				key = NO_PACKAGES;
		}
		break;

	case ShowOptions::map:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = SHOW_maps(false, lcmd[2]);
		}
		else
			ret = SHOW_maps(false, cmd[2]);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_MAP;
			else
				key = NO_MAPS;
		}
		break;

	case ShowOptions::publication:
		if (*cmd[2] == '"')
		{
			remove_delimited_double_quotes(lcmd[2]);
			ret = show_publications(lcmd[2], false);
		}
		else
			ret = show_publications(cmd[2], false);

		if (ret == OBJECT_NOT_FOUND)
		{
			if (*cmd[2])
				key = NO_PUBLICATION;
			else
				key = NO_PUBLICATIONS;
		}
		break;

	case ShowOptions::schema:
		return ps_ERR;
		break;

	default:
		return ps_ERR;
	} // switch

	if (ret == OBJECT_NOT_FOUND)
	{
		TEXT key_string[MSG_LENGTH];
		if (*cmd[2] == '"')
			IUTILS_msg_get(key, key_string, SafeArg() << lcmd[2]);
		else
			IUTILS_msg_get(key, key_string, SafeArg() << cmd[2]);
		STDERROUT(key_string);
	}

	return ret;
}


static void remove_delimited_double_quotes(TEXT* string)
{
/**************************************
 *
 *	r e m o v e _ d e l i m i t e d _ d o u b l e _ q u o t e s
 *
 **************************************
 *
 * Functional description
 *	Remove the delimited double quotes. Blanks could be part of
 *	delimited SQL identifier. Unescape embedded double quotes.
 *
 **************************************/
	IUTILS_remove_and_unescape_quotes(string, DBL_QUOTE);
}


static void make_priv_string(USHORT flags, char* string, bool useAny)
{
/**************************************
 *
 *	m a k e _ p r i v _ s t r i n g
 *
 **************************************
 *
 * Functional description
 *	Given a bit-vector of privileges, turn it into a
 *	string list.
 *
 **************************************/
	if (flags == RELATION_PRIV_ALL)
	{
		strcat(string, "ALL");
		return;
	}

	for (int i = 0; privs[i].priv_string; i++)
	{
		if (flags & privs[i].priv_flag)
		{
			if (*string)
				strcat(string, ", ");

			strcat(string, privs[i].priv_string);

			if (useAny && (privs[i].priv_flag == priv_ALTER || privs[i].priv_flag == priv_DROP))
				strcat(string, " ANY");
		}
	}
}


static processing_state show_all_tables(SSHORT sys_flag)
{
   struct fb_931_struct {
          short fb_932;	/* fbUtility */
          char  fb_933 [253];	/* RDB$RELATION_NAME */
   } fb_931;
   struct fb_929_struct {
          short fb_930;	/* RDB$SYSTEM_FLAG */
   } fb_929;
   struct fb_936_struct {
          short fb_937;	/* fbUtility */
          char  fb_938 [253];	/* RDB$RELATION_NAME */
   } fb_936;
/**************************************
 *
 *	s h o w _ a l l _ t a b l e s
 *
 **************************************
 *
 *	Print the names of all user tables from
 *	rdb$relations.  We use a dynamic query
 *
 *	Parameters:  sys_flag -- 0, show user tables
 *	1, show system tables only; -1, show views only
 *
 **************************************/
	bool first = true;

	if (sys_flag == -1)
	{
		// Views
		/*FOR REL IN RDB$RELATIONS WITH
			REL.RDB$VIEW_BLR NOT MISSING
			SORTED BY REL.RDB$RELATION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_934 && fbTrans && DB)
		      fb_934 = DB->compileRequest(fbStatus, sizeof(fb_935), fb_935);
		   if (fbTrans && fb_934)
		      fb_934->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_934->release(); fb_934 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_934->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_936));
		   if (!fb_936.fb_937 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			first = false;
			isqlGlob.printf("%s%s", fb_utils::exact_name(/*REL.RDB$RELATION_NAME*/
								     fb_936.fb_938), NEWLINE);
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}
	else // 23-Apr-2004 (only tables)
	{
		// The rdb$system_flag is not always set for non-system objects... this
		// query may potentially fail.
		/*FOR REL IN RDB$RELATIONS WITH
			(REL.RDB$SYSTEM_FLAG EQ sys_flag
				*//*OR (sys_flag == 0 AND REL.RDB$SYSTEM_FLAG MISSING)*//*)
			AND REL.RDB$VIEW_BLR MISSING
			SORTED BY REL.RDB$RELATION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_927 && fbTrans && DB)
		      fb_927 = DB->compileRequest(fbStatus, sizeof(fb_928), fb_928);
		   fb_929.fb_930 = sys_flag;
		   if (fbTrans && fb_927)
		      fb_927->startAndSend(fbStatus, fbTrans, 0, 0, 2, CAST_CONST_MSG(&fb_929));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_927->release(); fb_927 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_927->receive (fbStatus, 0, 1, 255, CAST_MSG(&fb_931));
		   if (!fb_931.fb_932 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			first = false;
			isqlGlob.printf("%s%s", fb_utils::exact_name(/*REL.RDB$RELATION_NAME*/
								     fb_931.fb_933), NEWLINE);
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}

	if (!first)
	{
		isqlGlob.printf(NEWLINE);
		return SKIP;
	}

	return OBJECT_NOT_FOUND;
}


static void show_charsets(SSHORT char_set_id, SSHORT collation)
{
   struct fb_922_struct {
          short fb_923;	/* fbUtility */
          char  fb_924 [253];	/* RDB$CHARACTER_SET_NAME */
          char  fb_925 [253];	/* RDB$COLLATION_NAME */
          char  fb_926 [253];	/* RDB$DEFAULT_COLLATE_NAME */
   } fb_922;
   struct fb_919_struct {
          short fb_920;	/* RDB$CHARACTER_SET_ID */
          short fb_921;	/* RDB$COLLATION_ID */
   } fb_919;
/*************************************
*
*	s h o w _ c h a r s e t s
*
**************************************
*
* Functional description
*	Show names of character set and collations
*
**************************************/
#ifdef	DEV_BUILD
	bool found = false;
#endif

	/*FOR COL IN RDB$COLLATIONS CROSS
		CST IN RDB$CHARACTER_SETS WITH
		COL.RDB$CHARACTER_SET_ID EQ CST.RDB$CHARACTER_SET_ID AND
		COL.RDB$COLLATION_ID EQ collation AND
		CST.RDB$CHARACTER_SET_ID EQ char_set_id*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_917 && fbTrans && DB)
	      fb_917 = DB->compileRequest(fbStatus, sizeof(fb_918), fb_918);
	   fb_919.fb_920 = char_set_id;
	   fb_919.fb_921 = collation;
	   if (fbTrans && fb_917)
	      fb_917->startAndSend(fbStatus, fbTrans, 0, 0, 4, CAST_CONST_MSG(&fb_919));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_917->release(); fb_917 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_917->receive (fbStatus, 0, 1, 761, CAST_MSG(&fb_922));
	   if (!fb_922.fb_923 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

#ifdef DEV_BUILD
		found = true;
#endif
		fb_utils::exact_name(/*CST.RDB$CHARACTER_SET_NAME*/
				     fb_922.fb_924);
		fb_utils::exact_name(/*COL.RDB$COLLATION_NAME*/
				     fb_922.fb_925);
		fb_utils::exact_name(/*CST.RDB$DEFAULT_COLLATE_NAME*/
				     fb_922.fb_926);

		if (strcmp(/*CST.RDB$DEFAULT_COLLATE_NAME*/
			   fb_922.fb_926, /*COL.RDB$COLLATION_NAME*/
  fb_922.fb_925) == 0 &&
			strcmp(/*CST.RDB$CHARACTER_SET_NAME*/
			       fb_922.fb_924, /*COL.RDB$COLLATION_NAME*/
  fb_922.fb_925) == 0)
		{
			// Collation is default and match charset name - do not show it.
			isqlGlob.printf(" CHARACTER SET %s", /*CST.RDB$CHARACTER_SET_NAME*/
							     fb_922.fb_924);
		}
		else
		{
			isqlGlob.printf(" CHARACTER SET %s COLLATE %s", /*CST.RDB$CHARACTER_SET_NAME*/
									fb_922.fb_924, /*COL.RDB$COLLATION_NAME*/
  fb_922.fb_925);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}
#ifdef DEV_BUILD
	if (!found)
	{
		TEXT Print_buffer[PRINT_BUFFER_LENGTH];
		sprintf(Print_buffer,
				"ISQL_get_character_set: charset %d collation %d not found.\n",
				char_set_id, collation);
		STDERROUT(Print_buffer);
	}
#endif
}


static processing_state show_check(const SCHAR* object)
{
   struct fb_912_struct {
          ISC_QUAD fb_913;	/* RDB$TRIGGER_SOURCE */
          short fb_914;	/* fbUtility */
          short fb_915;	/* gds__null_flag */
          char  fb_916 [253];	/* RDB$CONSTRAINT_NAME */
   } fb_912;
   struct fb_909_struct {
          short fb_910;	/* RDB$SYSTEM_FLAG */
          char  fb_911 [253];	/* RDB$RELATION_NAME */
   } fb_909;
/**************************************
 *
 *	s h o w _ c h e c k
 *
 **************************************
 *
 * Functional description
 *	Show check constraints for the named object
 *
 **************************************/
	bool first = true;

	if (!*object)
		return ps_ERR;
	// Query gets the check clauses for triggers stored for check constraints

	/*FOR TRG IN RDB$TRIGGERS CROSS
		CHK IN RDB$CHECK_CONSTRAINTS WITH
		TRG.RDB$TRIGGER_TYPE EQ 1 AND
		TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME AND
		//CHK.RDB$TRIGGER_NAME STARTING WITH "CHECK" AND
		TRG.RDB$SYSTEM_FLAG EQ int(fb_sysflag_check_constraint) AND
		TRG.RDB$RELATION_NAME EQ object
		SORTED BY CHK.RDB$CONSTRAINT_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_907 && fbTrans && DB)
	      fb_907 = DB->compileRequest(fbStatus, sizeof(fb_908), fb_908);
	   fb_909.fb_910 = int(fb_sysflag_check_constraint);
	   isc_vtov ((const char*) object, (char*) fb_909.fb_911, 253);
	   if (fbTrans && fb_907)
	      fb_907->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_909));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_907->release(); fb_907 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_907->receive (fbStatus, 0, 1, 265, CAST_MSG(&fb_912));
	   if (!fb_912.fb_914 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// Use print_blob to print the blob
		first = false;

		isqlGlob.printf("CONSTRAINT %s:%s  ",
			fb_utils::exact_name(/*CHK.RDB$CONSTRAINT_NAME*/
					     fb_912.fb_916), NEWLINE);

		if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
		     fb_912.fb_915)
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
								      fb_912.fb_913);
		isqlGlob.printf(NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static processing_state show_collations(const SCHAR* object, SSHORT sys_flag, const char* msg, bool compact)
{
   struct fb_895_struct {
          ISC_QUAD fb_896;	/* RDB$SPECIFIC_ATTRIBUTES */
          short fb_897;	/* fbUtility */
          short fb_898;	/* gds__null_flag */
          short fb_899;	/* gds__null_flag */
          short fb_900;	/* RDB$COLLATION_ATTRIBUTES */
          short fb_901;	/* gds__null_flag */
          short fb_902;	/* gds__null_flag */
          short fb_903;	/* RDB$SYSTEM_FLAG */
          char  fb_904 [253];	/* RDB$COLLATION_NAME */
          char  fb_905 [253];	/* RDB$CHARACTER_SET_NAME */
          char  fb_906 [253];	/* RDB$BASE_COLLATION_NAME */
   } fb_895;
/**************************************
 *
 *	s h o w _ c o l l a t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show collations.
 *
 **************************************/
	bool found = false;

	// Show all collations or named collation
	/*FOR CL IN RDB$COLLATIONS CROSS
		CS IN RDB$CHARACTER_SETS WITH
		CS.RDB$CHARACTER_SET_ID EQ CL.RDB$CHARACTER_SET_ID
		SORTED BY CL.RDB$COLLATION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_893 && fbTrans && DB)
	      fb_893 = DB->compileRequest(fbStatus, sizeof(fb_894), fb_894);
	   if (fbTrans && fb_893)
	      fb_893->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_893->release(); fb_893 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_893->receive (fbStatus, 0, 0, 781, CAST_MSG(&fb_895));
	   if (!fb_895.fb_897 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*CL.RDB$COLLATION_NAME*/
				     fb_895.fb_904);

		if ((!*object &&
			 ((!/*CL.RDB$SYSTEM_FLAG.NULL*/
			    fb_895.fb_902 && /*CL.RDB$SYSTEM_FLAG*/
    fb_895.fb_903 != 0 && sys_flag != 0) ||
			  (!(!/*CL.RDB$SYSTEM_FLAG.NULL*/
			      fb_895.fb_902 && /*CL.RDB$SYSTEM_FLAG*/
    fb_895.fb_903 != 0) && sys_flag != 1))) ||
			strcmp(/*CL.RDB$COLLATION_NAME*/
			       fb_895.fb_904, object) == 0)
		{
			if (!found)
			{
				found = true;

				if (msg)
					isqlGlob.printf("%s%s", msg, NEWLINE);
			}

			if (compact)
				isqlGlob.printf("%s%s", /*CL.RDB$COLLATION_NAME*/
							fb_895.fb_904, NEWLINE);
			else
			{
				isqlGlob.printf("%s", /*CL.RDB$COLLATION_NAME*/
						      fb_895.fb_904);

				fb_utils::exact_name(/*CS.RDB$CHARACTER_SET_NAME*/
						     fb_895.fb_905);
				isqlGlob.printf(", CHARACTER SET %s", /*CS.RDB$CHARACTER_SET_NAME*/
								      fb_895.fb_905);

				if (!/*CL.RDB$BASE_COLLATION_NAME.NULL*/
				     fb_895.fb_901)
				{
					fb_utils::exact_name(/*CL.RDB$BASE_COLLATION_NAME*/
							     fb_895.fb_906);
					isqlGlob.printf(", FROM EXTERNAL ('%s')", /*CL.RDB$BASE_COLLATION_NAME*/
										  fb_895.fb_906);
				}

				if (!/*CL.RDB$COLLATION_ATTRIBUTES.NULL*/
				     fb_895.fb_899)
				{
					if (/*CL.RDB$COLLATION_ATTRIBUTES*/
					    fb_895.fb_900 & TEXTTYPE_ATTR_PAD_SPACE)
						isqlGlob.printf(", PAD SPACE");

					if (/*CL.RDB$COLLATION_ATTRIBUTES*/
					    fb_895.fb_900 & TEXTTYPE_ATTR_CASE_INSENSITIVE)
						isqlGlob.printf(", CASE INSENSITIVE");

					if (/*CL.RDB$COLLATION_ATTRIBUTES*/
					    fb_895.fb_900 & TEXTTYPE_ATTR_ACCENT_INSENSITIVE)
						isqlGlob.printf(", ACCENT INSENSITIVE");
				}

				if (!/*CL.RDB$SPECIFIC_ATTRIBUTES.NULL*/
				     fb_895.fb_898)
				{
					isqlGlob.printf(", '");
					SHOW_print_metadata_text_blob (isqlGlob.Out, &/*CL.RDB$SPECIFIC_ATTRIBUTES*/
										      fb_895.fb_896);
					isqlGlob.printf("'");
				}

				if (!/*CL.RDB$SYSTEM_FLAG.NULL*/
				     fb_895.fb_902 && /*CL.RDB$SYSTEM_FLAG*/
    fb_895.fb_903 != 0)
					isqlGlob.printf(", SYSTEM");

				isqlGlob.printf("%s", NEWLINE);
			}
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	if (!found)
		return (OBJECT_NOT_FOUND);

	isqlGlob.printf(NEWLINE);

	return SKIP;
}


// ***********************
// s h o w _ c o m m e n t
// ***********************
// Helper that displays in correct syntax the COMMENT ON command for each object.
// It escapes identifiers with embedded double quotes and escapes the comment
// itself if it contains single quotes when we are honoring script extraction.
static void show_comment(const char* objtype, char* packageName, char* name1, char* name2,
	ISC_QUAD* blobfld, const commentMode showextract, const char* banner)
{
	const bool escape_quotes = showextract == cmmExtract;

	if (escape_quotes && banner)
		isqlGlob.prints(banner);

	if (packageName)
		fb_utils::exact_name(packageName);
	if (name1)
		fb_utils::exact_name(name1);
	if (name2)
		fb_utils::exact_name(name2);

	char packageNameBuffer[BUFFER_LENGTH256];
	char SQL_identifier2[BUFFER_LENGTH256];

	if (escape_quotes && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
		if (packageName)
		{
			IUTILS_copy_SQL_id (packageName, packageNameBuffer, DBL_QUOTE);
			packageName = packageNameBuffer;
		}

		if (name1)
		{
			IUTILS_copy_SQL_id (name1, SQL_identifier, DBL_QUOTE);
			name1 = SQL_identifier;
		}

		if (name2)
		{
			IUTILS_copy_SQL_id (name2, SQL_identifier2, DBL_QUOTE);
			name2 = SQL_identifier2;
		}
	}

	const char* quot = escape_quotes ? "'" : "";

	isqlGlob.printf("COMMENT ON %-12s", objtype);

	if (packageName || name1 || name2)
		isqlGlob.printf(" ");

	if (packageName)
		isqlGlob.printf("%s.", packageName);

	if (name1)
		isqlGlob.printf("%s", name1);

	if (name2)
		isqlGlob.printf(".%s", name2);

	isqlGlob.printf(" IS %s", quot);

	SHOW_print_metadata_text_blob(isqlGlob.Out, blobfld, escape_quotes);
	isqlGlob.printf("%s%s%s", quot, isqlGlob.global_Term, NEWLINE);
}


// *************************
// s h o w _ c o m m e n t s
// *************************
// Will extract and show descriptions (comments) for all supported db objects.
// This function does its task even if the server version doesn't support
// the COMMENT ON command. It will however skip generators and roles that
// didn't have description fields before ODS11.
// It will extract the main objects by category and inside each category,
// in alphabetical order. For tables and views, their fields are printed
// immediately after the table/field in rdb$field_position order, that's
// the order the users sees when doing a select * from tbl/view. For procedures,
// their parameters are printed immediately after the procedure, first the input
// params by position, the  the output params by position. All system objects
// as well as implicit domains and implicit triggers are skipped. For ODS < 11,
// we skip generators and roles because those system tables didn't have a
// rdb$description field.
// When showing comments, we don't escape quotes in neither names nor strings.
// When extracting comments, we do the usual escaping to make the script valid.
static processing_state show_comments(const commentMode showextract, const char* banner)
{
   struct fb_744_struct {
          ISC_QUAD fb_745;	/* SEC$DESCRIPTION */
          short fb_746;	/* fbUtility */
          char  fb_747 [253];	/* SEC$MAP_NAME */
   } fb_744;
   struct fb_750_struct {
          ISC_QUAD fb_751;	/* RDB$DESCRIPTION */
          short fb_752;	/* fbUtility */
          char  fb_753 [253];	/* RDB$MAP_NAME */
   } fb_750;
   struct fb_756_struct {
          ISC_QUAD fb_757;	/* RDB$DESCRIPTION */
          short fb_758;	/* fbUtility */
          char  fb_759 [253];	/* RDB$PACKAGE_NAME */
   } fb_756;
   struct fb_762_struct {
          ISC_QUAD fb_763;	/* RDB$DESCRIPTION */
          short fb_764;	/* fbUtility */
          char  fb_765 [253];	/* RDB$COLLATION_NAME */
   } fb_762;
   struct fb_768_struct {
          ISC_QUAD fb_769;	/* RDB$DESCRIPTION */
          short fb_770;	/* fbUtility */
          char  fb_771 [253];	/* RDB$CHARACTER_SET_NAME */
   } fb_768;
   struct fb_774_struct {
          ISC_QUAD fb_775;	/* RDB$DESCRIPTION */
          short fb_776;	/* fbUtility */
          char  fb_777 [253];	/* RDB$PACKAGE_NAME */
   } fb_774;
   struct fb_780_struct {
          ISC_QUAD fb_781;	/* RDB$DESCRIPTION */
          short fb_782;	/* fbUtility */
          char  fb_783 [253];	/* RDB$ROLE_NAME */
   } fb_780;
   struct fb_786_struct {
          ISC_QUAD fb_787;	/* RDB$DESCRIPTION */
          short fb_788;	/* fbUtility */
          char  fb_789 [253];	/* RDB$INDEX_NAME */
   } fb_786;
   struct fb_792_struct {
          ISC_QUAD fb_793;	/* RDB$DESCRIPTION */
          short fb_794;	/* fbUtility */
          char  fb_795 [253];	/* RDB$GENERATOR_NAME */
   } fb_792;
   struct fb_798_struct {
          ISC_QUAD fb_799;	/* RDB$DESCRIPTION */
          short fb_800;	/* fbUtility */
          char  fb_801 [253];	/* RDB$EXCEPTION_NAME */
   } fb_798;
   struct fb_804_struct {
          ISC_QUAD fb_805;	/* RDB$DESCRIPTION */
          short fb_806;	/* fbUtility */
          char  fb_807 [253];	/* RDB$FUNCTION_NAME */
   } fb_804;
   struct fb_813_struct {
          ISC_QUAD fb_814;	/* RDB$DESCRIPTION */
          short fb_815;	/* fbUtility */
          short fb_816;	/* gds__null_flag */
          char  fb_817 [253];	/* RDB$PACKAGE_NAME */
          char  fb_818 [253];	/* RDB$FUNCTION_NAME */
          char  fb_819 [253];	/* RDB$ARGUMENT_NAME */
   } fb_813;
   struct fb_810_struct {
          char  fb_811 [253];	/* RDB$FUNCTION_NAME */
          char  fb_812 [253];	/* RDB$PACKAGE_NAME */
   } fb_810;
   struct fb_822_struct {
          ISC_QUAD fb_823;	/* RDB$DESCRIPTION */
          short fb_824;	/* fbUtility */
          short fb_825;	/* gds__null_flag */
          char  fb_826 [253];	/* RDB$PACKAGE_NAME */
          char  fb_827 [253];	/* RDB$FUNCTION_NAME */
   } fb_822;
   struct fb_830_struct {
          ISC_QUAD fb_831;	/* RDB$DESCRIPTION */
          short fb_832;	/* fbUtility */
          char  fb_833 [253];	/* RDB$TRIGGER_NAME */
   } fb_830;
   struct fb_839_struct {
          ISC_QUAD fb_840;	/* RDB$DESCRIPTION */
          short fb_841;	/* fbUtility */
          char  fb_842 [253];	/* RDB$PARAMETER_NAME */
   } fb_839;
   struct fb_836_struct {
          char  fb_837 [253];	/* RDB$PROCEDURE_NAME */
          char  fb_838 [253];	/* RDB$PACKAGE_NAME */
   } fb_836;
   struct fb_845_struct {
          ISC_QUAD fb_846;	/* RDB$DESCRIPTION */
          short fb_847;	/* fbUtility */
          short fb_848;	/* gds__null_flag */
          short fb_849;	/* gds__null_flag */
          char  fb_850 [253];	/* RDB$PACKAGE_NAME */
          char  fb_851 [253];	/* RDB$PROCEDURE_NAME */
   } fb_845;
   struct fb_856_struct {
          ISC_QUAD fb_857;	/* RDB$DESCRIPTION */
          short fb_858;	/* fbUtility */
          char  fb_859 [253];	/* RDB$FIELD_NAME */
   } fb_856;
   struct fb_854_struct {
          char  fb_855 [253];	/* RDB$RELATION_NAME */
   } fb_854;
   struct fb_862_struct {
          ISC_QUAD fb_863;	/* RDB$DESCRIPTION */
          short fb_864;	/* fbUtility */
          short fb_865;	/* gds__null_flag */
          char  fb_866 [253];	/* RDB$RELATION_NAME */
   } fb_862;
   struct fb_871_struct {
          ISC_QUAD fb_872;	/* RDB$DESCRIPTION */
          short fb_873;	/* fbUtility */
          char  fb_874 [253];	/* RDB$FIELD_NAME */
   } fb_871;
   struct fb_869_struct {
          char  fb_870 [253];	/* RDB$RELATION_NAME */
   } fb_869;
   struct fb_877_struct {
          ISC_QUAD fb_878;	/* RDB$DESCRIPTION */
          short fb_879;	/* fbUtility */
          short fb_880;	/* gds__null_flag */
          char  fb_881 [253];	/* RDB$RELATION_NAME */
   } fb_877;
   struct fb_884_struct {
          ISC_QUAD fb_885;	/* RDB$DESCRIPTION */
          short fb_886;	/* fbUtility */
          char  fb_887 [253];	/* RDB$FIELD_NAME */
   } fb_884;
   struct fb_890_struct {
          ISC_QUAD fb_891;	/* RDB$DESCRIPTION */
          short fb_892;	/* fbUtility */
   } fb_890;
	// From dsql.h:
	//	ddl_database, ddl_domain, ddl_relation, ddl_view, ddl_procedure, ddl_trigger,
	//	ddl_udf, ddl_blob_filter, ddl_exception, ddl_generator, ddl_index, ddl_role,
	//	ddl_charset, ddl_collation//, ddl_sec_class

	bool first = true;

	/*FOR FIRST 1 DT IN RDB$DATABASE
		WITH DT.RDB$DESCRIPTION NOT MISSING*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_888 && fbTrans && DB)
	      fb_888 = DB->compileRequest(fbStatus, sizeof(fb_889), fb_889);
	   if (fbTrans && fb_888)
	      fb_888->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_888->release(); fb_888 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_888->receive (fbStatus, 0, 0, 10, CAST_MSG(&fb_890));
	   if (!fb_890.fb_892 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("DATABASE", NULL, NULL, NULL, &/*DT.RDB$DESCRIPTION*/
							    fb_890.fb_891, showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR DM IN RDB$FIELDS
	WITH DM.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
		AND (DM.RDB$SYSTEM_FLAG EQ 0 OR DM.RDB$SYSTEM_FLAG MISSING)
		AND DM.RDB$DESCRIPTION NOT MISSING
		SORTED BY DM.RDB$FIELD_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_882 && fbTrans && DB)
	      fb_882 = DB->compileRequest(fbStatus, sizeof(fb_883), fb_883);
	   if (fbTrans && fb_882)
	      fb_882->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_882->release(); fb_882 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_882->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_884));
	   if (!fb_884.fb_886 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("DOMAIN", NULL, /*DM.RDB$FIELD_NAME*/
					     fb_884.fb_887, NULL, &/*DM.RDB$DESCRIPTION*/
	 fb_884.fb_885,
			showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR RL IN RDB$RELATIONS
		WITH RL.RDB$VIEW_BLR MISSING
		AND (RL.RDB$SYSTEM_FLAG EQ 0 OR RL.RDB$SYSTEM_FLAG MISSING)
		SORTED BY RL.RDB$RELATION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_875 && fbTrans && DB)
	      fb_875 = DB->compileRequest(fbStatus, sizeof(fb_876), fb_876);
	   if (fbTrans && fb_875)
	      fb_875->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_875->release(); fb_875 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_875->receive (fbStatus, 0, 0, 265, CAST_MSG(&fb_877));
	   if (!fb_877.fb_879 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!/*RL.RDB$DESCRIPTION.NULL*/
		     fb_877.fb_880 && !UserBlob::blobIsNull(/*RL.RDB$DESCRIPTION*/
			  fb_877.fb_878))
		{
			show_comment("TABLE", NULL, /*RL.RDB$RELATION_NAME*/
						    fb_877.fb_881, NULL, &/*RL.RDB$DESCRIPTION*/
	 fb_877.fb_878,
				showextract, first ? banner : 0);
			first = false;
		}

		/*FOR RF IN RDB$RELATION_FIELDS
			WITH RF.RDB$RELATION_NAME = RL.RDB$RELATION_NAME
			AND RF.RDB$DESCRIPTION NOT MISSING
			SORTED BY RF.RDB$FIELD_POSITION*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_867 && fbTrans && DB)
		      fb_867 = DB->compileRequest(fbStatus, sizeof(fb_868), fb_868);
		   isc_vtov ((const char*) fb_877.fb_881, (char*) fb_869.fb_870, 253);
		   if (fbTrans && fb_867)
		      fb_867->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_869));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_867->release(); fb_867 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_867->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_871));
		   if (!fb_871.fb_873 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("   COLUMN", NULL, /*RL.RDB$RELATION_NAME*/
							fb_877.fb_881, /*RF.RDB$FIELD_NAME*/
  fb_871.fb_874,
				&/*RF.RDB$DESCRIPTION*/
				 fb_871.fb_872, showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR VW IN RDB$RELATIONS
		WITH VW.RDB$VIEW_BLR NOT MISSING
		AND (VW.RDB$SYSTEM_FLAG EQ 0 OR VW.RDB$SYSTEM_FLAG MISSING)
		SORTED BY VW.RDB$RELATION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_860 && fbTrans && DB)
	      fb_860 = DB->compileRequest(fbStatus, sizeof(fb_861), fb_861);
	   if (fbTrans && fb_860)
	      fb_860->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_860->release(); fb_860 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_860->receive (fbStatus, 0, 0, 265, CAST_MSG(&fb_862));
	   if (!fb_862.fb_864 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!/*VW.RDB$DESCRIPTION.NULL*/
		     fb_862.fb_865 && !UserBlob::blobIsNull(/*VW.RDB$DESCRIPTION*/
			  fb_862.fb_863))
		{
			show_comment("VIEW", NULL, /*VW.RDB$RELATION_NAME*/
						   fb_862.fb_866, NULL, &/*VW.RDB$DESCRIPTION*/
	 fb_862.fb_863,
				showextract, first ? banner : 0);
			first = false;
		}

		/*FOR RF IN RDB$RELATION_FIELDS
			WITH RF.RDB$RELATION_NAME = VW.RDB$RELATION_NAME
			AND RF.RDB$DESCRIPTION NOT MISSING
			SORTED BY RF.RDB$FIELD_POSITION*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_852 && fbTrans && DB)
		      fb_852 = DB->compileRequest(fbStatus, sizeof(fb_853), fb_853);
		   isc_vtov ((const char*) fb_862.fb_866, (char*) fb_854.fb_855, 253);
		   if (fbTrans && fb_852)
		      fb_852->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_854));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_852->release(); fb_852 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_852->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_856));
		   if (!fb_856.fb_858 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("   COLUMN", NULL, /*VW.RDB$RELATION_NAME*/
							fb_862.fb_866, /*RF.RDB$FIELD_NAME*/
  fb_856.fb_859,
				&/*RF.RDB$DESCRIPTION*/
				 fb_856.fb_857, showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR PR IN RDB$PROCEDURES
		WITH (PR.RDB$SYSTEM_FLAG EQ 0 OR PR.RDB$SYSTEM_FLAG MISSING)
		SORTED BY PR.RDB$PROCEDURE_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_843 && fbTrans && DB)
	      fb_843 = DB->compileRequest(fbStatus, sizeof(fb_844), fb_844);
	   if (fbTrans && fb_843)
	      fb_843->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_843->release(); fb_843 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_843->receive (fbStatus, 0, 0, 520, CAST_MSG(&fb_845));
	   if (!fb_845.fb_847 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!/*PR.RDB$DESCRIPTION.NULL*/
		     fb_845.fb_849 && !UserBlob::blobIsNull(/*PR.RDB$DESCRIPTION*/
			  fb_845.fb_846))
		{
			show_comment("PROCEDURE", (/*PR.RDB$PACKAGE_NAME.NULL*/
						   fb_845.fb_848 ? NULL : /*PR.RDB$PACKAGE_NAME*/
	  fb_845.fb_850),
				/*PR.RDB$PROCEDURE_NAME*/
				fb_845.fb_851, NULL, &/*PR.RDB$DESCRIPTION*/
	 fb_845.fb_846, showextract, first ? banner : 0);
			first = false;
		}

		/*FOR PA IN RDB$PROCEDURE_PARAMETERS
			WITH PA.RDB$PROCEDURE_NAME = PR.RDB$PROCEDURE_NAME
			AND PA.RDB$PACKAGE_NAME EQUIV NULLIF(PR.RDB$PACKAGE_NAME, '')
			AND PA.RDB$DESCRIPTION NOT MISSING
			SORTED BY PA.RDB$PARAMETER_TYPE, PA.RDB$PARAMETER_NUMBER*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_834 && fbTrans && DB)
		      fb_834 = DB->compileRequest(fbStatus, sizeof(fb_835), fb_835);
		   isc_vtov ((const char*) fb_845.fb_851, (char*) fb_836.fb_837, 253);
		   isc_vtov ((const char*) fb_845.fb_850, (char*) fb_836.fb_838, 253);
		   if (fbTrans && fb_834)
		      fb_834->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_836));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_834->release(); fb_834 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_834->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_839));
		   if (!fb_839.fb_841 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("   PROCEDURE PARAMETER",
				(/*PR.RDB$PACKAGE_NAME.NULL*/
				 fb_845.fb_848 ? NULL : /*PR.RDB$PACKAGE_NAME*/
	  fb_845.fb_850), /*PR.RDB$PROCEDURE_NAME*/
   fb_845.fb_851,
				/*PA.RDB$PARAMETER_NAME*/
				fb_839.fb_842, &/*PA.RDB$DESCRIPTION*/
   fb_839.fb_840, showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR TR IN RDB$TRIGGERS
		WITH TR.RDB$DESCRIPTION NOT MISSING
		AND (TR.RDB$SYSTEM_FLAG EQ 0 OR TR.RDB$SYSTEM_FLAG MISSING)
		SORTED BY TR.RDB$TRIGGER_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_828 && fbTrans && DB)
	      fb_828 = DB->compileRequest(fbStatus, sizeof(fb_829), fb_829);
	   if (fbTrans && fb_828)
	      fb_828->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_828->release(); fb_828 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_828->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_830));
	   if (!fb_830.fb_832 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("TRIGGER", NULL, /*TR.RDB$TRIGGER_NAME*/
					      fb_830.fb_833, NULL, &/*TR.RDB$DESCRIPTION*/
	 fb_830.fb_831,
			showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR UD IN RDB$FUNCTIONS
		WITH UD.RDB$DESCRIPTION NOT MISSING
		AND (UD.RDB$SYSTEM_FLAG EQ 0 OR UD.RDB$SYSTEM_FLAG MISSING)
		SORTED BY UD.RDB$FUNCTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_820 && fbTrans && DB)
	      fb_820 = DB->compileRequest(fbStatus, sizeof(fb_821), fb_821);
	   if (fbTrans && fb_820)
	      fb_820->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_820->release(); fb_820 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_820->receive (fbStatus, 0, 0, 518, CAST_MSG(&fb_822));
	   if (!fb_822.fb_824 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// Avoid syntax error when extracting scripts due to an historical bug in gbak.
		// See CORE-1174.
		if (UserBlob::blobIsNull(/*UD.RDB$DESCRIPTION*/
					 fb_822.fb_823))
			continue;

		show_comment("FUNCTION",
			(/*UD.RDB$PACKAGE_NAME.NULL*/
			 fb_822.fb_825 ? NULL : /*UD.RDB$PACKAGE_NAME*/
	  fb_822.fb_826), /*UD.RDB$FUNCTION_NAME*/
   fb_822.fb_827, NULL,
			&/*UD.RDB$DESCRIPTION*/
			 fb_822.fb_823, showextract, first ? banner : 0);
		first = false;

		/*FOR ARG IN RDB$FUNCTION_ARGUMENTS
			WITH ARG.RDB$FUNCTION_NAME = UD.RDB$FUNCTION_NAME
			AND ARG.RDB$PACKAGE_NAME EQUIV NULLIF(UD.RDB$PACKAGE_NAME, '')
			AND ARG.RDB$DESCRIPTION NOT MISSING
			SORTED BY ARG.RDB$ARGUMENT_POSITION*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_808 && fbTrans && DB)
		      fb_808 = DB->compileRequest(fbStatus, sizeof(fb_809), fb_809);
		   isc_vtov ((const char*) fb_822.fb_827, (char*) fb_810.fb_811, 253);
		   isc_vtov ((const char*) fb_822.fb_826, (char*) fb_810.fb_812, 253);
		   if (fbTrans && fb_808)
		      fb_808->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_810));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_808->release(); fb_808 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_808->receive (fbStatus, 0, 1, 771, CAST_MSG(&fb_813));
		   if (!fb_813.fb_815 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("   FUNCTION PARAMETER",
				(/*ARG.RDB$PACKAGE_NAME.NULL*/
				 fb_813.fb_816 ? NULL : /*ARG.RDB$PACKAGE_NAME*/
	  fb_813.fb_817), /*ARG.RDB$FUNCTION_NAME*/
   fb_813.fb_818,
				/*ARG.RDB$ARGUMENT_NAME*/
				fb_813.fb_819, &/*ARG.RDB$DESCRIPTION*/
   fb_813.fb_814, showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR BF IN RDB$FILTERS
		WITH BF.RDB$DESCRIPTION NOT MISSING
		AND (BF.RDB$SYSTEM_FLAG EQ 0 OR BF.RDB$SYSTEM_FLAG MISSING)
		SORTED BY BF.RDB$FUNCTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_802 && fbTrans && DB)
	      fb_802 = DB->compileRequest(fbStatus, sizeof(fb_803), fb_803);
	   if (fbTrans && fb_802)
	      fb_802->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_802->release(); fb_802 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_802->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_804));
	   if (!fb_804.fb_806 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("FILTER", NULL, /*BF.RDB$FUNCTION_NAME*/
					     fb_804.fb_807, NULL, &/*BF.RDB$DESCRIPTION*/
	 fb_804.fb_805,
			showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR XC IN RDB$EXCEPTIONS
		WITH XC.RDB$DESCRIPTION NOT MISSING
		AND (XC.RDB$SYSTEM_FLAG EQ 0 OR XC.RDB$SYSTEM_FLAG MISSING)
		SORTED BY XC.RDB$EXCEPTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_796 && fbTrans && DB)
	      fb_796 = DB->compileRequest(fbStatus, sizeof(fb_797), fb_797);
	   if (fbTrans && fb_796)
	      fb_796->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_796->release(); fb_796 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_796->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_798));
	   if (!fb_798.fb_800 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("EXCEPTION", NULL, /*XC.RDB$EXCEPTION_NAME*/
						fb_798.fb_801, NULL, &/*XC.RDB$DESCRIPTION*/
	 fb_798.fb_799,
			showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	if (isqlGlob.major_ods >= ODS_VERSION11)
	{
		/*FOR GR IN RDB$GENERATORS
			WITH GR.RDB$DESCRIPTION NOT MISSING
			AND (GR.RDB$SYSTEM_FLAG EQ 0 OR GR.RDB$SYSTEM_FLAG MISSING)
			SORTED BY GR.RDB$GENERATOR_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_790 && fbTrans && DB)
		      fb_790 = DB->compileRequest(fbStatus, sizeof(fb_791), fb_791);
		   if (fbTrans && fb_790)
		      fb_790->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_790->release(); fb_790 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_790->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_792));
		   if (!fb_792.fb_794 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("GENERATOR", NULL, /*GR.RDB$GENERATOR_NAME*/
							fb_792.fb_795, NULL, &/*GR.RDB$DESCRIPTION*/
	 fb_792.fb_793,
				showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}
	}

	/*FOR IX IN RDB$INDICES
		WITH IX.RDB$DESCRIPTION NOT MISSING
		AND (IX.RDB$SYSTEM_FLAG EQ 0 OR IX.RDB$SYSTEM_FLAG MISSING)
		SORTED BY IX.RDB$INDEX_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_784 && fbTrans && DB)
	      fb_784 = DB->compileRequest(fbStatus, sizeof(fb_785), fb_785);
	   if (fbTrans && fb_784)
	      fb_784->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_784->release(); fb_784 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_784->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_786));
	   if (!fb_786.fb_788 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("INDEX", NULL, /*IX.RDB$INDEX_NAME*/
					    fb_786.fb_789, NULL, &/*IX.RDB$DESCRIPTION*/
	 fb_786.fb_787,
			showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	if (isqlGlob.major_ods >= ODS_VERSION11)
	{
		/*FOR RO IN RDB$ROLES
			WITH RO.RDB$DESCRIPTION NOT MISSING
			AND (RO.RDB$SYSTEM_FLAG EQ 0 OR RO.RDB$SYSTEM_FLAG MISSING)
			SORTED BY RO.RDB$ROLE_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_778 && fbTrans && DB)
		      fb_778 = DB->compileRequest(fbStatus, sizeof(fb_779), fb_779);
		   if (fbTrans && fb_778)
		      fb_778->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_778->release(); fb_778 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_778->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_780));
		   if (!fb_780.fb_782 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("ROLE", NULL, /*RO.RDB$ROLE_NAME*/
						   fb_780.fb_783, NULL, &/*RO.RDB$DESCRIPTION*/
	 fb_780.fb_781,
				showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}
	}

	if (isqlGlob.major_ods >= ODS_VERSION12)
	{
		/*FOR PACK IN RDB$PACKAGES
			WITH PACK.RDB$DESCRIPTION NOT MISSING
			AND (PACK.RDB$SYSTEM_FLAG EQ 0 OR PACK.RDB$SYSTEM_FLAG MISSING)
			SORTED BY PACK.RDB$PACKAGE_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_772 && fbTrans && DB)
		      fb_772 = DB->compileRequest(fbStatus, sizeof(fb_773), fb_773);
		   if (fbTrans && fb_772)
		      fb_772->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_772->release(); fb_772 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_772->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_774));
		   if (!fb_774.fb_776 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("PACKAGE", NULL, /*PACK.RDB$PACKAGE_NAME*/
						      fb_774.fb_777, NULL, &/*PACK.RDB$DESCRIPTION*/
	 fb_774.fb_775,
				showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}
	}

	/*FOR CH IN RDB$CHARACTER_SETS
		WITH CH.RDB$DESCRIPTION NOT MISSING
		AND (CH.RDB$SYSTEM_FLAG EQ 0 OR CH.RDB$SYSTEM_FLAG MISSING)
		SORTED BY CH.RDB$CHARACTER_SET_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_766 && fbTrans && DB)
	      fb_766 = DB->compileRequest(fbStatus, sizeof(fb_767), fb_767);
	   if (fbTrans && fb_766)
	      fb_766->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_766->release(); fb_766 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_766->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_768));
	   if (!fb_768.fb_770 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("CHARACTER SET", NULL, /*CH.RDB$CHARACTER_SET_NAME*/
						    fb_768.fb_771, NULL,
			&/*CH.RDB$DESCRIPTION*/
			 fb_768.fb_769, showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR CL IN RDB$COLLATIONS
		WITH CL.RDB$DESCRIPTION NOT MISSING
		AND (CL.RDB$SYSTEM_FLAG EQ 0 OR CL.RDB$SYSTEM_FLAG MISSING)
		SORTED BY CL.RDB$COLLATION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_760 && fbTrans && DB)
	      fb_760 = DB->compileRequest(fbStatus, sizeof(fb_761), fb_761);
	   if (fbTrans && fb_760)
	      fb_760->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_760->release(); fb_760 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_760->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_762));
	   if (!fb_762.fb_764 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		show_comment("COLLATION", NULL, /*CL.RDB$COLLATION_NAME*/
						fb_762.fb_765, NULL, &/*CL.RDB$DESCRIPTION*/
	 fb_762.fb_763,
			showextract, first ? banner : 0);
		first = false;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	if (isqlGlob.major_ods >= ODS_VERSION12)
	{
		/*FOR PACK IN RDB$PACKAGES
			WITH PACK.RDB$DESCRIPTION NOT MISSING
			AND (PACK.RDB$SYSTEM_FLAG EQ 0 OR PACK.RDB$SYSTEM_FLAG MISSING)
			SORTED BY PACK.RDB$PACKAGE_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_754 && fbTrans && DB)
		      fb_754 = DB->compileRequest(fbStatus, sizeof(fb_755), fb_755);
		   if (fbTrans && fb_754)
		      fb_754->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_754->release(); fb_754 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_754->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_756));
		   if (!fb_756.fb_758 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("PACKAGE", NULL, /*PACK.RDB$PACKAGE_NAME*/
						      fb_756.fb_759, NULL, &/*PACK.RDB$DESCRIPTION*/
	 fb_756.fb_757,
				showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		/*FOR M IN RDB$AUTH_MAPPING
			WITH M.RDB$DESCRIPTION NOT MISSING
			AND (M.RDB$SYSTEM_FLAG EQ 0 OR M.RDB$SYSTEM_FLAG MISSING)
			SORTED BY M.RDB$MAP_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_748 && fbTrans && DB)
		      fb_748 = DB->compileRequest(fbStatus, sizeof(fb_749), fb_749);
		   if (fbTrans && fb_748)
		      fb_748->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_748->release(); fb_748 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_748->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_750));
		   if (!fb_750.fb_752 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("MAPPING", NULL, /*M.RDB$MAP_NAME*/
						      fb_750.fb_753, NULL, &/*M.RDB$DESCRIPTION*/
	 fb_750.fb_751,
				showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		/*FOR M IN SEC$GLOBAL_AUTH_MAPPING
			WITH M.SEC$DESCRIPTION NOT MISSING
			SORTED BY M.SEC$MAP_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_742 && fbTrans && DB)
		      fb_742 = DB->compileRequest(fbStatus, sizeof(fb_743), fb_743);
		   if (fbTrans && fb_742)
		      fb_742->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_742->release(); fb_742 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_742->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_744));
		   if (!fb_744.fb_746 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			show_comment("GLOBAL MAPPING", NULL, /*M.SEC$MAP_NAME*/
							     fb_744.fb_747, NULL, &/*M.SEC$DESCRIPTION*/
	 fb_744.fb_745,
				showextract, first ? banner : 0);
			first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}
	}
	return first ? OBJECT_NOT_FOUND : SKIP;
}


static void show_db()
{
   struct fb_704_struct {
          short fb_705;	/* fbUtility */
   } fb_704;
   struct fb_708_struct {
          ISC_LONG fb_709;	/* RDB$LINGER */
          short fb_710;	/* fbUtility */
          short fb_711;	/* gds__null_flag */
          short fb_712;	/* gds__null_flag */
          FB_BOOLEAN fb_713;	/* RDB$SQL_SECURITY */
   } fb_708;
   struct fb_716_struct {
          short fb_717;	/* fbUtility */
          short fb_718;	/* gds__null_flag */
          char  fb_719 [253];	/* RDB$CHARACTER_SET_NAME */
          char  fb_720 [253];	/* RDB$DEFAULT_COLLATE_NAME */
   } fb_716;
   struct fb_723_struct {
          char  fb_724 [256];	/* RDB$FILE_NAME */
          ISC_LONG fb_725;	/* RDB$FILE_START */
          ISC_LONG fb_726;	/* RDB$FILE_LENGTH */
          short fb_727;	/* fbUtility */
          short fb_728;	/* RDB$SHADOW_NUMBER */
          short fb_729;	/* gds__null_flag */
          short fb_730;	/* gds__null_flag */
          short fb_731;	/* gds__null_flag */
          short fb_732;	/* RDB$FILE_SEQUENCE */
          short fb_733;	/* gds__null_flag */
          short fb_734;	/* gds__null_flag */
          short fb_735;	/* RDB$FILE_FLAGS */
   } fb_723;
   struct fb_738_struct {
          short fb_739;	/* fbUtility */
          short fb_740;	/* gds__null_flag */
          char  fb_741 [253];	/* RDB$OWNER_NAME */
   } fb_738;
/**************************************
 *
 *	s h o w _ d b
 *
 **************************************
 *
 * Functional description
 *	Show info on this database.  cache, logfiles, etc
 *
 **************************************/

	// First print the name of the database

	isqlGlob.printf("Database: %s%s", isqlGlob.global_Db_name, NEWLINE);
	// Get the owner name
	/*FOR REL IN RDB$RELATIONS WITH
		REL.RDB$RELATION_NAME = "RDB$DATABASE"*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_736 && fbTrans && DB)
	      fb_736 = DB->compileRequest(fbStatus, sizeof(fb_737), fb_737);
	   if (fbTrans && fb_736)
	      fb_736->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_736->release(); fb_736 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_736->receive (fbStatus, 0, 0, 257, CAST_MSG(&fb_738));
	   if (!fb_738.fb_739 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!/*REL.RDB$OWNER_NAME.NULL*/
		     fb_738.fb_740) {
			isqlGlob.printf("%sOwner: %s%s", TAB_AS_SPACES, /*REL.RDB$OWNER_NAME*/
									fb_738.fb_741, NEWLINE);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}

	// Query for files

	/*FOR FIL IN RDB$FILES
		SORTED BY FIL.RDB$SHADOW_NUMBER, FIL.RDB$FILE_SEQUENCE*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_721 && fbTrans && DB)
	      fb_721 = DB->compileRequest(fbStatus, sizeof(fb_722), fb_722);
	   if (fbTrans && fb_721)
	      fb_721->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_721->release(); fb_721 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_721->receive (fbStatus, 0, 0, 282, CAST_MSG(&fb_723));
	   if (!fb_723.fb_727 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// reset nulls to zero

		if (/*FIL.RDB$FILE_FLAGS.NULL*/
		    fb_723.fb_734)
			/*FIL.RDB$FILE_FLAGS*/
			fb_723.fb_735 = 0;
		if (/*FIL.RDB$FILE_LENGTH.NULL*/
		    fb_723.fb_733)
			/*FIL.RDB$FILE_LENGTH*/
			fb_723.fb_726 = 0;
		if (/*FIL.RDB$FILE_SEQUENCE.NULL*/
		    fb_723.fb_731)
			/*FIL.RDB$FILE_SEQUENCE*/
			fb_723.fb_732 = 0;
		if (/*FIL.RDB$FILE_START.NULL*/
		    fb_723.fb_730)
			/*FIL.RDB$FILE_START*/
			fb_723.fb_725 = 0;
		if (!/*FIL.RDB$FILE_NAME.NULL*/
		     fb_723.fb_729)
			fb_utils::exact_name(/*FIL.RDB$FILE_NAME*/
					     fb_723.fb_724);

		if (/*FIL.RDB$FILE_FLAGS*/
		    fb_723.fb_735 == 0)
		{
			isqlGlob.printf(" File %d: \"%s\", length %ld, start %ld%s",
					 /*FIL.RDB$FILE_SEQUENCE*/
					 fb_723.fb_732, /*FIL.RDB$FILE_NAME*/
  fb_723.fb_724,
					 /*FIL.RDB$FILE_LENGTH*/
					 fb_723.fb_726, /*FIL.RDB$FILE_START*/
  fb_723.fb_725, NEWLINE);
		}
		else if (/*FIL.RDB$FILE_FLAGS*/
			 fb_723.fb_735 & FILE_shadow)
		{
			if (/*FIL.RDB$FILE_SEQUENCE*/
			    fb_723.fb_732) {
				isqlGlob.printf("%sfile %s ", TAB_AS_SPACES, /*FIL.RDB$FILE_NAME*/
									     fb_723.fb_724);
			}
			else
			{
				isqlGlob.printf(" Shadow %d: \"%s\" ", /*FIL.RDB$SHADOW_NUMBER*/
								       fb_723.fb_728, /*FIL.RDB$FILE_NAME*/
  fb_723.fb_724);
				if (/*FIL.RDB$FILE_FLAGS*/
				    fb_723.fb_735 & FILE_inactive) {
					isqlGlob.printf("inactive ");
				}
				if (/*FIL.RDB$FILE_FLAGS*/
				    fb_723.fb_735 & FILE_manual) {
					isqlGlob.printf("manual ");
				}
				else {
					isqlGlob.printf("auto ");
				}
				if (/*FIL.RDB$FILE_FLAGS*/
				    fb_723.fb_735 & FILE_conditional) {
					isqlGlob.printf("conditional ");
				}
			}
			if (/*FIL.RDB$FILE_LENGTH*/
			    fb_723.fb_726) {
				isqlGlob.printf("length %ld ", /*FIL.RDB$FILE_LENGTH*/
							       fb_723.fb_726);
			}
			if (/*FIL.RDB$FILE_START*/
			    fb_723.fb_725) {
				isqlGlob.printf("starting %ld", /*FIL.RDB$FILE_START*/
								fb_723.fb_725);
			}
			isqlGlob.printf(NEWLINE);
		}
		else if ((/*FIL.RDB$FILE_FLAGS*/
			  fb_723.fb_735 & FILE_difference) && !/*FIL.RDB$FILE_NAME.NULL*/
			fb_723.fb_729)
		{
			// This is an explicit name for the difference file typically named <db_name>.delta
			isqlGlob.printf("Explicit physical backup difference file: \"%s\"%s",
					/*FIL.RDB$FILE_NAME*/
					fb_723.fb_724, NEWLINE);
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}

	std::string info_buf;

	// First general database parameters

	bool translate = true;

	if (SHOW_dbb_parameters(DB, info_buf, db_items, sizeof(db_items), translate, NEWLINE)) {
		isqlGlob.prints(info_buf.c_str());
	}

	/*FOR DBB IN RDB$DATABASE
		CROSS CS IN RDB$CHARACTER_SETS
		WITH CS.RDB$CHARACTER_SET_NAME EQ DBB.RDB$CHARACTER_SET_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_714 && fbTrans && DB)
	      fb_714 = DB->compileRequest(fbStatus, sizeof(fb_715), fb_715);
	   if (fbTrans && fb_714)
	      fb_714->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_714->release(); fb_714 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_714->receive (fbStatus, 0, 0, 510, CAST_MSG(&fb_716));
	   if (!fb_716.fb_717 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*DBB.RDB$CHARACTER_SET_NAME*/
				     fb_716.fb_719);
		fb_utils::exact_name(/*CS.RDB$DEFAULT_COLLATE_NAME*/
				     fb_716.fb_720);

		isqlGlob.printf("Default Character set: %s", /*DBB.RDB$CHARACTER_SET_NAME*/
							     fb_716.fb_719);

		if (!/*CS.RDB$DEFAULT_COLLATE_NAME.NULL*/
		     fb_716.fb_718 &&
			strcmp(/*CS.RDB$DEFAULT_COLLATE_NAME*/
			       fb_716.fb_720, /*DBB.RDB$CHARACTER_SET_NAME*/
  fb_716.fb_719) != 0)
		{
			isqlGlob.printf(" (with Default Collation %s)", /*CS.RDB$DEFAULT_COLLATE_NAME*/
									fb_716.fb_720);
		}

		isqlGlob.printf("%s", NEWLINE);
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}

	int pass = 0;
	/*FOR DBB2 IN RDB$DATABASE*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_706 && fbTrans && DB)
	      fb_706 = DB->compileRequest(fbStatus, sizeof(fb_707), fb_707);
	   if (fbTrans && fb_706)
	      fb_706->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_706->release(); fb_706 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_706->receive (fbStatus, 0, 0, 11, CAST_MSG(&fb_708));
	   if (!fb_708.fb_710 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!/*DBB2.RDB$LINGER.NULL*/
		     fb_708.fb_712 && /*DBB2.RDB$LINGER*/
    fb_708.fb_709 > 0)
			isqlGlob.printf("Linger: %d seconds%s", /*DBB2.RDB$LINGER*/
								fb_708.fb_709, NEWLINE);

		if (!/*DBB2.RDB$SQL_SECURITY.NULL*/
		     fb_708.fb_711 && /*DBB2.RDB$SQL_SECURITY*/
    fb_708.fb_713 == FB_TRUE)
			isqlGlob.printf("SQL SECURITY DEFINER%s", NEWLINE);

		if (++pass > 1)
			isqlGlob.printf("RDB$DATABASE has more than one record%s", NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}

	if (isqlGlob.major_ods >= ODS_VERSION13)
	{
		bool published = false;

		/*FOR PUB IN RDB$PUBLICATIONS WITH
			PUB.RDB$ACTIVE_FLAG > 0*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_702 && fbTrans && DB)
		      fb_702 = DB->compileRequest(fbStatus, sizeof(fb_703), fb_703);
		   if (fbTrans && fb_702)
		      fb_702->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_702->release(); fb_702 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_702->receive (fbStatus, 0, 0, 2, CAST_MSG(&fb_704));
		   if (!fb_704.fb_705 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			published = true;
			break;

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return;
		/*END_ERROR;*/
		   }
		}

		isqlGlob.printf("Publication: %s%s", published ? "Enabled" : "Disabled", NEWLINE);
	}
}


// *********************************
// s h o w _ d e p e n d e n c i e s   (front-end)
// *********************************
// Iterate through all types of objects types (as per jrd/obj.h) calling the
// overloaded routine. We try to fetch all possible object types that share
// the same name.
static processing_state show_dependencies(const char* object)
{
	if (!object || !object[0])
		return ps_ERR;

	bool missing = true;

	for (int i = 0; i < FB_NELEM(Object_types); ++i)
	{
		if (show_dependencies(object, i) == SKIP)
		{
			missing = false;
			isqlGlob.printf("+++%s", NEWLINE);
		}
	}

	return missing ? OBJECT_NOT_FOUND : SKIP;
}


// *********************************
// s h o w _ d e p e n d e n c i e s    (processor)
// *********************************
// Discover dependencies. Format:
// - objects that depend on this object
// - objects this object depends on
// - if it's table or view, list fields that depend on other objects
// - if it's procedure, list parameters that depend on other objects (unlikely for now)
static processing_state show_dependencies(const char* object, int obj_type)
{
   struct fb_652_struct {
          ISC_QUAD fb_653;	/* RDB$COMPUTED_BLR */
          short fb_654;	/* fbUtility */
          short fb_655;	/* RDB$PARAMETER_TYPE */
          short fb_656;	/* RDB$DEPENDED_ON_TYPE */
          short fb_657;	/* gds__null_flag */
          char  fb_658 [253];	/* RDB$PARAMETER_NAME */
          char  fb_659 [253];	/* RDB$FIELD_NAME */
          char  fb_660 [253];	/* RDB$DEPENDED_ON_NAME */
   } fb_652;
   struct fb_649_struct {
          short fb_650;	/* RDB$DEPENDENT_TYPE */
          char  fb_651 [253];	/* RDB$PROCEDURE_NAME */
   } fb_649;
   struct fb_666_struct {
          ISC_QUAD fb_667;	/* RDB$COMPUTED_BLR */
          short fb_668;	/* fbUtility */
          short fb_669;	/* RDB$DEPENDED_ON_TYPE */
          short fb_670;	/* gds__null_flag */
          char  fb_671 [253];	/* RDB$FIELD_NAME */
          char  fb_672 [253];	/* RDB$FIELD_NAME */
          char  fb_673 [253];	/* RDB$DEPENDED_ON_NAME */
   } fb_666;
   struct fb_663_struct {
          short fb_664;	/* RDB$DEPENDENT_TYPE */
          char  fb_665 [253];	/* RDB$RELATION_NAME */
   } fb_663;
   struct fb_678_struct {
          short fb_679;	/* fbUtility */
   } fb_678;
   struct fb_676_struct {
          char  fb_677 [253];	/* RDB$RELATION_NAME */
   } fb_676;
   struct fb_685_struct {
          short fb_686;	/* fbUtility */
          short fb_687;	/* gds__null_flag */
          short fb_688;	/* RDB$DEPENDED_ON_TYPE */
          char  fb_689 [253];	/* RDB$DEPENDED_ON_NAME */
          char  fb_690 [253];	/* RDB$FIELD_NAME */
   } fb_685;
   struct fb_682_struct {
          short fb_683;	/* RDB$DEPENDENT_TYPE */
          char  fb_684 [253];	/* RDB$DEPENDENT_NAME */
   } fb_682;
   struct fb_696_struct {
          short fb_697;	/* fbUtility */
          short fb_698;	/* gds__null_flag */
          short fb_699;	/* RDB$DEPENDENT_TYPE */
          char  fb_700 [253];	/* RDB$DEPENDENT_NAME */
          char  fb_701 [253];	/* RDB$FIELD_NAME */
   } fb_696;
   struct fb_693_struct {
          short fb_694;	/* RDB$DEPENDED_ON_TYPE */
          char  fb_695 [253];	/* RDB$DEPENDED_ON_NAME */
   } fb_693;
	bool missing1 = true;

	/*FOR DEP1 IN RDB$DEPENDENCIES
		WITH DEP1.RDB$DEPENDED_ON_NAME EQ object AND
			 DEP1.RDB$DEPENDED_ON_TYPE EQ obj_type*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_691 && fbTrans && DB)
	      fb_691 = DB->compileRequest(fbStatus, sizeof(fb_692), fb_692);
	   fb_693.fb_694 = obj_type;
	   isc_vtov ((const char*) object, (char*) fb_693.fb_695, 253);
	   if (fbTrans && fb_691)
	      fb_691->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_693));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_691->release(); fb_691 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_691->receive (fbStatus, 0, 1, 512, CAST_MSG(&fb_696));
	   if (!fb_696.fb_697 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		if (missing1)
			missing1 = false;
		else
			isqlGlob.prints(", ");

		fb_utils::exact_name(/*DEP1.RDB$DEPENDENT_NAME*/
				     fb_696.fb_700);
		const char* type_name = Object_types[/*DEP1.RDB$DEPENDENT_TYPE*/
						     fb_696.fb_699];
		if (/*DEP1.RDB$FIELD_NAME.NULL*/
		    fb_696.fb_698)
			isqlGlob.printf("%s:%s", /*DEP1.RDB$DEPENDENT_NAME*/
						 fb_696.fb_700, type_name);
		else
		{
			fb_utils::exact_name(/*DEP1.RDB$FIELD_NAME*/
					     fb_696.fb_701);
			isqlGlob.printf("%s:%s->%s", /*DEP1.RDB$DEPENDENT_NAME*/
						     fb_696.fb_700, type_name, /*DEP1.RDB$FIELD_NAME*/
	     fb_696.fb_701);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	bool target_done = false;
	if (!missing1)
	{
		const char* type_name = Object_types[obj_type];
		isqlGlob.printf("%s%s[%s:%s]", NEWLINE, TAB_AS_SPACES, object, type_name);
		target_done = true;
	}

	bool missing2 = true;

	/*FOR DEP2 IN RDB$DEPENDENCIES
		WITH DEP2.RDB$DEPENDENT_NAME EQ object AND
			 DEP2.RDB$DEPENDENT_TYPE EQ obj_type*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_680 && fbTrans && DB)
	      fb_680 = DB->compileRequest(fbStatus, sizeof(fb_681), fb_681);
	   fb_682.fb_683 = obj_type;
	   isc_vtov ((const char*) object, (char*) fb_682.fb_684, 253);
	   if (fbTrans && fb_680)
	      fb_680->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_682));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_680->release(); fb_680 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_680->receive (fbStatus, 0, 1, 512, CAST_MSG(&fb_685));
	   if (!fb_685.fb_686 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		if (missing2)
		{
			if (!target_done)
			{
				const char* type_name = Object_types[obj_type];
				isqlGlob.printf("%s[%s:%s]", TAB_AS_SPACES, object, type_name);
				target_done = true;
			}

			isqlGlob.prints(NEWLINE);
			missing2 = false;
		}
		else
			isqlGlob.prints(", ");

		fb_utils::exact_name(/*DEP2.RDB$DEPENDED_ON_NAME*/
				     fb_685.fb_689);
		const char* type_name = Object_types[/*DEP2.RDB$DEPENDED_ON_TYPE*/
						     fb_685.fb_688];
		if (/*DEP2.RDB$FIELD_NAME.NULL*/
		    fb_685.fb_687)
			isqlGlob.printf("%s:%s", /*DEP2.RDB$DEPENDED_ON_NAME*/
						 fb_685.fb_689, type_name);
		else
		{
			fb_utils::exact_name(/*DEP2.RDB$FIELD_NAME*/
					     fb_685.fb_690);
			isqlGlob.printf("%s:%s<-%s", /*DEP2.RDB$DEPENDED_ON_NAME*/
						     fb_685.fb_689, type_name, /*DEP2.RDB$FIELD_NAME*/
	     fb_685.fb_690);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	bool is_table = false;

	/*FOR REL IN RDB$RELATIONS
		WITH REL.RDB$RELATION_NAME = object AND
			 REL.RDB$VIEW_BLR MISSING AND
			 (REL.RDB$DBKEY_LENGTH MISSING OR REL.RDB$DBKEY_LENGTH = 8)*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_674 && fbTrans && DB)
	      fb_674 = DB->compileRequest(fbStatus, sizeof(fb_675), fb_675);
	   isc_vtov ((const char*) object, (char*) fb_676.fb_677, 253);
	   if (fbTrans && fb_674)
	      fb_674->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_676));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_674->release(); fb_674 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_674->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_678));
	   if (!fb_678.fb_679 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		is_table = true;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	bool missing3 = true;
	if ((obj_type == obj_relation && is_table) || (obj_type == obj_view && !is_table))
	{
		/*FOR RFR IN RDB$RELATION_FIELDS
			CROSS FLD IN RDB$FIELDS
			CROSS DEP3 IN RDB$DEPENDENCIES
			WITH RFR.RDB$RELATION_NAME = object AND
				 RFR.RDB$FIELD_SOURCE = FLD.RDB$FIELD_NAME AND
				 DEP3.RDB$DEPENDENT_NAME = RFR.RDB$FIELD_SOURCE AND
				 DEP3.RDB$DEPENDENT_TYPE EQ obj_computed
			SORTED BY RFR.RDB$FIELD_POSITION*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_661 && fbTrans && DB)
		      fb_661 = DB->compileRequest(fbStatus, sizeof(fb_662), fb_662);
		   fb_663.fb_664 = obj_computed;
		   isc_vtov ((const char*) object, (char*) fb_663.fb_665, 253);
		   if (fbTrans && fb_661)
		      fb_661->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_663));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_661->release(); fb_661 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_661->receive (fbStatus, 0, 1, 773, CAST_MSG(&fb_666));
		   if (!fb_666.fb_668 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if (/*FLD.RDB$COMPUTED_BLR.NULL*/
			    fb_666.fb_670) // redundant
				continue;

			if (missing3)
			{
				if (!target_done)
				{
					const char* type_name = Object_types[obj_type];
					isqlGlob.printf("%s[%s:%s]", TAB_AS_SPACES, object, type_name);
					target_done = true;
				}

				isqlGlob.prints(NEWLINE);
				missing3 = false;
			}
			else
				isqlGlob.prints(", ");

			fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
					     fb_666.fb_671);
			fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
					     fb_666.fb_672);
			fb_utils::exact_name(/*DEP3.RDB$DEPENDED_ON_NAME*/
					     fb_666.fb_673);
			const char* type_name = Object_types[/*DEP3.RDB$DEPENDED_ON_TYPE*/
							     fb_666.fb_669];
			isqlGlob.printf("Field-%s(%s):%s->%s:%s", /*RFR.RDB$FIELD_NAME*/
								  fb_666.fb_671, /*FLD.RDB$FIELD_NAME*/
  fb_666.fb_672,
				Object_types[obj_computed], /*DEP3.RDB$DEPENDED_ON_NAME*/
							    fb_666.fb_673, type_name);
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}
	else if (obj_type == obj_procedure)
	{
		// This code shouldn't find anything under normal circumstances.
		/*FOR PAR IN RDB$PROCEDURE_PARAMETERS
			CROSS FLD IN RDB$FIELDS
			CROSS DEP3 IN RDB$DEPENDENCIES
			WITH PAR.RDB$PROCEDURE_NAME = object AND
				 PAR.RDB$FIELD_SOURCE = FLD.RDB$FIELD_NAME AND
				 DEP3.RDB$DEPENDENT_NAME = PAR.RDB$FIELD_SOURCE AND
				 DEP3.RDB$DEPENDENT_TYPE EQ obj_computed
			SORTED BY PAR.RDB$PARAMETER_TYPE, PAR.RDB$PARAMETER_NUMBER*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_647 && fbTrans && DB)
		      fb_647 = DB->compileRequest(fbStatus, sizeof(fb_648), fb_648);
		   fb_649.fb_650 = obj_computed;
		   isc_vtov ((const char*) object, (char*) fb_649.fb_651, 253);
		   if (fbTrans && fb_647)
		      fb_647->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_649));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_647->release(); fb_647 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_647->receive (fbStatus, 0, 1, 775, CAST_MSG(&fb_652));
		   if (!fb_652.fb_654 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			if (/*FLD.RDB$COMPUTED_BLR.NULL*/
			    fb_652.fb_657) // redundant
				continue;

			if (missing3)
			{
				if (!target_done)
				{
					const char* type_name = Object_types[obj_type];
					isqlGlob.printf("%s[%s:%s]", TAB_AS_SPACES, object, type_name);
					target_done = true;
				}

				isqlGlob.prints(NEWLINE);
				missing3 = false;
			}
			else
				isqlGlob.prints(", ");

			fb_utils::exact_name(/*PAR.RDB$PARAMETER_NAME*/
					     fb_652.fb_658);
			fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
					     fb_652.fb_659);
			fb_utils::exact_name(/*DEP3.RDB$DEPENDED_ON_NAME*/
					     fb_652.fb_660);
			const char* type_name = Object_types[/*DEP3.RDB$DEPENDED_ON_TYPE*/
							     fb_652.fb_656];
			isqlGlob.printf("%s-%s(%s):%s->%s:%s", /*PAR.RDB$PARAMETER_TYPE*/
							       fb_652.fb_655 ? "Output" : "Input",
				/*PAR.RDB$PARAMETER_NAME*/
				fb_652.fb_658, /*FLD.RDB$FIELD_NAME*/
  fb_652.fb_659,
				Object_types[obj_computed], /*DEP3.RDB$DEPENDED_ON_NAME*/
							    fb_652.fb_660, type_name);
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}


	if (!target_done)
		return OBJECT_NOT_FOUND;

	isqlGlob.prints(NEWLINE);
	return SKIP;
}


static processing_state show_dialect()
{
/**************************************
 *
 *	s h o w _ d i a l e c t
 *
 **************************************
 *
 *	Print out the SQL dialect information
 *
 **************************************/

	if (isqlGlob.db_SQL_dialect > 0)
	{
		isqlGlob.printf("%38s%d%s%d",
				"Client SQL dialect is set to: ", isqlGlob.SQL_dialect,
				" and database SQL dialect is: ", isqlGlob.db_SQL_dialect);
	}
	else if (isqlGlob.SQL_dialect == 0)
	{
		isqlGlob.printf("%38s%s",
				"Client SQL dialect has not been set",
				" and no database has been connected yet.");
	}
	else
	{
		isqlGlob.printf("%38s%d%s",
				"Client SQL dialect is set to: ", isqlGlob.SQL_dialect,
				". No database has been connected.");
	}
	isqlGlob.printf(NEWLINE);
	return SKIP;
}


static processing_state show_domains(const SCHAR* domain_name)
{
   struct fb_625_struct {
          ISC_QUAD fb_626;	/* RDB$VALIDATION_SOURCE */
          short fb_627;	/* fbUtility */
          short fb_628;	/* gds__null_flag */
          short fb_629;	/* RDB$NULL_FLAG */
          short fb_630;	/* gds__null_flag */
          short fb_631;	/* RDB$COLLATION_ID */
          short fb_632;	/* gds__null_flag */
          short fb_633;	/* RDB$CHARACTER_SET_ID */
          short fb_634;	/* RDB$SEGMENT_LENGTH */
          short fb_635;	/* RDB$FIELD_SCALE */
          short fb_636;	/* RDB$FIELD_PRECISION */
          short fb_637;	/* RDB$FIELD_SUB_TYPE */
          short fb_638;	/* RDB$FIELD_TYPE */
          short fb_639;	/* gds__null_flag */
          short fb_640;	/* RDB$DIMENSIONS */
          char  fb_641 [253];	/* RDB$FIELD_NAME */
   } fb_625;
   struct fb_623_struct {
          char  fb_624 [253];	/* RDB$FIELD_NAME */
   } fb_623;
   struct fb_644_struct {
          short fb_645;	/* fbUtility */
          char  fb_646 [253];	/* RDB$FIELD_NAME */
   } fb_644;
/*************************************
*
*	s h o w _ d o m a i n s
*
**************************************
*
* Functional description
*	Show all domains or the named domain
************************************/
	bool first = true;

	if (!*domain_name)
	{
		//  List all domain names in columns
		/*FOR FLD IN RDB$FIELDS WITH
			FLD.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
			AND FLD.RDB$SYSTEM_FLAG NE 1
			SORTED BY FLD.RDB$FIELD_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_642 && fbTrans && DB)
		      fb_642 = DB->compileRequest(fbStatus, sizeof(fb_643), fb_643);
		   if (fbTrans && fb_642)
		      fb_642->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_642->release(); fb_642 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_642->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_644));
		   if (!fb_644.fb_645 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			first = false;
			isqlGlob.printf("%s%s", fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
								     fb_644.fb_646), NEWLINE);
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
		if (!first)
			isqlGlob.printf(NEWLINE);
	}
	else
	{
		// List named domain

		/*FOR FLD IN RDB$FIELDS WITH
			FLD.RDB$FIELD_NAME EQ domain_name*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_621 && fbTrans && DB)
		      fb_621 = DB->compileRequest(fbStatus, sizeof(fb_622), fb_622);
		   isc_vtov ((const char*) domain_name, (char*) fb_623.fb_624, 253);
		   if (fbTrans && fb_621)
		      fb_621->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_623));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_621->release(); fb_621 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_621->receive (fbStatus, 0, 1, 289, CAST_MSG(&fb_625));
		   if (!fb_625.fb_627 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;;

			first = false;
			// Print the name of the domain
			fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
					     fb_625.fb_641);
			isqlGlob.printf("%-31s ", /*FLD.RDB$FIELD_NAME*/
						  fb_625.fb_641);

			// Array dimensions
			if (!/*FLD.RDB$DIMENSIONS.NULL*/
			     fb_625.fb_639)
			{
				isqlGlob.printf("ARRAY OF ");
				ISQL_array_dimensions (/*FLD.RDB$FIELD_NAME*/
						       fb_625.fb_641);
				isqlGlob.printf("%s                                ", NEWLINE);
			}

			if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
						   fb_625.fb_641, /*FLD.RDB$FIELD_TYPE*/
  fb_625.fb_638, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_625.fb_637,
					/*FLD.RDB$FIELD_PRECISION*/
					fb_625.fb_636, /*FLD.RDB$FIELD_SCALE*/
  fb_625.fb_635))
			{
				return ps_ERR;
			}

			// Length for CHARs
			if ((/*FLD.RDB$FIELD_TYPE*/
			     fb_625.fb_638 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_625.fb_638 == blr_varying)) {
				isqlGlob.printf("(%d)", ISQL_get_field_length(/*FLD.RDB$FIELD_NAME*/
									      fb_625.fb_641));
			}

			// Blob domains
			if (/*FLD.RDB$FIELD_TYPE*/
			    fb_625.fb_638 == blr_blob)
			{
				isqlGlob.printf(" segment %u, subtype ", (USHORT) /*FLD.RDB$SEGMENT_LENGTH*/
										  fb_625.fb_634);
				const int subtype  = /*FLD.RDB$FIELD_SUB_TYPE*/
						     fb_625.fb_637;
				if (subtype >= 0 && subtype <= MAX_BLOBSUBTYPES) {
					isqlGlob.prints(Sub_types[subtype]);
				}
				else {
					isqlGlob.printf("%d", subtype);
				}
			}

			// Show international character sets
			SSHORT char_set_id = 0;
			if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
			     fb_625.fb_632)
				char_set_id = /*FLD.RDB$CHARACTER_SET_ID*/
					      fb_625.fb_633;

			SSHORT collation = 0;
			if (!/*FLD.RDB$COLLATION_ID.NULL*/
			     fb_625.fb_630)
				collation = /*FLD.RDB$COLLATION_ID*/
					    fb_625.fb_631;

			if (((/*FLD.RDB$FIELD_TYPE*/
			      fb_625.fb_638 == blr_text ||
				/*FLD.RDB$FIELD_TYPE*/
				fb_625.fb_638 == blr_varying) && /*FLD.RDB$FIELD_SUB_TYPE*/
		    fb_625.fb_637 != fb_text_subtype_binary) ||
				/*FLD.RDB$FIELD_TYPE*/
				fb_625.fb_638 == blr_blob && /*FLD.RDB$FIELD_SUB_TYPE*/
		fb_625.fb_637 == isc_blob_text)
			{
				show_charsets(char_set_id, collation);
			}

			if (/*FLD.RDB$NULL_FLAG*/
			    fb_625.fb_629 != 1) {
				isqlGlob.printf(" Nullable");
			}
			else {
				isqlGlob.printf(" Not Null");
			}
			isqlGlob.printf(NEWLINE);

			ISC_QUAD default_source;
			ISQL_get_default_source (NULL, /*FLD.RDB$FIELD_NAME*/
						       fb_625.fb_641, &default_source);
			if (default_source.gds_quad_high)
			{
				isqlGlob.printf("                                ");
				SHOW_print_metadata_text_blob (isqlGlob.Out, &default_source);
				isqlGlob.printf(NEWLINE);
			}

			if (!/*FLD.RDB$VALIDATION_SOURCE.NULL*/
			     fb_625.fb_628)
			{
				isqlGlob.printf("                                ");
				SHOW_print_metadata_text_blob (isqlGlob.Out, &/*FLD.RDB$VALIDATION_SOURCE*/
									      fb_625.fb_626);
				isqlGlob.printf(NEWLINE);
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}
	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static processing_state show_exceptions(const SCHAR* object)
{
   struct fb_610_struct {
          short fb_611;	/* fbUtility */
          short fb_612;	/* RDB$DEPENDENT_TYPE */
          char  fb_613 [253];	/* RDB$DEPENDENT_NAME */
   } fb_610;
   struct fb_607_struct {
          short fb_608;	/* RDB$DEPENDED_ON_TYPE */
          char  fb_609 [253];	/* RDB$EXCEPTION_NAME */
   } fb_607;
   struct fb_616_struct {
          char  fb_617 [1024];	/* RDB$MESSAGE */
          short fb_618;	/* fbUtility */
          short fb_619;	/* gds__null_flag */
          char  fb_620 [253];	/* RDB$EXCEPTION_NAME */
   } fb_616;
/**************************************
 *
 *	s h o w _ e x c e p t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show exceptions and their dependencies
 *	This version fetches all the exceptions, and only prints the
 *	one you asked for if you ask for one.  It could be optimized
 *	like other such functions.
 *
 **************************************/
	bool first = true;
	SCHAR type[20];

	//fb_utils::exact_name(object); It already comes trimmed.

	/*FOR EXC IN RDB$EXCEPTIONS
		SORTED BY EXC.RDB$EXCEPTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_614 && fbTrans && DB)
	      fb_614 = DB->compileRequest(fbStatus, sizeof(fb_615), fb_615);
	   if (fbTrans && fb_614)
	      fb_614->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_614->release(); fb_614 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_614->receive (fbStatus, 0, 0, 1281, CAST_MSG(&fb_616));
	   if (!fb_616.fb_618 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
	{
		fb_utils::exact_name(/*EXC.RDB$EXCEPTION_NAME*/
				     fb_616.fb_620);
		// List all objects if none specified, or just the named exception

		if (!*object || !strcmp (/*EXC.RDB$EXCEPTION_NAME*/
					 fb_616.fb_620, object))
		{
			first = false;

			isqlGlob.printf("%s", /*EXC.RDB$EXCEPTION_NAME*/
					      fb_616.fb_620);

			if (!/*EXC.RDB$MESSAGE.NULL*/
			     fb_616.fb_619 && strlen(/*EXC.RDB$MESSAGE*/
	   fb_616.fb_617))
				isqlGlob.printf("; Msg: %s", /*EXC.RDB$MESSAGE*/
							     fb_616.fb_617);

			// Look up dependent objects --procedures and triggers
			bool first_dep = true;
			/*FOR DEP IN RDB$DEPENDENCIES WITH
				DEP.RDB$DEPENDED_ON_TYPE = obj_exception AND
				DEP.RDB$DEPENDED_ON_NAME EQ EXC.RDB$EXCEPTION_NAME
				SORTED BY DEP.RDB$DEPENDENT_TYPE, DEP.RDB$DEPENDENT_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_605 && fbTrans && DB)
			      fb_605 = DB->compileRequest(fbStatus, sizeof(fb_606), fb_606);
			   fb_607.fb_608 = obj_exception;
			   isc_vtov ((const char*) fb_616.fb_620, (char*) fb_607.fb_609, 253);
			   if (fbTrans && fb_605)
			      fb_605->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_607));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_605->release(); fb_605 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_605->receive (fbStatus, 0, 1, 257, CAST_MSG(&fb_610));
			   if (!fb_610.fb_611 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			{
				if (first_dep)
				{
					isqlGlob.printf("; Used by: ");
					first_dep = false;
				}
				else
					isqlGlob.printf(", ");

				fb_utils::exact_name(/*DEP.RDB$DEPENDENT_NAME*/
						     fb_610.fb_613);

				switch (/*DEP.RDB$DEPENDENT_TYPE*/
					fb_610.fb_612)
				{
					case obj_trigger:
						strcpy (type, "Trigger");
						break;
					case obj_procedure:
						strcpy (type, "Stored procedure");
						break;
					default:
						strcpy (type, "Unknown");
						break;
				}

				isqlGlob.printf("%s (%s)", /*DEP.RDB$DEPENDENT_NAME*/
							   fb_610.fb_613, type);
			}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg (fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}

			isqlGlob.printf(NEWLINE);
		}
	}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static processing_state show_filters(const SCHAR* object)
{
   struct fb_593_struct {
          char  fb_594 [256];	/* RDB$ENTRYPOINT */
          char  fb_595 [256];	/* RDB$MODULE_NAME */
          short fb_596;	/* fbUtility */
          short fb_597;	/* RDB$OUTPUT_SUB_TYPE */
          short fb_598;	/* RDB$INPUT_SUB_TYPE */
          char  fb_599 [253];	/* RDB$FUNCTION_NAME */
   } fb_593;
   struct fb_591_struct {
          char  fb_592 [253];	/* RDB$FUNCTION_NAME */
   } fb_591;
   struct fb_602_struct {
          short fb_603;	/* fbUtility */
          char  fb_604 [253];	/* RDB$FUNCTION_NAME */
   } fb_602;
/**************************************
 *
 *	s h o w _ f i l t e r s
 *
 **************************************
 *
 * Functional description
 *	Show blob filters in general or  for the named filters
 *
 **************************************/
	bool first = true;

	// Show all functions
	if (!*object)
	{
		/*FOR FIL IN RDB$FILTERS
		SORTED BY FIL.RDB$FUNCTION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_600 && fbTrans && DB)
		      fb_600 = DB->compileRequest(fbStatus, sizeof(fb_601), fb_601);
		   if (fbTrans && fb_600)
		      fb_600->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_600->release(); fb_600 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_600->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_602));
		   if (!fb_602.fb_603 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			first = false;
			fb_utils::exact_name(/*FIL.RDB$FUNCTION_NAME*/
					     fb_602.fb_604);
			isqlGlob.printf("%s%s", /*FIL.RDB$FUNCTION_NAME*/
						fb_602.fb_604, NEWLINE);
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
		if (!first)
		{
			isqlGlob.printf(NEWLINE);
			return (SKIP);
		}

		return OBJECT_NOT_FOUND;
	}

	// We have a filter name, so expand on it
	/*FOR FIL IN RDB$FILTERS WITH
	   FIL.RDB$FUNCTION_NAME EQ object*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_589 && fbTrans && DB)
	      fb_589 = DB->compileRequest(fbStatus, sizeof(fb_590), fb_590);
	   isc_vtov ((const char*) object, (char*) fb_591.fb_592, 253);
	   if (fbTrans && fb_589)
	      fb_589->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_591));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_589->release(); fb_589 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_589->receive (fbStatus, 0, 1, 771, CAST_MSG(&fb_593));
	   if (!fb_593.fb_596 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

	   first = false;

	   fb_utils::exact_name(/*FIL.RDB$FUNCTION_NAME*/
				fb_593.fb_599);
	   fb_utils::exact_name(/*FIL.RDB$MODULE_NAME*/
				fb_593.fb_595);
	   fb_utils::exact_name(/*FIL.RDB$ENTRYPOINT*/
				fb_593.fb_594);

	   isqlGlob.printf("BLOB Filter: %s %s%sInput subtype: %d Output subtype: %d%s",
				/*FIL.RDB$FUNCTION_NAME*/
				fb_593.fb_599, NEWLINE,
				TAB_AS_SPACES, /*FIL.RDB$INPUT_SUB_TYPE*/
					       fb_593.fb_598, /*FIL.RDB$OUTPUT_SUB_TYPE*/
  fb_593.fb_597, NEWLINE);
	   isqlGlob.printf("%sFilter library is %s%s%sEntry point is %s%s%s",
				TAB_AS_SPACES, /*FIL.RDB$MODULE_NAME*/
					       fb_593.fb_595, NEWLINE,
				TAB_AS_SPACES, /*FIL.RDB$ENTRYPOINT*/
					       fb_593.fb_594, NEWLINE,
				NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static processing_state show_sys_functions(const SCHAR* msg)
{
   struct fb_585_struct {
          short fb_586;	/* fbUtility */
          char  fb_587 [253];	/* RDB$PACKAGE_NAME */
          char  fb_588 [253];	/* RDB$FUNCTION_NAME */
   } fb_585;
/**************************************
 *
 *	s h o w _ s y s _ f u n c t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show system functions.
 *
 **************************************/

	bool first = true;

	/*FOR FUN IN RDB$FUNCTIONS
		WITH FUN.RDB$SYSTEM_FLAG EQ 1
		SORTED BY FUN.RDB$PACKAGE_NAME, FUN.RDB$FUNCTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_583 && fbTrans && DB)
	      fb_583 = DB->compileRequest(fbStatus, sizeof(fb_584), fb_584);
	   if (fbTrans && fb_583)
	      fb_583->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_583->release(); fb_583 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_583->receive (fbStatus, 0, 0, 508, CAST_MSG(&fb_585));
	   if (!fb_585.fb_586 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
	{
		if (first)
		{
			first = false;
			if (msg)
				isqlGlob.printf("%s%s", msg, NEWLINE);
		}

		// Strip trailing blanks
		MetaString package(/*FUN.RDB$PACKAGE_NAME*/
				   fb_585.fb_587);
		MetaString function(/*FUN.RDB$FUNCTION_NAME*/
				    fb_585.fb_588);

		isqlGlob.printf("%s%s%s%s", package.c_str(), package.hasData() ? "." : "", function.c_str(), NEWLINE);
	}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	return SKIP;
}


static processing_state show_functions(const SCHAR* funcname, bool quoted)
{
   struct fb_562_struct {
          char  fb_563 [256];	/* RDB$MODULE_NAME */
          short fb_564;	/* fbUtility */
          short fb_565;	/* gds__null_flag */
   } fb_562;
   struct fb_559_struct {
          char  fb_560 [253];	/* RDB$FUNCTION_NAME */
          char  fb_561 [253];	/* RDB$PACKAGE_NAME */
   } fb_559;
   struct fb_570_struct {
          short fb_571;	/* fbUtility */
          short fb_572;	/* RDB$DEPENDED_ON_TYPE */
          char  fb_573 [253];	/* RDB$DEPENDED_ON_NAME */
   } fb_570;
   struct fb_568_struct {
          char  fb_569 [253];	/* RDB$FUNCTION_NAME */
   } fb_568;
   struct fb_576_struct {
          short fb_577;	/* fbUtility */
          short fb_578;	/* gds__null_flag */
          short fb_579;	/* gds__null_flag */
          short fb_580;	/* RDB$VALID_BLR */
          char  fb_581 [253];	/* RDB$PACKAGE_NAME */
          char  fb_582 [253];	/* RDB$FUNCTION_NAME */
   } fb_576;
/**************************************
 *
 *	s h o w _ f u n c t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show functions in general or for the named function
 *
 **************************************/

	// If no function name was given, just list the functions

	if (!funcname || !strlen(funcname))
	{
		bool first = true;

		// This query gets the function name; the next query
		// gets all the dependencies if any

		/*FOR FUN IN RDB$FUNCTIONS
			SORTED BY FUN.RDB$PACKAGE_NAME, FUN.RDB$FUNCTION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_574 && fbTrans && DB)
		      fb_574 = DB->compileRequest(fbStatus, sizeof(fb_575), fb_575);
		   if (fbTrans && fb_574)
		      fb_574->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_574->release(); fb_574 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_574->receive (fbStatus, 0, 0, 514, CAST_MSG(&fb_576));
		   if (!fb_576.fb_577 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			first = false;

			MetaString package(/*FUN.RDB$PACKAGE_NAME*/
					   fb_576.fb_581);
			MetaString function(/*FUN.RDB$FUNCTION_NAME*/
					    fb_576.fb_582);

			isqlGlob.printf("%s%s%s", package.c_str(), package.hasData() ? "." : "", function.c_str());

			if (!(/*FUN.RDB$VALID_BLR.NULL*/
			      fb_576.fb_579 || /*FUN.RDB$VALID_BLR*/
    fb_576.fb_580))
				isqlGlob.printf("; Invalid");

			if (/*FUN.RDB$PACKAGE_NAME.NULL*/
			    fb_576.fb_578)
			{
				bool first_dep = true;
				/*FOR DEP IN RDB$DEPENDENCIES WITH
					FUN.RDB$FUNCTION_NAME EQ DEP.RDB$DEPENDENT_NAME
					REDUCED TO DEP.RDB$DEPENDED_ON_TYPE, DEP.RDB$DEPENDED_ON_NAME
					SORTED BY DEP.RDB$DEPENDED_ON_TYPE, DEP.RDB$DEPENDED_ON_NAME*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_566 && fbTrans && DB)
				      fb_566 = DB->compileRequest(fbStatus, sizeof(fb_567), fb_567);
				   isc_vtov ((const char*) fb_576.fb_582, (char*) fb_568.fb_569, 253);
				   if (fbTrans && fb_566)
				      fb_566->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_568));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_566->release(); fb_566 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_566->receive (fbStatus, 0, 1, 257, CAST_MSG(&fb_570));
				   if (!fb_570.fb_571 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
				{
					fb_utils::exact_name(/*DEP.RDB$DEPENDED_ON_NAME*/
							     fb_570.fb_573);

					if (first_dep)
					{
						isqlGlob.printf("; Dependencies: ");
						first_dep = false;
					}
					else
						isqlGlob.printf(", ");

					isqlGlob.printf("%s (%s)", fb_utils::exact_name(/*DEP.RDB$DEPENDED_ON_NAME*/
											fb_570.fb_573),
						Object_types[/*DEP.RDB$DEPENDED_ON_TYPE*/
							     fb_570.fb_572]);
				}
				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg (fbStatus);
					return ps_ERR;
				/*END_ERROR;*/
				   }
				}
			}

			isqlGlob.printf(NEWLINE);
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (first)
			return OBJECT_NOT_FOUND;
		return (SKIP);
	}

	processing_state return_state = OBJECT_NOT_FOUND;

	bool first = true;
	MetaString package, function;
	if (quoted)
		function = funcname;
	else
		parse_package(funcname, package, function);

	/*FOR FUN IN RDB$FUNCTIONS
		WITH FUN.RDB$FUNCTION_NAME EQ function.c_str()
			AND FUN.RDB$PACKAGE_NAME EQUIV NULLIF(package.c_str(), '')*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_557 && DB)
	      fb_557 = DB->compileRequest(fbStatus, sizeof(fb_558), fb_558);
	   isc_vtov ((const char*) function.c_str(), (char*) fb_559.fb_560, 253);
	   isc_vtov ((const char*) package.c_str(), (char*) fb_559.fb_561, 253);
	   fb_557->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_559));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_557->release(); fb_557 = NULL; }
	   else break;
	   }
	while (1)
	   {
	   fb_557->receive (fbStatus, 0, 1, 260, CAST_MSG(&fb_562));
	   if (!fb_562.fb_564) break;

		if (!/*FUN.RDB$MODULE_NAME.NULL*/
		     fb_562.fb_565)
		{
			fb_assert(package.isEmpty());
			return_state = show_func_legacy(function);
		}
		else if (isqlGlob.major_ods >= ODS_VERSION12)
			return_state = show_func(package, function);

	/*END_FOR*/
	   }
	}

	return return_state;
}


static processing_state show_func(const MetaString& package, const MetaString& function)
{
   struct fb_501_struct {
          short fb_502;	/* fbUtility */
          short fb_503;	/* gds__null_flag */
          short fb_504;	/* RDB$COLLATION_ID */
   } fb_501;
   struct fb_498_struct {
          char  fb_499 [253];	/* RDB$RELATION_NAME */
          char  fb_500 [253];	/* RDB$FIELD_NAME */
   } fb_498;
   struct fb_510_struct {
          ISC_QUAD fb_511;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_512;	/* RDB$DEFAULT_SOURCE */
          short fb_513;	/* fbUtility */
          short fb_514;	/* gds__null_flag */
          short fb_515;	/* gds__null_flag */
          short fb_516;	/* RDB$COLLATION_ID */
          short fb_517;	/* gds__null_flag */
          short fb_518;	/* RDB$COLLATION_ID */
          short fb_519;	/* gds__null_flag */
          short fb_520;	/* RDB$CHARACTER_SET_ID */
          short fb_521;	/* gds__null_flag */
          short fb_522;	/* RDB$CHARACTER_LENGTH */
          short fb_523;	/* RDB$FIELD_SCALE */
          short fb_524;	/* RDB$FIELD_PRECISION */
          short fb_525;	/* RDB$FIELD_SUB_TYPE */
          short fb_526;	/* RDB$FIELD_TYPE */
          short fb_527;	/* RDB$SYSTEM_FLAG */
          short fb_528;	/* gds__null_flag */
          short fb_529;	/* gds__null_flag */
          short fb_530;	/* gds__null_flag */
          short fb_531;	/* gds__null_flag */
          short fb_532;	/* RDB$ARGUMENT_MECHANISM */
          short fb_533;	/* RDB$ARGUMENT_POSITION */
          char  fb_534 [253];	/* RDB$ARGUMENT_NAME */
          char  fb_535 [253];	/* RDB$RELATION_NAME */
          char  fb_536 [253];	/* RDB$FIELD_NAME */
          char  fb_537 [253];	/* RDB$FIELD_NAME */
   } fb_510;
   struct fb_507_struct {
          char  fb_508 [253];	/* RDB$FUNCTION_NAME */
          char  fb_509 [253];	/* RDB$PACKAGE_NAME */
   } fb_507;
   struct fb_543_struct {
          ISC_QUAD fb_544;	/* RDB$FUNCTION_SOURCE */
          char  fb_545 [256];	/* RDB$ENTRYPOINT */
          short fb_546;	/* fbUtility */
          short fb_547;	/* RDB$RETURN_ARGUMENT */
          short fb_548;	/* gds__null_flag */
          short fb_549;	/* gds__null_flag */
          short fb_550;	/* gds__null_flag */
          short fb_551;	/* gds__null_flag */
          short fb_552;	/* gds__null_flag */
          short fb_553;	/* RDB$DETERMINISTIC_FLAG */
          FB_BOOLEAN fb_554;	/* RDB$SQL_SECURITY */
          char  fb_555 [253];	/* RDB$ENGINE_NAME */
          char  fb_556 [253];	/* RDB$FUNCTION_NAME */
   } fb_543;
   struct fb_540_struct {
          char  fb_541 [253];	/* RDB$FUNCTION_NAME */
          char  fb_542 [253];	/* RDB$PACKAGE_NAME */
   } fb_540;
/**************************************
 *
 *	s h o w _ f u n c
 *
 **************************************
 *
 * Functional description
 *	Show function.
 *
 **************************************/
	fb_assert(isqlGlob.major_ods >= ODS_VERSION12);

	bool first = true;

	/*FOR FUN IN RDB$FUNCTIONS
		WITH FUN.RDB$FUNCTION_NAME EQ function.c_str()
			AND FUN.RDB$PACKAGE_NAME EQUIV NULLIF(package.c_str(), '')*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_538 && fbTrans && DB)
	      fb_538 = DB->compileRequest(fbStatus, sizeof(fb_539), fb_539);
	   isc_vtov ((const char*) function.c_str(), (char*) fb_540.fb_541, 253);
	   isc_vtov ((const char*) package.c_str(), (char*) fb_540.fb_542, 253);
	   if (fbTrans && fb_538)
	      fb_538->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_540));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_538->release(); fb_538 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_538->receive (fbStatus, 0, 1, 787, CAST_MSG(&fb_543));
	   if (!fb_543.fb_546 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		first = false;

		if (!/*FUN.RDB$DETERMINISTIC_FLAG.NULL*/
		     fb_543.fb_552 && /*FUN.RDB$DETERMINISTIC_FLAG*/
    fb_543.fb_553)
			isqlGlob.printf("Deterministic function%s", NEWLINE);

		if (!/*FUN.RDB$ENTRYPOINT.NULL*/
		     fb_543.fb_551)
		{
			fb_utils::exact_name(/*FUN.RDB$ENTRYPOINT*/
					     fb_543.fb_545);
			isqlGlob.printf("External name: %s%s", /*FUN.RDB$ENTRYPOINT*/
							       fb_543.fb_545, NEWLINE);
		}

		if (!/*FUN.RDB$SQL_SECURITY.NULL*/
		     fb_543.fb_550)
		{
			const char* ss = /*FUN.RDB$SQL_SECURITY*/
					 fb_543.fb_554 ? "DEFINER" : "INVOKER";
			isqlGlob.printf("SQL SECURITY: %s%s", ss, NEWLINE);
		}

		if (!/*FUN.RDB$ENGINE_NAME.NULL*/
		     fb_543.fb_549)
		{
			fb_utils::exact_name(/*FUN.RDB$ENGINE_NAME*/
					     fb_543.fb_555);
			isqlGlob.printf("Engine: %s%s", /*FUN.RDB$ENGINE_NAME*/
							fb_543.fb_555, NEWLINE);
		}

		if (!/*FUN.RDB$FUNCTION_SOURCE.NULL*/
		     fb_543.fb_548)
		{
			isqlGlob.printf("Function text:%s", NEWLINE);
			isqlGlob.printf("=============================================================================%s", NEWLINE);
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FUN.RDB$FUNCTION_SOURCE*/
								     fb_543.fb_544);
			isqlGlob.printf("%s=============================================================================%s", NEWLINE, NEWLINE);
		}

		bool first_param = true;
		const SSHORT default_charset = ISQL_get_default_char_set_id();

		/*FOR ARG IN RDB$FUNCTION_ARGUMENTS CROSS
			FLD IN RDB$FIELDS WITH
			FUN.RDB$FUNCTION_NAME EQ ARG.RDB$FUNCTION_NAME AND
			ARG.RDB$PACKAGE_NAME EQUIV NULLIF(package.c_str(), '') AND
			ARG.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME
			SORTED BY ARG.RDB$ARGUMENT_POSITION*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_505 && fbTrans && DB)
		      fb_505 = DB->compileRequest(fbStatus, sizeof(fb_506), fb_506);
		   isc_vtov ((const char*) fb_543.fb_556, (char*) fb_507.fb_508, 253);
		   isc_vtov ((const char*) package.c_str(), (char*) fb_507.fb_509, 253);
		   if (fbTrans && fb_505)
		      fb_505->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_507));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_505->release(); fb_505 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_505->receive (fbStatus, 0, 1, 1070, CAST_MSG(&fb_510));
		   if (!fb_510.fb_513 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if (first_param)
			{
				isqlGlob.printf("Parameters:%s", NEWLINE);
				first_param = false;
			}
			fb_utils::exact_name(/*ARG.RDB$ARGUMENT_NAME*/
					     fb_510.fb_534);

			isqlGlob.printf("%-33s %s ", /*ARG.RDB$ARGUMENT_NAME*/
						     fb_510.fb_534,
				(/*ARG.RDB$ARGUMENT_POSITION*/
				 fb_510.fb_533 == /*FUN.RDB$RETURN_ARGUMENT*/
    fb_543.fb_547 ? "OUTPUT" : "INPUT"));

			prm_mech_t mechanism = prm_mech_normal;
			bool prm_default_source_null = true;
			ISC_QUAD prm_default_source;

			if (!/*ARG.RDB$ARGUMENT_MECHANISM.NULL*/
			     fb_510.fb_531)
				mechanism = (prm_mech_t) /*ARG.RDB$ARGUMENT_MECHANISM*/
							 fb_510.fb_532;

			if (/*ARG.RDB$ARGUMENT_POSITION*/
			    fb_510.fb_533 != /*FUN.RDB$RETURN_ARGUMENT*/
    fb_543.fb_547)
			{
				prm_default_source_null = /*ARG.RDB$DEFAULT_SOURCE.NULL*/
							  fb_510.fb_530;
				prm_default_source = /*ARG.RDB$DEFAULT_SOURCE*/
						     fb_510.fb_512;
			}

			char relationName[BUFFER_LENGTH256] = "";
			char relationField[BUFFER_LENGTH256] = "";

			if (!/*ARG.RDB$RELATION_NAME.NULL*/
			     fb_510.fb_529)
			{
				strcpy(relationName, /*ARG.RDB$RELATION_NAME*/
						     fb_510.fb_535);
				fb_utils::exact_name(relationName);
			}

			if (!/*ARG.RDB$FIELD_NAME.NULL*/
			     fb_510.fb_528)
			{
				strcpy(relationField, /*ARG.RDB$FIELD_NAME*/
						      fb_510.fb_536);
				fb_utils::exact_name(relationField);
			}

			const bool basedOnColumn = relationName[0] && relationField[0];
			// Decide if this is a user-created domain
			if (!fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
						       fb_510.fb_537) || /*FLD.RDB$SYSTEM_FLAG*/
     fb_510.fb_527 == 1 ||
				basedOnColumn)
			{
				isqlGlob.printf("(%s", (mechanism == prm_mech_type_of ? "TYPE OF " : ""));

				if (basedOnColumn)
					isqlGlob.printf("COLUMN %s.%s) ", relationName, relationField);
				else
				{
					fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
							     fb_510.fb_537);
					isqlGlob.printf("%s) ", /*FLD.RDB$FIELD_NAME*/
								fb_510.fb_537);
				}
			}

			if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
						   fb_510.fb_537, /*FLD.RDB$FIELD_TYPE*/
  fb_510.fb_526, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_510.fb_525,
					/*FLD.RDB$FIELD_PRECISION*/
					fb_510.fb_524, /*FLD.RDB$FIELD_SCALE*/
  fb_510.fb_523))
			{
				return ps_ERR;
			}

			// Use RDB$CHARACTER_LENGTH instead of RDB$FIELD_LENGTH
			//   FSG 19.Nov.2000
			if ((/*FLD.RDB$FIELD_TYPE*/
			     fb_510.fb_526 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_510.fb_526 == blr_varying) &&
				!/*FLD.RDB$CHARACTER_LENGTH.NULL*/
				 fb_510.fb_521)
			{
				isqlGlob.printf("(%d)", /*FLD.RDB$CHARACTER_LENGTH*/
							fb_510.fb_522);
			}

			// Show international character sets and collations

			if (((/*FLD.RDB$FIELD_TYPE*/
			      fb_510.fb_526 == blr_text ||
				/*FLD.RDB$FIELD_TYPE*/
				fb_510.fb_526 == blr_varying) && /*FLD.RDB$FIELD_SUB_TYPE*/
		    fb_510.fb_525 != fb_text_subtype_binary) ||
				/*FLD.RDB$FIELD_TYPE*/
				fb_510.fb_526 == blr_blob)
			{
				SSHORT charset = 0, collation = 0;

				if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
				     fb_510.fb_519)
					charset = /*FLD.RDB$CHARACTER_SET_ID*/
						  fb_510.fb_520;

				bool prm_collation_null = /*ARG.RDB$COLLATION_ID.NULL*/
							  fb_510.fb_517;
				if (!prm_collation_null)
					collation = /*ARG.RDB$COLLATION_ID*/
						    fb_510.fb_518;

				if (prm_collation_null)
				{
					/*FOR RFL IN RDB$RELATION_FIELDS
						WITH RFL.RDB$RELATION_NAME = ARG.RDB$RELATION_NAME AND
							RFL.RDB$FIELD_NAME = ARG.RDB$FIELD_NAME*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_496 && DB)
					      fb_496 = DB->compileRequest(fbStatus, sizeof(fb_497), fb_497);
					   isc_vtov ((const char*) fb_510.fb_535, (char*) fb_498.fb_499, 253);
					   isc_vtov ((const char*) fb_510.fb_536, (char*) fb_498.fb_500, 253);
					   fb_496->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_498));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_496->release(); fb_496 = NULL; }
					   else break;
					   }
					while (1)
					   {
					   fb_496->receive (fbStatus, 0, 1, 6, CAST_MSG(&fb_501));
					   if (!fb_501.fb_502) break;

						prm_collation_null = /*RFL.RDB$COLLATION_ID.NULL*/
								     fb_501.fb_503;
						if (!prm_collation_null)
							collation = /*RFL.RDB$COLLATION_ID*/
								    fb_501.fb_504;
					/*END_FOR*/
					   }
					}
				}

				if (prm_collation_null && !/*FLD.RDB$COLLATION_ID.NULL*/
							   fb_510.fb_515)
					collation = /*FLD.RDB$COLLATION_ID*/
						    fb_510.fb_516;

				show_charsets(charset, collation);
			}

			if (/*ARG.RDB$ARGUMENT_POSITION*/
			    fb_510.fb_533 != /*FUN.RDB$RETURN_ARGUMENT*/
    fb_543.fb_547) // input, try to show default and make Vlad happy.
			{
				if (!prm_default_source_null)
				{
					isqlGlob.printf(" ");
					SHOW_print_metadata_text_blob(isqlGlob.Out, &prm_default_source);
				}
				else if (fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
								   fb_510.fb_537) && !/*FLD.RDB$DEFAULT_SOURCE.NULL*/
      fb_510.fb_514)
				{
					isqlGlob.printf(" ");
					SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
										     fb_510.fb_511);
				}
			}

			isqlGlob.printf(NEWLINE);

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg (fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static processing_state show_func_legacy(const MetaString& function)
{
   struct fb_473_struct {
          short fb_474;	/* fbUtility */
          short fb_475;	/* RDB$BYTES_PER_CHARACTER */
          char  fb_476 [253];	/* RDB$CHARACTER_SET_NAME */
   } fb_473;
   struct fb_470_struct {
          short fb_471;	/* RDB$ARGUMENT_POSITION */
          char  fb_472 [253];	/* RDB$FUNCTION_NAME */
   } fb_470;
   struct fb_481_struct {
          char  fb_482 [256];	/* RDB$ENTRYPOINT */
          char  fb_483 [256];	/* RDB$MODULE_NAME */
          short fb_484;	/* fbUtility */
          short fb_485;	/* RDB$FIELD_LENGTH */
          short fb_486;	/* RDB$FIELD_SCALE */
          short fb_487;	/* RDB$FIELD_SUB_TYPE */
          short fb_488;	/* gds__null_flag */
          short fb_489;	/* RDB$FIELD_PRECISION */
          short fb_490;	/* RDB$FIELD_TYPE */
          short fb_491;	/* RDB$ARGUMENT_POSITION */
          short fb_492;	/* RDB$RETURN_ARGUMENT */
          short fb_493;	/* RDB$MECHANISM */
          char  fb_494 [253];	/* RDB$FUNCTION_NAME */
          char  fb_495 [253];	/* RDB$FUNCTION_NAME */
   } fb_481;
   struct fb_479_struct {
          char  fb_480 [253];	/* RDB$FUNCTION_NAME */
   } fb_479;
/**************************************
 *
 *	s h o w _ f u n c _ l e g a c y
 *
 **************************************
 *
 * Functional description
 *	Show legacy function.
 *
 **************************************/
	bool first = true;

	/*FOR FUN IN RDB$FUNCTIONS CROSS
		FNA IN RDB$FUNCTION_ARGUMENTS WITH
		FUN.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME AND
		FNA.RDB$PACKAGE_NAME MISSING AND
		FUN.RDB$FUNCTION_NAME EQ function.c_str() AND
		FUN.RDB$PACKAGE_NAME MISSING AND
		FUN.RDB$MODULE_NAME NOT MISSING
		SORTED BY FNA.RDB$ARGUMENT_POSITION*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_477 && fbTrans && DB)
	      fb_477 = DB->compileRequest(fbStatus, sizeof(fb_478), fb_478);
	   isc_vtov ((const char*) function.c_str(), (char*) fb_479.fb_480, 253);
	   if (fbTrans && fb_477)
	      fb_477->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_479));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_477->release(); fb_477 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_477->receive (fbStatus, 0, 1, 1038, CAST_MSG(&fb_481));
	   if (!fb_481.fb_484 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*FUN.RDB$FUNCTION_NAME*/
				     fb_481.fb_494);
		fb_utils::exact_name(/*FUN.RDB$MODULE_NAME*/
				     fb_481.fb_483);
		fb_utils::exact_name(/*FUN.RDB$ENTRYPOINT*/
				     fb_481.fb_482);
		if (first)
		{
			isqlGlob.printf("%sExternal function %s:%s", NEWLINE, /*FUN.RDB$FUNCTION_NAME*/
									      fb_481.fb_494, NEWLINE);
			isqlGlob.printf("Function library is %s%s", /*FUN.RDB$MODULE_NAME*/
								    fb_481.fb_483, NEWLINE);
			isqlGlob.printf("Entry point is %s%s", /*FUN.RDB$ENTRYPOINT*/
							       fb_481.fb_482, NEWLINE);
		}

		SSHORT ptype = (SSHORT) abs(/*FNA.RDB$MECHANISM*/
					    fb_481.fb_493);
		if (ptype > MAX_UDFPARAM_TYPES) {
			ptype = MAX_UDFPARAM_TYPES;
		}

		first = false;
		if (/*FUN.RDB$RETURN_ARGUMENT*/
		    fb_481.fb_492 == /*FNA.RDB$ARGUMENT_POSITION*/
    fb_481.fb_491)
		{
			isqlGlob.printf("Returns %s%s", UDF_param_types[ptype],
						(/*FNA.RDB$MECHANISM*/
						 fb_481.fb_493 < 0 ? " FREE_IT " : " "));
		}
		else {
			isqlGlob.printf("Argument %d:%s ", /*FNA.RDB$ARGUMENT_POSITION*/
							   fb_481.fb_491, UDF_param_types[ptype]);
		}

		for (int i = 0; Column_types[i].type; i++)
		{
			if (/*FNA.RDB$FIELD_TYPE*/
			    fb_481.fb_490 == Column_types[i].type)
			{
				bool precision_known = false;

				// Handle Integral subtypes NUMERIC and DECIMAL
				// We are ODS >= 10
				if ( (isqlGlob.major_ods >= ODS_VERSION10) &&
						((/*FNA.RDB$FIELD_TYPE*/
						  fb_481.fb_490 == blr_short) ||
						(/*FNA.RDB$FIELD_TYPE*/
						 fb_481.fb_490 == blr_long) ||
						(/*FNA.RDB$FIELD_TYPE*/
						 fb_481.fb_490 == blr_int64) ||
						(/*FNA.RDB$FIELD_TYPE*/
						 fb_481.fb_490 == blr_int128)) )
				{
					// We are Dialect >=3 since FIELD_PRECISION is non-NULL
					if (!/*FNA.RDB$FIELD_PRECISION.NULL*/
					     fb_481.fb_488 &&
						/*FNA.RDB$FIELD_SUB_TYPE*/
						fb_481.fb_487 > 0 &&
						/*FNA.RDB$FIELD_SUB_TYPE*/
						fb_481.fb_487 <= MAX_INTSUBTYPES)
					{
						isqlGlob.printf("%s(%d, %d)",
									Integral_subtypes[/*FNA.RDB$FIELD_SUB_TYPE*/
											  fb_481.fb_487],
									/*FNA.RDB$FIELD_PRECISION*/
									fb_481.fb_489,
									-/*FNA.RDB$FIELD_SCALE*/
									 fb_481.fb_486);
						precision_known = true;
					}
				}

				if (!precision_known)
				{
					// Take a stab at numerics and decimals
					if ((/*FNA.RDB$FIELD_TYPE*/
					     fb_481.fb_490 == blr_short) && (/*FNA.RDB$FIELD_SCALE*/
		   fb_481.fb_486 < 0))
						isqlGlob.printf("NUMERIC(4, %d)", -/*FNA.RDB$FIELD_SCALE*/
										   fb_481.fb_486);
					else if ((/*FNA.RDB$FIELD_TYPE*/
						  fb_481.fb_490 == blr_long) && (/*FNA.RDB$FIELD_SCALE*/
		  fb_481.fb_486 < 0))
						isqlGlob.printf("NUMERIC(9, %d)", -/*FNA.RDB$FIELD_SCALE*/
										   fb_481.fb_486);
					else if ((/*FNA.RDB$FIELD_TYPE*/
						  fb_481.fb_490 == blr_double) && (/*FNA.RDB$FIELD_SCALE*/
		    fb_481.fb_486 < 0))
						isqlGlob.printf("NUMERIC(15, %d)", -/*FNA.RDB$FIELD_SCALE*/
										    fb_481.fb_486);
					else
						isqlGlob.printf("%s", Column_types[i].type_name);
				}
				break;
			}

		}
		// Print length where appropriate
		if (/*FNA.RDB$FIELD_TYPE*/
		    fb_481.fb_490 == blr_text || /*FNA.RDB$FIELD_TYPE*/
		fb_481.fb_490 == blr_varying ||
			/*FNA.RDB$FIELD_TYPE*/
			fb_481.fb_490 == blr_cstring)
		{
			/*FOR V4FNA IN RDB$FUNCTION_ARGUMENTS CROSS
				CHARSET IN RDB$CHARACTER_SETS OVER RDB$CHARACTER_SET_ID
				WITH V4FNA.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME AND
				V4FNA.RDB$PACKAGE_NAME MISSING AND
				V4FNA.RDB$ARGUMENT_POSITION EQ FNA.RDB$ARGUMENT_POSITION*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_468 && fbTrans && DB)
			      fb_468 = DB->compileRequest(fbStatus, sizeof(fb_469), fb_469);
			   fb_470.fb_471 = fb_481.fb_491;
			   isc_vtov ((const char*) fb_481.fb_495, (char*) fb_470.fb_472, 253);
			   if (fbTrans && fb_468)
			      fb_468->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_470));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_468->release(); fb_468 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_468->receive (fbStatus, 0, 1, 257, CAST_MSG(&fb_473));
			   if (!fb_473.fb_474 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				fb_utils::exact_name(/*CHARSET.RDB$CHARACTER_SET_NAME*/
						     fb_473.fb_476);
				isqlGlob.printf("(%d) CHARACTER SET %s",
							(/*FNA.RDB$FIELD_LENGTH*/
							 fb_481.fb_485 / MAX (1, /*CHARSET.RDB$BYTES_PER_CHARACTER*/
	   fb_473.fb_475)),
							/*CHARSET.RDB$CHARACTER_SET_NAME*/
							fb_473.fb_476);

			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg (fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}
		}

		isqlGlob.printf(NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static processing_state show_generators(const SCHAR* object)
{
   struct fb_456_struct {
          ISC_INT64 fb_457;	/* RDB$INITIAL_VALUE */
          ISC_LONG fb_458;	/* RDB$GENERATOR_INCREMENT */
          short fb_459;	/* fbUtility */
          short fb_460;	/* gds__null_flag */
   } fb_456;
   struct fb_454_struct {
          char  fb_455 [253];	/* RDB$GENERATOR_NAME */
   } fb_454;
   struct fb_463_struct {
          short fb_464;	/* fbUtility */
          short fb_465;	/* gds__null_flag */
          short fb_466;	/* RDB$SYSTEM_FLAG */
          char  fb_467 [253];	/* RDB$GENERATOR_NAME */
   } fb_463;
/**************************************
 *
 *	s h o w _ g e n e r a t o r s
 *
 **************************************
 *
 * Functional description
 *	Show generators including the current number they return
 *      We do this by selecting the GEN_ID of each one,
 *         incrementing by 0 to not change the current value.
 *
 **************************************/
	bool found = false;
	const char genIdStr[] = "SELECT GEN_ID(%s, 0) FROM RDB$DATABASE";
	TEXT query[sizeof(genIdStr) + QUOTED_NAME_SIZE], gen_name[QUOTED_NAME_SIZE];

	Firebird::RefPtr<Firebird::IMetadataBuilder>
		mb(Firebird::REF_NO_INCR, fbMaster->getMetadataBuilder(fbStatus, 1));
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	// If the user has set his client dialect to 1, we take that to
	// mean that he wants to see just the lower 32 bits of the
	// generator, as in V5.  Otherwise, we show him the whole 64-bit value.
	const bool use64 = isqlGlob.SQL_dialect >= SQL_DIALECT_V6_TRANSITION;
	mb->setType(fbStatus, 0, use64 ? SQL_INT64 : SQL_LONG);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::RefPtr<Firebird::IMessageMetadata>
		outMetadata(Firebird::REF_NO_INCR, mb->getMetadata(fbStatus));
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::AlignedBuffer<20> outBuffer;
	fb_assert(outMetadata->getMessageLength(fbStatus) <= outBuffer.size());
	unsigned off = outMetadata->getOffset(fbStatus, 0);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	// Show all generators or named generator
	/*FOR GEN IN RDB$GENERATORS
		SORTED BY GEN.RDB$GENERATOR_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_461 && fbTrans && DB)
	      fb_461 = DB->compileRequest(fbStatus, sizeof(fb_462), fb_462);
	   if (fbTrans && fb_461)
	      fb_461->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_461->release(); fb_461 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_461->receive (fbStatus, 0, 0, 259, CAST_MSG(&fb_463));
	   if (!fb_463.fb_464 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*GEN.RDB$GENERATOR_NAME*/
				     fb_463.fb_467);

		if ((!*object && (/*GEN.RDB$SYSTEM_FLAG.NULL*/
				  fb_463.fb_465 || /*GEN.RDB$SYSTEM_FLAG*/
    fb_463.fb_466 == 0)) ||
			!strcmp(/*GEN.RDB$GENERATOR_NAME*/
				fb_463.fb_467, object))
		{
			// Get the current id for each generator

			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION) {
				IUTILS_copy_SQL_id (/*GEN.RDB$GENERATOR_NAME*/
						    fb_463.fb_467, gen_name, DBL_QUOTE);
			}
			else {
				// If we are extracting in dialect 1, identifiers may cause failures.
				strcpy(gen_name, /*GEN.RDB$GENERATOR_NAME*/
						 fb_463.fb_467);
			}

			fb_utils::snprintf(query, sizeof(query), genIdStr, gen_name);

			DB->execute(fbStatus, fbTrans, 0, query, isqlGlob.SQL_dialect,
				NULL, NULL, outMetadata, outBuffer);
			if (ISQL_errmsg (fbStatus))
				continue;

			found = true;
			ISC_INT64 val = use64 ? *((ISC_INT64*) &outBuffer[off]) : *((SLONG*) &outBuffer[off]);

			isqlGlob.printf("Generator %s, current value: %" SQUADFORMAT,
					 /*GEN.RDB$GENERATOR_NAME*/
					 fb_463.fb_467, val);
			if (isqlGlob.major_ods >= ODS_VERSION12)
			{
				/*FOR G2 IN RDB$GENERATORS
					WITH G2.RDB$GENERATOR_NAME = GEN.RDB$GENERATOR_NAME*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_452 && fbTrans && DB)
				      fb_452 = DB->compileRequest(fbStatus, sizeof(fb_453), fb_453);
				   isc_vtov ((const char*) fb_463.fb_467, (char*) fb_454.fb_455, 253);
				   if (fbTrans && fb_452)
				      fb_452->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_454));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_452->release(); fb_452 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_452->receive (fbStatus, 0, 1, 16, CAST_MSG(&fb_456));
				   if (!fb_456.fb_459 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					ISC_INT64 initval = !/*G2.RDB$INITIAL_VALUE.NULL*/
							     fb_456.fb_460 ? /*G2.RDB$INITIAL_VALUE*/
   fb_456.fb_457 : 0;
					isqlGlob.printf(", initial value: %" SQUADFORMAT ", increment: %" SLONGFORMAT,
						initval, /*G2.RDB$GENERATOR_INCREMENT*/
							 fb_456.fb_458);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg(fbStatus);
					return ps_ERR;
				/*END_ERROR;*/
				   }
				}
			}

			isqlGlob.prints(NEWLINE);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (!found)
		return (OBJECT_NOT_FOUND);
	return SKIP;
}


static void show_index(SCHAR* relation_name,
					   SCHAR* index_name,
					   const SSHORT unique_flag,
					   const SSHORT index_type,
					   const SSHORT inactive)
{
/**************************************
 *
 *	s h o w _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Show an index.
 *
 *	relation_name -- Name of table to investigate
 *
 **************************************/

	// Strip trailing blanks

	fb_utils::exact_name(relation_name);
	fb_utils::exact_name(index_name);

	isqlGlob.printf("%s%s%s INDEX ON %s", index_name,
			(unique_flag ? " UNIQUE" : ""),
			(index_type == 1 ? " DESCENDING" : ""), relation_name);

	// Get column names

	SCHAR collist[BUFFER_LENGTH512];

	if (ISQL_get_index_segments(collist, sizeof(collist), index_name, false))
	{
		isqlGlob.printf("(%s) %s%s", collist, (inactive ? "(inactive)" : ""), NEWLINE);
	}
}


static processing_state show_indices(const SCHAR* const* cmd)
{
   struct fb_412_struct {
          ISC_QUAD fb_413;	/* RDB$CONDITION_SOURCE */
          short fb_414;	/* fbUtility */
   } fb_412;
   struct fb_410_struct {
          char  fb_411 [253];	/* RDB$INDEX_NAME */
   } fb_410;
   struct fb_417_struct {
          ISC_QUAD fb_418;	/* RDB$EXPRESSION_SOURCE */
          ISC_QUAD fb_419;	/* RDB$EXPRESSION_BLR */
          short fb_420;	/* fbUtility */
          short fb_421;	/* gds__null_flag */
          short fb_422;	/* gds__null_flag */
          short fb_423;	/* RDB$INDEX_INACTIVE */
          short fb_424;	/* RDB$INDEX_TYPE */
          short fb_425;	/* RDB$UNIQUE_FLAG */
          char  fb_426 [253];	/* RDB$RELATION_NAME */
          char  fb_427 [253];	/* RDB$INDEX_NAME */
   } fb_417;
   struct fb_432_struct {
          ISC_QUAD fb_433;	/* RDB$CONDITION_SOURCE */
          short fb_434;	/* fbUtility */
   } fb_432;
   struct fb_430_struct {
          char  fb_431 [253];	/* RDB$INDEX_NAME */
   } fb_430;
   struct fb_440_struct {
          ISC_QUAD fb_441;	/* RDB$EXPRESSION_SOURCE */
          ISC_QUAD fb_442;	/* RDB$EXPRESSION_BLR */
          short fb_443;	/* fbUtility */
          short fb_444;	/* gds__null_flag */
          short fb_445;	/* gds__null_flag */
          short fb_446;	/* RDB$INDEX_TYPE */
          short fb_447;	/* RDB$UNIQUE_FLAG */
          short fb_448;	/* gds__null_flag */
          short fb_449;	/* RDB$INDEX_INACTIVE */
          char  fb_450 [253];	/* RDB$RELATION_NAME */
          char  fb_451 [253];	/* RDB$INDEX_NAME */
   } fb_440;
   struct fb_437_struct {
          char  fb_438 [253];	/* RDB$RELATION_NAME */
          char  fb_439 [253];	/* RDB$INDEX_NAME */
   } fb_437;
/**************************************
 *
 *	s h o w _ i n d i c e s
 *
 **************************************
 *
 * Functional description
 *	shows indices for a given table name or index name or all tables
 *
 *	Use a static SQL query to get the info and print it.
 *
 *	relation_name -- Name of table to investigate
 *
 **************************************/
	bool first = true;

	// The names stored in the database are all upper case

	const SCHAR* name = cmd[2];

	if (*name)
	{
		/*FOR IDX IN RDB$INDICES WITH
			IDX.RDB$RELATION_NAME EQ name OR
			IDX.RDB$INDEX_NAME EQ name
			SORTED BY IDX.RDB$INDEX_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_435 && fbTrans && DB)
		      fb_435 = DB->compileRequest(fbStatus, sizeof(fb_436), fb_436);
		   isc_vtov ((const char*) name, (char*) fb_437.fb_438, 253);
		   isc_vtov ((const char*) name, (char*) fb_437.fb_439, 253);
		   if (fbTrans && fb_435)
		      fb_435->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_437));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_435->release(); fb_435 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_435->receive (fbStatus, 0, 1, 536, CAST_MSG(&fb_440));
		   if (!fb_440.fb_443 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if (/*IDX.RDB$INDEX_INACTIVE.NULL*/
			    fb_440.fb_448)
				/*IDX.RDB$INDEX_INACTIVE*/
				fb_440.fb_449 = 0;

			show_index(/*IDX.RDB$RELATION_NAME*/
				   fb_440.fb_450, /*IDX.RDB$INDEX_NAME*/
  fb_440.fb_451,
					   /*IDX.RDB$UNIQUE_FLAG*/
					   fb_440.fb_447, /*IDX.RDB$INDEX_TYPE*/
  fb_440.fb_446, /*IDX.RDB$INDEX_INACTIVE*/
  fb_440.fb_449);

			if (!/*IDX.RDB$EXPRESSION_BLR.NULL*/
			     fb_440.fb_445)
			{
				isqlGlob.printf(" COMPUTED BY ");
				if (!/*IDX.RDB$EXPRESSION_SOURCE.NULL*/
				     fb_440.fb_444)
					SHOW_print_metadata_text_blob (isqlGlob.Out, &/*IDX.RDB$EXPRESSION_SOURCE*/
										      fb_440.fb_441);
				isqlGlob.printf(NEWLINE);
			}

			if (ENCODE_ODS(isqlGlob.major_ods, isqlGlob.minor_ods) >= ODS_13_1)
			{
				/*FOR IDX2 IN RDB$INDICES WITH
					IDX2.RDB$INDEX_NAME = IDX.RDB$INDEX_NAME
					AND IDX2.RDB$CONDITION_SOURCE NOT MISSING*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_428 && DB)
				      fb_428 = DB->compileRequest(fbStatus, sizeof(fb_429), fb_429);
				   isc_vtov ((const char*) fb_440.fb_451, (char*) fb_430.fb_431, 253);
				   fb_428->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_430));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_428->release(); fb_428 = NULL; }
				   else break;
				   }
				while (1)
				   {
				   fb_428->receive (fbStatus, 0, 1, 10, CAST_MSG(&fb_432));
				   if (!fb_432.fb_434) break;
				{
					isqlGlob.printf(" ");
					SHOW_print_metadata_text_blob(isqlGlob.Out, &/*IDX2.RDB$CONDITION_SOURCE*/
										     fb_432.fb_433);
					isqlGlob.printf(NEWLINE);
				}
				/*END_FOR*/
				   }
				}
			}

			first = false;
		/*END_FOR*/
		   }
		   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   { ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
		if (first)
			return (OBJECT_NOT_FOUND);
		return (SKIP);

	}
	else
	{
		/*FOR IDX IN RDB$INDICES CROSS
			REL IN RDB$RELATIONS OVER RDB$RELATION_NAME WITH
			REL.RDB$SYSTEM_FLAG NE 1 OR
			REL.RDB$SYSTEM_FLAG MISSING
			SORTED BY IDX.RDB$RELATION_NAME, IDX.RDB$INDEX_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_415 && fbTrans && DB)
		      fb_415 = DB->compileRequest(fbStatus, sizeof(fb_416), fb_416);
		   if (fbTrans && fb_415)
		      fb_415->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_415->release(); fb_415 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_415->receive (fbStatus, 0, 0, 534, CAST_MSG(&fb_417));
		   if (!fb_417.fb_420 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			first = false;

			show_index(/*IDX.RDB$RELATION_NAME*/
				   fb_417.fb_426, /*IDX.RDB$INDEX_NAME*/
  fb_417.fb_427,
					   /*IDX.RDB$UNIQUE_FLAG*/
					   fb_417.fb_425, /*IDX.RDB$INDEX_TYPE*/
  fb_417.fb_424, /*IDX.RDB$INDEX_INACTIVE*/
  fb_417.fb_423);

			if (!/*IDX.RDB$EXPRESSION_BLR.NULL*/
			     fb_417.fb_422)
			{
				isqlGlob.printf(" COMPUTED BY ");
				if (!/*IDX.RDB$EXPRESSION_SOURCE.NULL*/
				     fb_417.fb_421)
					SHOW_print_metadata_text_blob (isqlGlob.Out, &/*IDX.RDB$EXPRESSION_SOURCE*/
										      fb_417.fb_418);
				isqlGlob.printf(NEWLINE);
			}

			if (ENCODE_ODS(isqlGlob.major_ods, isqlGlob.minor_ods) >= ODS_13_1)
			{
				/*FOR IDX2 IN RDB$INDICES WITH
					IDX2.RDB$INDEX_NAME = IDX.RDB$INDEX_NAME
					AND IDX2.RDB$CONDITION_SOURCE NOT MISSING*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_408 && DB)
				      fb_408 = DB->compileRequest(fbStatus, sizeof(fb_409), fb_409);
				   isc_vtov ((const char*) fb_417.fb_427, (char*) fb_410.fb_411, 253);
				   fb_408->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_410));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_408->release(); fb_408 = NULL; }
				   else break;
				   }
				while (1)
				   {
				   fb_408->receive (fbStatus, 0, 1, 10, CAST_MSG(&fb_412));
				   if (!fb_412.fb_414) break;
				{
					isqlGlob.printf(" ");
					SHOW_print_metadata_text_blob(isqlGlob.Out, &/*IDX2.RDB$CONDITION_SOURCE*/
										     fb_412.fb_413);
					isqlGlob.printf(NEWLINE);
				}
				/*END_FOR*/
				   }
				}
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
		if (first)
			return (OBJECT_NOT_FOUND);
		return (SKIP);
	}
}


static processing_state show_packages(const SCHAR* package_name, bool sys, const SCHAR* msg)
{
   struct fb_390_struct {
          ISC_QUAD fb_391;	/* RDB$PACKAGE_BODY_SOURCE */
          ISC_QUAD fb_392;	/* RDB$PACKAGE_HEADER_SOURCE */
          short fb_393;	/* fbUtility */
          short fb_394;	/* gds__null_flag */
          short fb_395;	/* RDB$VALID_BODY_FLAG */
          short fb_396;	/* gds__null_flag */
          short fb_397;	/* gds__null_flag */
          short fb_398;	/* gds__null_flag */
          char  fb_399 [253];	/* RDB$PACKAGE_NAME */
          FB_BOOLEAN fb_400;	/* RDB$SQL_SECURITY */
   } fb_390;
   struct fb_388_struct {
          char  fb_389 [253];	/* RDB$PACKAGE_NAME */
   } fb_388;
   struct fb_403_struct {
          short fb_404;	/* fbUtility */
          short fb_405;	/* gds__null_flag */
          short fb_406;	/* RDB$SYSTEM_FLAG */
          char  fb_407 [253];	/* RDB$PACKAGE_NAME */
   } fb_403;
/*************************************
*
*	s h o w _ p a c k a g e s
*
**************************************
*
* Functional description
*	Show all packages or the named package
************************************/
	if (isqlGlob.major_ods < ODS_VERSION12)
		return OBJECT_NOT_FOUND;

	bool first = true;

	if (!(package_name && *package_name))
	{
		//  List all package names in columns
		/*FOR PACK IN RDB$PACKAGES
			SORTED BY PACK.RDB$PACKAGE_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_401 && fbTrans && DB)
		      fb_401 = DB->compileRequest(fbStatus, sizeof(fb_402), fb_402);
		   if (fbTrans && fb_401)
		      fb_401->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_401->release(); fb_401 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_401->receive (fbStatus, 0, 0, 259, CAST_MSG(&fb_403));
		   if (!fb_403.fb_404 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			bool system_flag = !/*PACK.RDB$SYSTEM_FLAG.NULL*/
					    fb_403.fb_405 && /*PACK.RDB$SYSTEM_FLAG*/
    fb_403.fb_406 > 0;

			if (system_flag == sys)
			{
				if (first && msg)
					isqlGlob.printf("%s%s", msg, NEWLINE);
				first = false;

				isqlGlob.printf("%s%s", fb_utils::exact_name(/*PACK.RDB$PACKAGE_NAME*/
									     fb_403.fb_407), NEWLINE);
			}
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
		if (!first)
			isqlGlob.printf(NEWLINE);
	}
	else
	{
		// List named package

		/*FOR PACK IN RDB$PACKAGES WITH
			PACK.RDB$PACKAGE_NAME EQ package_name*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_386 && fbTrans && DB)
		      fb_386 = DB->compileRequest(fbStatus, sizeof(fb_387), fb_387);
		   isc_vtov ((const char*) package_name, (char*) fb_388.fb_389, 253);
		   if (fbTrans && fb_386)
		      fb_386->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_388));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_386->release(); fb_386 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_386->receive (fbStatus, 0, 1, 282, CAST_MSG(&fb_390));
		   if (!fb_390.fb_393 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			first = false;
			// Print the name of the package
			fb_utils::exact_name(/*PACK.RDB$PACKAGE_NAME*/
					     fb_390.fb_399);
			isqlGlob.printf("%-31s ", /*PACK.RDB$PACKAGE_NAME*/
						  fb_390.fb_399);

			isqlGlob.printf(NEWLINE);

			if (!/*PACK.RDB$SQL_SECURITY.NULL*/
			     fb_390.fb_398)
			{
				const char* ss = /*PACK.RDB$SQL_SECURITY*/
						 fb_390.fb_400 ? "DEFINER" : "INVOKER";
				isqlGlob.printf("SQL SECURITY: %s%s", ss, NEWLINE);
			}

			if (!/*PACK.RDB$PACKAGE_HEADER_SOURCE.NULL*/
			     fb_390.fb_397)
			{
				isqlGlob.printf("%s%s", "Header source:", NEWLINE);
				SHOW_print_metadata_text_blob (isqlGlob.Out, &/*PACK.RDB$PACKAGE_HEADER_SOURCE*/
									      fb_390.fb_392);
				isqlGlob.printf(NEWLINE);
			}
			else
				isqlGlob.printf("Missing package header source.%s", NEWLINE);

			if (!/*PACK.RDB$PACKAGE_BODY_SOURCE.NULL*/
			     fb_390.fb_396)
			{
				isqlGlob.printf("%s%s%s:%s", NEWLINE, "Body source",
					(!/*PACK.RDB$VALID_BODY_FLAG.NULL*/
					  fb_390.fb_394 && /*PACK.RDB$VALID_BODY_FLAG*/
    fb_390.fb_395 != 0 ?
						"" : " (invalid)"),
					NEWLINE);
				SHOW_print_metadata_text_blob (isqlGlob.Out, &/*PACK.RDB$PACKAGE_BODY_SOURCE*/
									      fb_390.fb_391);
				isqlGlob.printf(NEWLINE);
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}

	if (first)
		return (OBJECT_NOT_FOUND);

	return (SKIP);
}


static void printIdent(bool quote, char* ident, const char* format = NULL)
{
	fb_utils::exact_name(ident);
	char quotedIdent[BUFFER_LENGTH256];
	if (quote && isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
	{
		IUTILS_copy_SQL_id(ident, quotedIdent, DBL_QUOTE);
		ident = quotedIdent;
	}

	isqlGlob.printf(format ? format : "%s", ident);
}


static void printMap(bool extract, bool global, char* name, char* usng, char* plugin, char* db,
	char* fromType, char* from, short toType, char* to)
{
	if (extract)
	{
		isqlGlob.printf("CREATE ");
		if (global)
			isqlGlob.printf("OR ALTER GLOBAL ");
	}
	printIdent(extract, name, extract ? "MAPPING %s " : "%s ");

	isqlGlob.printf("USING ");
	switch (usng[0])
	{
	case 'P':
		if (!plugin)
			isqlGlob.printf("ANY PLUGIN ");
		else
			printIdent(extract, plugin, "PLUGIN %s ");
		break;
	case 'S':
		isqlGlob.printf("ANY PLUGIN SERVERWIDE ");
		break;
	case '*':
		isqlGlob.printf("* ");
		break;
	case 'M':
		isqlGlob.printf("MAPPING ");
		break;
	default:
		isqlGlob.printf("/*unknown = %c*/ ", usng[0]);
	}

	if (db)
		printIdent(extract, db, "IN %s ");

	fb_utils::exact_name(from);
	bool anyObj = strcmp(from, "*") == 0;
	isqlGlob.printf("FROM %s", anyObj ? "ANY " : "");
	printIdent(extract, fromType, "%s ");
	if (!anyObj)
		printIdent(extract, from, "%s ");

	isqlGlob.printf("TO %s ", toType ? "ROLE" : "USER");
	if (to)
		printIdent(extract, to);
	isqlGlob.printf("%s%s", extract ? ";" : "", NEWLINE);
}


processing_state SHOW_maps(bool extract, const SCHAR* map_name)
{
   struct fb_358_struct {
          short fb_359;	/* fbUtility */
          short fb_360;	/* gds__null_flag */
          short fb_361;	/* SEC$MAP_TO_TYPE */
          short fb_362;	/* gds__null_flag */
          short fb_363;	/* gds__null_flag */
          char  fb_364 [253];	/* SEC$MAP_NAME */
          char  fb_365 [5];	/* SEC$MAP_USING */
          char  fb_366 [253];	/* SEC$MAP_PLUGIN */
          char  fb_367 [253];	/* SEC$MAP_DB */
          char  fb_368 [253];	/* SEC$MAP_FROM_TYPE */
          char  fb_369 [1021];	/* SEC$MAP_FROM */
          char  fb_370 [253];	/* SEC$MAP_TO */
   } fb_358;
   struct fb_373_struct {
          short fb_374;	/* fbUtility */
          short fb_375;	/* gds__null_flag */
          short fb_376;	/* RDB$MAP_TO_TYPE */
          short fb_377;	/* gds__null_flag */
          short fb_378;	/* gds__null_flag */
          char  fb_379 [253];	/* RDB$MAP_NAME */
          char  fb_380 [5];	/* RDB$MAP_USING */
          char  fb_381 [253];	/* RDB$MAP_PLUGIN */
          char  fb_382 [253];	/* RDB$MAP_DB */
          char  fb_383 [253];	/* RDB$MAP_FROM_TYPE */
          char  fb_384 [1021];	/* RDB$MAP_FROM */
          char  fb_385 [253];	/* RDB$MAP_TO */
   } fb_373;
/*************************************
*
*	s h o w _ m a p s
*
**************************************
*
* Functional description
*	Show all maps or maps to map_name
************************************/
	if (isqlGlob.major_ods < ODS_VERSION12)
		return OBJECT_NOT_FOUND;

	bool first = true;

	//  List all mappings
	/*FOR M IN RDB$AUTH_MAPPING WITH
		(M.RDB$SYSTEM_FLAG NE 1 OR M.RDB$SYSTEM_FLAG MISSING)
		SORTED BY M.RDB$MAP_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_371 && fbTrans && DB)
	      fb_371 = DB->compileRequest(fbStatus, sizeof(fb_372), fb_372);
	   if (fbTrans && fb_371)
	      fb_371->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_371->release(); fb_371 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_371->receive (fbStatus, 0, 0, 2301, CAST_MSG(&fb_373));
	   if (!fb_373.fb_374 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		Firebird::NoCaseString nm = /*M.RDB$MAP_NAME*/
					    fb_373.fb_379;
		nm.trim();

		if ((!*map_name) || (nm == map_name))
		{
			if (first && extract)
				isqlGlob.printf("%s/* Mapping security objects for this database */%s", NEWLINE, NEWLINE);
			first = false;

			printMap(extract, false, /*M.RDB$MAP_NAME*/
						 fb_373.fb_379, /*M.RDB$MAP_USING*/
  fb_373.fb_380,
				(/*M.RDB$MAP_PLUGIN.NULL*/
				 fb_373.fb_378 ? NULL : /*M.RDB$MAP_PLUGIN*/
	  fb_373.fb_381),
				(/*M.RDB$MAP_DB.NULL*/
				 fb_373.fb_377 ? NULL : /*M.RDB$MAP_DB*/
	  fb_373.fb_382),
				/*M.RDB$MAP_FROM_TYPE*/
				fb_373.fb_383, /*M.RDB$MAP_FROM*/
  fb_373.fb_384,
				/*M.RDB$MAP_TO_TYPE*/
				fb_373.fb_376, (/*M.RDB$MAP_TO.NULL*/
   fb_373.fb_375 ? NULL : /*M.RDB$MAP_TO*/
	  fb_373.fb_385));
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	bool firstGlobal = true;

	//  List global mappings
	/*FOR M IN SEC$GLOBAL_AUTH_MAPPING
		SORTED BY M.SEC$MAP_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_356 && fbTrans && DB)
	      fb_356 = DB->compileRequest(fbStatus, sizeof(fb_357), fb_357);
	   if (fbTrans && fb_356)
	      fb_356->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_356->release(); fb_356 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_356->receive (fbStatus, 0, 0, 2301, CAST_MSG(&fb_358));
	   if (!fb_358.fb_359 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		Firebird::NoCaseString nm = /*M.SEC$MAP_NAME*/
					    fb_358.fb_364;
		nm.trim();

		if ((!*map_name) || (nm == map_name))
		{
			if (firstGlobal)
			{
				const char* sep = extract ? "/" : "**";
				isqlGlob.printf("%s%s* Global mapping *%s%s",
					(first && (!extract)) ? "" : NEWLINE, sep, sep, NEWLINE);
				firstGlobal = false;
			}

			first = false;

			printMap(extract, true, /*M.SEC$MAP_NAME*/
						fb_358.fb_364, /*M.SEC$MAP_USING*/
  fb_358.fb_365,
				(/*M.SEC$MAP_PLUGIN.NULL*/
				 fb_358.fb_363 ? NULL : /*M.SEC$MAP_PLUGIN*/
	  fb_358.fb_366),
				(/*M.SEC$MAP_DB.NULL*/
				 fb_358.fb_362 ? NULL : /*M.SEC$MAP_DB*/
	  fb_358.fb_367),
				/*M.SEC$MAP_FROM_TYPE*/
				fb_358.fb_368, /*M.SEC$MAP_FROM*/
  fb_358.fb_369,
				/*M.SEC$MAP_TO_TYPE*/
				fb_358.fb_361, (/*M.SEC$MAP_TO.NULL*/
   fb_358.fb_360 ? NULL : /*M.SEC$MAP_TO*/
	  fb_358.fb_370));
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		if (!extract)
			ISQL_errmsg(fbStatus);		// report error but not return error on it
	/*END_ERROR;*/
	   }
	}

	return first ? OBJECT_NOT_FOUND : SKIP;
}


static processing_state show_proc(const SCHAR* procname, bool quoted, bool sys, const char* msg)
{
   struct fb_284_struct {
          short fb_285;	/* fbUtility */
          short fb_286;	/* gds__null_flag */
          short fb_287;	/* RDB$COLLATION_ID */
   } fb_284;
   struct fb_281_struct {
          char  fb_282 [253];	/* RDB$RELATION_NAME */
          char  fb_283 [253];	/* RDB$FIELD_NAME */
   } fb_281;
   struct fb_293_struct {
          ISC_QUAD fb_294;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_295;	/* RDB$DEFAULT_SOURCE */
          short fb_296;	/* fbUtility */
          short fb_297;	/* gds__null_flag */
          short fb_298;	/* gds__null_flag */
          short fb_299;	/* RDB$CHARACTER_SET_ID */
          short fb_300;	/* gds__null_flag */
          short fb_301;	/* RDB$CHARACTER_LENGTH */
          short fb_302;	/* RDB$FIELD_SCALE */
          short fb_303;	/* RDB$FIELD_PRECISION */
          short fb_304;	/* RDB$FIELD_SUB_TYPE */
          short fb_305;	/* RDB$FIELD_TYPE */
          short fb_306;	/* RDB$SYSTEM_FLAG */
          short fb_307;	/* gds__null_flag */
          short fb_308;	/* RDB$COLLATION_ID */
          short fb_309;	/* gds__null_flag */
          short fb_310;	/* gds__null_flag */
          short fb_311;	/* gds__null_flag */
          short fb_312;	/* RDB$COLLATION_ID */
          short fb_313;	/* gds__null_flag */
          short fb_314;	/* gds__null_flag */
          short fb_315;	/* RDB$PARAMETER_MECHANISM */
          short fb_316;	/* RDB$PARAMETER_TYPE */
          char  fb_317 [253];	/* RDB$PARAMETER_NAME */
          char  fb_318 [253];	/* RDB$RELATION_NAME */
          char  fb_319 [253];	/* RDB$FIELD_NAME */
          char  fb_320 [253];	/* RDB$FIELD_NAME */
   } fb_293;
   struct fb_290_struct {
          char  fb_291 [253];	/* RDB$PROCEDURE_NAME */
          char  fb_292 [253];	/* RDB$PACKAGE_NAME */
   } fb_290;
   struct fb_326_struct {
          ISC_QUAD fb_327;	/* RDB$PROCEDURE_SOURCE */
          char  fb_328 [256];	/* RDB$ENTRYPOINT */
          short fb_329;	/* fbUtility */
          short fb_330;	/* gds__null_flag */
          short fb_331;	/* gds__null_flag */
          short fb_332;	/* gds__null_flag */
          short fb_333;	/* gds__null_flag */
          FB_BOOLEAN fb_334;	/* RDB$SQL_SECURITY */
          char  fb_335 [253];	/* RDB$ENGINE_NAME */
          char  fb_336 [253];	/* RDB$PROCEDURE_NAME */
   } fb_326;
   struct fb_323_struct {
          char  fb_324 [253];	/* RDB$PROCEDURE_NAME */
          char  fb_325 [253];	/* RDB$PACKAGE_NAME */
   } fb_323;
   struct fb_341_struct {
          short fb_342;	/* fbUtility */
          short fb_343;	/* RDB$DEPENDED_ON_TYPE */
          char  fb_344 [253];	/* RDB$DEPENDED_ON_NAME */
   } fb_341;
   struct fb_339_struct {
          char  fb_340 [253];	/* RDB$PROCEDURE_NAME */
   } fb_339;
   struct fb_347_struct {
          short fb_348;	/* fbUtility */
          short fb_349;	/* gds__null_flag */
          short fb_350;	/* gds__null_flag */
          short fb_351;	/* RDB$VALID_BLR */
          short fb_352;	/* gds__null_flag */
          short fb_353;	/* RDB$SYSTEM_FLAG */
          char  fb_354 [253];	/* RDB$PACKAGE_NAME */
          char  fb_355 [253];	/* RDB$PROCEDURE_NAME */
   } fb_347;
/**************************************
 *
 *	s h o w _ p r o c
 *
 **************************************
 *
 * Functional description
 *	shows text of a stored procedure given a name.
 *	or lists procedures if no argument.
 *
 *	procname -- Name of procedure to investigate
 *
 **************************************/

	// If no procedure name was given, just list the procedures

	if (!procname || !strlen(procname))
	{
		// This query gets the procedure name; the next query
		// gets all the dependencies if any

		bool first_proc = true;

		/*FOR PRC IN RDB$PROCEDURES
			SORTED BY PRC.RDB$PACKAGE_NAME, PRC.RDB$PROCEDURE_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_345 && fbTrans && DB)
		      fb_345 = DB->compileRequest(fbStatus, sizeof(fb_346), fb_346);
		   if (fbTrans && fb_345)
		      fb_345->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_345->release(); fb_345 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_345->receive (fbStatus, 0, 0, 518, CAST_MSG(&fb_347));
		   if (!fb_347.fb_348 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			bool system_flag = !/*PRC.RDB$SYSTEM_FLAG.NULL*/
					    fb_347.fb_352 && /*PRC.RDB$SYSTEM_FLAG*/
    fb_347.fb_353 > 0;

			if (system_flag == sys)
			{
				if (first_proc)
				{
					if (msg)
						isqlGlob.printf("%s%s%s", NEWLINE, msg, NEWLINE);
					first_proc = false;
				}

				// Strip trailing blanks
				MetaString package(/*PRC.RDB$PACKAGE_NAME*/
						   fb_347.fb_354);
				MetaString procedure(/*PRC.RDB$PROCEDURE_NAME*/
						     fb_347.fb_355);

				isqlGlob.printf("%s%s%s", package.c_str(), package.hasData() ? "." : "", procedure.c_str());

				if (!(/*PRC.RDB$VALID_BLR.NULL*/
				      fb_347.fb_350 || /*PRC.RDB$VALID_BLR*/
    fb_347.fb_351))
					isqlGlob.printf("; Invalid");

				if (/*PRC.RDB$PACKAGE_NAME.NULL*/
				    fb_347.fb_349)
				{
					bool first_dep = true;
					/*FOR DEP IN RDB$DEPENDENCIES WITH
						PRC.RDB$PROCEDURE_NAME EQ DEP.RDB$DEPENDENT_NAME
						REDUCED TO DEP.RDB$DEPENDED_ON_TYPE, DEP.RDB$DEPENDED_ON_NAME
						SORTED BY DEP.RDB$DEPENDED_ON_TYPE, DEP.RDB$DEPENDED_ON_NAME*/
					{
					for (int retries = 0; retries < 2; ++retries)
					   {
					   {
					   if (!DB)
					      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
					   if (DB && !fbTrans)
					      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
					   }
					   if (!fb_337 && fbTrans && DB)
					      fb_337 = DB->compileRequest(fbStatus, sizeof(fb_338), fb_338);
					   isc_vtov ((const char*) fb_347.fb_355, (char*) fb_339.fb_340, 253);
					   if (fbTrans && fb_337)
					      fb_337->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_339));
					   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_337->release(); fb_337 = NULL; }
					   else break;
					   }
					if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
					while (1)
					   {
					   fb_337->receive (fbStatus, 0, 1, 257, CAST_MSG(&fb_341));
					   if (!fb_341.fb_342 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
					{
						fb_utils::exact_name(/*DEP.RDB$DEPENDED_ON_NAME*/
								     fb_341.fb_344);

						if (first_dep)
						{
							isqlGlob.printf("; Dependencies: ");
							first_dep = false;
						}
						else
							isqlGlob.printf(", ");

						isqlGlob.printf("%s (%s)", fb_utils::exact_name(/*DEP.RDB$DEPENDED_ON_NAME*/
												fb_341.fb_344),
							 Object_types[/*DEP.RDB$DEPENDED_ON_TYPE*/
								      fb_341.fb_343]);
					}
					/*END_FOR*/
					   }
					   };
					/*ON_ERROR*/
					if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
					   {
						ISQL_errmsg (fbStatus);
						return ps_ERR;
					/*END_ERROR;*/
					   }
					}
				}

				isqlGlob.printf(NEWLINE);
			}
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (first_proc)
			return OBJECT_NOT_FOUND;

		isqlGlob.printf(NEWLINE);
		return (SKIP);
	}

	// A procedure was named, so print all the info on that procedure

	bool first = true;
	MetaString package, procedure;
	if (quoted)
		procedure = procname;
	else
		parse_package(procname, package, procedure);

	/*FOR PRC IN RDB$PROCEDURES WITH
		PRC.RDB$PROCEDURE_NAME EQ procedure.c_str() AND
		PRC.RDB$PACKAGE_NAME EQUIV NULLIF(package.c_str(), '')*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_321 && fbTrans && DB)
	      fb_321 = DB->compileRequest(fbStatus, sizeof(fb_322), fb_322);
	   isc_vtov ((const char*) procedure.c_str(), (char*) fb_323.fb_324, 253);
	   isc_vtov ((const char*) package.c_str(), (char*) fb_323.fb_325, 253);
	   if (fbTrans && fb_321)
	      fb_321->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_323));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_321->release(); fb_321 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_321->receive (fbStatus, 0, 1, 781, CAST_MSG(&fb_326));
	   if (!fb_326.fb_329 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		first = false;

		if (!/*PRC.RDB$ENTRYPOINT.NULL*/
		     fb_326.fb_333)
		{
			fb_utils::exact_name(/*PRC.RDB$ENTRYPOINT*/
					     fb_326.fb_328);
			isqlGlob.printf("External name: %s%s", /*PRC.RDB$ENTRYPOINT*/
							       fb_326.fb_328, NEWLINE);
		}

		if (!/*PRC.RDB$SQL_SECURITY.NULL*/
		     fb_326.fb_332)
		{
			const char* ss = /*PRC.RDB$SQL_SECURITY*/
					 fb_326.fb_334 ? "DEFINER" : "INVOKER";
			isqlGlob.printf("SQL SECURITY: %s%s", ss, NEWLINE);
		}

		if (!/*PRC.RDB$ENGINE_NAME.NULL*/
		     fb_326.fb_331)
		{
			fb_utils::exact_name(/*PRC.RDB$ENGINE_NAME*/
					     fb_326.fb_335);
			isqlGlob.printf("Engine: %s%s", /*PRC.RDB$ENGINE_NAME*/
							fb_326.fb_335, NEWLINE);
		}

		if (!/*PRC.RDB$PROCEDURE_SOURCE.NULL*/
		     fb_326.fb_330)
		{
			isqlGlob.printf("Procedure text:%s", NEWLINE);
			isqlGlob.printf("=============================================================================%s", NEWLINE);
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*PRC.RDB$PROCEDURE_SOURCE*/
								      fb_326.fb_327);
			isqlGlob.printf("%s=============================================================================%s", NEWLINE, NEWLINE);
		}

		bool first_param = true;

		/*FOR PRM IN RDB$PROCEDURE_PARAMETERS CROSS
			FLD IN RDB$FIELDS WITH
			PRM.RDB$PROCEDURE_NAME EQ PRC.RDB$PROCEDURE_NAME AND
			PRM.RDB$PACKAGE_NAME EQUIV NULLIF(package.c_str(), '') AND
			PRM.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME
			SORTED BY PRM.RDB$PARAMETER_TYPE, PRM.RDB$PARAMETER_NUMBER*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_288 && fbTrans && DB)
		      fb_288 = DB->compileRequest(fbStatus, sizeof(fb_289), fb_289);
		   isc_vtov ((const char*) fb_326.fb_336, (char*) fb_290.fb_291, 253);
		   isc_vtov ((const char*) package.c_str(), (char*) fb_290.fb_292, 253);
		   if (fbTrans && fb_288)
		      fb_288->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_290));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_288->release(); fb_288 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_288->receive (fbStatus, 0, 1, 1070, CAST_MSG(&fb_293));
		   if (!fb_293.fb_296 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if (first_param)
			{
				isqlGlob.printf("Parameters:%s", NEWLINE);
				first_param = false;
			}
			fb_utils::exact_name(/*PRM.RDB$PARAMETER_NAME*/
					     fb_293.fb_317);

			isqlGlob.printf("%-33s %s ", /*PRM.RDB$PARAMETER_NAME*/
						     fb_293.fb_317,
					 (/*PRM.RDB$PARAMETER_TYPE*/
					  fb_293.fb_316 ? "OUTPUT" : "INPUT"));

			prm_mech_t mechanism = prm_mech_normal;
			bool prm_default_source_null = true;
			ISC_QUAD prm_default_source;

			SSHORT collation = 0;
			bool prm_collation_null = true;

			if (!/*PRM.RDB$PARAMETER_MECHANISM.NULL*/
			     fb_293.fb_314)
				mechanism = (prm_mech_t) /*PRM.RDB$PARAMETER_MECHANISM*/
							 fb_293.fb_315;

			if (/*PRM.RDB$PARAMETER_TYPE*/
			    fb_293.fb_316 == 0)
			{
				prm_default_source_null = /*PRM.RDB$DEFAULT_SOURCE.NULL*/
							  fb_293.fb_313;
				prm_default_source = /*PRM.RDB$DEFAULT_SOURCE*/
						     fb_293.fb_295;
			}

			prm_collation_null = /*PRM.RDB$COLLATION_ID.NULL*/
					     fb_293.fb_311;
			if (!prm_collation_null)
				collation = /*PRM.RDB$COLLATION_ID*/
					    fb_293.fb_312;

			char relationName[BUFFER_LENGTH256] = "";
			char relationField[BUFFER_LENGTH256] = "";

			if (!/*PRM.RDB$RELATION_NAME.NULL*/
			     fb_293.fb_310)
			{
				strcpy(relationName, /*PRM.RDB$RELATION_NAME*/
						     fb_293.fb_318);
				fb_utils::exact_name(relationName);
			}

			if (!/*PRM.RDB$FIELD_NAME.NULL*/
			     fb_293.fb_309)
			{
				strcpy(relationField, /*PRM.RDB$FIELD_NAME*/
						      fb_293.fb_319);
				fb_utils::exact_name(relationField);
			}

			if (prm_collation_null)
			{
				/*FOR RFL IN RDB$RELATION_FIELDS
					WITH RFL.RDB$RELATION_NAME = PRM.RDB$RELATION_NAME AND
						 RFL.RDB$FIELD_NAME = PRM.RDB$FIELD_NAME*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_279 && DB)
				      fb_279 = DB->compileRequest(fbStatus, sizeof(fb_280), fb_280);
				   isc_vtov ((const char*) fb_293.fb_318, (char*) fb_281.fb_282, 253);
				   isc_vtov ((const char*) fb_293.fb_319, (char*) fb_281.fb_283, 253);
				   fb_279->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_281));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_279->release(); fb_279 = NULL; }
				   else break;
				   }
				while (1)
				   {
				   fb_279->receive (fbStatus, 0, 1, 6, CAST_MSG(&fb_284));
				   if (!fb_284.fb_285) break;

					prm_collation_null = /*RFL.RDB$COLLATION_ID.NULL*/
							     fb_284.fb_286;
					if (!prm_collation_null)
						collation = /*RFL.RDB$COLLATION_ID*/
							    fb_284.fb_287;
				/*END_FOR*/
				   }
				}
			}

			if (prm_collation_null && !/*FLD.RDB$COLLATION_ID.NULL*/
						   fb_293.fb_307)
				collation = /*FLD.RDB$COLLATION_ID*/
					    fb_293.fb_308;

			const bool basedOnColumn = relationName[0] && relationField[0];
			// Decide if this is a user-created domain
			if (!fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
						       fb_293.fb_320) || /*FLD.RDB$SYSTEM_FLAG*/
     fb_293.fb_306 == 1 ||
				basedOnColumn)
			{
				isqlGlob.printf("(%s", (mechanism == prm_mech_type_of ? "TYPE OF " : ""));

				if (basedOnColumn)
					isqlGlob.printf("COLUMN %s.%s) ", relationName, relationField);
				else
				{
					fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
							     fb_293.fb_320);
					isqlGlob.printf("%s) ", /*FLD.RDB$FIELD_NAME*/
								fb_293.fb_320);
				}
			}

			if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
						   fb_293.fb_320, /*FLD.RDB$FIELD_TYPE*/
  fb_293.fb_305, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_293.fb_304,
					/*FLD.RDB$FIELD_PRECISION*/
					fb_293.fb_303, /*FLD.RDB$FIELD_SCALE*/
  fb_293.fb_302))
			{
				return ps_ERR;
			}

			// Use RDB$CHARACTER_LENGTH instead of RDB$FIELD_LENGTH
			//   FSG 19.Nov.2000
			if ((/*FLD.RDB$FIELD_TYPE*/
			     fb_293.fb_305 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_293.fb_305 == blr_varying) &&
				!/*FLD.RDB$CHARACTER_LENGTH.NULL*/
				 fb_293.fb_300)
			{
				isqlGlob.printf("(%d)", /*FLD.RDB$CHARACTER_LENGTH*/
							fb_293.fb_301);
			}

			// Show international character sets and collations

			if (((/*FLD.RDB$FIELD_TYPE*/
			      fb_293.fb_305 == blr_text ||
				/*FLD.RDB$FIELD_TYPE*/
				fb_293.fb_305 == blr_varying) && /*FLD.RDB$FIELD_SUB_TYPE*/
		    fb_293.fb_304 != fb_text_subtype_binary) ||
				/*FLD.RDB$FIELD_TYPE*/
				fb_293.fb_305 == blr_blob)
			{
				SSHORT charset = 0;
				if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
				     fb_293.fb_298)
					charset = /*FLD.RDB$CHARACTER_SET_ID*/
						  fb_293.fb_299;

				show_charsets(charset, collation);
			}

			if (/*PRM.RDB$PARAMETER_TYPE*/
			    fb_293.fb_316 == 0) // input, try to show default and make Vlad happy.
			{
				if (!prm_default_source_null)
				{
					isqlGlob.printf(" ");
					SHOW_print_metadata_text_blob(isqlGlob.Out, &prm_default_source);
				}
				else if (fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
								   fb_293.fb_320) && !/*FLD.RDB$DEFAULT_SOURCE.NULL*/
      fb_293.fb_297)
				{
					isqlGlob.printf(" ");
					SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
										     fb_293.fb_294);
				}
			}

			isqlGlob.printf(NEWLINE);
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg (fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (first)
		return (OBJECT_NOT_FOUND);
	return (SKIP);
}


static void parse_package(const char* procname, MetaString& package, MetaString& procedure)
{
	for (const char* point = procname; *point; ++point)
	{
		if (*point == '.')
		{
			package.assign(procname, point - procname);
			procedure = ++point;
			return;
		}
	}

	package = "";
	procedure = procname;
}


static processing_state show_publications(const SCHAR* pub_name, bool sys, const SCHAR* msg)
{
   struct fb_263_struct {
          short fb_264;	/* fbUtility */
          short fb_265;	/* gds__null_flag */
          short fb_266;	/* RDB$SYSTEM_FLAG */
          char  fb_267 [253];	/* RDB$PUBLICATION_NAME */
   } fb_263;
   struct fb_272_struct {
          short fb_273;	/* fbUtility */
          short fb_274;	/* gds__null_flag */
          short fb_275;	/* RDB$AUTO_ENABLE */
          short fb_276;	/* gds__null_flag */
          short fb_277;	/* RDB$ACTIVE_FLAG */
          char  fb_278 [253];	/* RDB$PUBLICATION_NAME */
   } fb_272;
   struct fb_270_struct {
          char  fb_271 [253];	/* RDB$PUBLICATION_NAME */
   } fb_270;
/**************************************
 *
 *	s h o w _ p u b l i c a t i o n s
 *
 **************************************
 *
 * Functional description
 *	Show publications.
 *
 **************************************/
	if (isqlGlob.major_ods < ODS_VERSION13)
		return OBJECT_NOT_FOUND;

	bool first = true;

	if (pub_name && *pub_name)
	{
		// List named publication

		/*FOR PUB IN RDB$PUBLICATIONS WITH
			PUB.RDB$PUBLICATION_NAME EQ pub_name*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_268 && fbTrans && DB)
		      fb_268 = DB->compileRequest(fbStatus, sizeof(fb_269), fb_269);
		   isc_vtov ((const char*) pub_name, (char*) fb_270.fb_271, 253);
		   if (fbTrans && fb_268)
		      fb_268->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_270));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_268->release(); fb_268 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_268->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_272));
		   if (!fb_272.fb_273 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			first = false;

			fb_utils::exact_name(/*PUB.RDB$PUBLICATION_NAME*/
					     fb_272.fb_278);
			isqlGlob.printf("%s: ", /*PUB.RDB$PUBLICATION_NAME*/
						fb_272.fb_278);

			const bool active_flag = (!/*PUB.RDB$ACTIVE_FLAG.NULL*/
						   fb_272.fb_276 && /*PUB.RDB$ACTIVE_FLAG*/
    fb_272.fb_277 > 0);
			isqlGlob.printf("%s", active_flag ? "Enabled" : "Disabled");

			if (!/*PUB.RDB$AUTO_ENABLE.NULL*/
			     fb_272.fb_274 && /*PUB.RDB$AUTO_ENABLE*/
    fb_272.fb_275 > 0)
				isqlGlob.printf(", Auto-enable");

			isqlGlob.printf(NEWLINE);

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}
	else
	{
		//  List all publications

		/*FOR PUB IN RDB$PUBLICATIONS
			SORTED BY PUB.RDB$PUBLICATION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_261 && fbTrans && DB)
		      fb_261 = DB->compileRequest(fbStatus, sizeof(fb_262), fb_262);
		   if (fbTrans && fb_261)
		      fb_261->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_261->release(); fb_261 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_261->receive (fbStatus, 0, 0, 259, CAST_MSG(&fb_263));
		   if (!fb_263.fb_264 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			const bool system_flag = (!/*PUB.RDB$SYSTEM_FLAG.NULL*/
						   fb_263.fb_265 && /*PUB.RDB$SYSTEM_FLAG*/
    fb_263.fb_266 > 0);

			if (system_flag == sys)
			{
				if (first && msg)
					isqlGlob.printf("%s%s", msg, NEWLINE);

				first = false;

				isqlGlob.printf("%s%s", fb_utils::exact_name(/*PUB.RDB$PUBLICATION_NAME*/
									     fb_263.fb_267), NEWLINE);
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (!first)
			isqlGlob.printf(NEWLINE);
	}

	if (first)
		return (OBJECT_NOT_FOUND);

	return (SKIP);
}


static void show_pub_table(const SCHAR* table_name)
{
   struct fb_256_struct {
          short fb_257;	/* fbUtility */
          short fb_258;	/* gds__null_flag */
          short fb_259;	/* RDB$ACTIVE_FLAG */
          char  fb_260 [253];	/* RDB$PUBLICATION_NAME */
   } fb_256;
   struct fb_254_struct {
          char  fb_255 [253];	/* RDB$TABLE_NAME */
   } fb_254;
/**************************************
 *
 *	s h o w _ p u b _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Show publication status for the given table.
 *
 **************************************/
	if (isqlGlob.major_ods < ODS_VERSION13)
		return;

	bool first = true;

	if (table_name && *table_name)
	{
		/*FOR PTAB IN RDB$PUBLICATION_TABLES CROSS
			PUB IN RDB$PUBLICATIONS OVER RDB$PUBLICATION_NAME WITH
			PTAB.RDB$TABLE_NAME EQ table_name
			SORTED BY DESCENDING PUB.RDB$SYSTEM_FLAG, PUB.RDB$PUBLICATION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_252 && fbTrans && DB)
		      fb_252 = DB->compileRequest(fbStatus, sizeof(fb_253), fb_253);
		   isc_vtov ((const char*) table_name, (char*) fb_254.fb_255, 253);
		   if (fbTrans && fb_252)
		      fb_252->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_254));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_252->release(); fb_252 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_252->receive (fbStatus, 0, 1, 259, CAST_MSG(&fb_256));
		   if (!fb_256.fb_257 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			const auto pub_name = fb_utils::exact_name(/*PUB.RDB$PUBLICATION_NAME*/
								   fb_256.fb_260);

			if (first)
			{
				TEXT msg[MSG_LENGTH];
				IUTILS_msg_get(MSG_PUBLICATIONS, msg);
				isqlGlob.printf("%s %s", msg, pub_name);
			}
			else
				isqlGlob.printf(", %s", pub_name);

			const bool active_flag = (!/*PUB.RDB$ACTIVE_FLAG.NULL*/
						   fb_256.fb_258 && /*PUB.RDB$ACTIVE_FLAG*/
    fb_256.fb_259 > 0);
			isqlGlob.printf(" (%s)", active_flag ? "Enabled" : "Disabled");

			first = false;

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return;
		/*END_ERROR;*/
		   }
		}
	}

	if (!first)
		isqlGlob.printf(NEWLINE);
}


bool SHOW_system_privileges(const char* role, const char* prefix, bool lf)
{
   struct fb_241_struct {
          short fb_242;	/* fbUtility */
          char  fb_243 [253];	/* RDB$TYPE_NAME */
   } fb_241;
   struct fb_238_struct {
          short fb_239;	/* RDB$TYPE */
          short fb_240;	/* RDB$TYPE */
   } fb_238;
   struct fb_248_struct {
          char  fb_249 [8];	/* RDB$SYSTEM_PRIVILEGES */
          short fb_250;	/* fbUtility */
          short fb_251;	/* gds__null_flag */
   } fb_248;
   struct fb_246_struct {
          char  fb_247 [253];	/* RDB$ROLE_NAME */
   } fb_246;
	bool first = true;

	/*FOR X IN RDB$ROLES WITH
		X.RDB$ROLE_NAME EQ role*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_244 && fbTrans && DB)
	      fb_244 = DB->compileRequest(fbStatus, sizeof(fb_245), fb_245);
	   isc_vtov ((const char*) role, (char*) fb_246.fb_247, 253);
	   if (fbTrans && fb_244)
	      fb_244->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_246));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_244->release(); fb_244 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_244->receive (fbStatus, 0, 1, 12, CAST_MSG(&fb_248));
	   if (!fb_248.fb_250 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
	{
		if (!/*X.RDB$SYSTEM_PRIVILEGES.NULL*/
		     fb_248.fb_251)
		{
			for (unsigned byte = 0; byte < sizeof(/*X.RDB$SYSTEM_PRIVILEGES*/
							      fb_248.fb_249); ++byte)
			{
				char b = /*X.RDB$SYSTEM_PRIVILEGES*/
					 fb_248.fb_249[byte];
				for (int bit = 0; bit < 8; ++bit)
				{
					if (b & (1 << bit))
					{
						/*FOR T IN RDB$TYPES
							WITH T.RDB$FIELD_NAME EQ 'RDB$SYSTEM_PRIVILEGES' AND
								 T.RDB$TYPE EQ (byte * 8 + bit)*/
						{
						for (int retries = 0; retries < 2; ++retries)
						   {
						   {
						   if (!DB)
						      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
						   if (DB && !fbTrans)
						      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
						   }
						   if (!fb_236 && DB)
						      fb_236 = DB->compileRequest(fbStatus, sizeof(fb_237), fb_237);
						   fb_238.fb_239 = bit;
						   fb_238.fb_240 = byte;
						   fb_236->startAndSend(fbStatus, fbTrans, 0, 0, 4, CAST_CONST_MSG(&fb_238));
						   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_236->release(); fb_236 = NULL; }
						   else break;
						   }
						while (1)
						   {
						   fb_236->receive (fbStatus, 0, 1, 255, CAST_MSG(&fb_241));
						   if (!fb_241.fb_242) break;
						{
							if (first)
							{
								if (lf)
									isqlGlob.printf("%s", NEWLINE);
								isqlGlob.printf("%s", prefix);
							}
							else
								isqlGlob.printf(",");

							first = false;
							isqlGlob.printf(" %s", fb_utils::exact_name(/*T.RDB$TYPE_NAME*/
												    fb_241.fb_243));
						}
						/*END_FOR*/
						   }
						}
					}
				}
			}
		}
	}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return false;
	/*END_ERROR;*/
	   }
	}

	return !first;
}


static processing_state show_role(const SCHAR* object, bool system, const char* msg)
{
   struct fb_218_struct {
          short fb_219;	/* fbUtility */
          char  fb_220 [253];	/* RDB$RELATION_NAME */
          char  fb_221 [253];	/* RDB$USER */
   } fb_218;
   struct fb_213_struct {
          short fb_214;	/* RDB$USER_TYPE */
          short fb_215;	/* RDB$USER_TYPE */
          short fb_216;	/* RDB$OBJECT_TYPE */
          char  fb_217 [253];	/* RDB$RELATION_NAME */
   } fb_213;
   struct fb_226_struct {
          short fb_227;	/* fbUtility */
          char  fb_228 [253];	/* RDB$ROLE_NAME */
   } fb_226;
   struct fb_224_struct {
          char  fb_225 [253];	/* RDB$ROLE_NAME */
   } fb_224;
   struct fb_231_struct {
          short fb_232;	/* fbUtility */
          short fb_233;	/* gds__null_flag */
          short fb_234;	/* RDB$SYSTEM_FLAG */
          char  fb_235 [253];	/* RDB$ROLE_NAME */
   } fb_231;
	if (isqlGlob.major_ods < ODS_VERSION9)
		return OBJECT_NOT_FOUND;

	if (object == NULL)
	{
		// show role with no parameters, show all roles
		// **************************************
		// * Print the names of all roles from
		// * RDB$ROLES.  We use a dynamic query
		// * If there is any roles, then returns SKIP.
		// *	Otherwise returns OBJECT_NOT_FOUND.
		// **************************************/
		bool first = true;

		/*FOR X IN RDB$ROLES WITH
			X.RDB$ROLE_NAME NOT MISSING
			SORTED BY X.RDB$ROLE_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_229 && fbTrans && DB)
		      fb_229 = DB->compileRequest(fbStatus, sizeof(fb_230), fb_230);
		   if (fbTrans && fb_229)
		      fb_229->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_229->release(); fb_229 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_229->receive (fbStatus, 0, 0, 259, CAST_MSG(&fb_231));
		   if (!fb_231.fb_232 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			bool system_flag = !/*X.RDB$SYSTEM_FLAG.NULL*/
					    fb_231.fb_233 && /*X.RDB$SYSTEM_FLAG*/
    fb_231.fb_234 > 0;

			if (system_flag == system)
			{
				if (first && msg)
					isqlGlob.printf("%s%s", msg, NEWLINE);
				first = false;

				isqlGlob.printf("%s%s", fb_utils::exact_name(/*X.RDB$ROLE_NAME*/
									     fb_231.fb_235), NEWLINE);

				/***
				if (SHOW_system_privileges(X.RDB$ROLE_NAME, "System privileges:", !odd))
				{
					isqlGlob.printf("%s", NEWLINE);
					odd = true;
				}
				***/
			}
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (!first)
		{
			isqlGlob.printf(NEWLINE);
			return SKIP;
		}

		return OBJECT_NOT_FOUND;
	}

	// show role with role supplied, display users and other roles granted this role
	SCHAR role_name[BUFFER_LENGTH256];
	bool first = true;

	/*FOR FIRST 1 R IN RDB$ROLES WITH R.RDB$ROLE_NAME EQ object*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_222 && fbTrans && DB)
	      fb_222 = DB->compileRequest(fbStatus, sizeof(fb_223), fb_223);
	   isc_vtov ((const char*) object, (char*) fb_224.fb_225, 253);
	   if (fbTrans && fb_222)
	      fb_222->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_224));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_222->release(); fb_222 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_222->receive (fbStatus, 0, 1, 255, CAST_MSG(&fb_226));
	   if (!fb_226.fb_227 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		/*FOR PRV IN RDB$USER_PRIVILEGES WITH
			PRV.RDB$OBJECT_TYPE   EQ obj_sql_role AND
			(PRV.RDB$USER_TYPE    EQ obj_user      OR
			 PRV.RDB$USER_TYPE     EQ obj_sql_role) AND
			PRV.RDB$RELATION_NAME EQ object       AND
			PRV.RDB$PRIVILEGE     EQ 'M'
			SORTED BY  PRV.RDB$USER*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_211 && fbTrans && DB)
		      fb_211 = DB->compileRequest(fbStatus, sizeof(fb_212), fb_212);
		   fb_213.fb_214 = obj_sql_role;
		   fb_213.fb_215 = obj_user;
		   fb_213.fb_216 = obj_sql_role;
		   isc_vtov ((const char*) object, (char*) fb_213.fb_217, 253);
		   if (fbTrans && fb_211)
		      fb_211->startAndSend(fbStatus, fbTrans, 0, 0, 259, CAST_CONST_MSG(&fb_213));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_211->release(); fb_211 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_211->receive (fbStatus, 0, 1, 508, CAST_MSG(&fb_218));
		   if (!fb_218.fb_219 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if (first)
			{
				first = false;
				fb_utils::exact_name(/*PRV.RDB$RELATION_NAME*/
						     fb_218.fb_220);
				strcpy(role_name, /*PRV.RDB$RELATION_NAME*/
						  fb_218.fb_220);
				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(role_name, SQL_identifier, DBL_QUOTE);
				else
					strcpy(SQL_identifier, role_name);

				isqlGlob.printf("Role %s is granted to:%s", SQL_identifier, NEWLINE);
			}

			fb_utils::exact_name(/*PRV.RDB$USER*/
					     fb_218.fb_221);
			isqlGlob.printf("%s%s", /*PRV.RDB$USER*/
						fb_218.fb_221, NEWLINE);

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg (fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		if (first)
		{
		    first = false;
			fb_utils::exact_name(/*R.RDB$ROLE_NAME*/
					     fb_226.fb_228);
			strcpy(role_name, /*R.RDB$ROLE_NAME*/
					  fb_226.fb_228);
			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
				IUTILS_copy_SQL_id(role_name, SQL_identifier, DBL_QUOTE);
			else
				strcpy(SQL_identifier, role_name);

		    isqlGlob.printf("Role %s isn't granted to anyone.%s", SQL_identifier, NEWLINE);
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	if (first)
		return (OBJECT_NOT_FOUND);
	else if (SHOW_system_privileges(object, "System privileges:", false))
		isqlGlob.printf("%s", NEWLINE);

	return (SKIP);
}


// ***********************
// s h o w _ s e c c l a s
// ***********************
// Show low-level, GDML security for an object. It may be table/view or procedure.
// Using SHOW SECCLASS <name> DET[AIL] will print the contents of the sec blob.
// Using SHOW SECCLASS * DET[AIL] will print the db-wide sec class in rdb$database.
static processing_state show_secclass(const char* object, const char* opt)
{
   struct fb_140_struct {
          ISC_QUAD fb_141;	/* RDB$ACL */
          short fb_142;	/* fbUtility */
          char  fb_143 [253];	/* RDB$SECURITY_CLASS */
   } fb_140;
   struct fb_138_struct {
          char  fb_139 [253];	/* RDB$EXCEPTION_NAME */
   } fb_138;
   struct fb_148_struct {
          ISC_QUAD fb_149;	/* RDB$ACL */
          short fb_150;	/* fbUtility */
          char  fb_151 [253];	/* RDB$SECURITY_CLASS */
   } fb_148;
   struct fb_146_struct {
          char  fb_147 [253];	/* RDB$GENERATOR_NAME */
   } fb_146;
   struct fb_156_struct {
          ISC_QUAD fb_157;	/* RDB$ACL */
          short fb_158;	/* fbUtility */
          char  fb_159 [253];	/* RDB$SECURITY_CLASS */
   } fb_156;
   struct fb_154_struct {
          char  fb_155 [253];	/* RDB$PACKAGE_NAME */
   } fb_154;
   struct fb_164_struct {
          ISC_QUAD fb_165;	/* RDB$ACL */
          short fb_166;	/* fbUtility */
          char  fb_167 [253];	/* RDB$SECURITY_CLASS */
   } fb_164;
   struct fb_162_struct {
          char  fb_163 [253];	/* RDB$FUNCTION_NAME */
   } fb_162;
   struct fb_172_struct {
          ISC_QUAD fb_173;	/* RDB$ACL */
          short fb_174;	/* fbUtility */
          char  fb_175 [253];	/* RDB$SECURITY_CLASS */
   } fb_172;
   struct fb_170_struct {
          char  fb_171 [253];	/* RDB$PROCEDURE_NAME */
   } fb_170;
   struct fb_180_struct {
          ISC_QUAD fb_181;	/* RDB$ACL */
          short fb_182;	/* fbUtility */
          char  fb_183 [253];	/* RDB$FIELD_NAME */
          char  fb_184 [253];	/* RDB$SECURITY_CLASS */
   } fb_180;
   struct fb_178_struct {
          char  fb_179 [253];	/* RDB$RELATION_NAME */
   } fb_178;
   struct fb_189_struct {
          ISC_QUAD fb_190;	/* RDB$ACL */
          ISC_QUAD fb_191;	/* RDB$VIEW_BLR */
          short fb_192;	/* fbUtility */
          short fb_193;	/* gds__null_flag */
          char  fb_194 [253];	/* RDB$SECURITY_CLASS */
   } fb_189;
   struct fb_187_struct {
          char  fb_188 [253];	/* RDB$RELATION_NAME */
   } fb_187;
   struct fb_199_struct {
          ISC_QUAD fb_200;	/* RDB$ACL */
          ISC_QUAD fb_201;	/* RDB$VIEW_BLR */
          short fb_202;	/* fbUtility */
          short fb_203;	/* gds__null_flag */
          char  fb_204 [253];	/* RDB$SECURITY_CLASS */
   } fb_199;
   struct fb_197_struct {
          char  fb_198 [253];	/* RDB$RELATION_NAME */
   } fb_197;
   struct fb_207_struct {
          ISC_QUAD fb_208;	/* RDB$ACL */
          short fb_209;	/* fbUtility */
          char  fb_210 [253];	/* RDB$SECURITY_CLASS */
   } fb_207;
	if (!object || !*object)
		return ps_ERR;

	const bool detail = opt &&
		(fb_utils::stricmp(opt, "DETAIL") == 0 ||
		 fb_utils::stricmp(opt, "DET") == 0);
	IsqlVar var;
	memset(&var, 0, sizeof(var));
	var.subType = isc_blob_acl;

	int count = 0;

	if (strcmp(object, "*") == 0)
	{
		/*FOR D IN RDB$DATABASE
		CROSS SC IN RDB$SECURITY_CLASSES
		OVER RDB$SECURITY_CLASS*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_205 && fbTrans && DB)
		      fb_205 = DB->compileRequest(fbStatus, sizeof(fb_206), fb_206);
		   if (fbTrans && fb_205)
		      fb_205->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_205->release(); fb_205 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_205->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_207));
		   if (!fb_207.fb_209 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			++count;
			isqlGlob.printf("Database-wide's sec class %s%s",
				fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
						     fb_207.fb_210), NEWLINE);
			if (detail)
			{
				var.value.setPtr = &/*SC.RDB$ACL*/
						    fb_207.fb_208;
				ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
			}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		return count ? SKIP : OBJECT_NOT_FOUND;
	}

	/*FOR REL IN RDB$RELATIONS
	CROSS SC IN RDB$SECURITY_CLASSES
	OVER RDB$SECURITY_CLASS
	WITH REL.RDB$RELATION_NAME EQ object*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_195 && fbTrans && DB)
	      fb_195 = DB->compileRequest(fbStatus, sizeof(fb_196), fb_196);
	   isc_vtov ((const char*) object, (char*) fb_197.fb_198, 253);
	   if (fbTrans && fb_195)
	      fb_195->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_197));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_195->release(); fb_195 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_195->receive (fbStatus, 0, 1, 273, CAST_MSG(&fb_199));
	   if (!fb_199.fb_202 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		++count;
		isqlGlob.printf("%s's main sec class %s%s",
			/*REL.RDB$VIEW_BLR.NULL*/
			fb_199.fb_203 ? "Table" : "View",
			fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
					     fb_199.fb_204), NEWLINE);
		if (detail)
		{
			var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
								    fb_199.fb_200);
			ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR REL2 IN RDB$RELATIONS
	CROSS SC IN RDB$SECURITY_CLASSES
	WITH REL2.RDB$RELATION_NAME EQ object
	AND REL2.RDB$DEFAULT_CLASS EQ SC.RDB$SECURITY_CLASS*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_185 && fbTrans && DB)
	      fb_185 = DB->compileRequest(fbStatus, sizeof(fb_186), fb_186);
	   isc_vtov ((const char*) object, (char*) fb_187.fb_188, 253);
	   if (fbTrans && fb_185)
	      fb_185->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_187));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_185->release(); fb_185 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_185->receive (fbStatus, 0, 1, 273, CAST_MSG(&fb_189));
	   if (!fb_189.fb_192 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		++count;
		isqlGlob.printf("%s's default sec class %s%s",
			/*REL2.RDB$VIEW_BLR.NULL*/
			fb_189.fb_193 ? "Table" : "View",
			fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
					     fb_189.fb_194), NEWLINE);
		if (detail)
		{
			var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
								    fb_189.fb_190);
			ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR RF IN RDB$RELATION_FIELDS
	CROSS SC IN RDB$SECURITY_CLASSES
	OVER RDB$SECURITY_CLASS
	WITH RF.RDB$RELATION_NAME EQ object
	SORTED BY RF.RDB$FIELD_POSITION*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_176 && fbTrans && DB)
	      fb_176 = DB->compileRequest(fbStatus, sizeof(fb_177), fb_177);
	   isc_vtov ((const char*) object, (char*) fb_178.fb_179, 253);
	   if (fbTrans && fb_176)
	      fb_176->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_178));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_176->release(); fb_176 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_176->receive (fbStatus, 0, 1, 516, CAST_MSG(&fb_180));
	   if (!fb_180.fb_182 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		++count;
		isqlGlob.printf("   Field %s - sec class %s%s", fb_utils::exact_name(/*RF.RDB$FIELD_NAME*/
										     fb_180.fb_183),
			/*SC.RDB$SECURITY_CLASS*/
			fb_180.fb_184, NEWLINE);
		if (detail)
		{
			var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
								    fb_180.fb_181);
			ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR PR IN RDB$PROCEDURES
	CROSS SC IN RDB$SECURITY_CLASSES
	OVER RDB$SECURITY_CLASS
	WITH PR.RDB$PROCEDURE_NAME EQ object AND
		 PR.RDB$PACKAGE_NAME MISSING*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_168 && fbTrans && DB)
	      fb_168 = DB->compileRequest(fbStatus, sizeof(fb_169), fb_169);
	   isc_vtov ((const char*) object, (char*) fb_170.fb_171, 253);
	   if (fbTrans && fb_168)
	      fb_168->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_170));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_168->release(); fb_168 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_168->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_172));
	   if (!fb_172.fb_174 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		++count;
		isqlGlob.printf("Procedure's sec class %s%s",
			fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
					     fb_172.fb_175), NEWLINE);
		if (detail)
		{
			var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
								    fb_172.fb_173);
			ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	if (isqlGlob.major_ods >= ODS_VERSION12)
	{
		/*FOR FUN IN RDB$FUNCTIONS
		CROSS SC IN RDB$SECURITY_CLASSES
		OVER RDB$SECURITY_CLASS
		WITH FUN.RDB$FUNCTION_NAME EQ object AND
			 FUN.RDB$PACKAGE_NAME MISSING*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_160 && fbTrans && DB)
		      fb_160 = DB->compileRequest(fbStatus, sizeof(fb_161), fb_161);
		   isc_vtov ((const char*) object, (char*) fb_162.fb_163, 253);
		   if (fbTrans && fb_160)
		      fb_160->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_162));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_160->release(); fb_160 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_160->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_164));
		   if (!fb_164.fb_166 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			++count;
			isqlGlob.printf("Function's sec class %s%s",
				fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
						     fb_164.fb_167), NEWLINE);
			if (detail)
			{
				var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
									    fb_164.fb_165);
				ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
			}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		/*FOR PKG IN RDB$PACKAGES
		CROSS SC IN RDB$SECURITY_CLASSES
		OVER RDB$SECURITY_CLASS
		WITH PKG.RDB$PACKAGE_NAME EQ object*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_152 && fbTrans && DB)
		      fb_152 = DB->compileRequest(fbStatus, sizeof(fb_153), fb_153);
		   isc_vtov ((const char*) object, (char*) fb_154.fb_155, 253);
		   if (fbTrans && fb_152)
		      fb_152->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_154));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_152->release(); fb_152 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_152->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_156));
		   if (!fb_156.fb_158 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			++count;
			isqlGlob.printf("Package's sec class %s%s",
				fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
						     fb_156.fb_159), NEWLINE);
			if (detail)
			{
				var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
									    fb_156.fb_157);
				ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
			}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		/*FOR GEN IN RDB$GENERATORS
		CROSS SC IN RDB$SECURITY_CLASSES
		OVER RDB$SECURITY_CLASS
		WITH GEN.RDB$GENERATOR_NAME EQ object*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_144 && fbTrans && DB)
		      fb_144 = DB->compileRequest(fbStatus, sizeof(fb_145), fb_145);
		   isc_vtov ((const char*) object, (char*) fb_146.fb_147, 253);
		   if (fbTrans && fb_144)
		      fb_144->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_146));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_144->release(); fb_144 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_144->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_148));
		   if (!fb_148.fb_150 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			++count;
			isqlGlob.printf("Sequence's sec class %s%s",
				fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
						     fb_148.fb_151), NEWLINE);
			if (detail)
			{
				var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
									    fb_148.fb_149);
				ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
			}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}

		/*FOR XCP IN RDB$EXCEPTIONS
		CROSS SC IN RDB$SECURITY_CLASSES
		OVER RDB$SECURITY_CLASS
		WITH XCP.RDB$EXCEPTION_NAME EQ object*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_136 && fbTrans && DB)
		      fb_136 = DB->compileRequest(fbStatus, sizeof(fb_137), fb_137);
		   isc_vtov ((const char*) object, (char*) fb_138.fb_139, 253);
		   if (fbTrans && fb_136)
		      fb_136->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_138));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_136->release(); fb_136 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_136->receive (fbStatus, 0, 1, 263, CAST_MSG(&fb_140));
		   if (!fb_140.fb_142 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			++count;
			isqlGlob.printf("Exception's sec class %s%s",
				fb_utils::exact_name(/*SC.RDB$SECURITY_CLASS*/
						     fb_140.fb_143), NEWLINE);
			if (detail)
			{
				var.value.asChar = reinterpret_cast<char*>(&/*SC.RDB$ACL*/
									    fb_140.fb_141);
				ISQL_print_item_blob(isqlGlob.Out, &var, fbTrans, isc_blob_acl);
			}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR*/
		   }
		}
	}

	return count ? SKIP : OBJECT_NOT_FOUND;
}


static processing_state show_table(const SCHAR* relation_name, bool isView)
{
   struct fb_43_struct {
          short fb_44;	/* fbUtility */
          char  fb_45 [253];	/* RDB$CONSTRAINT_NAME */
          char  fb_46 [253];	/* RDB$TRIGGER_NAME */
   } fb_43;
   struct fb_41_struct {
          char  fb_42 [253];	/* RDB$RELATION_NAME */
   } fb_41;
   struct fb_51_struct {
          short fb_52;	/* fbUtility */
          short fb_53;	/* gds__null_flag */
          short fb_54;	/* RDB$INDEX_TYPE */
          char  fb_55 [253];	/* RDB$INDEX_NAME */
   } fb_51;
   struct fb_49_struct {
          char  fb_50 [253];	/* RDB$INDEX_NAME */
   } fb_49;
   struct fb_60_struct {
          char  fb_61 [12];	/* RDB$DELETE_RULE */
          char  fb_62 [12];	/* RDB$UPDATE_RULE */
          short fb_63;	/* fbUtility */
          short fb_64;	/* gds__null_flag */
          short fb_65;	/* gds__null_flag */
          char  fb_66 [253];	/* RDB$INDEX_NAME */
          char  fb_67 [253];	/* RDB$RELATION_NAME */
   } fb_60;
   struct fb_58_struct {
          char  fb_59 [253];	/* RDB$CONSTRAINT_NAME */
   } fb_58;
   struct fb_72_struct {
          char  fb_73 [12];	/* RDB$CONSTRAINT_TYPE */
          short fb_74;	/* fbUtility */
          char  fb_75 [253];	/* RDB$CONSTRAINT_NAME */
          char  fb_76 [253];	/* RDB$INDEX_NAME */
   } fb_72;
   struct fb_70_struct {
          char  fb_71 [253];	/* RDB$RELATION_NAME */
   } fb_70;
   struct fb_81_struct {
          ISC_QUAD fb_82;	/* RDB$VIEW_SOURCE */
          short fb_83;	/* fbUtility */
          short fb_84;	/* gds__null_flag */
   } fb_81;
   struct fb_79_struct {
          char  fb_80 [253];	/* RDB$RELATION_NAME */
   } fb_79;
   struct fb_89_struct {
          ISC_QUAD fb_90;	/* RDB$VALIDATION_SOURCE */
          ISC_QUAD fb_91;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_92;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_93;	/* RDB$COMPUTED_SOURCE */
          ISC_QUAD fb_94;	/* RDB$COMPUTED_BLR */
          short fb_95;	/* fbUtility */
          short fb_96;	/* gds__null_flag */
          short fb_97;	/* gds__null_flag */
          short fb_98;	/* gds__null_flag */
          short fb_99;	/* RDB$IDENTITY_TYPE */
          short fb_100;	/* gds__null_flag */
          short fb_101;	/* gds__null_flag */
          short fb_102;	/* RDB$NULL_FLAG */
          short fb_103;	/* RDB$NULL_FLAG */
          short fb_104;	/* RDB$SEGMENT_LENGTH */
          short fb_105;	/* gds__null_flag */
          short fb_106;	/* RDB$COLLATION_ID */
          short fb_107;	/* gds__null_flag */
          short fb_108;	/* RDB$COLLATION_ID */
          short fb_109;	/* gds__null_flag */
          short fb_110;	/* RDB$CHARACTER_SET_ID */
          short fb_111;	/* RDB$FIELD_SCALE */
          short fb_112;	/* RDB$FIELD_PRECISION */
          short fb_113;	/* RDB$FIELD_SUB_TYPE */
          short fb_114;	/* RDB$FIELD_TYPE */
          short fb_115;	/* gds__null_flag */
          short fb_116;	/* gds__null_flag */
          short fb_117;	/* gds__null_flag */
          short fb_118;	/* RDB$DIMENSIONS */
          short fb_119;	/* RDB$SYSTEM_FLAG */
          char  fb_120 [253];	/* RDB$FIELD_NAME */
          char  fb_121 [253];	/* RDB$FIELD_NAME */
          char  fb_122 [253];	/* RDB$BASE_FIELD */
          char  fb_123 [253];	/* RDB$GENERATOR_NAME */
   } fb_89;
   struct fb_87_struct {
          char  fb_88 [253];	/* RDB$RELATION_NAME */
   } fb_87;
   struct fb_128_struct {
          ISC_QUAD fb_129;	/* RDB$VIEW_BLR */
          char  fb_130 [256];	/* RDB$EXTERNAL_FILE */
          short fb_131;	/* fbUtility */
          short fb_132;	/* gds__null_flag */
          short fb_133;	/* gds__null_flag */
          short fb_134;	/* gds__null_flag */
          FB_BOOLEAN fb_135;	/* RDB$SQL_SECURITY */
   } fb_128;
   struct fb_126_struct {
          char  fb_127 [253];	/* RDB$RELATION_NAME */
   } fb_126;
/**************************************
 *
 *	s h o w _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	shows columns, types, info for a given table name
 *	and text of views.
 *	Use a SQL query to get the info and print it.
 *	This also shows integrity constraints and triggers
 *
 *	relation_name -- Name of table to investigate
 *
 **************************************/
	bool first = true;

	// Query to obtain relation information
	// REL.RDB$VIEW_BLR NOT MISSING

	/*FOR REL IN RDB$RELATIONS
		WITH REL.RDB$RELATION_NAME EQ relation_name*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_124 && fbTrans && DB)
	      fb_124 = DB->compileRequest(fbStatus, sizeof(fb_125), fb_125);
	   isc_vtov ((const char*) relation_name, (char*) fb_126.fb_127, 253);
	   if (fbTrans && fb_124)
	      fb_124->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_126));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_124->release(); fb_124 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_124->receive (fbStatus, 0, 1, 273, CAST_MSG(&fb_128));
	   if (!fb_128.fb_131 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		if (first)
		{
			if (!/*REL.RDB$SQL_SECURITY.NULL*/
			     fb_128.fb_134)
			{
				const char* ss = /*REL.RDB$SQL_SECURITY*/
						 fb_128.fb_135 ? "DEFINER" : "INVOKER";
				isqlGlob.printf("SQL SECURITY: %s%s", ss, NEWLINE);
			}

			if (!/*REL.RDB$EXTERNAL_FILE.NULL*/
			     fb_128.fb_133)
				isqlGlob.printf("External file: %s%s", /*REL.RDB$EXTERNAL_FILE*/
								       fb_128.fb_130, NEWLINE);
		}
		first = false;
		if ((isView && /*REL.RDB$VIEW_BLR.NULL*/
			       fb_128.fb_132) || (!isView && !/*REL.RDB$VIEW_BLR.NULL*/
		  fb_128.fb_132))
			first = true;
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	if (first)
		return (OBJECT_NOT_FOUND);

	/*
	FOR RFR IN RDB$RELATION_FIELDS CROSS
		REL IN RDB$RELATIONS CROSS
		FLD IN RDB$FIELDS WITH
		RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
		RFR.RDB$RELATION_NAME EQ relation_name AND
		REL.RDB$RELATION_NAME EQ RFR.RDB$RELATION_NAME
		SORTED BY RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME
	*/

	/*FOR RFR IN RDB$RELATION_FIELDS CROSS
		FLD IN RDB$FIELDS WITH
		RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
		RFR.RDB$RELATION_NAME EQ relation_name
		SORTED BY RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_85 && fbTrans && DB)
	      fb_85 = DB->compileRequest(fbStatus, sizeof(fb_86), fb_86);
	   isc_vtov ((const char*) relation_name, (char*) fb_87.fb_88, 253);
	   if (fbTrans && fb_85)
	      fb_85->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_87));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_85->release(); fb_85 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_85->receive (fbStatus, 0, 1, 1102, CAST_MSG(&fb_89));
	   if (!fb_89.fb_95 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// Get length of colname to align columns for printing

		fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
				     fb_89.fb_120);

		// Print the column name in first column

		isqlGlob.printf("%-31s ", /*RFR.RDB$FIELD_NAME*/
					  fb_89.fb_120);

		// Decide if this is a user-created domain
		if (!(fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
						fb_89.fb_121) && /*FLD.RDB$SYSTEM_FLAG*/
     fb_89.fb_119 != 1))
		{
			fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
					     fb_89.fb_121);
			isqlGlob.printf("(%s) ", /*FLD.RDB$FIELD_NAME*/
						 fb_89.fb_121);
		}

		// Detect the existence of arrays

		if (!/*FLD.RDB$DIMENSIONS.NULL*/
		     fb_89.fb_117)
		{
			isqlGlob.printf("ARRAY OF ");
			ISQL_array_dimensions (/*FLD.RDB$FIELD_NAME*/
					       fb_89.fb_121);
			isqlGlob.printf("%s                                ", NEWLINE);
		}

		// If a computed field, show the source and exit
		// Note that view columns which are computed are dealt with later.
		if (!/*FLD.RDB$COMPUTED_BLR.NULL*/
		     fb_89.fb_116 && !isView)
		{
			isqlGlob.printf("Computed by: ");
			if (!/*FLD.RDB$COMPUTED_SOURCE.NULL*/
			     fb_89.fb_115)
				SHOW_print_metadata_text_blob (isqlGlob.Out, &/*FLD.RDB$COMPUTED_SOURCE*/
									      fb_89.fb_93);
			isqlGlob.printf(NEWLINE);
			continue;
		}

		if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
					   fb_89.fb_121, /*FLD.RDB$FIELD_TYPE*/
  fb_89.fb_114, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_89.fb_113,
				/*FLD.RDB$FIELD_PRECISION*/
				fb_89.fb_112, /*FLD.RDB$FIELD_SCALE*/
  fb_89.fb_111))
		{
			return ps_ERR;
		}

		SSHORT char_set_id = 0;
		if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
		     fb_89.fb_109)
			char_set_id = /*FLD.RDB$CHARACTER_SET_ID*/
				      fb_89.fb_110;

		SSHORT collation = 0;
		if (!/*RFR.RDB$COLLATION_ID.NULL*/
		     fb_89.fb_107)
			collation = /*RFR.RDB$COLLATION_ID*/
				    fb_89.fb_108;
		else if (!/*FLD.RDB$COLLATION_ID.NULL*/
			  fb_89.fb_105)
			collation = /*FLD.RDB$COLLATION_ID*/
				    fb_89.fb_106;

		if ((/*FLD.RDB$FIELD_TYPE*/
		     fb_89.fb_114 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_89.fb_114 == blr_varying)) {
			isqlGlob.printf("(%d)", ISQL_get_field_length(/*FLD.RDB$FIELD_NAME*/
								      fb_89.fb_121));

			if (/*FLD.RDB$FIELD_SUB_TYPE*/
			    fb_89.fb_113 != fb_text_subtype_binary)
			{
				// Show international character sets and collations
				show_charsets(char_set_id, collation);
			}
		}

		if (/*FLD.RDB$FIELD_TYPE*/
		    fb_89.fb_114 == blr_blob)
		{
			isqlGlob.printf(" segment %u, subtype ", (USHORT) /*FLD.RDB$SEGMENT_LENGTH*/
									  fb_89.fb_104);
			const int subtype  = /*FLD.RDB$FIELD_SUB_TYPE*/
					     fb_89.fb_113;
			if (subtype >= 0 && subtype <= MAX_BLOBSUBTYPES)
			{
				isqlGlob.prints(Sub_types[subtype]);
			}
			else
			{
				isqlGlob.printf("%d", subtype);
			}

			if (subtype == isc_blob_text)
			{
				// Show international character sets and collations
				show_charsets(char_set_id, collation);
			}
		}

		if (!/*FLD.RDB$COMPUTED_BLR.NULL*/
		     fb_89.fb_116)
		{
			// A view expression. Other computed fields will not reach this point.
			isqlGlob.printf(" Expression%s", NEWLINE);
			continue;
		}

		// The null flag is either 1 or null (for nullable)

		if (/*RFR.RDB$NULL_FLAG*/
		    fb_89.fb_103 == 1 || /*FLD.RDB$NULL_FLAG*/
	 fb_89.fb_102 == 1 ||
			(!/*RFR.RDB$BASE_FIELD.NULL*/
			  fb_89.fb_101 && !ISQL_get_null_flag (relation_name, /*RFR.RDB$FIELD_NAME*/
					fb_89.fb_120)))
		{
			isqlGlob.printf(" Not Null ");
		}
		else
		{
			isqlGlob.printf(" Nullable ");
		}

		if (!/*RFR.RDB$GENERATOR_NAME.NULL*/
		     fb_89.fb_100)
		{
			isqlGlob.printf("Identity (%s)",
				(/*RFR.RDB$IDENTITY_TYPE*/
				 fb_89.fb_99 == IDENT_TYPE_BY_DEFAULT ? "by default" :
				 /*RFR.RDB$IDENTITY_TYPE*/
				 fb_89.fb_99 == IDENT_TYPE_ALWAYS ? "always" : ""));
		}

		// Handle defaults for columns

		if (!/*RFR.RDB$DEFAULT_SOURCE.NULL*/
		     fb_89.fb_98)
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*RFR.RDB$DEFAULT_SOURCE*/
								      fb_89.fb_92);
		else if (!/*FLD.RDB$DEFAULT_SOURCE.NULL*/
			  fb_89.fb_97)
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
								      fb_89.fb_91);
		isqlGlob.printf(NEWLINE);

		// Validation clause for domains
		if (!/*FLD.RDB$VALIDATION_SOURCE.NULL*/
		     fb_89.fb_96)
		{
			isqlGlob.printf("                                ");
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*FLD.RDB$VALIDATION_SOURCE*/
								      fb_89.fb_90);
			isqlGlob.printf(NEWLINE);
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	// If this is a view and there were columns, print the view text

	if (!first)
	{
		/*FOR REL IN RDB$RELATIONS WITH
			REL.RDB$RELATION_NAME EQ relation_name AND
			REL.RDB$VIEW_BLR NOT MISSING*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_77 && fbTrans && DB)
		      fb_77 = DB->compileRequest(fbStatus, sizeof(fb_78), fb_78);
		   isc_vtov ((const char*) relation_name, (char*) fb_79.fb_80, 253);
		   if (fbTrans && fb_77)
		      fb_77->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_79));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_77->release(); fb_77 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_77->receive (fbStatus, 0, 1, 12, CAST_MSG(&fb_81));
		   if (!fb_81.fb_83 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			isqlGlob.printf("View Source:%s==== ======%s", NEWLINE, NEWLINE);
			if (!/*REL.RDB$VIEW_SOURCE.NULL*/
			     fb_81.fb_84)
				SHOW_print_metadata_text_blob (isqlGlob.Out, &/*REL.RDB$VIEW_SOURCE*/
									      fb_81.fb_82);
			isqlGlob.printf(NEWLINE);
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}
	}

	// Handle any referential or primary constraint on this table

	SCHAR collist[BUFFER_LENGTH512];

	// Static queries for obtaining referential constraints

	/*FOR RELC1 IN RDB$RELATION_CONSTRAINTS WITH
		RELC1.RDB$RELATION_NAME EQ relation_name
		SORTED BY RELC1.RDB$CONSTRAINT_TYPE, RELC1.RDB$CONSTRAINT_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_68 && fbTrans && DB)
	      fb_68 = DB->compileRequest(fbStatus, sizeof(fb_69), fb_69);
	   isc_vtov ((const char*) relation_name, (char*) fb_70.fb_71, 253);
	   if (fbTrans && fb_68)
	      fb_68->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_70));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_68->release(); fb_68 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_68->receive (fbStatus, 0, 1, 520, CAST_MSG(&fb_72));
	   if (!fb_72.fb_74 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*RELC1.RDB$CONSTRAINT_NAME*/
				     fb_72.fb_75);
		fb_utils::exact_name(/*RELC1.RDB$INDEX_NAME*/
				     fb_72.fb_76);
		ISQL_get_index_segments (collist, sizeof(collist), /*RELC1.RDB$INDEX_NAME*/
								   fb_72.fb_76, false);
		bool isPK = false;
		bool isUK = false;

		if (!strncmp (/*RELC1.RDB$CONSTRAINT_TYPE*/
			      fb_72.fb_73, "PRIMARY", 7))
		{
			isPK = true;
			isqlGlob.printf("CONSTRAINT %s:%s", /*RELC1.RDB$CONSTRAINT_NAME*/
							    fb_72.fb_75, NEWLINE);
			isqlGlob.printf("  Primary key (%s)", collist);
		}
		else if (!strncmp (/*RELC1.RDB$CONSTRAINT_TYPE*/
				   fb_72.fb_73, "UNIQUE", 6))
		{
			isUK = true;
			isqlGlob.printf("CONSTRAINT %s:%s", /*RELC1.RDB$CONSTRAINT_NAME*/
							    fb_72.fb_75, NEWLINE);
			isqlGlob.printf("  Unique key (%s)", collist);
		}
		else if (!strncmp (/*RELC1.RDB$CONSTRAINT_TYPE*/
				   fb_72.fb_73, "FOREIGN", 7))
		{
			isqlGlob.printf("CONSTRAINT %s:%s", /*RELC1.RDB$CONSTRAINT_NAME*/
							    fb_72.fb_75, NEWLINE);
			isqlGlob.printf("  Foreign key (%s)", collist);

			/*FOR RELC2 IN RDB$RELATION_CONSTRAINTS CROSS
				REFC IN RDB$REF_CONSTRAINTS WITH
				RELC2.RDB$CONSTRAINT_NAME EQ REFC.RDB$CONST_NAME_UQ AND
				REFC.RDB$CONSTRAINT_NAME EQ RELC1.RDB$CONSTRAINT_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_56 && fbTrans && DB)
			      fb_56 = DB->compileRequest(fbStatus, sizeof(fb_57), fb_57);
			   isc_vtov ((const char*) fb_72.fb_75, (char*) fb_58.fb_59, 253);
			   if (fbTrans && fb_56)
			      fb_56->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_58));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_56->release(); fb_56 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_56->receive (fbStatus, 0, 1, 536, CAST_MSG(&fb_60));
			   if (!fb_60.fb_63 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				ISQL_get_index_segments (collist, sizeof(collist), /*RELC2.RDB$INDEX_NAME*/
										   fb_60.fb_66, false);
				fb_utils::exact_name(/*RELC2.RDB$RELATION_NAME*/
						     fb_60.fb_67);

				isqlGlob.printf("    References %s (%s)", /*RELC2.RDB$RELATION_NAME*/
									  fb_60.fb_67, collist);

				if (!/*REFC.RDB$UPDATE_RULE.NULL*/
				     fb_60.fb_65)
				{
					IUTILS_truncate_term (/*REFC.RDB$UPDATE_RULE*/
							      fb_60.fb_62, static_cast<USHORT>(strlen(/*REFC.RDB$UPDATE_RULE*/
			     fb_60.fb_62)));
					ISQL_ri_action_print (/*REFC.RDB$UPDATE_RULE*/
							      fb_60.fb_62, " On Update", false);
				}

				if (!/*REFC.RDB$DELETE_RULE.NULL*/
				     fb_60.fb_64)
				{
					IUTILS_truncate_term (/*REFC.RDB$DELETE_RULE*/
							      fb_60.fb_61, static_cast<USHORT>(strlen(/*REFC.RDB$DELETE_RULE*/
			     fb_60.fb_61)));
					ISQL_ri_action_print (/*REFC.RDB$DELETE_RULE*/
							      fb_60.fb_61, " On Delete", false);
				}

				isqlGlob.printf(NEWLINE);

			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg (fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}
		}

		if (isPK || isUK) // Special handling for PRIMARY KEY and UNIQUE constraints.
		{
			/*FOR IDX IN RDB$INDICES
			WITH IDX.RDB$INDEX_NAME = RELC1.RDB$INDEX_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_47 && fbTrans && DB)
			      fb_47 = DB->compileRequest(fbStatus, sizeof(fb_48), fb_48);
			   isc_vtov ((const char*) fb_72.fb_76, (char*) fb_49.fb_50, 253);
			   if (fbTrans && fb_47)
			      fb_47->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_49));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_47->release(); fb_47 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_47->receive (fbStatus, 0, 1, 259, CAST_MSG(&fb_51));
			   if (!fb_51.fb_52 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
				// Yes, the same RDB$... naming convention is used for both domains and indices.
				const bool explicit_index =
					((isPK && !fb_utils::implicit_pk(/*IDX.RDB$INDEX_NAME*/
									 fb_51.fb_55)) ||
					(isUK && !fb_utils::implicit_domain(/*RELC1.RDB$INDEX_NAME*/
									    fb_72.fb_76))) &&
				    strcmp(/*RELC1.RDB$CONSTRAINT_NAME*/
					   fb_72.fb_75, /*RELC1.RDB$INDEX_NAME*/
  fb_72.fb_76);
				const bool descending_index = !/*IDX.RDB$INDEX_TYPE.NULL*/
							       fb_51.fb_53 && /*IDX.RDB$INDEX_TYPE*/
    fb_51.fb_54 == 1;
				if (explicit_index || descending_index)
				{
					isqlGlob.printf(" uses explicit %s index",
									descending_index ? "descending" : "ascending");
				}
				if (explicit_index)
					isqlGlob.printf(" %s", /*RELC1.RDB$INDEX_NAME*/
							       fb_72.fb_76);

				isqlGlob.prints(NEWLINE);
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg (fbStatus);
				return ps_ERR;
			/*END_ERROR*/
			   }
			}
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR*/
	   }
	}

	/*FOR R_C IN RDB$RELATION_CONSTRAINTS CROSS
		C_C IN RDB$CHECK_CONSTRAINTS
		WITH R_C.RDB$RELATION_NAME   EQ relation_name
		AND R_C.RDB$CONSTRAINT_TYPE EQ 'NOT NULL'
		AND R_C.RDB$CONSTRAINT_NAME EQ C_C.RDB$CONSTRAINT_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_39 && fbTrans && DB)
	      fb_39 = DB->compileRequest(fbStatus, sizeof(fb_40), fb_40);
	   isc_vtov ((const char*) relation_name, (char*) fb_41.fb_42, 253);
	   if (fbTrans && fb_39)
	      fb_39->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_41));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_39->release(); fb_39 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_39->receive (fbStatus, 0, 1, 508, CAST_MSG(&fb_43));
	   if (!fb_43.fb_44 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!fb_utils::implicit_integrity(/*R_C.RDB$CONSTRAINT_NAME*/
						  fb_43.fb_45))
		{
			fb_utils::exact_name(/*C_C.RDB$TRIGGER_NAME*/
					     fb_43.fb_46);
			fb_utils::exact_name(/*R_C.RDB$CONSTRAINT_NAME*/
					     fb_43.fb_45);
			isqlGlob.printf("CONSTRAINT %s:%s", /*R_C.RDB$CONSTRAINT_NAME*/
							    fb_43.fb_45, NEWLINE);
			isqlGlob.printf("  Not Null Column (%s)%s", /*C_C.RDB$TRIGGER_NAME*/
								    fb_43.fb_46, NEWLINE);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}

	// Do check constraints

	show_check(relation_name);

	// Do triggers

	show_trigger(relation_name, false, false);

	// Do publications

	show_pub_table(relation_name);

	if (first)
		return (OBJECT_NOT_FOUND);
	return SKIP;
}


static processing_state show_trigger(const SCHAR* object, bool show_source, bool isTriggerName)
{
   struct fb_5_struct {
          ISC_QUAD fb_6;	/* RDB$TRIGGER_SOURCE */
          char  fb_7 [256];	/* RDB$ENTRYPOINT */
          ISC_INT64 fb_8;	/* RDB$TRIGGER_TYPE */
          short fb_9;	/* fbUtility */
          short fb_10;	/* gds__null_flag */
          short fb_11;	/* gds__null_flag */
          short fb_12;	/* gds__null_flag */
          short fb_13;	/* RDB$TRIGGER_INACTIVE */
          short fb_14;	/* RDB$TRIGGER_SEQUENCE */
          short fb_15;	/* gds__null_flag */
          short fb_16;	/* gds__null_flag */
          char  fb_17 [253];	/* RDB$TRIGGER_NAME */
          char  fb_18 [253];	/* RDB$RELATION_NAME */
          FB_BOOLEAN fb_19;	/* RDB$SQL_SECURITY */
          char  fb_20 [253];	/* RDB$ENGINE_NAME */
   } fb_5;
   struct fb_2_struct {
          char  fb_3 [253];	/* RDB$RELATION_NAME */
          char  fb_4 [253];	/* RDB$TRIGGER_NAME */
   } fb_2;
   struct fb_23_struct {
          short fb_24;	/* fbUtility */
          short fb_25;	/* gds__null_flag */
          short fb_26;	/* RDB$VALID_BLR */
          short fb_27;	/* RDB$SYSTEM_FLAG */
          short fb_28;	/* gds__null_flag */
          char  fb_29 [253];	/* RDB$TRIGGER_NAME */
          char  fb_30 [253];	/* RDB$RELATION_NAME */
   } fb_23;
   struct fb_33_struct {
          short fb_34;	/* fbUtility */
          short fb_35;	/* gds__null_flag */
          short fb_36;	/* RDB$VALID_BLR */
          short fb_37;	/* RDB$SYSTEM_FLAG */
          char  fb_38 [253];	/* RDB$TRIGGER_NAME */
   } fb_33;
/**************************************
 *
 *	s h o w _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Show triggers in general or for the named object or trigger
 *
 **************************************/
	bool first = true;

	// Show all triggers
	if (!*object)
	{
		bool has_dbtrig = false;

		if (ENCODE_ODS(isqlGlob.major_ods, isqlGlob.minor_ods) >= ODS_11_1)
		{
			/*FOR TRG IN RDB$TRIGGERS
				WITH (TRG.RDB$SYSTEM_FLAG EQ 0 OR TRG.RDB$SYSTEM_FLAG MISSING) AND
					 TRG.RDB$RELATION_NAME MISSING
				SORTED BY TRG.RDB$TRIGGER_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_31 && fbTrans && DB)
			      fb_31 = DB->compileRequest(fbStatus, sizeof(fb_32), fb_32);
			   if (fbTrans && fb_31)
			      fb_31->start(fbStatus, fbTrans, 0);
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_31->release(); fb_31 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_31->receive (fbStatus, 0, 0, 261, CAST_MSG(&fb_33));
			   if (!fb_33.fb_34 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			{
				if (!has_dbtrig)
					has_dbtrig = true;

				fb_utils::exact_name(/*TRG.RDB$TRIGGER_NAME*/
						     fb_33.fb_38);

				isqlGlob.printf("%s", /*TRG.RDB$TRIGGER_NAME*/
						      fb_33.fb_38);

				if (/*TRG.RDB$SYSTEM_FLAG*/
				    fb_33.fb_37 == 1)
					isqlGlob.printf("; System");

				if (!(/*TRG.RDB$VALID_BLR.NULL*/
				      fb_33.fb_35 || /*TRG.RDB$VALID_BLR*/
    fb_33.fb_36))
					isqlGlob.printf("; Invalid");

				isqlGlob.printf("%s", NEWLINE);
			}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg(fbStatus);
				return ps_ERR;
			/*END_ERROR;*/
			   }
			}
		}

		/*FOR TRG IN RDB$TRIGGERS CROSS REL IN RDB$RELATIONS
			//WITH (REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
			//NOT (ANY CHK IN RDB$CHECK_CONSTRAINTS WITH
			//	 TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME)
			WITH (TRG.RDB$SYSTEM_FLAG EQ 0 OR TRG.RDB$SYSTEM_FLAG MISSING) AND
			TRG.RDB$RELATION_NAME = REL.RDB$RELATION_NAME
			SORTED BY TRG.RDB$RELATION_NAME, TRG.RDB$TRIGGER_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_21 && fbTrans && DB)
		      fb_21 = DB->compileRequest(fbStatus, sizeof(fb_22), fb_22);
		   if (fbTrans && fb_21)
		      fb_21->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_21->release(); fb_21 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_21->receive (fbStatus, 0, 0, 516, CAST_MSG(&fb_23));
		   if (!fb_23.fb_24 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		{
			if (first)
			{
				if (has_dbtrig)
					isqlGlob.printf("%s", NEWLINE);

				first = false;
			}

			fb_utils::exact_name(/*TRG.RDB$TRIGGER_NAME*/
					     fb_23.fb_29);
			fb_utils::exact_name(/*TRG.RDB$RELATION_NAME*/
					     fb_23.fb_30);

			isqlGlob.printf("%s", /*TRG.RDB$TRIGGER_NAME*/
					      fb_23.fb_29);

			if (!/*TRG.RDB$RELATION_NAME.NULL*/
			     fb_23.fb_28)
				isqlGlob.printf("; Table: %s", /*TRG.RDB$RELATION_NAME*/
							       fb_23.fb_30);

			if (/*TRG.RDB$SYSTEM_FLAG*/
			    fb_23.fb_27 == 1)
				isqlGlob.printf("; System");

			if (!(/*TRG.RDB$VALID_BLR.NULL*/
			      fb_23.fb_25 || /*TRG.RDB$VALID_BLR*/
    fb_23.fb_26))
				isqlGlob.printf("; Invalid");

			isqlGlob.printf("%s", NEWLINE);
		}
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return ps_ERR;
		/*END_ERROR;*/
		   }
		}

		if (first && !has_dbtrig)
			return OBJECT_NOT_FOUND;

		return (SKIP);
	}

	// Show triggers for the named object
	// and avoid check constraints
	/*BASED_ON RDB$TRIGGERS.RDB$TRIGGER_NAME triggerName;*/
	char
	   triggerName[253];

	/*BASED_ON RDB$TRIGGERS.RDB$RELATION_NAME relationName;*/
	char
	   relationName[253];


	if (isTriggerName)
	{
		sprintf(triggerName, "%s", object);
		relationName[0] = '\0';
	}
	else
	{
		sprintf(relationName, "%s", object);
		triggerName[0] = '\0';
	}

	/*FOR TRG IN RDB$TRIGGERS WITH
		(TRG.RDB$RELATION_NAME EQ relationName OR
			TRG.RDB$TRIGGER_NAME EQ triggerName) AND
		(TRG.RDB$SYSTEM_FLAG EQ 0 OR TRG.RDB$SYSTEM_FLAG MISSING)
		SORTED BY TRG.RDB$RELATION_NAME, TRG.RDB$TRIGGER_TYPE,
		TRG.RDB$TRIGGER_SEQUENCE, TRG.RDB$TRIGGER_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_0 && fbTrans && DB)
	      fb_0 = DB->compileRequest(fbStatus, sizeof(fb_1), fb_1);
	   isc_vtov ((const char*) relationName, (char*) fb_2.fb_3, 253);
	   isc_vtov ((const char*) triggerName, (char*) fb_2.fb_4, 253);
	   if (fbTrans && fb_0)
	      fb_0->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_2));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_0->release(); fb_0 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_0->receive (fbStatus, 0, 1, 1048, CAST_MSG(&fb_5));
	   if (!fb_5.fb_9 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		//bool skip = false;
		// Skip triggers for check constraints
		//FOR FIRST 1 CHK IN RDB$CHECK_CONSTRAINTS WITH
		//	TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME
		//	skip = true;
		//END_FOR
		//ON_ERROR
		//	ISQL_errmsg (fbStatus);
		//	return ps_ERR;
		//END_ERROR;

		//if (skip)
		//	continue;
		fb_utils::exact_name(/*TRG.RDB$TRIGGER_NAME*/
				     fb_5.fb_17);

		if (first)
		{
			if (!/*TRG.RDB$RELATION_NAME.NULL*/
			     fb_5.fb_16)
			{
				fb_utils::exact_name(/*TRG.RDB$RELATION_NAME*/
						     fb_5.fb_18);

				isqlGlob.printf("%sTriggers on Table %s:%s", NEWLINE, /*TRG.RDB$RELATION_NAME*/
										      fb_5.fb_18, NEWLINE);
			}

			first = false;
		}

		const char* ss = /*TRG.RDB$SQL_SECURITY.NULL*/
				 fb_5.fb_15 ? "" :
							(/*TRG.RDB$SQL_SECURITY*/
							 fb_5.fb_19 ? ", SQL SECURITY DEFINER" : ", SQL SECURITY INVOKER");

		isqlGlob.printf("%s, Sequence: %d, Type: %s, %s%s%s",
				 /*TRG.RDB$TRIGGER_NAME*/
				 fb_5.fb_17,
				 /*TRG.RDB$TRIGGER_SEQUENCE*/
				 fb_5.fb_14,
				 SHOW_trigger_action(/*TRG.RDB$TRIGGER_TYPE*/
						     fb_5.fb_8).c_str(),
				 (/*TRG.RDB$TRIGGER_INACTIVE*/
				  fb_5.fb_13 ? "Inactive" : "Active"),
				 ss,
				 NEWLINE);

		if (show_source)
		{
			if (!/*TRG.RDB$ENTRYPOINT.NULL*/
			     fb_5.fb_12)
			{
				fb_utils::exact_name(/*TRG.RDB$ENTRYPOINT*/
						     fb_5.fb_7);
				isqlGlob.printf("External name: %s%s", /*TRG.RDB$ENTRYPOINT*/
								       fb_5.fb_7, NEWLINE);
			}

			if (!/*TRG.RDB$ENGINE_NAME.NULL*/
			     fb_5.fb_11)
			{
				fb_utils::exact_name(/*TRG.RDB$ENGINE_NAME*/
						     fb_5.fb_20);
				isqlGlob.printf("Engine: %s%s", /*TRG.RDB$ENGINE_NAME*/
								fb_5.fb_20, NEWLINE);
			}

			// Use print_blob to print the blob

			if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
			     fb_5.fb_10)
			{
				isqlGlob.printf("Trigger text:%s", NEWLINE);
				isqlGlob.printf("=============================================================================%s", NEWLINE);
				SHOW_print_metadata_text_blob (isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
									      fb_5.fb_6);
				isqlGlob.printf("%s=============================================================================%s", NEWLINE, NEWLINE);
			}
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return ps_ERR;
	/*END_ERROR;*/
	   }
	}
	if (first)
		return OBJECT_NOT_FOUND;
	return SKIP;
}

static processing_state show_users12()
{
/**************************************
 *
 *	s h o w _ u s e r s 1 2
 *
 **************************************
 *
 * Functional description
 *	Show users logged in and/or defined for the database.
 *
 **************************************/

	const char* getusers = "select "
		"case when coalesce(MON$USER, SEC$USER_NAME) = current_user then '#' "
			 "when SEC$USER_NAME is distinct from null then ' ' else '-' end, "
		"coalesce(MON$USER, SEC$USER_NAME), count(MON$USER) "
		"from mon$attachments m full join sec$users u on m.MON$USER = u.SEC$USER_NAME "
		"where coalesce(MON$SYSTEM_FLAG, 0) = 0 "
		"group by MON$USER, SEC$USER_NAME order by coalesce(MON$USER, SEC$USER_NAME)";

	Firebird::RefPtr<Firebird::IMetadataBuilder>
		mb(Firebird::REF_NO_INCR, fbMaster->getMetadataBuilder(fbStatus, 3));
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	mb->setType(fbStatus, 0, SQL_TEXT);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	mb->setLength(fbStatus, 0, 1);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	mb->setType(fbStatus, 1, SQL_VARYING);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	mb->setLength(fbStatus, 1, MAX_SQL_IDENTIFIER_SIZE);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	mb->setType(fbStatus, 2, SQL_LONG);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::RefPtr<Firebird::IMessageMetadata>
		outMetadata(Firebird::REF_NO_INCR, mb->getMetadata(fbStatus));
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::AlignedBuffer<MAX_SQL_IDENTIFIER_SIZE + 32> outBuffer;
	fb_assert(outMetadata->getMessageLength(fbStatus) <= outBuffer.size());

	char* mark = (char*) &outBuffer[outMetadata->getOffset(fbStatus, 0)];
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	vary* username = (vary*) &outBuffer[outMetadata->getOffset(fbStatus, 1)];
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	int* cnt = (int*) &outBuffer[outMetadata->getOffset(fbStatus, 2)];
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::IResultSet* rs = DB->openCursor(fbStatus, fbTrans, 0, getusers, isqlGlob.SQL_dialect,
		NULL, NULL, outMetadata, NULL, 0);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	processing_state rc = OBJECT_NOT_FOUND;

	while (rs->fetchNext(fbStatus, outBuffer) == Firebird::IStatus::RESULT_OK)
	{
		if (rc == OBJECT_NOT_FOUND)
		{
			// First time in the loop, print title.
			TEXT msg[MSG_LENGTH]; // When messages can be translated.
			IUTILS_msg_get(USERS_IN_DB, msg);
			isqlGlob.printf("%s\n", msg);
			rc = SKIP; // We found at least one user.
		}

		isqlGlob.printf("%3d %c%.*s", *cnt, *mark, username->vary_length, username->vary_string);
		isqlGlob.printf("\n");
	}
	if (ISQL_errmsg (fbStatus))
		rc = ps_ERR;

	rs->release();

	return rc;
}

static processing_state show_users()
{
/**************************************
 *
 *	s h o w _ u s e r s
 *
 **************************************
 *
 * Functional description
 *	Show users logged in the database (not users defined for this database).
 *
 **************************************/

	if (isqlGlob.major_ods >= ODS_VERSION12)
		return show_users12();

	const UCHAR user_items[] =
	{
		isc_info_user_names,
		isc_info_end
	};

	TEXT msg[MSG_LENGTH]; // When messages can be translated.

	Firebird::RefPtr<Firebird::IMetadataBuilder>
		mb(Firebird::REF_NO_INCR, fbMaster->getMetadataBuilder(fbStatus, 1));
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	mb->setType(fbStatus, 0, SQL_VARYING);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	mb->setLength(fbStatus, 0, MAX_SQL_IDENTIFIER_SIZE);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::RefPtr<Firebird::IMessageMetadata>
		outMetadata(Firebird::REF_NO_INCR, mb->getMetadata(fbStatus));
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;

	Firebird::AlignedBuffer<MAX_SQL_IDENTIFIER_SIZE + 32> outBuffer;
	fb_assert(outMetadata->getMessageLength(fbStatus) <= outBuffer.size());
	unsigned off = outMetadata->getOffset(fbStatus, 0);
	if (ISQL_errmsg (fbStatus))
		return ps_ERR;
	vary* my_user = (vary*) &outBuffer[off];

	const char* getuser = isqlGlob.major_ods < ODS_VERSION10 ?
	    "select user from rdb$database" : "select current_user from rdb$database";
	DB->execute(fbStatus, fbTrans, 0, getuser, isqlGlob.SQL_dialect,
		NULL, NULL, outMetadata, outBuffer);
	if (ISQL_errmsg (fbStatus))
		return OBJECT_NOT_FOUND;
	my_user->vary_string[my_user->vary_length] = 0; // Just for the debugger.

	UCHAR buffer[BUFFER_LENGTH512];
	DB->getInfo(fbStatus, sizeof(user_items), user_items, sizeof(buffer), buffer);
	if (ISQL_errmsg(fbStatus))
		return ps_ERR;

	processing_state rc = OBJECT_NOT_FOUND;

	for (Firebird::ClumpletReader p(Firebird::ClumpletReader::InfoResponse, buffer, sizeof(buffer)); !p.isEof(); p.moveNext())
	{
		switch (p.getClumpTag())
		{
		case isc_info_user_names:
			{
				if (rc == OBJECT_NOT_FOUND)
				{
					// First time in the loop, print title.
					IUTILS_msg_get(USERS_IN_DB, msg);
					isqlGlob.printf("%s\n", msg);
					rc = SKIP; // We found at least one user.
				}

				unsigned len = p.getBytes()[0];
				fb_assert(len == p.getClumpLength() - 1);
				const UCHAR* uname = p.getBytes() + 1;
				// Let's mark all attachments with our same user with a # prefix.
				bool same(len == my_user->vary_length && !memcmp(my_user->vary_string, uname, len));
				isqlGlob.printf("%c %.*s", same ? '#' : ' ', len, uname);
				isqlGlob.printf("\n");
			}
			break;

		case isc_info_truncated:
			isqlGlob.printf("\n");

			IUTILS_msg_get(OUTPUT_TRUNCATED, msg);
			isqlGlob.printf("%s\n", msg);
			return rc; // If we got some items, we are (partially) successful.
		}
	}

	return rc;
}

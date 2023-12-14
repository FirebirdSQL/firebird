/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/*********** Preprocessed module -- do not edit ***************/
/***************** gpre version WI-T6.0.0.179-dev Firebird 6.0 Initial **********************/
/*
 *	PROGRAM:	Interactive SQL utility
 *	MODULE:		extract.epp
 *	DESCRIPTION:	Definition extract routines
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
 * Revision 1.3  2000/11/22 17:07:25  patrickgriffin
 * In get_procedure_args change comment style from // to c style
 *
 * ...pat
 *
 * 2001.09.09 Claudio Valderrama: procedure's parameter names may need
 *   double quotes if they are in dialect 3 and have special characters.
 * 2001.09.21 Claudio Valderrama: Show correct mechanism for UDF parameters
 *   and support the RETURNS PARAMETER <n> syntax.
 * 2001.10.01 Claudio Valderrama: list_all_grants2() and EXTRACT_list_grants()
 *   to better organize the code that should be called to handle SHOW GRANTS.
 *
 * Revision 1.2  2000/11/18 16:49:24  fsg
 * Increased PRINT_BUFFER_LENGTH to 2048 to show larger plans
 * Fixed Bug #122563 in extract.e get_procedure_args
 * Apparently this has to be done in show.e also,
 * but that is for another day :-)
 *
 * 2003.02.04 Dmitry Yemanov: support for universal triggers
 */

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>				// isdigit
#include "../jrd/constants.h"
#include "ibase.h"
#include "../yvalve/gds_proto.h"
#include "../common/intlobj_new.h"
#include "../isql/isql.h"
#include "../isql/extra_proto.h"
#include "../isql/isql_proto.h"
#include "../isql/show_proto.h"
#include "../isql/iutils_proto.h"
#include "../jrd/ini.h"
#include "../jrd/obj.h"
#include "../jrd/ods.h"
#include "../common/utils_proto.h"
#include "../jrd/constants.h"

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
   fb_1l = 88;
static const unsigned char
   fb_1 [] = {
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
		     blr_rid, 5,0, 0, 
		     blr_boolean, 
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
   };	/* end of blr string for request fb_1 */

static Firebird::IRequest*
   fb_7 = 0;		/* request handle */

static const unsigned
   fb_8l = 140;
static const unsigned char
   fb_8 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
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
			   blr_or, 
			      blr_neq, 
				 blr_fid, 0, 4,0, 
				 blr_literal, blr_long, 0, 1,0,0,0,
			      blr_missing, 
				 blr_fid, 0, 4,0, 
			   blr_and, 
			      blr_not, 
				 blr_missing, 
				    blr_fid, 0, 0,0, 
			      blr_eql, 
				 blr_fid, 0, 15,0, 
				 blr_parameter, 0, 0,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 3,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter2, 1, 0,0, 2,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 8,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 13,0, 
			   blr_parameter, 1, 4,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_8 */

static Firebird::IRequest*
   fb_17 = 0;		/* request handle */

static const unsigned
   fb_18l = 129;
static const unsigned char
   fb_18 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 7,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 42,0, 0, 
		  blr_boolean, 
		     blr_or, 
			blr_neq, 
			   blr_fid, 0, 6,0, 
			   blr_literal, blr_long, 0, 1,0,0,0,
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
			blr_parameter2, 0, 0,0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter2, 0, 6,0, 3,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_18 */

static Firebird::IRequest*
   fb_27 = 0;		/* request handle */

static const unsigned
   fb_28l = 133;
static const unsigned char
   fb_28 [] = {
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
		  blr_rid, 42,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 6,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 6,0, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 2,0, 
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
			blr_fid, 0, 3,0, 
			blr_parameter2, 0, 3,0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_28 */

static Firebird::IRequest*
   fb_36 = 0;		/* request handle */

static const unsigned
   fb_37l = 87;
static const unsigned char
   fb_37 [] = {
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
   };	/* end of blr string for request fb_37 */

static Firebird::IRequest*
   fb_43 = 0;		/* request handle */

static const unsigned
   fb_44l = 191;
static const unsigned char
   fb_44 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 9,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
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
			blr_and, 
			   blr_or, 
			      blr_neq, 
				 blr_fid, 1, 4,0, 
				 blr_literal, blr_long, 0, 1,0,0,0,
			      blr_missing, 
				 blr_fid, 1, 4,0, 
			   blr_not, 
			      blr_any, 
				 blr_rse, 1, 
				    blr_rid, 22,0, 2, 
				    blr_boolean, 
				       blr_eql, 
					  blr_fid, 2, 5,0, 
					  blr_fid, 0, 0,0, 
				    blr_end, 
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
			blr_fid, 0, 7,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 8,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_44 */

static Firebird::IRequest*
   fb_55 = 0;		/* request handle */

static const unsigned
   fb_56l = 95;
static const unsigned char
   fb_56 [] = {
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
   };	/* end of blr string for request fb_56 */

static Firebird::IRequest*
   fb_64 = 0;		/* request handle */

static const unsigned
   fb_65l = 158;
static const unsigned char
   fb_65 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 20,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_matching2, 
			      blr_fid, 0, 0,0, 
			      blr_literal, blr_text, 5,0, 'R','D','B','$',43,
			      blr_literal, blr_text, 15,0, 43,61,91,'0',45,'9',93,91,'0',45,'9',93,42,32,42,
			blr_and, 
			   blr_not, 
			      blr_matching2, 
				 blr_fid, 0, 0,0, 
				 blr_literal, blr_text, 5,0, 'S','Q','L','$',43,
				 blr_literal, blr_text, 15,0, 43,61,91,'0',45,'9',93,91,'0',45,'9',93,42,32,42,
			   blr_or, 
			      blr_missing, 
				 blr_fid, 0, 2,0, 
			      blr_neq, 
				 blr_fid, 0, 2,0, 
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
   };	/* end of blr string for request fb_65 */

static Firebird::IRequest*
   fb_69 = 0;		/* request handle */

static const unsigned
   fb_70l = 189;
static const unsigned char
   fb_70 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 13,0, 
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
	    blr_cstring, 253,0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 14,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 7,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 7,0, 
			blr_and, 
			   blr_missing, 
			      blr_fid, 0, 9,0, 
			   blr_missing, 
			      blr_fid, 0, 4,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 11,0, 
			blr_parameter2, 0, 0,0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter2, 0, 1,0, 6,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 19,0, 
			blr_parameter2, 0, 8,0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 9,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 10,0, 
		     blr_assignment, 
			blr_fid, 0, 20,0, 
			blr_parameter2, 0, 11,0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter2, 0, 12,0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_70 */

static Firebird::IRequest*
   fb_85 = 0;		/* request handle */

static const unsigned
   fb_86l = 126;
static const unsigned char
   fb_86 [] = {
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
		  blr_rid, 14,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 7,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 7,0, 
			blr_and, 
			   blr_missing, 
			      blr_fid, 0, 9,0, 
			   blr_missing, 
			      blr_fid, 0, 4,0, 
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
			blr_fid, 0, 19,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
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
   };	/* end of blr string for request fb_86 */

static Firebird::IRequest*
   fb_93 = 0;		/* request handle */

static const unsigned
   fb_94l = 91;
static const unsigned char
   fb_94 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 3,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 28,0, 0, 
		     blr_boolean, 
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
			   blr_fid, 0, 8,0, 
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
   };	/* end of blr string for request fb_94 */

static Firebird::IRequest*
   fb_101 = 0;		/* request handle */

static const unsigned
   fb_102l = 174;
static const unsigned char
   fb_102 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 10,0, 
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
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 15,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_parameter, 0, 0,0, 
			      blr_fid, 0, 0,0, 
			   blr_missing, 
			      blr_fid, 0, 10,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 1,0, 
			blr_assignment, 
			   blr_fid, 0, 2,0, 
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 4,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 6,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 8,0, 
			   blr_parameter2, 1, 6,0, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 7,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter, 1, 8,0, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter, 1, 9,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_102 */

static Firebird::IRequest*
   fb_116 = 0;		/* request handle */

static const unsigned
   fb_117l = 140;
static const unsigned char
   fb_117 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 5,0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 14,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 7,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 7,0, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 4,0, 
			   blr_missing, 
			      blr_fid, 0, 9,0, 
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
			blr_fid, 0, 4,0, 
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 6,0, 
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_117 */

static Firebird::IRequest*
   fb_124 = 0;		/* request handle */

static const unsigned
   fb_125l = 248;
static const unsigned char
   fb_125 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 11,0, 
	    blr_cstring2, 0,0, 12,0, 
	    blr_cstring2, 0,0, 12,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 3, 
		  blr_rid, 22,0, 0, 
		  blr_rid, 22,0, 1, 
		  blr_rid, 23,0, 2, 
		  blr_boolean, 
		     blr_and, 
			blr_eql, 
			   blr_fid, 0, 1,0, 
			   blr_literal, blr_text, 11,0, 'F','O','R','E','I','G','N',32,'K','E','Y',
			blr_and, 
			   blr_eql, 
			      blr_fid, 2, 1,0, 
			      blr_fid, 1, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 2, 0,0, 
				 blr_fid, 0, 0,0, 
			      blr_or, 
				 blr_eql, 
				    blr_fid, 1, 1,0, 
				    blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
				 blr_eql, 
				    blr_fid, 1, 1,0, 
				    blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 0, 2,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 2, 4,0, 
			blr_parameter2, 0, 0,0, 3,0, 
		     blr_assignment, 
			blr_fid, 2, 3,0, 
			blr_parameter2, 0, 1,0, 4,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter, 0, 6,0, 
		     blr_assignment, 
			blr_fid, 1, 2,0, 
			blr_parameter, 0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 8,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter2, 0, 9,0, 5,0, 
		     blr_assignment, 
			blr_fid, 1, 5,0, 
			blr_parameter, 0, 10,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_125 */

static Firebird::IRequest*
   fb_138 = 0;		/* request handle */

static const unsigned
   fb_139l = 119;
static const unsigned char
   fb_139 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 6,0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
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
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter, 0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_139 */

static Firebird::IRequest*
   fb_147 = 0;		/* request handle */

static const unsigned
   fb_148l = 83;
static const unsigned char
   fb_148 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_cstring2, 0,0, 0,4, 
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
   };	/* end of blr string for request fb_148 */

static Firebird::IRequest*
   fb_153 = 0;		/* request handle */

static const unsigned
   fb_154l = 357;
static const unsigned char
   fb_154 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 26,0, 
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
	 blr_begin, 
	    blr_for, 
	       blr_rse, 3, 
		  blr_rid, 6,0, 0, 
		  blr_rid, 5,0, 1, 
		  blr_rid, 2,0, 2, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
			blr_and, 
			   blr_missing, 
			      blr_fid, 0, 0,0, 
			   blr_and, 
			      blr_not, 
				 blr_missing, 
				    blr_fid, 2, 4,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 1, 2,0, 
				    blr_fid, 2, 0,0, 
				 blr_eql, 
				    blr_fid, 1, 1,0, 
				    blr_fid, 0, 8,0, 
		  blr_sort, 3, 
		     blr_ascending, 
			blr_fid, 0, 8,0, 
		     blr_ascending, 
			blr_fid, 1, 6,0, 
		     blr_ascending, 
			blr_fid, 1, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 2, 5,0, 
			blr_parameter2, 0, 0,0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 2, 25,0, 
			blr_parameter2, 0, 4,0, 3,0, 
		     blr_assignment, 
			blr_fid, 2, 17,0, 
			blr_parameter, 0, 5,0, 
		     blr_assignment, 
			blr_fid, 2, 22,0, 
			blr_parameter2, 0, 7,0, 6,0, 
		     blr_assignment, 
			blr_fid, 2, 24,0, 
			blr_parameter2, 0, 10,0, 9,0, 
		     blr_assignment, 
			blr_fid, 2, 8,0, 
			blr_parameter, 0, 11,0, 
		     blr_assignment, 
			blr_fid, 2, 9,0, 
			blr_parameter, 0, 12,0, 
		     blr_assignment, 
			blr_fid, 2, 27,0, 
			blr_parameter, 0, 13,0, 
		     blr_assignment, 
			blr_fid, 2, 11,0, 
			blr_parameter, 0, 14,0, 
		     blr_assignment, 
			blr_fid, 2, 26,0, 
			blr_parameter2, 0, 15,0, 8,0, 
		     blr_assignment, 
			blr_fid, 1, 18,0, 
			blr_parameter2, 0, 17,0, 16,0, 
		     blr_assignment, 
			blr_fid, 2, 10,0, 
			blr_parameter, 0, 18,0, 
		     blr_assignment, 
			blr_fid, 2, 15,0, 
			blr_parameter, 0, 19,0, 
		     blr_assignment, 
			blr_fid, 0, 15,0, 
			blr_parameter2, 0, 21,0, 20,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 22,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 23,0, 
		     blr_assignment, 
			blr_fid, 1, 0,0, 
			blr_parameter, 0, 24,0, 
		     blr_assignment, 
			blr_fid, 2, 0,0, 
			blr_parameter, 0, 25,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_154 */

static Firebird::IRequest*
   fb_182 = 0;		/* request handle */

static const unsigned
   fb_183l = 135;
static const unsigned char
   fb_183 [] = {
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
			   blr_neq, 
			      blr_fid, 0, 15,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 3,0, 
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
   };	/* end of blr string for request fb_183 */

static Firebird::IRequest*
   fb_188 = 0;		/* request handle */

static const unsigned
   fb_189l = 243;
static const unsigned char
   fb_189 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 16,0, 
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
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 0,0, 4,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 23,0, 
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 25,0, 
			blr_parameter2, 0, 5,0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 26,0, 
			blr_parameter2, 0, 7,0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 22,0, 
			blr_parameter2, 0, 9,0, 8,0, 
		     blr_assignment, 
			blr_fid, 0, 17,0, 
			blr_parameter, 0, 10,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 11,0, 
		     blr_assignment, 
			blr_fid, 0, 27,0, 
			blr_parameter, 0, 12,0, 
		     blr_assignment, 
			blr_fid, 0, 11,0, 
			blr_parameter, 0, 13,0, 
		     blr_assignment, 
			blr_fid, 0, 10,0, 
			blr_parameter, 0, 14,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 15,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_189 */

static Firebird::IRequest*
   fb_207 = 0;		/* request handle */

static const unsigned
   fb_208l = 258;
static const unsigned char
   fb_208 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 20,0, 
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
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 2,0, 0, 
		     blr_rid, 5,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 2,0, 
			      blr_fid, 0, 0,0, 
			   blr_eql, 
			      blr_fid, 1, 1,0, 
			      blr_parameter, 0, 0,0, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 0, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 0, 3,0, 
			   blr_parameter2, 1, 0,0, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter2, 1, 1,0, 6,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 23,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 25,0, 
			   blr_parameter2, 1, 7,0, 3,0, 
			blr_assignment, 
			   blr_fid, 0, 26,0, 
			   blr_parameter2, 1, 9,0, 8,0, 
			blr_assignment, 
			   blr_fid, 0, 22,0, 
			   blr_parameter2, 1, 11,0, 10,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 0, 9,0, 
			   blr_parameter, 1, 13,0, 
			blr_assignment, 
			   blr_fid, 0, 27,0, 
			   blr_parameter, 1, 14,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 15,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter, 1, 16,0, 
			blr_assignment, 
			   blr_fid, 0, 15,0, 
			   blr_parameter2, 1, 18,0, 17,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 19,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_208 */

static Firebird::IRequest*
   fb_232 = 0;		/* request handle */

static const unsigned
   fb_233l = 151;
static const unsigned char
   fb_233 [] = {
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
   };	/* end of blr string for request fb_233 */

static Firebird::IRequest*
   fb_247 = 0;		/* request handle */

static const unsigned
   fb_248l = 74;
static const unsigned char
   fb_248 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 1,0, 
	    blr_short, 0, 
	 blr_message, 0, 1,0, 
	    blr_bool, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 1, 
		     blr_rid, 1,0, 0, 
		     blr_boolean, 
			blr_and, 
			   blr_not, 
			      blr_missing, 
				 blr_fid, 0, 5,0, 
			   blr_eql, 
			      blr_fid, 0, 5,0, 
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
   };	/* end of blr string for request fb_248 */

static Firebird::IRequest*
   fb_253 = 0;		/* request handle */

static const unsigned
   fb_254l = 81;
static const unsigned char
   fb_254 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_long, 0, 
	    blr_short, 0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 1,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 4,0, 
			blr_gtr, 
			   blr_fid, 0, 4,0, 
			   blr_literal, blr_long, 0, 0,0,0,0,
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
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
   };	/* end of blr string for request fb_254 */

static Firebird::IRequest*
   fb_258 = 0;		/* request handle */

static const unsigned
   fb_259l = 80;
static const unsigned char
   fb_259 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 1,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_not, 
			   blr_missing, 
			      blr_fid, 0, 3,0, 
			blr_neq, 
			   blr_fid, 0, 3,0, 
			   blr_literal, blr_text, 1,0, 32,
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 0,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 1,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_259 */

static Firebird::IRequest*
   fb_263 = 0;		/* request handle */

static const unsigned
   fb_264l = 165;
static const unsigned char
   fb_264 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 9,0, 
	    blr_quad, 0, 
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
		     blr_and, 
			blr_eql, 
			   blr_fid, 1, 4,0, 
			   blr_fid, 0, 2,0, 
			blr_or, 
			   blr_missing, 
			      blr_fid, 0, 4,0, 
			   blr_neq, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
		  blr_sort, 2, 
		     blr_ascending, 
			blr_fid, 1, 0,0, 
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
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 6,0, 
		     blr_assignment, 
			blr_fid, 1, 0,0, 
			blr_parameter, 0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 8,0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 1,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_264 */

static Firebird::IRequest*
   fb_275 = 0;		/* request handle */

static const unsigned
   fb_276l = 92;
static const unsigned char
   fb_276 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 3,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 28,0, 0, 
		  blr_boolean, 
		     blr_not, 
			blr_eql, 
			   blr_fid, 0, 0,0, 
			   blr_fid, 0, 3,0, 
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
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 2,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_276 */

static Firebird::IRequest*
   fb_281 = 0;		/* request handle */

static const unsigned
   fb_282l = 181;
static const unsigned char
   fb_282 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_quad, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_short, 0, 
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
				 blr_fid, 0, 8,0, 
				 blr_parameter, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 0,0, 
				    blr_fid, 1, 1,0, 
				 blr_any, 
				    blr_rse, 1, 
				       blr_rid, 22,0, 2, 
				       blr_boolean, 
					  blr_and, 
					     blr_eql, 
						blr_fid, 1, 0,0, 
						blr_fid, 2, 0,0, 
					     blr_eql, 
						blr_fid, 2, 1,0, 
						blr_literal, blr_text, 5,0, 'C','H','E','C','K',
				       blr_end, 
		     blr_sort, 1, 
			blr_ascending, 
			   blr_fid, 1, 0,0, 
		     blr_project, 1, 
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
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 3,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_282 */

static Firebird::IRequest*
   fb_291 = 0;		/* request handle */

static const unsigned
   fb_292l = 240;
static const unsigned char
   fb_292 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 16,0, 
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
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 2, 
		  blr_rid, 12,0, 0, 
		  blr_rid, 6,0, 1, 
		  blr_boolean, 
		     blr_and, 
			blr_eql, 
			   blr_fid, 1, 8,0, 
			   blr_fid, 0, 1,0, 
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
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter2, 0, 0,0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 13,0, 
			blr_parameter2, 0, 1,0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter, 0, 8,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 9,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 11,0, 10,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 12,0, 
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 13,0, 
		     blr_assignment, 
			blr_fid, 0, 14,0, 
			blr_parameter2, 0, 14,0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 12,0, 
			blr_parameter2, 0, 15,0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 3,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_292 */

static Firebird::IRequest*
   fb_310 = 0;		/* request handle */

static const unsigned
   fb_311l = 215;
static const unsigned char
   fb_311 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 15,0, 
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
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
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
		  blr_sort, 3, 
		     blr_ascending, 
			blr_fid, 0, 3,0, 
		     blr_ascending, 
			blr_fid, 0, 2,0, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 4,0, 
			blr_parameter2, 0, 0,0, 4,0, 
		     blr_assignment, 
			blr_fid, 0, 13,0, 
			blr_parameter2, 0, 1,0, 6,0, 
		     blr_assignment, 
			blr_fid, 0, 3,0, 
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 2,0, 
			blr_parameter, 0, 8,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 9,0, 
		     blr_assignment, 
			blr_fid, 0, 7,0, 
			blr_parameter2, 0, 11,0, 10,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 12,0, 
		     blr_assignment, 
			blr_fid, 0, 14,0, 
			blr_parameter2, 0, 13,0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 12,0, 
			blr_parameter2, 0, 14,0, 5,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 3,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_311 */

static Firebird::IRequest*
   fb_328 = 0;		/* request handle */

static const unsigned
   fb_329l = 121;
static const unsigned char
   fb_329 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 5,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 6,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
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
			blr_fid, 0, 15,0, 
			blr_parameter2, 0, 2,0, 1,0, 
		     blr_assignment, 
			blr_fid, 0, 8,0, 
			blr_parameter, 0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 9,0, 
			blr_parameter, 0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_329 */

static Firebird::IRequest*
   fb_336 = 0;		/* request handle */

static const unsigned
   fb_337l = 157;
static const unsigned char
   fb_337 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 10,0, 
	    blr_quad, 0, 
	    blr_cstring2, 0,0, 0,1, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_bool, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 26,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 10,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 10,0, 
			blr_missing, 
			   blr_fid, 0, 16,0, 
		  blr_sort, 1, 
		     blr_ascending, 
			blr_fid, 0, 0,0, 
		  blr_end, 
	       blr_send, 0, 
		  blr_begin, 
		     blr_assignment, 
			blr_fid, 0, 5,0, 
			blr_parameter2, 0, 0,0, 3,0, 
		     blr_assignment, 
			blr_fid, 0, 15,0, 
			blr_parameter2, 0, 1,0, 6,0, 
		     blr_assignment, 
			blr_literal, blr_long, 0, 1,0,0,0,
			blr_parameter, 0, 2,0, 
		     blr_assignment, 
			blr_fid, 0, 0,0, 
			blr_parameter, 0, 7,0, 
		     blr_assignment, 
			blr_fid, 0, 18,0, 
			blr_parameter2, 0, 8,0, 5,0, 
		     blr_assignment, 
			blr_fid, 0, 14,0, 
			blr_parameter2, 0, 9,0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 2,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_337 */

static Firebird::IRequest*
   fb_349 = 0;		/* request handle */

static const unsigned
   fb_350l = 109;
static const unsigned char
   fb_350 [] = {
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
		  blr_rid, 26,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 10,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 10,0, 
			blr_missing, 
			   blr_fid, 0, 16,0, 
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
			blr_fid, 0, 11,0, 
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
   };	/* end of blr string for request fb_350 */

static Firebird::IRequest*
   fb_356 = 0;		/* request handle */

static const unsigned
   fb_357l = 88;
static const unsigned char
   fb_357 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 30,0, 0, 
		  blr_boolean, 
		     blr_or, 
			blr_neq, 
			   blr_fid, 0, 4,0, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			blr_missing, 
			   blr_fid, 0, 4,0, 
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
   };	/* end of blr string for request fb_357 */

static Firebird::IRequest*
   fb_361 = 0;		/* request handle */

static const unsigned
   fb_362l = 88;
static const unsigned char
   fb_362 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 20,0, 0, 
		  blr_boolean, 
		     blr_or, 
			blr_neq, 
			   blr_fid, 0, 2,0, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			blr_missing, 
			   blr_fid, 0, 2,0, 
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
   };	/* end of blr string for request fb_362 */

static Firebird::IRequest*
   fb_366 = 0;		/* request handle */

static const unsigned
   fb_367l = 94;
static const unsigned char
   fb_367 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 14,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 7,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 7,0, 
			blr_missing, 
			   blr_fid, 0, 9,0, 
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
   };	/* end of blr string for request fb_367 */

static Firebird::IRequest*
   fb_371 = 0;		/* request handle */

static const unsigned
   fb_372l = 88;
static const unsigned char
   fb_372 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 42,0, 0, 
		  blr_boolean, 
		     blr_or, 
			blr_neq, 
			   blr_fid, 0, 6,0, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			blr_missing, 
			   blr_fid, 0, 6,0, 
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
   };	/* end of blr string for request fb_372 */

static Firebird::IRequest*
   fb_376 = 0;		/* request handle */

static const unsigned
   fb_377l = 94;
static const unsigned char
   fb_377 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 26,0, 0, 
		  blr_boolean, 
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 10,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 10,0, 
			blr_missing, 
			   blr_fid, 0, 16,0, 
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
   };	/* end of blr string for request fb_377 */

static Firebird::IRequest*
   fb_381 = 0;		/* request handle */

static const unsigned
   fb_382l = 102;
static const unsigned char
   fb_382 [] = {
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
		     blr_and, 
			blr_or, 
			   blr_neq, 
			      blr_fid, 0, 4,0, 
			      blr_literal, blr_long, 0, 1,0,0,0,
			   blr_missing, 
			      blr_fid, 0, 4,0, 
			blr_starting, 
			   blr_fid, 0, 9,0, 
			   blr_literal, blr_text, 4,0, 'S','Q','L','$',
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
   };	/* end of blr string for request fb_382 */

static Firebird::IRequest*
   fb_386 = 0;		/* request handle */

static const unsigned
   fb_387l = 96;
static const unsigned char
   fb_387 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 0, 5,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_begin, 
	    blr_for, 
	       blr_rse, 1, 
		  blr_rid, 31,0, 0, 
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
		     blr_assignment, 
			blr_fid, 0, 1,0, 
			blr_parameter, 0, 4,0, 
		     blr_end, 
	    blr_send, 0, 
	       blr_assignment, 
		  blr_literal, blr_long, 0, 0,0,0,0,
		  blr_parameter, 0, 0,0, 
	    blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_387 */

static Firebird::IRequest*
   fb_394 = 0;		/* request handle */

static const unsigned
   fb_395l = 349;
static const unsigned char
   fb_395 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 30,0, 
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
		     blr_rid, 15,0, 0, 
		     blr_rid, 2,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 0, 0,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 12,0, 
				 blr_fid, 1, 0,0, 
			      blr_missing, 
				 blr_fid, 0, 10,0, 
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
			   blr_parameter2, 1, 1,0, 22,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 25,0, 
			   blr_parameter2, 1, 5,0, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 24,0, 
			   blr_parameter2, 1, 8,0, 7,0, 
			blr_assignment, 
			   blr_fid, 1, 8,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 1, 9,0, 
			   blr_parameter, 1, 10,0, 
			blr_assignment, 
			   blr_fid, 1, 27,0, 
			   blr_parameter, 1, 11,0, 
			blr_assignment, 
			   blr_fid, 1, 11,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 1, 26,0, 
			   blr_parameter2, 1, 13,0, 6,0, 
			blr_assignment, 
			   blr_fid, 1, 10,0, 
			   blr_parameter, 1, 14,0, 
			blr_assignment, 
			   blr_fid, 1, 15,0, 
			   blr_parameter, 1, 15,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter2, 1, 19,0, 18,0, 
			blr_assignment, 
			   blr_fid, 0, 16,0, 
			   blr_parameter2, 1, 21,0, 20,0, 
			blr_assignment, 
			   blr_fid, 0, 15,0, 
			   blr_parameter2, 1, 24,0, 23,0, 
			blr_assignment, 
			   blr_fid, 0, 1,0, 
			   blr_parameter, 1, 25,0, 
			blr_assignment, 
			   blr_fid, 0, 19,0, 
			   blr_parameter2, 1, 26,0, 17,0, 
			blr_assignment, 
			   blr_fid, 0, 18,0, 
			   blr_parameter2, 1, 27,0, 16,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter, 1, 28,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 29,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_395 */

static Firebird::IRequest*
   fb_429 = 0;		/* request handle */

static const unsigned
   fb_430l = 350;
static const unsigned char
   fb_430 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 29,0, 
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
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_short, 0, 
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
			      blr_parameter, 0, 1,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 0, 4,0, 
				 blr_fid, 1, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 3,0, 
				    blr_parameter, 0, 0,0, 
				 blr_missing, 
				    blr_fid, 0, 14,0, 
		     blr_sort, 1, 
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
			   blr_parameter2, 1, 1,0, 22,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 2,0, 
			blr_assignment, 
			   blr_fid, 1, 25,0, 
			   blr_parameter2, 1, 5,0, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 24,0, 
			   blr_parameter2, 1, 8,0, 7,0, 
			blr_assignment, 
			   blr_fid, 1, 8,0, 
			   blr_parameter, 1, 9,0, 
			blr_assignment, 
			   blr_fid, 1, 9,0, 
			   blr_parameter, 1, 10,0, 
			blr_assignment, 
			   blr_fid, 1, 27,0, 
			   blr_parameter, 1, 11,0, 
			blr_assignment, 
			   blr_fid, 1, 11,0, 
			   blr_parameter, 1, 12,0, 
			blr_assignment, 
			   blr_fid, 1, 26,0, 
			   blr_parameter2, 1, 13,0, 6,0, 
			blr_assignment, 
			   blr_fid, 1, 10,0, 
			   blr_parameter, 1, 14,0, 
			blr_assignment, 
			   blr_fid, 1, 15,0, 
			   blr_parameter, 1, 15,0, 
			blr_assignment, 
			   blr_fid, 0, 11,0, 
			   blr_parameter2, 1, 19,0, 18,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter2, 1, 21,0, 20,0, 
			blr_assignment, 
			   blr_fid, 0, 9,0, 
			   blr_parameter2, 1, 24,0, 23,0, 
			blr_assignment, 
			   blr_fid, 0, 13,0, 
			   blr_parameter2, 1, 25,0, 17,0, 
			blr_assignment, 
			   blr_fid, 0, 12,0, 
			   blr_parameter2, 1, 26,0, 16,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 27,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 28,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 2,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_430 */

static Firebird::IRequest*
   fb_464 = 0;		/* request handle */

static const unsigned
   fb_465l = 197;
static const unsigned char
   fb_465 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 7,0, 
	    blr_cstring2, 0,0, 12,0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 2, 
		     blr_rid, 22,0, 0, 
		     blr_rid, 4,0, 1, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 0,0, 
			      blr_fid, 0, 5,0, 
			   blr_and, 
			      blr_or, 
				 blr_eql, 
				    blr_fid, 0, 1,0, 
				    blr_literal, blr_text, 11,0, 'P','R','I','M','A','R','Y',32,'K','E','Y',
				 blr_eql, 
				    blr_fid, 0, 1,0, 
				    blr_literal, blr_text, 6,0, 'U','N','I','Q','U','E',
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
			   blr_fid, 1, 7,0, 
			   blr_parameter2, 1, 3,0, 2,0, 
			blr_assignment, 
			   blr_fid, 0, 0,0, 
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 0, 5,0, 
			   blr_parameter, 1, 6,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 1,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_465 */

static Firebird::IRequest*
   fb_476 = 0;		/* request handle */

static const unsigned
   fb_477l = 126;
static const unsigned char
   fb_477 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 2,0, 
	    blr_short, 0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 2,0, 
	    blr_cstring, 253,0, 
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
			      blr_fid, 1, 1,0, 
			      blr_parameter, 0, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 1, 0,0, 
				 blr_fid, 0, 0,0, 
			      blr_and, 
				 blr_eql, 
				    blr_fid, 0, 1,0, 
				    blr_literal, blr_text, 8,0, 'N','O','T',32,'N','U','L','L',
				 blr_eql, 
				    blr_fid, 0, 2,0, 
				    blr_parameter, 0, 1,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 0,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 1,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 0,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_477 */

static Firebird::IRequest*
   fb_484 = 0;		/* request handle */

static const unsigned
   fb_485l = 99;
static const unsigned char
   fb_485 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 5,0, 
	    blr_int64, 0, 
	    blr_long, 0, 
	    blr_short, 0, 
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
			   blr_parameter2, 1, 0,0, 4,0, 
			blr_assignment, 
			   blr_fid, 0, 7,0, 
			   blr_parameter2, 1, 1,0, 3,0, 
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
   };	/* end of blr string for request fb_485 */

static Firebird::IRequest*
   fb_494 = 0;		/* request handle */

static const unsigned
   fb_495l = 439;
static const unsigned char
   fb_495 [] = {
      blr_version4,
      blr_begin, 
	 blr_message, 1, 38,0, 
	    blr_quad, 0, 
	    blr_quad, 0, 
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
	    blr_bool, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	    blr_cstring, 253,0, 
	 blr_message, 0, 1,0, 
	    blr_cstring, 253,0, 
	 blr_receive, 0, 
	    blr_begin, 
	       blr_for, 
		  blr_rse, 3, 
		     blr_rid, 6,0, 0, 
		     blr_rid, 5,0, 1, 
		     blr_rid, 2,0, 2, 
		     blr_boolean, 
			blr_and, 
			   blr_eql, 
			      blr_fid, 1, 2,0, 
			      blr_fid, 2, 0,0, 
			   blr_and, 
			      blr_eql, 
				 blr_fid, 1, 1,0, 
				 blr_fid, 0, 8,0, 
			      blr_eql, 
				 blr_fid, 0, 8,0, 
				 blr_parameter, 0, 0,0, 
		     blr_sort, 2, 
			blr_ascending, 
			   blr_fid, 1, 6,0, 
			blr_ascending, 
			   blr_fid, 1, 0,0, 
		     blr_end, 
		  blr_send, 1, 
		     blr_begin, 
			blr_assignment, 
			   blr_fid, 1, 17,0, 
			   blr_parameter2, 1, 0,0, 8,0, 
			blr_assignment, 
			   blr_fid, 2, 5,0, 
			   blr_parameter2, 1, 1,0, 9,0, 
			blr_assignment, 
			   blr_fid, 2, 4,0, 
			   blr_parameter2, 1, 2,0, 10,0, 
			blr_assignment, 
			   blr_fid, 0, 10,0, 
			   blr_parameter2, 1, 3,0, 28,0, 
			blr_assignment, 
			   blr_literal, blr_long, 0, 1,0,0,0,
			   blr_parameter, 1, 4,0, 
			blr_assignment, 
			   blr_fid, 1, 16,0, 
			   blr_parameter, 1, 5,0, 
			blr_assignment, 
			   blr_fid, 1, 20,0, 
			   blr_parameter, 1, 6,0, 
			blr_assignment, 
			   blr_fid, 2, 25,0, 
			   blr_parameter2, 1, 12,0, 11,0, 
			blr_assignment, 
			   blr_fid, 2, 17,0, 
			   blr_parameter, 1, 13,0, 
			blr_assignment, 
			   blr_fid, 2, 22,0, 
			   blr_parameter2, 1, 15,0, 14,0, 
			blr_assignment, 
			   blr_fid, 2, 24,0, 
			   blr_parameter2, 1, 18,0, 17,0, 
			blr_assignment, 
			   blr_fid, 2, 8,0, 
			   blr_parameter, 1, 19,0, 
			blr_assignment, 
			   blr_fid, 2, 9,0, 
			   blr_parameter, 1, 20,0, 
			blr_assignment, 
			   blr_fid, 2, 27,0, 
			   blr_parameter, 1, 21,0, 
			blr_assignment, 
			   blr_fid, 2, 11,0, 
			   blr_parameter, 1, 22,0, 
			blr_assignment, 
			   blr_fid, 2, 26,0, 
			   blr_parameter2, 1, 23,0, 16,0, 
			blr_assignment, 
			   blr_fid, 1, 18,0, 
			   blr_parameter2, 1, 25,0, 24,0, 
			blr_assignment, 
			   blr_fid, 2, 10,0, 
			   blr_parameter, 1, 26,0, 
			blr_assignment, 
			   blr_fid, 2, 15,0, 
			   blr_parameter, 1, 27,0, 
			blr_assignment, 
			   blr_fid, 0, 16,0, 
			   blr_parameter2, 1, 31,0, 30,0, 
			blr_assignment, 
			   blr_fid, 0, 13,0, 
			   blr_parameter, 1, 32,0, 
			blr_assignment, 
			   blr_fid, 0, 17,0, 
			   blr_parameter2, 1, 33,0, 29,0, 
			blr_assignment, 
			   blr_fid, 1, 0,0, 
			   blr_parameter, 1, 34,0, 
			blr_assignment, 
			   blr_fid, 2, 0,0, 
			   blr_parameter, 1, 35,0, 
			blr_assignment, 
			   blr_fid, 1, 19,0, 
			   blr_parameter2, 1, 36,0, 7,0, 
			blr_assignment, 
			   blr_fid, 1, 1,0, 
			   blr_parameter, 1, 37,0, 
			blr_end, 
	       blr_send, 1, 
		  blr_assignment, 
		     blr_literal, blr_long, 0, 0,0,0,0,
		     blr_parameter, 1, 4,0, 
	       blr_end, 
	 blr_end, 
      blr_eoc
   };	/* end of blr string for request fb_495 */




static bool extract_rel_constraints(const char* relation_name);
static void get_procedure_args(const char* proc_name, SSHORT default_char_set_id);
static void get_function_args_ods12(const char*, USHORT, SSHORT default_char_set_id);
static void list_all_grants();
static processing_state list_all_grants2(bool, const SCHAR*);
static void list_all_tables(LegacyTables flag, SSHORT);
static void list_all_triggers();
static void list_check();
static void list_charsets();
static void list_collations();
static void list_create_db();
static void list_domain_table(const SCHAR*, SSHORT);
static void list_domains(SSHORT);
static void listDomainConstraints();
static void listRelationComputed(LegacyTables flag, SSHORT);
static void list_exceptions();
static void list_filters();
static void list_foreign();
static void list_functions_ods12_headers(SSHORT default_char_set_id);
static void list_functions_ods12_bodies(SSHORT default_char_set_id);
static void list_functions_legacy();
static void list_generators();
static void list_indexes();
static void list_package_bodies();
static void list_package_headers();
static void list_procedure_bodies(SSHORT default_char_set_id);
static void list_procedure_headers(SSHORT default_char_set_id);
static void list_views();

static const char* const Procterm = "^";	// TXNN: script use only

static TEXT SQL_identifier[BUFFER_LENGTH256];
static TEXT SQL_identifier2[BUFFER_XLARGE]; // should be >= 1026 bytes (exception text + two quotes)



int EXTRACT_ddl(LegacyTables flag, const SCHAR* tabname)
{
/**************************************
 *
 *	E X T R A C T _ d d l
 *
 **************************************
 *
 * Functional description
 *	Extract all sql information
 *	0 flag means SQL only tables. 1 flag means all tables
 *
 **************************************/
	bool did_attach = false;

	if (!DB)
	{
		DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		if (ISQL_errmsg(fbStatus))
			return FINI_ERROR;
		did_attach = true;

		// Make it read owner name to display grantor correctly
		SHOW_read_owner();
	}

	ISQL_get_version(false);
	if (isqlGlob.SQL_dialect != isqlGlob.db_SQL_dialect)
	{
		isqlGlob.printf("/*=========================================================*/%s", NEWLINE);
		isqlGlob.printf("/*=                                                      ==*/%s", NEWLINE);
		isqlGlob.printf("/*=     Command Line -sqldialect %d is overwritten by    ==*/%s",
				isqlGlob.SQL_dialect, NEWLINE);
		isqlGlob.printf("/*=     Database SQL Dialect %d.                          ==*/%s",
				isqlGlob.db_SQL_dialect, NEWLINE);
		isqlGlob.printf("/*=                                                      ==*/%s", NEWLINE);
		isqlGlob.printf("/*=========================================================*/%s", NEWLINE);
	}
	isqlGlob.printf(NEWLINE);

	isqlGlob.printf("SET SQL DIALECT %d; %s", isqlGlob.db_SQL_dialect, NEWLINE);

	isqlGlob.printf(NEWLINE);

	bool did_start = false;
	if (!fbTrans)
	{
		fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		if (ISQL_errmsg(fbStatus))
			return FINI_ERROR;
		did_start = true;
	}

	const SSHORT default_char_set_id = ISQL_get_default_char_set_id();
	int ret_code = FINI_OK;

	// If a table name was passed, extract only that table and domains
	if (*tabname)
	{
		if (EXTRACT_list_table(tabname, NULL, true, default_char_set_id))
		{
			SCHAR errbuf[MSG_LENGTH];
			IUTILS_msg_get(NOT_FOUND, errbuf, SafeArg() << tabname);
			STDERROUT(errbuf);
			ret_code = FINI_ERROR;
		}
	}
	else
	{
		list_create_db();
		list_filters();
		list_charsets();
		list_collations();
		list_generators();
		list_domains(default_char_set_id);
		list_all_tables(flag, default_char_set_id);
		list_functions_legacy();
		list_functions_ods12_headers(default_char_set_id);
		list_procedure_headers(default_char_set_id);
		list_package_headers();
		list_indexes();
		list_foreign();
		list_views();
		list_exceptions();
		list_functions_ods12_bodies(default_char_set_id);
		list_procedure_bodies(default_char_set_id);
		list_package_bodies();
		listDomainConstraints();
		list_check();
		listRelationComputed(flag, default_char_set_id);
		list_all_triggers();
		list_all_grants();
		SHOW_maps(true, "");
		SHOW_comments(false); // Let's make this an option later.
	}

	if (fbTrans && did_start)
	{
		fbTrans->commit(fbStatus);
		if (ISQL_errmsg(fbStatus))
			return FINI_ERROR;
		fbTrans = NULL;
	}

	if (DB && did_attach)
	{
		DB->detach(fbStatus);
		if (ISQL_errmsg(fbStatus))
			return FINI_ERROR;
		DB = NULL;
	}

	return ret_code;
}


processing_state EXTRACT_list_grants(const SCHAR* terminator)
{
/**************************************
 *
 *	E X T R A C T _ l i s t _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Print the permissions on all user tables, views and procedures.
 *
 **************************************/
	return list_all_grants2(false, terminator);
}


int EXTRACT_list_table(const SCHAR* relation_name,
						  const SCHAR* new_name,
						  bool domain_flag,
						  SSHORT default_char_set_id)
{
   struct fb_481_struct {
          short fb_482;	/* fbUtility */
          char  fb_483 [253];	/* RDB$CONSTRAINT_NAME */
   } fb_481;
   struct fb_478_struct {
          char  fb_479 [253];	/* RDB$FIELD_NAME */
          char  fb_480 [253];	/* RDB$RELATION_NAME */
   } fb_478;
   struct fb_488_struct {
          ISC_INT64 fb_489;	/* RDB$INITIAL_VALUE */
          ISC_LONG fb_490;	/* RDB$GENERATOR_INCREMENT */
          short fb_491;	/* fbUtility */
          short fb_492;	/* gds__null_flag */
          short fb_493;	/* gds__null_flag */
   } fb_488;
   struct fb_486_struct {
          char  fb_487 [253];	/* RDB$GENERATOR_NAME */
   } fb_486;
   struct fb_498_struct {
          ISC_QUAD fb_499;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_500;	/* RDB$COMPUTED_SOURCE */
          ISC_QUAD fb_501;	/* RDB$COMPUTED_BLR */
          char  fb_502 [256];	/* RDB$EXTERNAL_FILE */
          short fb_503;	/* fbUtility */
          short fb_504;	/* RDB$NULL_FLAG */
          short fb_505;	/* RDB$IDENTITY_TYPE */
          short fb_506;	/* gds__null_flag */
          short fb_507;	/* gds__null_flag */
          short fb_508;	/* gds__null_flag */
          short fb_509;	/* gds__null_flag */
          short fb_510;	/* gds__null_flag */
          short fb_511;	/* RDB$COLLATION_ID */
          short fb_512;	/* RDB$SEGMENT_LENGTH */
          short fb_513;	/* gds__null_flag */
          short fb_514;	/* RDB$DIMENSIONS */
          short fb_515;	/* gds__null_flag */
          short fb_516;	/* gds__null_flag */
          short fb_517;	/* RDB$CHARACTER_LENGTH */
          short fb_518;	/* RDB$FIELD_LENGTH */
          short fb_519;	/* RDB$FIELD_SCALE */
          short fb_520;	/* RDB$FIELD_PRECISION */
          short fb_521;	/* RDB$FIELD_SUB_TYPE */
          short fb_522;	/* RDB$CHARACTER_SET_ID */
          short fb_523;	/* gds__null_flag */
          short fb_524;	/* RDB$COLLATION_ID */
          short fb_525;	/* RDB$FIELD_TYPE */
          short fb_526;	/* RDB$SYSTEM_FLAG */
          short fb_527;	/* gds__null_flag */
          short fb_528;	/* gds__null_flag */
          short fb_529;	/* gds__null_flag */
          short fb_530;	/* RDB$RELATION_TYPE */
          char  fb_531 [253];	/* RDB$OWNER_NAME */
          FB_BOOLEAN fb_532;	/* RDB$SQL_SECURITY */
          char  fb_533 [253];	/* RDB$FIELD_NAME */
          char  fb_534 [253];	/* RDB$FIELD_NAME */
          char  fb_535 [253];	/* RDB$GENERATOR_NAME */
          char  fb_536 [253];	/* RDB$RELATION_NAME */
   } fb_498;
   struct fb_496_struct {
          char  fb_497 [253];	/* RDB$RELATION_NAME */
   } fb_496;
/**************************************
 *
 *	E X T R A C T _ l i s t _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	Shows columns, types, info for a given table name
 *	and text of views.
 *	Use a GDML query to get the info and print it.
 *	If a new_name is passed, substitute it for relation_name
 *
 *	relation_name -- Name of table to investigate
 *	new_name -- Name of a new name for a replacement table
 *	domain_flag -- extract needed domains before the table
 *	default_char_set_id -- character set def to supress
 *
 **************************************/
/**************************************
 *	default_char_set_id warrants special
 *	consideration.  If the metadata for a
 *	table is being extracted when there is
 *	really no need to redundantly and repeatedly
 *	list the databases default character set
 *	for every field.
 *
 *	At the same time there is a need to list
 *	the character set NONE when it is not
 *	the default character set for the database.
 *
 *	EXCEPT!  If the metadata is being extracted
 *	with the intention of coping that tables structure
 *	into another database, and it is not possible
 *	to know the default character set for the
 *	target database, then list every fields
 *	character set.  This includes the character
 *	set NONE.
 *
 *	Fields with no character set definition will
 *	not have any character set listed.
 *
 *	Use -1 as the default_char_set_id
 *	in this case.
 *
 *	POTENTIAL TRAP!  Consider the following:
 *	When copying a table from one database
 *	to another how should fields using the
 *	default character set be handled?
 *
 *	If both databases have the same default
 *	character set, then there is no problem
 *	or confusion.
 *
 *	If the databases have different default
 *	character sets then should fields using
 *	the default is the source database use
 *	the default of the target database?
 *
 **************************************/

	bool first = true;
	SCHAR char_sets[CHARSET_COLLATE_SIZE];
	rel_t rel_type = rel_persistent;
	char ss[28] = "";

	// Query to obtain relation detail information

	/*FOR REL IN RDB$RELATIONS CROSS
		RFR IN RDB$RELATION_FIELDS CROSS
		FLD IN RDB$FIELDS WITH
		RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
		RFR.RDB$RELATION_NAME EQ REL.RDB$RELATION_NAME AND
		REL.RDB$RELATION_NAME EQ relation_name
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
	   if (!fb_494 && fbTrans && DB)
	      fb_494 = DB->compileRequest(fbStatus, sizeof(fb_495), fb_495);
	   isc_vtov ((const char*) relation_name, (char*) fb_496.fb_497, 253);
	   if (fbTrans && fb_494)
	      fb_494->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_496));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_494->release(); fb_494 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_494->receive (fbStatus, 0, 1, 1602, CAST_MSG(&fb_498));
	   if (!fb_498.fb_503 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (!/*REL.RDB$RELATION_TYPE.NULL*/
		     fb_498.fb_529)
			rel_type = (rel_t) /*REL.RDB$RELATION_TYPE*/
					   fb_498.fb_530;

		SSHORT collation = 0;
		SSHORT char_set_id = 0;

		if (first)
		{
			first = false;
			// Do we need to print domains
			if (domain_flag)
				list_domain_table (relation_name, default_char_set_id);

			fb_utils::exact_name(/*REL.RDB$OWNER_NAME*/
					     fb_498.fb_531);
			isqlGlob.printf("%s/* Table: %s, Owner: %s */%s",
					 NEWLINE,
					 relation_name,
					 /*REL.RDB$OWNER_NAME*/
					 fb_498.fb_531,
					 NEWLINE);

			if (rel_type == rel_global_temp_preserve || rel_type == rel_global_temp_delete)
				isqlGlob.printf("CREATE GLOBAL TEMPORARY TABLE ");
			else
				isqlGlob.printf("CREATE TABLE ");

			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				if (new_name)
					IUTILS_copy_SQL_id (new_name, SQL_identifier, DBL_QUOTE);
				else
					IUTILS_copy_SQL_id (relation_name, SQL_identifier, DBL_QUOTE);
				isqlGlob.printf("%s ", SQL_identifier);
			}
			else
				isqlGlob.printf("%s ", new_name ? new_name : relation_name);

			if (!/*REL.RDB$SQL_SECURITY.NULL*/
			     fb_498.fb_528)
			{
				if (/*REL.RDB$SQL_SECURITY*/
				    fb_498.fb_532)
					strcpy(ss, "SQL SECURITY DEFINER");
				else
					strcpy(ss, "SQL SECURITY INVOKER");
			}

			if (!/*REL.RDB$EXTERNAL_FILE.NULL*/
			     fb_498.fb_527)
			{
				IUTILS_copy_SQL_id (/*REL.RDB$EXTERNAL_FILE*/
						    fb_498.fb_502, SQL_identifier2, SINGLE_QUOTE);
				isqlGlob.printf("EXTERNAL FILE %s ", SQL_identifier2);
			}

			isqlGlob.printf("(");
		}
		else
			isqlGlob.printf(",%s%s", NEWLINE, TAB_AS_SPACES);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
								 fb_498.fb_533), SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("%s ", SQL_identifier);
		}
		else
			isqlGlob.printf("%s ", fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
								    fb_498.fb_533));

		/*
		** If this is a known domain, then just print the domain rather than type
		** Domains won't have length, array, or blob definitions, but they
		** may have not null, default and check overriding their definitions
		*/

		if (!(fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
						fb_498.fb_534) && /*FLD.RDB$SYSTEM_FLAG*/
     fb_498.fb_526 != 1))
		{
			fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
					     fb_498.fb_534);
			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				IUTILS_copy_SQL_id (/*FLD.RDB$FIELD_NAME*/
						    fb_498.fb_534, SQL_identifier, DBL_QUOTE);
				isqlGlob.prints(SQL_identifier);
			}
			else
				isqlGlob.prints(/*FLD.RDB$FIELD_NAME*/
						fb_498.fb_534);

			// International character sets
			// Print only the character set
			if ((/*FLD.RDB$FIELD_TYPE*/
			     fb_498.fb_525 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_498.fb_525 == blr_varying) &&
				!/*RFR.RDB$COLLATION_ID.NULL*/
				 fb_498.fb_523)
			{
				// There is no need to print character set for domain at all because it cannot be overriden.
				// Collation will be printed later.
				collation = /*RFR.RDB$COLLATION_ID*/
					    fb_498.fb_524;
				char_set_id = /*FLD.RDB$CHARACTER_SET_ID*/
					      fb_498.fb_522;
			}
		}
		else
		{
			if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
						   fb_498.fb_534, /*FLD.RDB$FIELD_TYPE*/
  fb_498.fb_525,
					/*FLD.RDB$FIELD_SUB_TYPE*/
					fb_498.fb_521, /*FLD.RDB$FIELD_PRECISION*/
  fb_498.fb_520, /*FLD.RDB$FIELD_SCALE*/
  fb_498.fb_519))
			{
				return FINI_ERROR;
			}

			if ((/*FLD.RDB$FIELD_TYPE*/
			     fb_498.fb_525 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_498.fb_525 == blr_varying))
			{
				isqlGlob.printf("(%d)",
					ISQL_get_char_length(
						/*FLD.RDB$FIELD_LENGTH*/
						fb_498.fb_518,
						/*FLD.RDB$CHARACTER_LENGTH.NULL*/
						fb_498.fb_516, /*FLD.RDB$CHARACTER_LENGTH*/
  fb_498.fb_517,
						/*FLD.RDB$CHARACTER_SET_ID.NULL*/
						fb_498.fb_515, /*FLD.RDB$CHARACTER_SET_ID*/
  fb_498.fb_522
					)
				);
			}

			// Catch arrays after printing the type

			if (!/*FLD.RDB$DIMENSIONS.NULL*/
			     fb_498.fb_513)
				ISQL_array_dimensions(/*FLD.RDB$FIELD_NAME*/
						      fb_498.fb_534);

			if (/*FLD.RDB$FIELD_TYPE*/
			    fb_498.fb_525 == blr_blob)
			{
				const int subtype = /*FLD.RDB$FIELD_SUB_TYPE*/
						    fb_498.fb_521;
				isqlGlob.printf(" SUB_TYPE ");

				if ((subtype > 0) && (subtype <= MAX_BLOBSUBTYPES))
					isqlGlob.prints(Sub_types[subtype]);
				else
					isqlGlob.printf("%d", subtype);

				isqlGlob.printf(" SEGMENT SIZE %u", (USHORT) /*FLD.RDB$SEGMENT_LENGTH*/
									     fb_498.fb_512);
			}

			// International character sets
			if ((((/*FLD.RDB$FIELD_TYPE*/
			       fb_498.fb_525 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_498.fb_525 == blr_varying) &&
				  /*FLD.RDB$FIELD_SUB_TYPE*/
				  fb_498.fb_521 == fb_text_subtype_text) ||
				 /*FLD.RDB$FIELD_TYPE*/
				 fb_498.fb_525 == blr_blob) &&
				!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
				 fb_498.fb_515)
			{
				char_sets[0] = '\0';

				// Override rdb$fields id with relation_fields if present

				if (!/*RFR.RDB$COLLATION_ID.NULL*/
				     fb_498.fb_523)
					collation = /*RFR.RDB$COLLATION_ID*/
						    fb_498.fb_524;
				else if (!/*FLD.RDB$COLLATION_ID.NULL*/
					  fb_498.fb_510)
					collation = /*FLD.RDB$COLLATION_ID*/
						    fb_498.fb_511;

				if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
				     fb_498.fb_515)
					char_set_id = /*FLD.RDB$CHARACTER_SET_ID*/
						      fb_498.fb_522;
				ISQL_get_character_sets(char_set_id, collation, default_char_set_id, Get::CHARSET_ONLY, false, true, char_sets);
				if (char_sets[0])
					isqlGlob.prints(char_sets);
				// CVC: Someone deleted the code that checks intchar when handling collations
				// several lines below, so it didn't have any effect. Commented it.
				//if (!char_set_id)
				//	intchar = true;
			}
		}

		// Check for computed fields
		if (!/*FLD.RDB$COMPUTED_BLR.NULL*/
		     fb_498.fb_509)
		{
			isqlGlob.printf(" COMPUTED BY ");

			if (domain_flag)
			{
				if (!/*FLD.RDB$COMPUTED_SOURCE.NULL*/
				     fb_498.fb_508)
					ISQL_print_validation (isqlGlob.Out, &/*FLD.RDB$COMPUTED_SOURCE*/
									      fb_498.fb_500, true, fbTrans);
			}
			else if (!/*RFR.RDB$COLLATION_ID.NULL*/
				  fb_498.fb_523)
			{
				SCHAR collate_name[CHARSET_COLLATE_SIZE];
				collate_name[0] = '\0';
				ISQL_get_character_sets(char_set_id, collation, default_char_set_id, Get::COLLATE_ONLY, false, true, collate_name);

				if (collate_name[0])
				{
					isqlGlob.printf("(CAST(NULL AS VARCHAR(1)%s)", char_sets);
					isqlGlob.printf("%s)", collate_name);
				}
				else
					isqlGlob.printf("(NULL)");

				collation = 0;
			}
			else
				isqlGlob.printf("(NULL)");
		}

		// Handle defaults for columns

		if (!/*RFR.RDB$DEFAULT_SOURCE.NULL*/
		     fb_498.fb_507)
		{
			isqlGlob.printf(" ");
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*RFR.RDB$DEFAULT_SOURCE*/
								     fb_498.fb_499, false, true);
		}

		if (!/*RFR.RDB$GENERATOR_NAME.NULL*/
		     fb_498.fb_506)
		{
			/*FOR GEN IN RDB$GENERATORS
				WITH GEN.RDB$GENERATOR_NAME = RFR.RDB$GENERATOR_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_484 && fbTrans && DB)
			      fb_484 = DB->compileRequest(fbStatus, sizeof(fb_485), fb_485);
			   isc_vtov ((const char*) fb_498.fb_535, (char*) fb_486.fb_487, 253);
			   if (fbTrans && fb_484)
			      fb_484->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_486));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_484->release(); fb_484 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_484->receive (fbStatus, 0, 1, 18, CAST_MSG(&fb_488));
			   if (!fb_488.fb_491 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
			{
				isqlGlob.printf(" GENERATED %s AS IDENTITY",
					(/*RFR.RDB$IDENTITY_TYPE*/
					 fb_498.fb_505 == IDENT_TYPE_BY_DEFAULT ? "BY DEFAULT" :
					 /*RFR.RDB$IDENTITY_TYPE*/
					 fb_498.fb_505 == IDENT_TYPE_ALWAYS ? "ALWAYS" : ""));

				const bool printInitial = !/*GEN.RDB$INITIAL_VALUE.NULL*/
							   fb_488.fb_493 && /*GEN.RDB$INITIAL_VALUE*/
    fb_488.fb_489 != 0;
				const bool printIncrement = !/*GEN.RDB$GENERATOR_INCREMENT.NULL*/
							     fb_488.fb_492 && /*GEN.RDB$GENERATOR_INCREMENT*/
    fb_488.fb_490 != 1;

				if (printInitial || printIncrement)
				{
					isqlGlob.printf(" (");

					if (printInitial)
					{
						isqlGlob.printf("START WITH %" SQUADFORMAT "%s",
							/*GEN.RDB$INITIAL_VALUE*/
							fb_488.fb_489, (printIncrement ? " " : ""));
					}

					if (printIncrement)
						isqlGlob.printf("INCREMENT %" SLONGFORMAT, /*GEN.RDB$GENERATOR_INCREMENT*/
											   fb_488.fb_490);

					isqlGlob.printf(")");
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
		}

		/* The null flag is either 1 or null (for nullable) .  if there is
		**  a constraint name, print that too.  Domains cannot have named
		**  constraints.  The column name is in rdb$trigger_name in
		**  rdb$check_constraints.  We hope we get at most one row back.
		*/

		if (/*RFR.RDB$NULL_FLAG*/
		    fb_498.fb_504 == 1)
		{
			/*FOR RCO IN RDB$RELATION_CONSTRAINTS CROSS
				CON IN RDB$CHECK_CONSTRAINTS WITH
				CON.RDB$TRIGGER_NAME = RFR.RDB$FIELD_NAME AND
				CON.RDB$CONSTRAINT_NAME = RCO.RDB$CONSTRAINT_NAME AND
				RCO.RDB$CONSTRAINT_TYPE EQ "NOT NULL" AND
				RCO.RDB$RELATION_NAME = RFR.RDB$RELATION_NAME*/
			{
			for (int retries = 0; retries < 2; ++retries)
			   {
			   {
			   if (!DB)
			      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
			   if (DB && !fbTrans)
			      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
			   }
			   if (!fb_476 && fbTrans && DB)
			      fb_476 = DB->compileRequest(fbStatus, sizeof(fb_477), fb_477);
			   isc_vtov ((const char*) fb_498.fb_533, (char*) fb_478.fb_479, 253);
			   isc_vtov ((const char*) fb_498.fb_536, (char*) fb_478.fb_480, 253);
			   if (fbTrans && fb_476)
			      fb_476->startAndSend(fbStatus, fbTrans, 0, 0, 506, CAST_CONST_MSG(&fb_478));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_476->release(); fb_476 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_476->receive (fbStatus, 0, 1, 255, CAST_MSG(&fb_481));
			   if (!fb_481.fb_482 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				if (!fb_utils::implicit_integrity(/*CON.RDB$CONSTRAINT_NAME*/
								  fb_481.fb_483))
				{
					fb_utils::exact_name(/*CON.RDB$CONSTRAINT_NAME*/
							     fb_481.fb_483);
					if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					{
						IUTILS_copy_SQL_id (/*CON.RDB$CONSTRAINT_NAME*/
								    fb_481.fb_483, SQL_identifier, DBL_QUOTE);
						isqlGlob.printf(" CONSTRAINT %s", SQL_identifier);
					}
					else
						isqlGlob.printf(" CONSTRAINT %s", /*CON.RDB$CONSTRAINT_NAME*/
										  fb_481.fb_483);
				}
			/*END_FOR*/
			   }
			   };
			/*ON_ERROR*/
			if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
			   {
				ISQL_errmsg (fbStatus);
				return FINI_ERROR;
			/*END_ERROR;*/
			   }
			}

			isqlGlob.printf(" NOT NULL");
		}

		// Handle collations after defaults

		if (!/*RFR.RDB$COLLATION_ID.NULL*/
		     fb_498.fb_523)
		{
			char_sets[0] = '\0';
			ISQL_get_character_sets(char_set_id, collation, default_char_set_id, Get::COLLATE_ONLY, false, true, char_sets);
			if (char_sets[0])
				isqlGlob.prints(char_sets);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return FINI_ERROR;
	/*END_ERROR;*/
	   }
	}

	// Do primary and unique keys only. References come later.
	if (!extract_rel_constraints(relation_name))
		return FINI_ERROR;

	// Check constaints are now deferred

	if (first) // we extracted nothing
		return FINI_ERROR;

	const char* gtt_scope = (rel_type == rel_global_temp_preserve) ? "ON COMMIT PRESERVE ROWS" :
							((rel_type == rel_global_temp_delete) ? "ON COMMIT DELETE ROWS" : "");

	const char* opt_delim = *gtt_scope && *ss ? ", " : "";

	if (*gtt_scope || *ss)
		isqlGlob.printf(")%s%s%s%s", NEWLINE, gtt_scope, opt_delim , ss);
	else
		isqlGlob.printf(")");

	isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
	return FINI_OK;
}


static bool extract_rel_constraints(const char* relation_name)
{
   struct fb_468_struct {
          char  fb_469 [12];	/* RDB$CONSTRAINT_TYPE */
          short fb_470;	/* fbUtility */
          short fb_471;	/* gds__null_flag */
          short fb_472;	/* RDB$INDEX_TYPE */
          char  fb_473 [253];	/* RDB$CONSTRAINT_NAME */
          char  fb_474 [253];	/* RDB$INDEX_NAME */
          char  fb_475 [253];	/* RDB$INDEX_NAME */
   } fb_468;
   struct fb_466_struct {
          char  fb_467 [253];	/* RDB$RELATION_NAME */
   } fb_466;
/**************************************************
 *
 *	e x t r a c t _ r e l _ c o n s t r a i n t s
 *
 **************************************************
 *
 * Functional description
 *	This function extracts the relation constraints of type PK and UK.
 *
 **************************************/

	SCHAR collist[BUFFER_LENGTH512 * 2];

	// PK and UK are always based on indices for now, hence this join is safe.
	/*FOR RELC IN RDB$RELATION_CONSTRAINTS
		CROSS IDX IN RDB$INDICES OVER RDB$INDEX_NAME
		WITH
		(RELC.RDB$CONSTRAINT_TYPE EQ "PRIMARY KEY" OR
		RELC.RDB$CONSTRAINT_TYPE EQ "UNIQUE") AND
		RELC.RDB$RELATION_NAME EQ relation_name
		SORTED BY RELC.RDB$CONSTRAINT_TYPE, RELC.RDB$CONSTRAINT_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_464 && fbTrans && DB)
	      fb_464 = DB->compileRequest(fbStatus, sizeof(fb_465), fb_465);
	   isc_vtov ((const char*) relation_name, (char*) fb_466.fb_467, 253);
	   if (fbTrans && fb_464)
	      fb_464->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_466));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_464->release(); fb_464 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_464->receive (fbStatus, 0, 1, 777, CAST_MSG(&fb_468));
	   if (!fb_468.fb_470 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		isqlGlob.printf(",%s", NEWLINE);
		fb_utils::exact_name(/*RELC.RDB$CONSTRAINT_NAME*/
				     fb_468.fb_473);
		fb_utils::exact_name(/*IDX.RDB$INDEX_NAME*/
				     fb_468.fb_474);

		// If the name of the constraint is not INTEG_..., print it
 		if (!fb_utils::implicit_integrity(/*RELC.RDB$CONSTRAINT_NAME*/
						  fb_468.fb_473))
		{
			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				IUTILS_copy_SQL_id (/*RELC.RDB$CONSTRAINT_NAME*/
						    fb_468.fb_473, SQL_identifier, DBL_QUOTE);
				isqlGlob.printf("CONSTRAINT %s ", SQL_identifier);
			}
			else
				isqlGlob.printf("CONSTRAINT %s ", /*RELC.RDB$CONSTRAINT_NAME*/
								  fb_468.fb_473);
		}

		ISQL_get_index_segments (collist, sizeof(collist), /*RELC.RDB$INDEX_NAME*/
								   fb_468.fb_475, true);

		const bool isPK = !strncmp (/*RELC.RDB$CONSTRAINT_TYPE*/
					    fb_468.fb_469, "PRIMARY", 7);
		if (isPK)
			isqlGlob.printf("PRIMARY KEY (%s)", collist);
		else //if (!strncmp (RELC.RDB$CONSTRAINT_TYPE, "UNIQUE", 6)) // redundant
			isqlGlob.printf("UNIQUE (%s)", collist);

		// Yes, the same RDB$... naming convention is used for both domains and indices.
		const bool explicit_index = ((isPK && !fb_utils::implicit_pk(/*IDX.RDB$INDEX_NAME*/
									     fb_468.fb_474)) ||
			(!isPK && !fb_utils::implicit_domain(/*IDX.RDB$INDEX_NAME*/
							     fb_468.fb_474))) &&
		    strcmp(/*RELC.RDB$CONSTRAINT_NAME*/
			   fb_468.fb_473, /*IDX.RDB$INDEX_NAME*/
  fb_468.fb_474);
		const bool descending_index = !/*IDX.RDB$INDEX_TYPE.NULL*/
					       fb_468.fb_471 && /*IDX.RDB$INDEX_TYPE*/
    fb_468.fb_472 == 1;
		if (explicit_index || descending_index)
		{
			isqlGlob.printf(" USING %sINDEX", descending_index ? "DESCENDING " : "");
			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				IUTILS_copy_SQL_id(/*IDX.RDB$INDEX_NAME*/
						   fb_468.fb_474, SQL_identifier, DBL_QUOTE);
				isqlGlob.printf(" %s", SQL_identifier);
			}
			else
				isqlGlob.printf(" %s", /*IDX.RDB$INDEX_NAME*/
						       fb_468.fb_474);
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

	return true;
}


static void get_procedure_args(const char* proc_name, SSHORT default_char_set_id)
{
   struct fb_434_struct {
          ISC_QUAD fb_435;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_436;	/* RDB$DEFAULT_SOURCE */
          short fb_437;	/* fbUtility */
          short fb_438;	/* gds__null_flag */
          short fb_439;	/* gds__null_flag */
          short fb_440;	/* RDB$COLLATION_ID */
          short fb_441;	/* gds__null_flag */
          short fb_442;	/* gds__null_flag */
          short fb_443;	/* RDB$CHARACTER_LENGTH */
          short fb_444;	/* RDB$FIELD_LENGTH */
          short fb_445;	/* RDB$FIELD_SCALE */
          short fb_446;	/* RDB$FIELD_PRECISION */
          short fb_447;	/* RDB$FIELD_SUB_TYPE */
          short fb_448;	/* RDB$CHARACTER_SET_ID */
          short fb_449;	/* RDB$FIELD_TYPE */
          short fb_450;	/* RDB$SYSTEM_FLAG */
          short fb_451;	/* gds__null_flag */
          short fb_452;	/* gds__null_flag */
          short fb_453;	/* gds__null_flag */
          short fb_454;	/* RDB$PARAMETER_MECHANISM */
          short fb_455;	/* gds__null_flag */
          short fb_456;	/* RDB$NULL_FLAG */
          short fb_457;	/* gds__null_flag */
          short fb_458;	/* gds__null_flag */
          short fb_459;	/* RDB$COLLATION_ID */
          char  fb_460 [253];	/* RDB$RELATION_NAME */
          char  fb_461 [253];	/* RDB$FIELD_NAME */
          char  fb_462 [253];	/* RDB$PARAMETER_NAME */
          char  fb_463 [253];	/* RDB$FIELD_NAME */
   } fb_434;
   struct fb_431_struct {
          short fb_432;	/* RDB$PARAMETER_TYPE */
          char  fb_433 [253];	/* RDB$PROCEDURE_NAME */
   } fb_431;
/**************************************
 *
 *	g e t _ p r o c e d u r e _ a r g s
 *
 **************************************
 *
 * Functional description
 *	This function extracts the procedure parameters and adds it to the
 *	extract file.
 *  Make sure to pass here only the names of procedures that are global.
 *
 **************************************/
	SCHAR char_sets[95];

	// query to retrieve the parameters.


	// placed the two identical code blocks into one
	// for loop as suggested by Ann H. and Claudio V.
	// FSG 18.Nov.2000

	//  Parameter types 0 = input
	//  Parameter types 1 = return

	for (SSHORT ptype = 0; ptype < 2; ptype++)
	{
		bool first_time = true;

		/*FOR PRM IN RDB$PROCEDURE_PARAMETERS CROSS
			FLD IN RDB$FIELDS WITH
			PRM.RDB$PROCEDURE_NAME = proc_name AND
			PRM.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
			PRM.RDB$PARAMETER_TYPE = ptype AND
			PRM.RDB$PACKAGE_NAME MISSING
			SORTED BY PRM.RDB$PARAMETER_NUMBER*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_429 && fbTrans && DB)
		      fb_429 = DB->compileRequest(fbStatus, sizeof(fb_430), fb_430);
		   fb_431.fb_432 = ptype;
		   isc_vtov ((const char*) proc_name, (char*) fb_431.fb_433, 253);
		   if (fbTrans && fb_429)
		      fb_429->startAndSend(fbStatus, fbTrans, 0, 0, 255, CAST_CONST_MSG(&fb_431));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_429->release(); fb_429 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_429->receive (fbStatus, 0, 1, 1074, CAST_MSG(&fb_434));
		   if (!fb_434.fb_437 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			bool prm_collation_id_null = /*PRM.RDB$COLLATION_ID.NULL*/
						     fb_434.fb_458;
			SSHORT prm_collation_id = /*PRM.RDB$COLLATION_ID*/
						  fb_434.fb_459;

			bool prm_default_source_null = /*PRM.RDB$DEFAULT_SOURCE.NULL*/
						       fb_434.fb_457;
			ISC_QUAD prm_default_source = /*PRM.RDB$DEFAULT_SOURCE*/
						      fb_434.fb_436;

			bool prm_null_flag_null = /*PRM.RDB$NULL_FLAG.NULL*/
						  fb_434.fb_455;
			bool prm_null_flag = /*PRM.RDB$NULL_FLAG*/
					     fb_434.fb_456;

			prm_mech_t prm_mech = prm_mech_normal;

			if (!/*PRM.RDB$PARAMETER_MECHANISM.NULL*/
			     fb_434.fb_453)
				prm_mech = (prm_mech_t) /*PRM.RDB$PARAMETER_MECHANISM*/
							fb_434.fb_454;

			char relationName[BUFFER_LENGTH256] = "";
			char relationField[BUFFER_LENGTH256] = "";

			if (!/*PRM.RDB$RELATION_NAME.NULL*/
			     fb_434.fb_452)
			{
				fb_utils::exact_name(/*PRM.RDB$RELATION_NAME*/
						     fb_434.fb_460);
				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(/*PRM.RDB$RELATION_NAME*/
							   fb_434.fb_460, relationName, DBL_QUOTE);
				else
					strcpy(relationName, /*PRM.RDB$RELATION_NAME*/
							     fb_434.fb_460);
			}

			if (!/*PRM.RDB$FIELD_NAME.NULL*/
			     fb_434.fb_451)
			{
				fb_utils::exact_name(/*PRM.RDB$FIELD_NAME*/
						     fb_434.fb_461);
				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(/*PRM.RDB$FIELD_NAME*/
							   fb_434.fb_461, relationField, DBL_QUOTE);
				else
					strcpy(relationField, /*PRM.RDB$FIELD_NAME*/
							      fb_434.fb_461);
			}

			if (first_time)
			{
				first_time = false;
				if (ptype == 0)
				{
					// this is the input part
					isqlGlob.printf("(");
				}
				else
				{
					// we are in the output part
					isqlGlob.printf("RETURNS (");
				}
			}
			else
				isqlGlob.printf(",%s", NEWLINE);

			fb_utils::exact_name(/*PRM.RDB$PARAMETER_NAME*/
					     fb_434.fb_462);

			// CVC: Parameter names need check for dialect 3, too.

			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
				IUTILS_copy_SQL_id (/*PRM.RDB$PARAMETER_NAME*/
						    fb_434.fb_462, SQL_identifier, DBL_QUOTE);
			else
				strcpy (SQL_identifier, /*PRM.RDB$PARAMETER_NAME*/
							fb_434.fb_462);

			isqlGlob.printf("%s ", SQL_identifier);

			const bool basedOnColumn = relationName[0] && relationField[0];
			if (!fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
						       fb_434.fb_463) || /*FLD.RDB$SYSTEM_FLAG*/
     fb_434.fb_450 == 1 ||
				basedOnColumn)
			{
				if (prm_mech == prm_mech_type_of)
					isqlGlob.printf("TYPE OF ");

				if (basedOnColumn)
					isqlGlob.printf("COLUMN %s.%s", relationName, relationField);
				else
				{
					fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
							     fb_434.fb_463);
					if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					{
						IUTILS_copy_SQL_id (/*FLD.RDB$FIELD_NAME*/
								    fb_434.fb_463, SQL_identifier, DBL_QUOTE);
						isqlGlob.prints(SQL_identifier);
					}
					else
						isqlGlob.prints(/*FLD.RDB$FIELD_NAME*/
								fb_434.fb_463);
				}

				// International character sets
				// Print only the collation
				if ((/*FLD.RDB$FIELD_TYPE*/
				     fb_434.fb_449 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_434.fb_449 == blr_varying) &&
					!prm_collation_id_null)
				{
					char_sets[0] = '\0';
					ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
								fb_434.fb_448, prm_collation_id, default_char_set_id, Get::COLLATE_ONLY,
						!prm_null_flag_null && prm_null_flag, true, char_sets);
					if (char_sets[0])
						isqlGlob.prints(char_sets);
				}
				else if (!prm_null_flag_null && prm_null_flag)
					isqlGlob.printf(" NOT NULL");

				if (ptype == 0) // input, try to extract default and make Vlad happy.
				{
					if (!prm_default_source_null)
					{
						isqlGlob.printf(" ");
						SHOW_print_metadata_text_blob(isqlGlob.Out, &prm_default_source, false, true);
					}
				}
			}
			else
			{
				if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
							   fb_434.fb_463, /*FLD.RDB$FIELD_TYPE*/
  fb_434.fb_449, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_434.fb_447,
						/*FLD.RDB$FIELD_PRECISION*/
						fb_434.fb_446, /*FLD.RDB$FIELD_SCALE*/
  fb_434.fb_445))
				{
					return; // ps_ERR;
				}

				// Changed this to return RDB$CHARACTER_LENGTH if available
				// Fix for Bug #122563
				//   FSG 18.Nov.2000
				if ((/*FLD.RDB$FIELD_TYPE*/
				     fb_434.fb_449 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_434.fb_449 == blr_varying))
				{
					isqlGlob.printf("(%d)",
						ISQL_get_char_length(
							/*FLD.RDB$FIELD_LENGTH*/
							fb_434.fb_444,
							/*FLD.RDB$CHARACTER_LENGTH.NULL*/
							fb_434.fb_442, /*FLD.RDB$CHARACTER_LENGTH*/
  fb_434.fb_443,
							/*FLD.RDB$CHARACTER_SET_ID.NULL*/
							fb_434.fb_441, /*FLD.RDB$CHARACTER_SET_ID*/
  fb_434.fb_448
						)
					);
				}

				// Show international character sets and collations
				if ((!/*FLD.RDB$COLLATION_ID.NULL*/
				      fb_434.fb_439 || !/*FLD.RDB$CHARACTER_SET_ID.NULL*/
     fb_434.fb_441) &&
					!(((/*FLD.RDB$FIELD_TYPE*/
					    fb_434.fb_449 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_434.fb_449 == blr_varying)) &&
					  /*FLD.RDB$FIELD_SUB_TYPE*/
					  fb_434.fb_447 != fb_text_subtype_text))
				{
					char_sets[0] = 0;

					SSHORT collation = 0;

					if (!prm_collation_id_null)
						collation = prm_collation_id;
					else if (!/*FLD.RDB$COLLATION_ID.NULL*/
						  fb_434.fb_439)
						collation = /*FLD.RDB$COLLATION_ID*/
							    fb_434.fb_440;

					if (/*FLD.RDB$CHARACTER_SET_ID.NULL*/
					    fb_434.fb_441)
						/*FLD.RDB$CHARACTER_SET_ID*/
						fb_434.fb_448 = 0;

					ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
								fb_434.fb_448, collation, default_char_set_id, Get::BOTH,
						!prm_null_flag_null && prm_null_flag, true, char_sets);
					if (char_sets[0])
						isqlGlob.prints(char_sets);
				}
				else if (!prm_null_flag_null && prm_null_flag)
					isqlGlob.printf(" NOT NULL");

				if (ptype == 0) // input, try to extract default and make Vlad happy.
				{
					if (!prm_default_source_null)
					{
						isqlGlob.printf(" ");
						SHOW_print_metadata_text_blob(isqlGlob.Out, &prm_default_source, false, true);
					}
					else if (!/*FLD.RDB$DEFAULT_SOURCE.NULL*/
						  fb_434.fb_438)
					{
						isqlGlob.printf(" ");
						SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
											     fb_434.fb_435, false, true);
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
			return;
		/*END_ERROR;*/
		   }
		}

		// If there was at least one param, close parens

		if (!first_time)
			isqlGlob.printf(")%s", NEWLINE);

	}						// end for ptype
}


static void get_function_args_ods12(const char* func_name, USHORT out_arg, SSHORT default_char_set_id)
{
   struct fb_398_struct {
          ISC_QUAD fb_399;	/* RDB$DEFAULT_SOURCE */
          ISC_QUAD fb_400;	/* RDB$DEFAULT_SOURCE */
          short fb_401;	/* fbUtility */
          short fb_402;	/* gds__null_flag */
          short fb_403;	/* gds__null_flag */
          short fb_404;	/* RDB$COLLATION_ID */
          short fb_405;	/* gds__null_flag */
          short fb_406;	/* gds__null_flag */
          short fb_407;	/* RDB$CHARACTER_LENGTH */
          short fb_408;	/* RDB$FIELD_LENGTH */
          short fb_409;	/* RDB$FIELD_SCALE */
          short fb_410;	/* RDB$FIELD_PRECISION */
          short fb_411;	/* RDB$FIELD_SUB_TYPE */
          short fb_412;	/* RDB$CHARACTER_SET_ID */
          short fb_413;	/* RDB$FIELD_TYPE */
          short fb_414;	/* RDB$SYSTEM_FLAG */
          short fb_415;	/* gds__null_flag */
          short fb_416;	/* gds__null_flag */
          short fb_417;	/* gds__null_flag */
          short fb_418;	/* RDB$ARGUMENT_MECHANISM */
          short fb_419;	/* gds__null_flag */
          short fb_420;	/* RDB$NULL_FLAG */
          short fb_421;	/* gds__null_flag */
          short fb_422;	/* gds__null_flag */
          short fb_423;	/* RDB$COLLATION_ID */
          short fb_424;	/* RDB$ARGUMENT_POSITION */
          char  fb_425 [253];	/* RDB$RELATION_NAME */
          char  fb_426 [253];	/* RDB$FIELD_NAME */
          char  fb_427 [253];	/* RDB$ARGUMENT_NAME */
          char  fb_428 [253];	/* RDB$FIELD_NAME */
   } fb_398;
   struct fb_396_struct {
          char  fb_397 [253];	/* RDB$FUNCTION_NAME */
   } fb_396;
/**************************************
 *
 *	g e t _ f u n c t i o n _ a r g s _ o d s 1 2
 *
 **************************************
 *
 * Functional description
 *	This function extracts the function parameters and adds it to the
 *	extract file.
 *  Make sure to pass here only the names of functions that are global.
 *
 **************************************/
	SCHAR char_sets[95];

	// Pass 0 - inputs, pass 1 - return value

	for (int pass = 0; pass < 2; pass++)
	{
		bool first_time = true;

		/*FOR ARG IN RDB$FUNCTION_ARGUMENTS CROSS
			FLD IN RDB$FIELDS WITH
			ARG.RDB$FUNCTION_NAME = func_name AND
			ARG.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
			ARG.RDB$PACKAGE_NAME MISSING
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
		   if (!fb_394 && fbTrans && DB)
		      fb_394 = DB->compileRequest(fbStatus, sizeof(fb_395), fb_395);
		   isc_vtov ((const char*) func_name, (char*) fb_396.fb_397, 253);
		   if (fbTrans && fb_394)
		      fb_394->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_396));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_394->release(); fb_394 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_394->receive (fbStatus, 0, 1, 1076, CAST_MSG(&fb_398));
		   if (!fb_398.fb_401 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			if ((pass == 0 && /*ARG.RDB$ARGUMENT_POSITION*/
					  fb_398.fb_424 == out_arg) ||
				(pass != 0 && /*ARG.RDB$ARGUMENT_POSITION*/
					      fb_398.fb_424 != out_arg))
			{
				continue;
			}

			bool prm_collation_id_null = /*ARG.RDB$COLLATION_ID.NULL*/
						     fb_398.fb_422;
			SSHORT prm_collation_id = /*ARG.RDB$COLLATION_ID*/
						  fb_398.fb_423;

			bool prm_default_source_null = /*ARG.RDB$DEFAULT_SOURCE.NULL*/
						       fb_398.fb_421;
			ISC_QUAD prm_default_source = /*ARG.RDB$DEFAULT_SOURCE*/
						      fb_398.fb_400;

			bool prm_null_flag_null = /*ARG.RDB$NULL_FLAG.NULL*/
						  fb_398.fb_419;
			bool prm_null_flag = /*ARG.RDB$NULL_FLAG*/
					     fb_398.fb_420;

			prm_mech_t prm_mech = prm_mech_normal;

			if (!/*ARG.RDB$ARGUMENT_MECHANISM.NULL*/
			     fb_398.fb_417)
				prm_mech = (prm_mech_t) /*ARG.RDB$ARGUMENT_MECHANISM*/
							fb_398.fb_418;

			char relationName[BUFFER_LENGTH256] = "";
			char relationField[BUFFER_LENGTH256] = "";

			if (!/*ARG.RDB$RELATION_NAME.NULL*/
			     fb_398.fb_416)
			{
				fb_utils::exact_name(/*ARG.RDB$RELATION_NAME*/
						     fb_398.fb_425);

				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(/*ARG.RDB$RELATION_NAME*/
							   fb_398.fb_425, relationName, DBL_QUOTE);
				else
					strcpy(relationName, /*ARG.RDB$RELATION_NAME*/
							     fb_398.fb_425);
			}

			if (!/*ARG.RDB$FIELD_NAME.NULL*/
			     fb_398.fb_415)
			{
				fb_utils::exact_name(/*ARG.RDB$FIELD_NAME*/
						     fb_398.fb_426);

				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(/*ARG.RDB$FIELD_NAME*/
							   fb_398.fb_426, relationField, DBL_QUOTE);
				else
					strcpy(relationField, /*ARG.RDB$FIELD_NAME*/
							      fb_398.fb_426);
			}

			if (first_time)
			{
				first_time = false;
				if (pass == 0)
				{
					// this is the input part
					isqlGlob.printf("(");
				}
				else
				{
					// we are in the output part
					isqlGlob.printf("RETURNS ");
				}
			}
			else
				isqlGlob.printf(",%s", NEWLINE);

			if (pass == 0)
			{
				fb_utils::exact_name(/*ARG.RDB$ARGUMENT_NAME*/
						     fb_398.fb_427);

				// CVC: Parameter names need check for dialect 3, too.

				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					IUTILS_copy_SQL_id(/*ARG.RDB$ARGUMENT_NAME*/
							   fb_398.fb_427, SQL_identifier, DBL_QUOTE);
				else
					strcpy (SQL_identifier, /*ARG.RDB$ARGUMENT_NAME*/
								fb_398.fb_427);

				isqlGlob.printf("%s ", SQL_identifier);
			}

			const bool basedOnColumn = relationName[0] && relationField[0];
			if (!fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
						       fb_398.fb_428) || /*FLD.RDB$SYSTEM_FLAG*/
     fb_398.fb_414 == 1 ||
				basedOnColumn)
			{
				if (prm_mech == prm_mech_type_of)
					isqlGlob.printf("TYPE OF ");

				if (basedOnColumn)
					isqlGlob.printf("COLUMN %s.%s", relationName, relationField);
				else
				{
					fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
							     fb_398.fb_428);

					if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
					{
						IUTILS_copy_SQL_id (/*FLD.RDB$FIELD_NAME*/
								    fb_398.fb_428, SQL_identifier, DBL_QUOTE);
						isqlGlob.prints(SQL_identifier);
					}
					else
						isqlGlob.prints(/*FLD.RDB$FIELD_NAME*/
								fb_398.fb_428);
				}

				// International character sets
				// Print only the collation
				if ((/*FLD.RDB$FIELD_TYPE*/
				     fb_398.fb_413 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_398.fb_413 == blr_varying) &&
					!prm_collation_id_null)
				{
					char_sets[0] = '\0';
					ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
								fb_398.fb_412, prm_collation_id, default_char_set_id, Get::COLLATE_ONLY,
						!prm_null_flag_null && prm_null_flag, true, char_sets);
					if (char_sets[0])
						isqlGlob.prints(char_sets);
				}
				else if (!prm_null_flag_null && prm_null_flag)
					isqlGlob.printf(" NOT NULL");

				if (pass == 0) // input, try to extract default and make Vlad happy.
				{
					if (!prm_default_source_null)
					{
						isqlGlob.printf(" ");
						SHOW_print_metadata_text_blob(isqlGlob.Out, &prm_default_source, false, true);
					}
				}
			}
			else
			{
				if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
							   fb_398.fb_428, /*FLD.RDB$FIELD_TYPE*/
  fb_398.fb_413, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_398.fb_411,
						/*FLD.RDB$FIELD_PRECISION*/
						fb_398.fb_410, /*FLD.RDB$FIELD_SCALE*/
  fb_398.fb_409))
				{
					return; // ps_ERR;
				}

				// Changed this to return RDB$CHARACTER_LENGTH if available
				// Fix for Bug #122563
				//   FSG 18.Nov.2000
				if ((/*FLD.RDB$FIELD_TYPE*/
				     fb_398.fb_413 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_398.fb_413 == blr_varying))
				{
					isqlGlob.printf("(%d)",
						ISQL_get_char_length(
							/*FLD.RDB$FIELD_LENGTH*/
							fb_398.fb_408,
							/*FLD.RDB$CHARACTER_LENGTH.NULL*/
							fb_398.fb_406, /*FLD.RDB$CHARACTER_LENGTH*/
  fb_398.fb_407,
							/*FLD.RDB$CHARACTER_SET_ID.NULL*/
							fb_398.fb_405, /*FLD.RDB$CHARACTER_SET_ID*/
  fb_398.fb_412
						)
					);
				}

				// Show international character sets and collations
				if ((!/*FLD.RDB$COLLATION_ID.NULL*/
				      fb_398.fb_403 || !/*FLD.RDB$CHARACTER_SET_ID.NULL*/
     fb_398.fb_405) &&
					!(((/*FLD.RDB$FIELD_TYPE*/
					    fb_398.fb_413 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_398.fb_413 == blr_varying)) &&
					  /*FLD.RDB$FIELD_SUB_TYPE*/
					  fb_398.fb_411 != fb_text_subtype_text))
				{
					char_sets[0] = 0;

					SSHORT collation = 0;

					if (!prm_collation_id_null)
						collation = prm_collation_id;
					else if (!/*FLD.RDB$COLLATION_ID.NULL*/
						  fb_398.fb_403)
						collation = /*FLD.RDB$COLLATION_ID*/
							    fb_398.fb_404;

					if (/*FLD.RDB$CHARACTER_SET_ID.NULL*/
					    fb_398.fb_405)
						/*FLD.RDB$CHARACTER_SET_ID*/
						fb_398.fb_412 = 0;

					ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
								fb_398.fb_412, collation, default_char_set_id, Get::BOTH,
						!prm_null_flag_null && prm_null_flag, true, char_sets);

					if (char_sets[0])
						isqlGlob.prints(char_sets);
				}
				else if (!prm_null_flag_null && prm_null_flag)
					isqlGlob.printf(" NOT NULL");

				if (pass == 0) // input, try to extract default and make Vlad happy.
				{
					if (!prm_default_source_null)
					{
						isqlGlob.printf(" ");
						SHOW_print_metadata_text_blob(isqlGlob.Out, &prm_default_source, false, true);
					}
					else if (!/*FLD.RDB$DEFAULT_SOURCE.NULL*/
						  fb_398.fb_402)
					{
						isqlGlob.printf(" ");
						SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
											     fb_398.fb_399, false, true);
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
			return;
		/*END_ERROR;*/
		   }
		}

		// If there was at least one param, close parens

		if (!first_time)
		{
			if (pass == 0)
				isqlGlob.printf(")%s", NEWLINE);
			else
				isqlGlob.printf("%s", NEWLINE);
		}

	}						// end for ptype
}


static void list_all_grants()
{
/**************************************
 *
 *	l i s t _ a l l _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Print the permissions on all user tables, views and procedures.
 *
 *	Wrapper around list_all_grants2().
 *
 **************************************/
	list_all_grants2(true, isqlGlob.global_Term);
}


static processing_state list_all_grants2(bool show_role_list, const SCHAR* terminator)
{
   struct fb_358_struct {
          short fb_359;	/* fbUtility */
          char  fb_360 [253];	/* RDB$EXCEPTION_NAME */
   } fb_358;
   struct fb_363_struct {
          short fb_364;	/* fbUtility */
          char  fb_365 [253];	/* RDB$GENERATOR_NAME */
   } fb_363;
   struct fb_368_struct {
          short fb_369;	/* fbUtility */
          char  fb_370 [253];	/* RDB$FUNCTION_NAME */
   } fb_368;
   struct fb_373_struct {
          short fb_374;	/* fbUtility */
          char  fb_375 [253];	/* RDB$PACKAGE_NAME */
   } fb_373;
   struct fb_378_struct {
          short fb_379;	/* fbUtility */
          char  fb_380 [253];	/* RDB$PROCEDURE_NAME */
   } fb_378;
   struct fb_383_struct {
          short fb_384;	/* fbUtility */
          char  fb_385 [253];	/* RDB$RELATION_NAME */
   } fb_383;
   struct fb_388_struct {
          short fb_389;	/* fbUtility */
          short fb_390;	/* gds__null_flag */
          short fb_391;	/* RDB$SYSTEM_FLAG */
          char  fb_392 [253];	/* RDB$ROLE_NAME */
          char  fb_393 [253];	/* RDB$OWNER_NAME */
   } fb_388;
/**************************************
 *
 *	l i s t _ a l l _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Print the permissions on all user tables.
 *
 *	Get separate permissions on table/views and then procedures.
 *
 **************************************/
	bool first_role = true;
	TEXT prev_owner[44];

	// Only extract (not show) wants the role list and with escaped quoted identifiers.
	const bool mangle = show_role_list;

	// Process GRANT roles
	if (isqlGlob.major_ods >= ODS_VERSION9 && show_role_list)
	{
		prev_owner[0] = '\0';

		/*FOR XX IN RDB$ROLES
			SORTED BY XX.RDB$ROLE_NAME*/
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
		   if (fbTrans && fb_386)
		      fb_386->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_386->release(); fb_386 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_386->receive (fbStatus, 0, 0, 512, CAST_MSG(&fb_388));
		   if (!fb_388.fb_389 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			bool system_flag = !/*XX.RDB$SYSTEM_FLAG.NULL*/
					    fb_388.fb_390 && /*XX.RDB$SYSTEM_FLAG*/
    fb_388.fb_391 > 0;

			if (!system_flag)
			{
				if (first_role)
				{
					isqlGlob.printf("%s/* Grant roles for this database */%s", NEWLINE, NEWLINE);
					first_role = false;
				}

				// Null terminate name string
				fb_utils::exact_name(/*XX.RDB$ROLE_NAME*/
						     fb_388.fb_392);
				fb_utils::exact_name(/*XX.RDB$OWNER_NAME*/
						     fb_388.fb_393);

				if (strcmp (prev_owner, /*XX.RDB$OWNER_NAME*/
							fb_388.fb_393) != 0)
				{
					isqlGlob.printf("%s/* Role: %s, Owner: %s */%s",
							 NEWLINE,
							 /*XX.RDB$ROLE_NAME*/
							 fb_388.fb_392,
							 /*XX.RDB$OWNER_NAME*/
							 fb_388.fb_393,
							 NEWLINE);
					strcpy (prev_owner, /*XX.RDB$OWNER_NAME*/
							    fb_388.fb_393);
				}

				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
				{
					IUTILS_copy_SQL_id (/*XX.RDB$ROLE_NAME*/
							    fb_388.fb_392, SQL_identifier, DBL_QUOTE);
					isqlGlob.printf("CREATE ROLE %s", SQL_identifier);
				}
				else
					isqlGlob.printf("CREATE ROLE %s", /*XX.RDB$ROLE_NAME*/
									  fb_388.fb_392);

				SHOW_system_privileges(/*XX.RDB$ROLE_NAME*/
						       fb_388.fb_392, " SET SYSTEM PRIVILEGES TO", false);
				isqlGlob.printf(";%s", NEWLINE);
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
		    return OBJECT_NOT_FOUND;
		/*END_ERROR;*/
		   }
		}
	}

	// This version of cursor gets only sql tables identified by security class
	// and misses views, getting only null view_source

	char banner[100];
	fb_utils::snprintf(banner, sizeof(banner), "%s/* Grant permissions for this database */%s", NEWLINE, NEWLINE);

	bool first = true;

	/*FOR REL IN RDB$RELATIONS WITH
		(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
		REL.RDB$SECURITY_CLASS STARTING "SQL$"
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
	   if (!fb_381 && fbTrans && DB)
	      fb_381 = DB->compileRequest(fbStatus, sizeof(fb_382), fb_382);
	   if (fbTrans && fb_381)
	      fb_381->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_381->release(); fb_381 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_381->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_383));
	   if (!fb_383.fb_384 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// Null terminate name string

		fb_utils::exact_name(/*REL.RDB$RELATION_NAME*/
				     fb_383.fb_385);

		const processing_state rc =
			SHOW_grants2(/*REL.RDB$RELATION_NAME*/
				     fb_383.fb_385, terminator, obj_relation,
						   first ? banner : 0, mangle);
		if (rc == SKIP) {
			first = false;
		}

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return OBJECT_NOT_FOUND;
	/*END_ERROR;*/
	   }
	}

	if (first)
		SHOW_grant_roles2(terminator, &first, banner, mangle);
	else
		SHOW_grant_roles2(terminator, 0, 0, mangle);

	// For stored procedures, but ignore procedures inside packages.
	/*FOR PRC IN RDB$PROCEDURES
		WITH (PRC.RDB$SYSTEM_FLAG NE 1 OR PRC.RDB$SYSTEM_FLAG MISSING)
		AND PRC.RDB$PACKAGE_NAME MISSING
		SORTED BY PRC.RDB$PROCEDURE_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_376 && fbTrans && DB)
	      fb_376 = DB->compileRequest(fbStatus, sizeof(fb_377), fb_377);
	   if (fbTrans && fb_376)
	      fb_376->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_376->release(); fb_376 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_376->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_378));
	   if (!fb_378.fb_379 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// Null terminate name string
		fb_utils::exact_name(/*PRC.RDB$PROCEDURE_NAME*/
				     fb_378.fb_380);

		const processing_state rc =
			SHOW_grants2(/*PRC.RDB$PROCEDURE_NAME*/
				     fb_378.fb_380, terminator, obj_procedure,
							   first ? banner : 0, mangle);
		if (rc == SKIP)
			first = false;

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		return OBJECT_NOT_FOUND;
	/*END_ERROR;*/
	   }
	}

	if (isqlGlob.major_ods >= ODS_VERSION12)
	{
		/*FOR PACK IN RDB$PACKAGES
			WITH (PACK.RDB$SYSTEM_FLAG NE 1 OR PACK.RDB$SYSTEM_FLAG MISSING)
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
		   fb_371->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_373));
		   if (!fb_373.fb_374 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// Null terminate name string
			fb_utils::exact_name(/*PACK.RDB$PACKAGE_NAME*/
					     fb_373.fb_375);
			const processing_state rc =
				SHOW_grants2(/*PACK.RDB$PACKAGE_NAME*/
					     fb_373.fb_375, terminator, obj_package_header,
													first ? banner : 0, mangle);
			if (rc == SKIP)
				first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		/*END_ERROR*/
		   }
		}

		/*FOR FUN IN RDB$FUNCTIONS
			WITH (FUN.RDB$SYSTEM_FLAG NE 1 OR FUN.RDB$SYSTEM_FLAG MISSING) AND
				 FUN.RDB$PACKAGE_NAME MISSING
			SORTED BY FUN.RDB$FUNCTION_NAME*/
		{
		for (int retries = 0; retries < 2; ++retries)
		   {
		   {
		   if (!DB)
		      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
		   if (DB && !fbTrans)
		      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
		   }
		   if (!fb_366 && fbTrans && DB)
		      fb_366 = DB->compileRequest(fbStatus, sizeof(fb_367), fb_367);
		   if (fbTrans && fb_366)
		      fb_366->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_366->release(); fb_366 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_366->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_368));
		   if (!fb_368.fb_369 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// Null terminate name string
			fb_utils::exact_name(/*FUN.RDB$FUNCTION_NAME*/
					     fb_368.fb_370);
			const processing_state rc =
				SHOW_grants2(/*FUN.RDB$FUNCTION_NAME*/
					     fb_368.fb_370, terminator, obj_udf,
													first ? banner : 0, mangle);
			if (rc == SKIP)
				first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		/*END_ERROR*/
		   }
		}

		/*FOR GEN IN RDB$GENERATORS
			WITH (GEN.RDB$SYSTEM_FLAG NE 1 OR GEN.RDB$SYSTEM_FLAG MISSING)
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
		   if (!fb_361 && fbTrans && DB)
		      fb_361 = DB->compileRequest(fbStatus, sizeof(fb_362), fb_362);
		   if (fbTrans && fb_361)
		      fb_361->start(fbStatus, fbTrans, 0);
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_361->release(); fb_361 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_361->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_363));
		   if (!fb_363.fb_364 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// Null terminate name string
			fb_utils::exact_name(/*GEN.RDB$GENERATOR_NAME*/
					     fb_363.fb_365);
			const processing_state rc =
				SHOW_grants2(/*GEN.RDB$GENERATOR_NAME*/
					     fb_363.fb_365, terminator, obj_generator,
													 first ? banner : 0, mangle);
			if (rc == SKIP)
				first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		/*END_ERROR*/
		   }
		}

		/*FOR XCP IN RDB$EXCEPTIONS
			WITH (XCP.RDB$SYSTEM_FLAG NE 1 OR XCP.RDB$SYSTEM_FLAG MISSING)
			SORTED BY XCP.RDB$EXCEPTION_NAME*/
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
		   fb_356->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_358));
		   if (!fb_358.fb_359 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// Null terminate name string
			fb_utils::exact_name(/*XCP.RDB$EXCEPTION_NAME*/
					     fb_358.fb_360);
			const processing_state rc =
				SHOW_grants2(/*XCP.RDB$EXCEPTION_NAME*/
					     fb_358.fb_360, terminator, obj_exception,
													 first ? banner : 0, mangle);
			if (rc == SKIP)
				first = false;
		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		/*END_ERROR*/
		   }
		}

		/***
		FOR FLD IN RDB$FIELDS WITH
			FLD.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
			AND FLD.RDB$SYSTEM_FLAG NE 1
			SORTED BY FLD.RDB$FIELD_NAME
		{
			// Null terminate name string
			fb_utils::exact_name(FLD.RDB$FIELD_NAME);
			const processing_state rc =
				SHOW_grants2(FLD.RDB$FIELD_NAME, terminator, obj_field,
					(first ? banner : 0), mangle);

			if (rc == SKIP)
				first = false;
		}
		END_FOR
		ON_ERROR
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		END_ERROR

		FOR CS IN RDB$CHARACTER_SETS
			SORTED BY CS.RDB$CHARACTER_SET_NAME
		{
			// Null terminate name string
			fb_utils::exact_name(CS.RDB$CHARACTER_SET_NAME);
			const processing_state rc =
				SHOW_grants2(CS.RDB$CHARACTER_SET_NAME, terminator, obj_charset,
					(first ? banner : 0), mangle);

			if (rc == SKIP)
				first = false;
		}
		END_FOR
		ON_ERROR
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		END_ERROR

		FOR COL IN RDB$COLLATIONS
			SORTED BY COL.RDB$COLLATION_NAME
		{
			// Null terminate name string
			fb_utils::exact_name(COL.RDB$COLLATION_NAME);
			const processing_state rc =
				SHOW_grants2(COL.RDB$COLLATION_NAME, terminator, obj_collation,
					(first ? banner : 0), mangle);

			if (rc == SKIP)
				first = false;
		}
		END_FOR
		ON_ERROR
			ISQL_errmsg(fbStatus);
			return OBJECT_NOT_FOUND;
		END_ERROR
		***/

		// Process DDL permissions
		for (SSHORT obj = obj_database; obj < obj_type_MAX; obj++)
		{
			if (!isDdlObject(obj))
				continue;

			const processing_state rc =
				SHOW_grants2(getSecurityClassName(obj), terminator, obj, first ? banner : 0, mangle);
			if (rc == SKIP)
				first = false;
		}
	}

	return first_role && first ? OBJECT_NOT_FOUND : SKIP;
}


static void print_proc_prefix(int obj_type, bool headerOnly)
{
	if (obj_type == obj_procedure || obj_type == obj_udf ||
		obj_type == obj_package_header || obj_type == obj_package_body)
	{
		isqlGlob.printf("%sCOMMIT WORK%s%s", NEWLINE, isqlGlob.global_Term, NEWLINE);
	}

	if (obj_type != obj_trigger)
		isqlGlob.printf("SET AUTODDL OFF%s%s", isqlGlob.global_Term, NEWLINE);

	isqlGlob.printf("SET TERM %s %s%s", Procterm, isqlGlob.global_Term, NEWLINE);

	const char* legend = NULL;
	switch (obj_type)
	{
	case obj_procedure:
		legend = headerOnly ? "Stored procedures headers" : "Stored procedures bodies";
		break;
	case obj_udf:
		legend = headerOnly ? "Stored functions headers" : "Stored functions bodies";
		break;
	case obj_trigger:
		legend = "Triggers only will work for SQL triggers";
		break;
	case obj_package_header:
		legend = "Package headers";
		break;
	case obj_package_body:
		legend = "Package bodies";
		break;
	}
	if (legend)
		isqlGlob.printf("%s/* %s */%s", NEWLINE, legend, NEWLINE);
}


static void print_proc_suffix(int obj_type)
{
	isqlGlob.printf("%sSET TERM %s %s%s", NEWLINE, isqlGlob.global_Term, Procterm, NEWLINE);
	isqlGlob.printf("COMMIT WORK%s%s", isqlGlob.global_Term, NEWLINE);

	if (obj_type != obj_trigger)
		isqlGlob.printf("SET AUTODDL ON%s%s", isqlGlob.global_Term, NEWLINE);
}

static void list_procedure_headers(SSHORT default_char_set_id)
{
   struct fb_351_struct {
          short fb_352;	/* fbUtility */
          short fb_353;	/* gds__null_flag */
          short fb_354;	/* RDB$PROCEDURE_TYPE */
          char  fb_355 [253];	/* RDB$PROCEDURE_NAME */
   } fb_351;
/**************************************
 *
 *	l i s t _ p r o c e d u r e _ h e a d e r s
 *
 **************************************
 *
 * Functional description
 * Create all procedure declarations
 * with empty body. This will allow us to create objects
 * that depend on them. The alteration to their
 * correct form is postponed to list_procedure_bodies.
 * Add the parameter names when these procedures are created.
 *
 **************************************/

	fb_assert(isqlGlob.major_ods >= ODS_VERSION12);

	bool header = true;
	static const char* const create_procedure	= "CREATE OR ALTER PROCEDURE %s ";
	static const char* const body_execut_proc	= "BEGIN EXIT; END %s%s";
	static const char* const body_select_proc	= "BEGIN SUSPEND; END %s%s";

	//  First the dummy procedures
	// create the procedures with their parameters

	/*FOR PRC IN RDB$PROCEDURES
		WITH (PRC.RDB$SYSTEM_FLAG NE 1 OR PRC.RDB$SYSTEM_FLAG MISSING)
		AND PRC.RDB$PACKAGE_NAME MISSING
		SORTED BY PRC.RDB$PROCEDURE_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_349 && fbTrans && DB)
	      fb_349 = DB->compileRequest(fbStatus, sizeof(fb_350), fb_350);
	   if (fbTrans && fb_349)
	      fb_349->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_349->release(); fb_349 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_349->receive (fbStatus, 0, 0, 259, CAST_MSG(&fb_351));
	   if (!fb_351.fb_352 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		if (header)
		{
			print_proc_prefix(obj_procedure, true);
			header = false;
		}
		fb_utils::exact_name(/*PRC.RDB$PROCEDURE_NAME*/
				     fb_351.fb_355);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*PRC.RDB$PROCEDURE_NAME*/
					    fb_351.fb_355, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf(create_procedure, SQL_identifier);
		}
		else
		{
			isqlGlob.printf(create_procedure, /*PRC.RDB$PROCEDURE_NAME*/
							  fb_351.fb_355, default_char_set_id);
		}

		get_procedure_args(/*PRC.RDB$PROCEDURE_NAME*/
				   fb_351.fb_355, default_char_set_id);
		isqlGlob.printf("AS %s", NEWLINE);

		prc_t proc_type = /*PRC.RDB$PROCEDURE_TYPE.NULL*/
				  fb_351.fb_353 ? prc_legacy : (prc_t) /*PRC.RDB$PROCEDURE_TYPE*/
			fb_351.fb_354;

		// We'll assume for safety that prc_legacy can be mapped to prc_selectable.
		if (proc_type == prc_executable)
			isqlGlob.printf(body_execut_proc, Procterm, NEWLINE);
		else
			isqlGlob.printf(body_select_proc, Procterm, NEWLINE);

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
	// Only reset the terminators if there were procs to print
	if (!header)
		print_proc_suffix(obj_procedure);
}



static void list_procedure_bodies(SSHORT default_char_set_id)
{
   struct fb_338_struct {
          ISC_QUAD fb_339;	/* RDB$PROCEDURE_SOURCE */
          char  fb_340 [256];	/* RDB$ENTRYPOINT */
          short fb_341;	/* fbUtility */
          short fb_342;	/* gds__null_flag */
          short fb_343;	/* gds__null_flag */
          short fb_344;	/* gds__null_flag */
          short fb_345;	/* gds__null_flag */
          char  fb_346 [253];	/* RDB$PROCEDURE_NAME */
          FB_BOOLEAN fb_347;	/* RDB$SQL_SECURITY */
          char  fb_348 [253];	/* RDB$ENGINE_NAME */
   } fb_338;
/**************************************
 *
 *	l i s t _  p r o c e d u r e _ b o d i e s
 *
 **************************************
 *
 * Functional description
 *	Fill them with life
 *
 **************************************/

	fb_assert(isqlGlob.major_ods >= ODS_VERSION12);

	bool header = true;

	// This query gets the procedure name and the source.  We then nest a query
	// to retrieve the parameters. Alter is used, because the procedures are already there
	TEXT msg[MSG_LENGTH];

	/*FOR PRC IN RDB$PROCEDURES
		WITH (PRC.RDB$SYSTEM_FLAG NE 1 OR PRC.RDB$SYSTEM_FLAG MISSING)
		AND PRC.RDB$PACKAGE_NAME MISSING
		SORTED BY PRC.RDB$PROCEDURE_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_336 && fbTrans && DB)
	      fb_336 = DB->compileRequest(fbStatus, sizeof(fb_337), fb_337);
	   if (fbTrans && fb_336)
	      fb_336->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_336->release(); fb_336 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_336->receive (fbStatus, 0, 0, 781, CAST_MSG(&fb_338));
	   if (!fb_338.fb_341 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		if (header)
		{
			print_proc_prefix(obj_procedure, false);
			header = false;
		}
		fb_utils::exact_name(/*PRC.RDB$PROCEDURE_NAME*/
				     fb_338.fb_346);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*PRC.RDB$PROCEDURE_NAME*/
					    fb_338.fb_346, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("%sALTER PROCEDURE %s ", NEWLINE, SQL_identifier);
		}
		else
			isqlGlob.printf("%sALTER PROCEDURE %s ", NEWLINE, /*PRC.RDB$PROCEDURE_NAME*/
									  fb_338.fb_346);

		get_procedure_args(/*PRC.RDB$PROCEDURE_NAME*/
				   fb_338.fb_346, default_char_set_id);

		// Print the procedure body

		if (!/*PRC.RDB$ENTRYPOINT.NULL*/
		     fb_338.fb_345)
		{
			fb_utils::exact_name(/*PRC.RDB$ENTRYPOINT*/
					     fb_338.fb_340);
			IUTILS_copy_SQL_id(/*PRC.RDB$ENTRYPOINT*/
					   fb_338.fb_340, SQL_identifier2, SINGLE_QUOTE);
			isqlGlob.printf("EXTERNAL NAME %s%s", SQL_identifier2, NEWLINE);
		}

		if (!/*PRC.RDB$SQL_SECURITY.NULL*/
		     fb_338.fb_344)
		{
			const char* ss = /*PRC.RDB$SQL_SECURITY*/
					 fb_338.fb_347 ? "SQL SECURITY DEFINER" : "SQL SECURITY INVOKER";
			isqlGlob.printf("%s%s", ss, NEWLINE);
		}

		if (!/*PRC.RDB$ENGINE_NAME.NULL*/
		     fb_338.fb_343)
		{
			fb_utils::exact_name(/*PRC.RDB$ENGINE_NAME*/
					     fb_338.fb_348);
			isqlGlob.printf("ENGINE %s", /*PRC.RDB$ENGINE_NAME*/
						     fb_338.fb_348);

			if (!/*PRC.RDB$PROCEDURE_SOURCE.NULL*/
			     fb_338.fb_342)
			{
				isqlGlob.printf("%sAS '", NEWLINE);
				SHOW_print_metadata_text_blob(isqlGlob.Out, &/*PRC.RDB$PROCEDURE_SOURCE*/
									     fb_338.fb_339, true);
				isqlGlob.printf("'%s", NEWLINE);
			}
		}
		else if (!/*PRC.RDB$PROCEDURE_SOURCE.NULL*/
			  fb_338.fb_342)
		{
			isqlGlob.printf("AS %s", NEWLINE);
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*PRC.RDB$PROCEDURE_SOURCE*/
								     fb_338.fb_339);
		}

		isqlGlob.printf(" %s%s", Procterm, NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		IUTILS_msg_get(GEN_ERR, msg, SafeArg() << isc_sqlcode(fbStatus->getErrors()));
		STDERROUT(msg);			// Statement failed, SQLCODE = %d\n\n
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}

	// Only reset the terminators if there were procs to print
	if (!header)
		print_proc_suffix(obj_procedure);
}



static void list_all_tables(LegacyTables flag, SSHORT default_char_set_id)
{
   struct fb_330_struct {
          short fb_331;	/* fbUtility */
          short fb_332;	/* gds__null_flag */
          short fb_333;	/* RDB$FLAGS */
          char  fb_334 [253];	/* RDB$RELATION_NAME */
          char  fb_335 [253];	/* RDB$SECURITY_CLASS */
   } fb_330;
/**************************************
 *
 *	l i s t _ a l l _ t a b l e s
 *
 **************************************
 *
 * Functional description
 *	Extract the names of all user tables from
 *	rdb$relations.  Filter SQL tables by
 *	security class after we fetch them
 *	Parameters:  flag -- 0, get all tables
 *
 **************************************/

	// This version of cursor gets only sql tables identified by security class
	// and misses views, getting only null view_source

	/*FOR REL IN RDB$RELATIONS WITH
		(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
		REL.RDB$VIEW_BLR MISSING
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
	   if (!fb_328 && fbTrans && DB)
	      fb_328 = DB->compileRequest(fbStatus, sizeof(fb_329), fb_329);
	   if (fbTrans && fb_328)
	      fb_328->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_328->release(); fb_328 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_328->receive (fbStatus, 0, 0, 512, CAST_MSG(&fb_330));
	   if (!fb_330.fb_331 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// If this is not an SQL table and we aren't doing ALL objects
		if ((/*REL.RDB$FLAGS.NULL*/
		     fb_330.fb_332 || !(/*REL.RDB$FLAGS*/
      fb_330.fb_333 & REL_sql)) && (flag != ALL_objects) )
			continue;
		// Null terminate name string

		fb_utils::exact_name(/*REL.RDB$RELATION_NAME*/
				     fb_330.fb_334);

		if (flag || !strncmp (/*REL.RDB$SECURITY_CLASS*/
				      fb_330.fb_335, "SQL$", 4))
			EXTRACT_list_table (/*REL.RDB$RELATION_NAME*/
					    fb_330.fb_334, NULL,  false, default_char_set_id);
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		/*ROLLBACK;*/
		{
		fbTrans->rollback(fbStatus);
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS))
		   fbTrans = 0;
		}
		return;
	/*END_ERROR;*/
	   }
	}

}


static void list_all_triggers()
{
   struct fb_293_struct {
          ISC_QUAD fb_294;	/* RDB$TRIGGER_SOURCE */
          char  fb_295 [256];	/* RDB$ENTRYPOINT */
          ISC_INT64 fb_296;	/* RDB$TRIGGER_TYPE */
          short fb_297;	/* fbUtility */
          short fb_298;	/* gds__null_flag */
          short fb_299;	/* gds__null_flag */
          short fb_300;	/* gds__null_flag */
          short fb_301;	/* gds__null_flag */
          short fb_302;	/* RDB$TRIGGER_SEQUENCE */
          short fb_303;	/* RDB$FLAGS */
          short fb_304;	/* gds__null_flag */
          short fb_305;	/* RDB$TRIGGER_INACTIVE */
          char  fb_306 [253];	/* RDB$TRIGGER_NAME */
          char  fb_307 [253];	/* RDB$RELATION_NAME */
          FB_BOOLEAN fb_308;	/* RDB$SQL_SECURITY */
          char  fb_309 [253];	/* RDB$ENGINE_NAME */
   } fb_293;
   struct fb_312_struct {
          ISC_QUAD fb_313;	/* RDB$TRIGGER_SOURCE */
          char  fb_314 [256];	/* RDB$ENTRYPOINT */
          ISC_INT64 fb_315;	/* RDB$TRIGGER_TYPE */
          short fb_316;	/* fbUtility */
          short fb_317;	/* gds__null_flag */
          short fb_318;	/* gds__null_flag */
          short fb_319;	/* gds__null_flag */
          short fb_320;	/* gds__null_flag */
          short fb_321;	/* RDB$TRIGGER_SEQUENCE */
          short fb_322;	/* RDB$FLAGS */
          short fb_323;	/* gds__null_flag */
          short fb_324;	/* RDB$TRIGGER_INACTIVE */
          char  fb_325 [253];	/* RDB$TRIGGER_NAME */
          FB_BOOLEAN fb_326;	/* RDB$SQL_SECURITY */
          char  fb_327 [253];	/* RDB$ENGINE_NAME */
   } fb_312;
/**************************************
 *
 *	l i s t _ a l l _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Lists triggers in general on non-system
 *	tables with sql source only.
 *
 **************************************/
	bool header = true;

	// Query gets the trigger info for non-system triggers with
	// source that are not part of an SQL constraint.

	/*FOR TRG IN RDB$TRIGGERS
		WITH (TRG.RDB$SYSTEM_FLAG EQ 0 OR TRG.RDB$SYSTEM_FLAG MISSING) AND
			 TRG.RDB$RELATION_NAME MISSING
		SORTED BY TRG.RDB$TRIGGER_TYPE, TRG.RDB$TRIGGER_SEQUENCE, TRG.RDB$TRIGGER_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_310 && fbTrans && DB)
	      fb_310 = DB->compileRequest(fbStatus, sizeof(fb_311), fb_311);
	   if (fbTrans && fb_310)
	      fb_310->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_310->release(); fb_310 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_310->receive (fbStatus, 0, 0, 797, CAST_MSG(&fb_312));
	   if (!fb_312.fb_316 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (header)
		{
			print_proc_prefix(obj_trigger, false);
			header = false;
		}
		fb_utils::exact_name(/*TRG.RDB$TRIGGER_NAME*/
				     fb_312.fb_325);

		if (/*TRG.RDB$TRIGGER_INACTIVE.NULL*/
		    fb_312.fb_323)
			/*TRG.RDB$TRIGGER_INACTIVE*/
			fb_312.fb_324 = 0;

		//  If trigger is not SQL put it in comments
		if (!(/*TRG.RDB$FLAGS*/
		      fb_312.fb_322 & TRG_sql))
			isqlGlob.printf("/* ");

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*TRG.RDB$TRIGGER_NAME*/
					    fb_312.fb_325, SQL_identifier,  DBL_QUOTE);
		else
			strcpy (SQL_identifier,  /*TRG.RDB$TRIGGER_NAME*/
						 fb_312.fb_325);

		isqlGlob.printf("CREATE TRIGGER %s %s%s %s POSITION %d %s",
				 SQL_identifier, NEWLINE,
				 (/*TRG.RDB$TRIGGER_INACTIVE*/
				  fb_312.fb_324 ? "INACTIVE" : "ACTIVE"),
				 SHOW_trigger_action(/*TRG.RDB$TRIGGER_TYPE*/
						     fb_312.fb_315).c_str(), /*TRG.RDB$TRIGGER_SEQUENCE*/
	   fb_312.fb_321,
				 NEWLINE);

		if (!/*TRG.RDB$SQL_SECURITY.NULL*/
		     fb_312.fb_320)
		{
			const char* ss = /*TRG.RDB$SQL_SECURITY*/
					 fb_312.fb_326 ? "SQL SECURITY DEFINER" : "SQL SECURITY INVOKER";
			isqlGlob.printf("%s%s", ss, NEWLINE);
		}

		if (!/*TRG.RDB$ENTRYPOINT.NULL*/
		     fb_312.fb_319)
		{
			fb_utils::exact_name(/*TRG.RDB$ENTRYPOINT*/
					     fb_312.fb_314);
			IUTILS_copy_SQL_id(/*TRG.RDB$ENTRYPOINT*/
					   fb_312.fb_314, SQL_identifier2, SINGLE_QUOTE);
			isqlGlob.printf("EXTERNAL NAME %s%s", SQL_identifier2, NEWLINE);
		}

		if (!/*TRG.RDB$ENGINE_NAME.NULL*/
		     fb_312.fb_318)
		{
			fb_utils::exact_name(/*TRG.RDB$ENGINE_NAME*/
					     fb_312.fb_327);
			isqlGlob.printf("ENGINE %s", /*TRG.RDB$ENGINE_NAME*/
						     fb_312.fb_327);

			if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
			     fb_312.fb_317)
			{
				isqlGlob.printf("%sAS '", NEWLINE);
				SHOW_print_metadata_text_blob(isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
									     fb_312.fb_313, true);
				isqlGlob.printf("'%s", NEWLINE);
			}
		}
		else if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
			  fb_312.fb_317)
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
								     fb_312.fb_313);

		isqlGlob.printf(" %s%s", Procterm, NEWLINE);
		isqlGlob.printf(NEWLINE);

		if (!(/*TRG.RDB$FLAGS*/
		      fb_312.fb_322 & TRG_sql))
		{
			isqlGlob.printf("*/%s", NEWLINE);
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

	/*FOR TRG IN RDB$TRIGGERS CROSS REL IN RDB$RELATIONS OVER RDB$RELATION_NAME
		//WITH (REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
		//NOT (ANY CHK IN RDB$CHECK_CONSTRAINTS WITH
		//TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME)
		WITH (TRG.RDB$SYSTEM_FLAG EQ 0 OR TRG.RDB$SYSTEM_FLAG MISSING)
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
	   if (!fb_291 && fbTrans && DB)
	      fb_291 = DB->compileRequest(fbStatus, sizeof(fb_292), fb_292);
	   if (fbTrans && fb_291)
	      fb_291->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_291->release(); fb_291 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_291->receive (fbStatus, 0, 0, 1050, CAST_MSG(&fb_293));
	   if (!fb_293.fb_297 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (header)
		{
			isqlGlob.printf("SET TERM %s %s%s", Procterm, isqlGlob.global_Term, NEWLINE);
			isqlGlob.printf("%s/* Triggers only will work for SQL triggers */%s",
					 NEWLINE,
					 NEWLINE);
			header = false;
		}
		fb_utils::exact_name(/*TRG.RDB$TRIGGER_NAME*/
				     fb_293.fb_306);
		fb_utils::exact_name(/*TRG.RDB$RELATION_NAME*/
				     fb_293.fb_307);

		if (/*TRG.RDB$TRIGGER_INACTIVE.NULL*/
		    fb_293.fb_304)
			/*TRG.RDB$TRIGGER_INACTIVE*/
			fb_293.fb_305 = 0;

		//  If trigger is not SQL put it in comments
		if (!(/*TRG.RDB$FLAGS*/
		      fb_293.fb_303 & TRG_sql))
			isqlGlob.printf("/* ");

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*TRG.RDB$TRIGGER_NAME*/
					    fb_293.fb_306, SQL_identifier,  DBL_QUOTE);
			IUTILS_copy_SQL_id (/*TRG.RDB$RELATION_NAME*/
					    fb_293.fb_307, SQL_identifier2, DBL_QUOTE);
		}
		else
		{
			strcpy (SQL_identifier,  /*TRG.RDB$TRIGGER_NAME*/
						 fb_293.fb_306);
			strcpy (SQL_identifier2, /*TRG.RDB$RELATION_NAME*/
						 fb_293.fb_307);
		}

		isqlGlob.printf("CREATE TRIGGER %s FOR %s %s%s %s POSITION %d %s",
				 SQL_identifier, SQL_identifier2, NEWLINE,
				 (/*TRG.RDB$TRIGGER_INACTIVE*/
				  fb_293.fb_305 ? "INACTIVE" : "ACTIVE"),
				 SHOW_trigger_action(/*TRG.RDB$TRIGGER_TYPE*/
						     fb_293.fb_296).c_str(), /*TRG.RDB$TRIGGER_SEQUENCE*/
	   fb_293.fb_302,
				 NEWLINE);

		if (!/*TRG.RDB$SQL_SECURITY.NULL*/
		     fb_293.fb_301)
		{
			const char* ss = /*TRG.RDB$SQL_SECURITY*/
					 fb_293.fb_308 ? "SQL SECURITY DEFINER" : "SQL SECURITY INVOKER";
			isqlGlob.printf("%s%s", ss, NEWLINE);
		}

		if (!/*TRG.RDB$ENTRYPOINT.NULL*/
		     fb_293.fb_300)
		{
			fb_utils::exact_name(/*TRG.RDB$ENTRYPOINT*/
					     fb_293.fb_295);
			IUTILS_copy_SQL_id(/*TRG.RDB$ENTRYPOINT*/
					   fb_293.fb_295, SQL_identifier2, SINGLE_QUOTE);
			isqlGlob.printf("EXTERNAL NAME %s%s", SQL_identifier2, NEWLINE);
		}

		if (!/*TRG.RDB$ENGINE_NAME.NULL*/
		     fb_293.fb_299)
		{
			fb_utils::exact_name(/*TRG.RDB$ENGINE_NAME*/
					     fb_293.fb_309);
			isqlGlob.printf("ENGINE %s", /*TRG.RDB$ENGINE_NAME*/
						     fb_293.fb_309);

			if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
			     fb_293.fb_298)
			{
				isqlGlob.printf("%sAS '", NEWLINE);
				SHOW_print_metadata_text_blob(isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
									     fb_293.fb_294, true);
				isqlGlob.printf("'%s", NEWLINE);
			}
		}
		else if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
			  fb_293.fb_298)
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
								     fb_293.fb_294);

		isqlGlob.printf(" %s%s", Procterm, NEWLINE);
		isqlGlob.printf(NEWLINE);

		if (!(/*TRG.RDB$FLAGS*/
		      fb_293.fb_303 & TRG_sql))
		{
			isqlGlob.printf("*/%s", NEWLINE);
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

	if (!header)
		print_proc_suffix(obj_trigger);
}


static void list_check()
{
   struct fb_285_struct {
          ISC_QUAD fb_286;	/* RDB$TRIGGER_SOURCE */
          short fb_287;	/* fbUtility */
          short fb_288;	/* gds__null_flag */
          char  fb_289 [253];	/* RDB$RELATION_NAME */
          char  fb_290 [253];	/* RDB$CONSTRAINT_NAME */
   } fb_285;
   struct fb_283_struct {
          short fb_284;	/* RDB$SYSTEM_FLAG */
   } fb_283;
/**************************************
 *
 *	l i s t _ c h e c k
 *
 **************************************
 *
 * Functional description
 *	List check constraints for all objects to allow forward references
 *
 **************************************/

	// Query gets the check clauses for triggers stored for check constraints

	bool first = true;

	/*FOR TRG IN RDB$TRIGGERS CROSS
		CHK IN RDB$CHECK_CONSTRAINTS WITH
		TRG.RDB$TRIGGER_TYPE EQ 1 AND
		TRG.RDB$SYSTEM_FLAG EQ int(fb_sysflag_check_constraint) AND
		TRG.RDB$TRIGGER_NAME EQ CHK.RDB$TRIGGER_NAME AND
		(ANY RELC IN RDB$RELATION_CONSTRAINTS WITH
		CHK.RDB$CONSTRAINT_NAME EQ RELC.RDB$CONSTRAINT_NAME
		AND RELC.RDB$CONSTRAINT_TYPE EQ "CHECK")
		REDUCED TO CHK.RDB$CONSTRAINT_NAME
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
	   if (!fb_281 && fbTrans && DB)
	      fb_281 = DB->compileRequest(fbStatus, sizeof(fb_282), fb_282);
	   fb_283.fb_284 = int(fb_sysflag_check_constraint);
	   if (fbTrans && fb_281)
	      fb_281->startAndSend(fbStatus, fbTrans, 0, 0, 2, CAST_CONST_MSG(&fb_283));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_281->release(); fb_281 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_281->receive (fbStatus, 0, 1, 518, CAST_MSG(&fb_285));
	   if (!fb_285.fb_287 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("%s/* Table constraints */%s", NEWLINE, NEWLINE);
			first = false;
		}

		fb_utils::exact_name(/*TRG.RDB$RELATION_NAME*/
				     fb_285.fb_289);

		isqlGlob.printf(NEWLINE);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*TRG.RDB$RELATION_NAME*/
					    fb_285.fb_289, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("ALTER TABLE %s ADD %s%s", SQL_identifier, NEWLINE, TAB_AS_SPACES);
		}
		else
			isqlGlob.printf("ALTER TABLE %s ADD %s%s", /*TRG.RDB$RELATION_NAME*/
								   fb_285.fb_289, NEWLINE, TAB_AS_SPACES);

		// If the name of the constraint is not INTEG_..., print it
		if (!fb_utils::implicit_integrity(/*CHK.RDB$CONSTRAINT_NAME*/
						  fb_285.fb_290))
		{
			fb_utils::exact_name(/*CHK.RDB$CONSTRAINT_NAME*/
					     fb_285.fb_290);
			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				IUTILS_copy_SQL_id (/*CHK.RDB$CONSTRAINT_NAME*/
						    fb_285.fb_290, SQL_identifier, DBL_QUOTE);
				isqlGlob.printf("CONSTRAINT %s ", SQL_identifier);
			}
			else
				isqlGlob.printf("CONSTRAINT %s ", /*CHK.RDB$CONSTRAINT_NAME*/
								  fb_285.fb_290);
		}

		if (!/*TRG.RDB$TRIGGER_SOURCE.NULL*/
		     fb_285.fb_288)
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*TRG.RDB$TRIGGER_SOURCE*/
								      fb_285.fb_286);

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);

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


static void list_charsets()
{
   struct fb_277_struct {
          short fb_278;	/* fbUtility */
          char  fb_279 [253];	/* RDB$CHARACTER_SET_NAME */
          char  fb_280 [253];	/* RDB$DEFAULT_COLLATE_NAME */
   } fb_277;
/**************************************
 *
 *	l i s t _ c h a r s e t s
 *
 **************************************
 *
 * Functional description
 *	Alter character sets.
 *
 **************************************/

	bool first = true;

	/*FOR CS IN RDB$CHARACTER_SETS
		WITH NOT CS.RDB$CHARACTER_SET_NAME EQ CS.RDB$DEFAULT_COLLATE_NAME
		SORTED BY CS.RDB$CHARACTER_SET_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_275 && fbTrans && DB)
	      fb_275 = DB->compileRequest(fbStatus, sizeof(fb_276), fb_276);
	   if (fbTrans && fb_275)
	      fb_275->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_275->release(); fb_275 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_275->receive (fbStatus, 0, 0, 508, CAST_MSG(&fb_277));
	   if (!fb_277.fb_278 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("%s/*  Character sets */%s", NEWLINE, NEWLINE);
			first = false;
		}

		isqlGlob.printf("ALTER CHARACTER SET ");

		fb_utils::exact_name(/*CS.RDB$CHARACTER_SET_NAME*/
				     fb_277.fb_279);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*CS.RDB$CHARACTER_SET_NAME*/
					    fb_277.fb_279, SQL_identifier, DBL_QUOTE);
		else
			strcpy (SQL_identifier, /*CS.RDB$CHARACTER_SET_NAME*/
						fb_277.fb_279);
		isqlGlob.printf("%s", SQL_identifier);

		fb_utils::exact_name(/*CS.RDB$DEFAULT_COLLATE_NAME*/
				     fb_277.fb_280);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*CS.RDB$DEFAULT_COLLATE_NAME*/
					    fb_277.fb_280, SQL_identifier, DBL_QUOTE);
		else
			strcpy (SQL_identifier, /*CS.RDB$DEFAULT_COLLATE_NAME*/
						fb_277.fb_280);
		isqlGlob.printf(" SET DEFAULT COLLATION %s", SQL_identifier);

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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

	if (! first)
		isqlGlob.printf(NEWLINE);
}


static void list_collations()
{
   struct fb_265_struct {
          ISC_QUAD fb_266;	/* RDB$SPECIFIC_ATTRIBUTES */
          short fb_267;	/* fbUtility */
          short fb_268;	/* gds__null_flag */
          short fb_269;	/* gds__null_flag */
          short fb_270;	/* RDB$COLLATION_ATTRIBUTES */
          short fb_271;	/* gds__null_flag */
          char  fb_272 [253];	/* RDB$COLLATION_NAME */
          char  fb_273 [253];	/* RDB$CHARACTER_SET_NAME */
          char  fb_274 [253];	/* RDB$BASE_COLLATION_NAME */
   } fb_265;
/**************************************
 *
 *	l i s t _ c o l l a t i o n s
 *
 **************************************
 *
 * Functional description
 *	Re create all non-system collations
 *
 **************************************/

	if (isqlGlob.major_ods < ODS_VERSION11)
		return;

	bool first = true;

	/*FOR CL IN RDB$COLLATIONS CROSS
		CS IN RDB$CHARACTER_SETS WITH
		CS.RDB$CHARACTER_SET_ID EQ CL.RDB$CHARACTER_SET_ID AND
		(CL.RDB$SYSTEM_FLAG MISSING OR CL.RDB$SYSTEM_FLAG NE 1)
		SORTED BY CS.RDB$CHARACTER_SET_NAME, CL.RDB$COLLATION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_263 && fbTrans && DB)
	      fb_263 = DB->compileRequest(fbStatus, sizeof(fb_264), fb_264);
	   if (fbTrans && fb_263)
	      fb_263->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_263->release(); fb_263 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_263->receive (fbStatus, 0, 0, 777, CAST_MSG(&fb_265));
	   if (!fb_265.fb_267 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("%s/*  Collations */%s", NEWLINE, NEWLINE);
			first = false;
		}

		isqlGlob.printf("CREATE COLLATION ");

		fb_utils::exact_name(/*CL.RDB$COLLATION_NAME*/
				     fb_265.fb_272);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*CL.RDB$COLLATION_NAME*/
					    fb_265.fb_272, SQL_identifier, DBL_QUOTE);
		else
			strcpy (SQL_identifier, /*CL.RDB$COLLATION_NAME*/
						fb_265.fb_272);
		isqlGlob.printf("%s", SQL_identifier);

		fb_utils::exact_name(/*CS.RDB$CHARACTER_SET_NAME*/
				     fb_265.fb_273);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*CS.RDB$CHARACTER_SET_NAME*/
					    fb_265.fb_273, SQL_identifier, DBL_QUOTE);
		else
			strcpy (SQL_identifier, /*CS.RDB$CHARACTER_SET_NAME*/
						fb_265.fb_273);
		isqlGlob.printf(" FOR %s", SQL_identifier);

		if (!/*CL.RDB$BASE_COLLATION_NAME.NULL*/
		     fb_265.fb_271)
		{
			fb_utils::exact_name(/*CL.RDB$BASE_COLLATION_NAME*/
					     fb_265.fb_274);
			IUTILS_copy_SQL_id (/*CL.RDB$BASE_COLLATION_NAME*/
					    fb_265.fb_274, SQL_identifier, SINGLE_QUOTE);
			isqlGlob.printf(" FROM EXTERNAL (%s)", SQL_identifier);
		}

		if (!/*CL.RDB$COLLATION_ATTRIBUTES.NULL*/
		     fb_265.fb_269)
		{
			if (/*CL.RDB$COLLATION_ATTRIBUTES*/
			    fb_265.fb_270 & TEXTTYPE_ATTR_PAD_SPACE)
				isqlGlob.printf(" PAD SPACE");

			if (/*CL.RDB$COLLATION_ATTRIBUTES*/
			    fb_265.fb_270 & TEXTTYPE_ATTR_CASE_INSENSITIVE)
				isqlGlob.printf(" CASE INSENSITIVE");

			if (/*CL.RDB$COLLATION_ATTRIBUTES*/
			    fb_265.fb_270 & TEXTTYPE_ATTR_ACCENT_INSENSITIVE)
				isqlGlob.printf(" ACCENT INSENSITIVE");
		}

		if (!/*CL.RDB$SPECIFIC_ATTRIBUTES.NULL*/
		     fb_265.fb_268)
		{
			isqlGlob.printf(" '");
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*CL.RDB$SPECIFIC_ATTRIBUTES*/
								      fb_265.fb_266);
			isqlGlob.printf("'");
		}

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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

	isqlGlob.printf(NEWLINE);
}


static void list_create_db()
{
   struct fb_234_struct {
          char  fb_235 [256];	/* RDB$FILE_NAME */
          ISC_LONG fb_236;	/* RDB$FILE_START */
          ISC_LONG fb_237;	/* RDB$FILE_LENGTH */
          short fb_238;	/* fbUtility */
          short fb_239;	/* RDB$SHADOW_NUMBER */
          short fb_240;	/* gds__null_flag */
          short fb_241;	/* gds__null_flag */
          short fb_242;	/* gds__null_flag */
          short fb_243;	/* RDB$FILE_SEQUENCE */
          short fb_244;	/* gds__null_flag */
          short fb_245;	/* gds__null_flag */
          short fb_246;	/* RDB$FILE_FLAGS */
   } fb_234;
   struct fb_251_struct {
          short fb_252;	/* fbUtility */
   } fb_251;
   struct fb_249_struct {
          FB_BOOLEAN fb_250;	/* RDB$SQL_SECURITY */
   } fb_249;
   struct fb_255_struct {
          ISC_LONG fb_256;	/* RDB$LINGER */
          short fb_257;	/* fbUtility */
   } fb_255;
   struct fb_260_struct {
          short fb_261;	/* fbUtility */
          char  fb_262 [253];	/* RDB$CHARACTER_SET_NAME */
   } fb_260;
/**************************************
 *
 *	l i s t _ c r e a t e _ d b
 *
 **************************************
 *
 * Functional description
 *	Print the create database command if requested.  At least put
 *	the page size in a comment with the extracted db name
 *
 **************************************/
	static const UCHAR page_items[] =
	{
		isc_info_page_size,
		isc_info_end
	};

	// Comment out the create database if no db param was specified
	bool nodb = false;
	if (!*isqlGlob.global_Target_db)
	{
		isqlGlob.printf("/* ");
		strcpy(isqlGlob.global_Target_db, isqlGlob.global_Db_name);
		nodb = true;
	}
	isqlGlob.printf("CREATE DATABASE '%s' ", isqlGlob.global_Target_db);

	// Get the page size from db_info call
	std::string info_buf;
	// CVC: Finally I got the idea: translate is associated with WISQL that
	// no longer exists. Localizing the messages means also not printing
	// any CRLF and therefore the output looks ugly.
	const bool translate = true;
	if (SHOW_dbb_parameters(DB, info_buf, page_items, sizeof(page_items), translate, " "))
		isqlGlob.printf("%s", info_buf.c_str());

	/*FOR DBP IN RDB$DATABASE
		WITH DBP.RDB$CHARACTER_SET_NAME NOT MISSING
		AND DBP.RDB$CHARACTER_SET_NAME != " "*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_258 && fbTrans && DB)
	      fb_258 = DB->compileRequest(fbStatus, sizeof(fb_259), fb_259);
	   if (fbTrans && fb_258)
	      fb_258->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_258->release(); fb_258 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_258->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_260));
	   if (!fb_260.fb_261 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		isqlGlob.printf("DEFAULT CHARACTER SET %s%s",
			fb_utils::exact_name(/*DBP.RDB$CHARACTER_SET_NAME*/
					     fb_260.fb_262), isqlGlob.global_Term);
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

	/*FOR DBP2 IN RDB$DATABASE
		WITH DBP2.RDB$LINGER NOT MISSING
		AND DBP2.RDB$LINGER > 0*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_253 && fbTrans && DB)
	      fb_253 = DB->compileRequest(fbStatus, sizeof(fb_254), fb_254);
	   if (fbTrans && fb_253)
	      fb_253->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_253->release(); fb_253 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_253->receive (fbStatus, 0, 0, 6, CAST_MSG(&fb_255));
	   if (!fb_255.fb_257 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		isqlGlob.printf("%sALTER DATABASE SET LINGER TO %d%s", NEWLINE, /*DBP2.RDB$LINGER*/
										fb_255.fb_256,
			isqlGlob.global_Term);
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

	/*FOR DBPSS IN RDB$DATABASE
		WITH DBPSS.RDB$SQL_SECURITY NOT MISSING
		AND DBPSS.RDB$SQL_SECURITY == FB_TRUE*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_247 && fbTrans && DB)
	      fb_247 = DB->compileRequest(fbStatus, sizeof(fb_248), fb_248);
	   fb_249.fb_250 = FB_TRUE;
	   if (fbTrans && fb_247)
	      fb_247->startAndSend(fbStatus, fbTrans, 0, 0, 1, CAST_CONST_MSG(&fb_249));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_247->release(); fb_247 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_247->receive (fbStatus, 0, 1, 2, CAST_MSG(&fb_251));
	   if (!fb_251.fb_252 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		isqlGlob.printf("%sALTER DATABASE SET DEFAULT SQL SECURITY DEFINER%s", NEWLINE, isqlGlob.global_Term);
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

	if (nodb)
		isqlGlob.printf(" */%s", NEWLINE);
	else
		isqlGlob.printf("%s", NEWLINE);

	// List secondary files and shadows as alter db and create shadow in comment

	bool first = true;

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
	   if (!fb_232 && fbTrans && DB)
	      fb_232 = DB->compileRequest(fbStatus, sizeof(fb_233), fb_233);
	   if (fbTrans && fb_232)
	      fb_232->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_232->release(); fb_232 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_232->receive (fbStatus, 0, 0, 282, CAST_MSG(&fb_234));
	   if (!fb_234.fb_238 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// reset nulls to zero
		if (/*FIL.RDB$FILE_FLAGS.NULL*/
		    fb_234.fb_245)
			/*FIL.RDB$FILE_FLAGS*/
			fb_234.fb_246 = 0;
		if (/*FIL.RDB$FILE_LENGTH.NULL*/
		    fb_234.fb_244)
			/*FIL.RDB$FILE_LENGTH*/
			fb_234.fb_237 = 0;
		if (/*FIL.RDB$FILE_SEQUENCE.NULL*/
		    fb_234.fb_242)
			/*FIL.RDB$FILE_SEQUENCE*/
			fb_234.fb_243 = 0;
		if (/*FIL.RDB$FILE_START.NULL*/
		    fb_234.fb_241)
			/*FIL.RDB$FILE_START*/
			fb_234.fb_236 = 0;
		if (!/*FIL.RDB$FILE_NAME.NULL*/
		     fb_234.fb_240)
			fb_utils::exact_name(/*FIL.RDB$FILE_NAME*/
					     fb_234.fb_235);

		// Pure secondary files
		if (/*FIL.RDB$FILE_FLAGS*/
		    fb_234.fb_246 == 0)
		{
			if (first)
			{
				isqlGlob.printf("%s/* Add secondary files in comments %s", NEWLINE, NEWLINE);
				first = false;
			}

			isqlGlob.printf("%sALTER DATABASE ADD FILE '%s'", NEWLINE, /*FIL.RDB$FILE_NAME*/
										   fb_234.fb_235);
			if (/*FIL.RDB$FILE_START*/
			    fb_234.fb_236)
			{
				isqlGlob.printf(" STARTING %ld", /*FIL.RDB$FILE_START*/
								 fb_234.fb_236);
			}
			if (/*FIL.RDB$FILE_LENGTH*/
			    fb_234.fb_237)
			{
				isqlGlob.printf(" LENGTH %ld", /*FIL.RDB$FILE_LENGTH*/
							       fb_234.fb_237);
			}
			isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
		}
		else if (/*FIL.RDB$FILE_FLAGS*/
			 fb_234.fb_246 & FILE_shadow)
		{
			if (first)
			{
				isqlGlob.printf("%s/* Add secondary files in comments %s", NEWLINE, NEWLINE);
				first = false;
			}

			if (/*FIL.RDB$FILE_SEQUENCE*/
			    fb_234.fb_243)
			{
				isqlGlob.printf("%sFILE '%s' ", TAB_AS_SPACES, /*FIL.RDB$FILE_NAME*/
									       fb_234.fb_235);
			}
			else
			{
				isqlGlob.printf("%sCREATE SHADOW %d '%s' ",
						 NEWLINE,
						 /*FIL.RDB$SHADOW_NUMBER*/
						 fb_234.fb_239,
						 /*FIL.RDB$FILE_NAME*/
						 fb_234.fb_235);
				if (/*FIL.RDB$FILE_FLAGS*/
				    fb_234.fb_246 & FILE_inactive)
					isqlGlob.printf("INACTIVE ");
				if (/*FIL.RDB$FILE_FLAGS*/
				    fb_234.fb_246 & FILE_manual)
					isqlGlob.printf("MANUAL ");
				else
					isqlGlob.printf("AUTO ");
				if (/*FIL.RDB$FILE_FLAGS*/
				    fb_234.fb_246 & FILE_conditional)
					isqlGlob.printf("CONDITIONAL ");
			}
			if (/*FIL.RDB$FILE_LENGTH*/
			    fb_234.fb_237)
			{
				isqlGlob.printf("LENGTH %ld ", /*FIL.RDB$FILE_LENGTH*/
							       fb_234.fb_237);
			}
			if (/*FIL.RDB$FILE_START*/
			    fb_234.fb_236)
			{
				isqlGlob.printf("STARTING %ld ", /*FIL.RDB$FILE_START*/
								 fb_234.fb_236);
			}
			isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
		}
		else if ((/*FIL.RDB$FILE_FLAGS*/
			  fb_234.fb_246 & FILE_difference) && !/*FIL.RDB$FILE_NAME.NULL*/
			fb_234.fb_240)
		{
			if (first)
			{
				isqlGlob.printf("%s/* Add secondary files in comments %s", NEWLINE, NEWLINE);
				first = false;
			}

			// This is an explicit name for the difference file typically named <db_name>.delta
			isqlGlob.printf("%sALTER DATABASE ADD DIFFERENCE FILE '%s'", NEWLINE, /*FIL.RDB$FILE_NAME*/
											      fb_234.fb_235);
			isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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

	if (!first)
	{
		isqlGlob.printf("%s */%s", NEWLINE, NEWLINE);
		//isqlGlob.printf("%s%s%s", isqlGlob.global_Term, NEWLINE, NEWLINE);
	}
}


static void list_domain_table(const SCHAR* table_name, SSHORT default_char_set_id)
{
   struct fb_211_struct {
          ISC_QUAD fb_212;	/* RDB$VALIDATION_SOURCE */
          ISC_QUAD fb_213;	/* RDB$DEFAULT_SOURCE */
          short fb_214;	/* fbUtility */
          short fb_215;	/* gds__null_flag */
          short fb_216;	/* RDB$NULL_FLAG */
          short fb_217;	/* gds__null_flag */
          short fb_218;	/* gds__null_flag */
          short fb_219;	/* RDB$COLLATION_ID */
          short fb_220;	/* gds__null_flag */
          short fb_221;	/* RDB$CHARACTER_SET_ID */
          short fb_222;	/* gds__null_flag */
          short fb_223;	/* RDB$DIMENSIONS */
          short fb_224;	/* RDB$SEGMENT_LENGTH */
          short fb_225;	/* RDB$FIELD_SCALE */
          short fb_226;	/* RDB$FIELD_PRECISION */
          short fb_227;	/* RDB$FIELD_SUB_TYPE */
          short fb_228;	/* RDB$FIELD_TYPE */
          short fb_229;	/* gds__null_flag */
          short fb_230;	/* RDB$SYSTEM_FLAG */
          char  fb_231 [253];	/* RDB$FIELD_NAME */
   } fb_211;
   struct fb_209_struct {
          char  fb_210 [253];	/* RDB$RELATION_NAME */
   } fb_209;
/**************************************
 *
 *	l i s t _ d o m a i n _ t a b l e
 *
 **************************************
 *
 * Functional description
 *	List domains as identified by fields with any constraints on them
 *	for the named table
 *
 *	Parameters:  table_name == only extract domains for this table
 *	default_char_set_id -- character set def to supress
 *
 **************************************/
	bool first = true;
	SCHAR char_sets[CHARSET_COLLATE_SIZE];

	/*FOR FLD IN RDB$FIELDS CROSS
		RFR IN RDB$RELATION_FIELDS WITH
		RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
		RFR.RDB$RELATION_NAME EQ table_name
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
	   if (!fb_207 && fbTrans && DB)
	      fb_207 = DB->compileRequest(fbStatus, sizeof(fb_208), fb_208);
	   isc_vtov ((const char*) table_name, (char*) fb_209.fb_210, 253);
	   if (fbTrans && fb_207)
	      fb_207->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_209));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_207->release(); fb_207 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_207->receive (fbStatus, 0, 1, 303, CAST_MSG(&fb_211));
	   if (!fb_211.fb_214 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// Skip over artificial domains
		if (fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
					      fb_211.fb_231) &&
			(/*FLD.RDB$SYSTEM_FLAG.NULL*/
			 fb_211.fb_229 || /*FLD.RDB$SYSTEM_FLAG*/
    fb_211.fb_230 != 1))
		{
			continue;
		}

		if (first)
		{
			isqlGlob.printf("/* Domain definitions */%s", NEWLINE);
			first = false;
		}
		fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
				     fb_211.fb_231);

		isqlGlob.printf("CREATE DOMAIN %s AS ", /*FLD.RDB$FIELD_NAME*/
							fb_211.fb_231);

		if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
					   fb_211.fb_231, /*FLD.RDB$FIELD_TYPE*/
  fb_211.fb_228, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_211.fb_227,
				/*FLD.RDB$FIELD_PRECISION*/
				fb_211.fb_226, /*FLD.RDB$FIELD_SCALE*/
  fb_211.fb_225))
		{
			return; // ps_ERR;
		}

		if (/*FLD.RDB$FIELD_TYPE*/
		    fb_211.fb_228 == blr_blob)
		{
			const int subtype = /*FLD.RDB$FIELD_SUB_TYPE*/
					    fb_211.fb_227;
			isqlGlob.printf(" SUB_TYPE ");
			if ((subtype > 0) && (subtype <= MAX_BLOBSUBTYPES))
			{
				isqlGlob.prints(Sub_types[subtype]);
			}
			else
			{
				isqlGlob.printf("%d", subtype);
			}
			isqlGlob.printf(" SEGMENT SIZE %u", (USHORT) /*FLD.RDB$SEGMENT_LENGTH*/
								     fb_211.fb_224);
		}
		else if ((/*FLD.RDB$FIELD_TYPE*/
			  fb_211.fb_228 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_211.fb_228 == blr_varying))
		{
			// Length for chars
			isqlGlob.printf("(%d)", ISQL_get_field_length(/*FLD.RDB$FIELD_NAME*/
								      fb_211.fb_231));
		}

		if (!/*FLD.RDB$DIMENSIONS.NULL*/
		     fb_211.fb_222)
			ISQL_array_dimensions(/*FLD.RDB$FIELD_NAME*/
					      fb_211.fb_231);

		// Bug 8261: do not show the collation information just yet!  If you
		// do, then the domain syntax when printed is not correct.

		// Since the character set is part of the field type, display that information now.
		if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
		     fb_211.fb_220 &&
			!(((/*FLD.RDB$FIELD_TYPE*/
			    fb_211.fb_228 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_211.fb_228 == blr_varying)) &&
			  /*FLD.RDB$FIELD_SUB_TYPE*/
			  fb_211.fb_227 != fb_text_subtype_text))
		{

			char_sets[0] = 0;
			ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
						fb_211.fb_221, /*FLD.RDB$COLLATION_ID*/
  fb_211.fb_219, default_char_set_id, Get::CHARSET_ONLY, false, true, char_sets);
			if (char_sets[0])
				isqlGlob.prints(char_sets);
		}

		if (!/*FLD.RDB$DEFAULT_SOURCE.NULL*/
		     fb_211.fb_218)
		{
			isqlGlob.printf("%s%s ", NEWLINE, TAB_AS_SPACES);
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
								     fb_211.fb_213, false, true);
		}
		if (!/*FLD.RDB$VALIDATION_SOURCE.NULL*/
		     fb_211.fb_217)
		{
			isqlGlob.printf("%s%s ", NEWLINE, TAB_AS_SPACES);
			ISQL_print_validation (isqlGlob.Out, &/*FLD.RDB$VALIDATION_SOURCE*/
							      fb_211.fb_212, false, fbTrans);
		}
		if (/*FLD.RDB$NULL_FLAG*/
		    fb_211.fb_216 == 1)
			isqlGlob.printf(" NOT NULL");

		// Bug 8261:  Now show the collation order information
		/* Show the collation order if one has been specified.  If the collation
		   order is the default for the character set being used, then no collation
		   order will be shown ( because it isn't needed ).

		Even if the collation id is 0, it may be non-default for the character set.*/

		if (!/*FLD.RDB$COLLATION_ID.NULL*/
		     fb_211.fb_215)
		{
			char_sets[0] = 0;
			ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
						fb_211.fb_221, /*FLD.RDB$COLLATION_ID*/
  fb_211.fb_219, default_char_set_id, Get::COLLATE_ONLY,
				false, true, char_sets);

			if (char_sets[0])
				isqlGlob.prints(char_sets);
		}

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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


static void list_domains(SSHORT default_char_set_id)
{
   struct fb_190_struct {
          ISC_QUAD fb_191;	/* RDB$DEFAULT_SOURCE */
          short fb_192;	/* fbUtility */
          short fb_193;	/* gds__null_flag */
          short fb_194;	/* RDB$NULL_FLAG */
          short fb_195;	/* gds__null_flag */
          short fb_196;	/* RDB$COLLATION_ID */
          short fb_197;	/* gds__null_flag */
          short fb_198;	/* RDB$CHARACTER_SET_ID */
          short fb_199;	/* gds__null_flag */
          short fb_200;	/* RDB$DIMENSIONS */
          short fb_201;	/* RDB$SEGMENT_LENGTH */
          short fb_202;	/* RDB$FIELD_SCALE */
          short fb_203;	/* RDB$FIELD_PRECISION */
          short fb_204;	/* RDB$FIELD_SUB_TYPE */
          short fb_205;	/* RDB$FIELD_TYPE */
          char  fb_206 [253];	/* RDB$FIELD_NAME */
   } fb_190;
/**************************************
 *
 *	l i s t _ d o m a i n s
 *
 **************************************
 *
 * Functional description
 *	List domains
 *
 *	Parameters:
 *	default_char_set_id -- character set def to supress
 *
 **************************************/
	bool first = true;
	SCHAR char_sets[CHARSET_COLLATE_SIZE];

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
	   if (!fb_188 && fbTrans && DB)
	      fb_188 = DB->compileRequest(fbStatus, sizeof(fb_189), fb_189);
	   if (fbTrans && fb_188)
	      fb_188->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_188->release(); fb_188 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_188->receive (fbStatus, 0, 0, 289, CAST_MSG(&fb_190));
	   if (!fb_190.fb_192 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("/* Domain definitions */%s", NEWLINE);
			first = false;
		}
		fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
				     fb_190.fb_206);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id(/*FLD.RDB$FIELD_NAME*/
					   fb_190.fb_206, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("CREATE DOMAIN %s AS ", SQL_identifier);
		}
		else
			isqlGlob.printf("CREATE DOMAIN %s AS ", /*FLD.RDB$FIELD_NAME*/
								fb_190.fb_206);

		if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
					   fb_190.fb_206, /*FLD.RDB$FIELD_TYPE*/
  fb_190.fb_205, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_190.fb_204,
				/*FLD.RDB$FIELD_PRECISION*/
				fb_190.fb_203, /*FLD.RDB$FIELD_SCALE*/
  fb_190.fb_202))
		{
			return; // ps_ERR;
		}

		if (/*FLD.RDB$FIELD_TYPE*/
		    fb_190.fb_205 == blr_blob)
		{
			const int subtype = /*FLD.RDB$FIELD_SUB_TYPE*/
					    fb_190.fb_204;
			isqlGlob.printf(" SUB_TYPE ");

			if ((subtype > 0) && (subtype <= MAX_BLOBSUBTYPES))
			{
				isqlGlob.prints(Sub_types[subtype]);
			}
			else
			{
				isqlGlob.printf("%d", subtype);
			}
			isqlGlob.printf(" SEGMENT SIZE %u", (USHORT) /*FLD.RDB$SEGMENT_LENGTH*/
								     fb_190.fb_201);
		}
		else if ((/*FLD.RDB$FIELD_TYPE*/
			  fb_190.fb_205 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_190.fb_205 == blr_varying))
		{
			// Length for chars
			isqlGlob.printf("(%d)", ISQL_get_field_length(/*FLD.RDB$FIELD_NAME*/
								      fb_190.fb_206));
		}

		if (!/*FLD.RDB$DIMENSIONS.NULL*/
		     fb_190.fb_199)
			ISQL_array_dimensions(/*FLD.RDB$FIELD_NAME*/
					      fb_190.fb_206);

		// Bug 8261: do not show the collation information just yet!  If you
		// do, then the domain syntax when printed is not correct.

		// Since the character set is part of the field type, display that information now.
		if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
		     fb_190.fb_197 &&
			!(((/*FLD.RDB$FIELD_TYPE*/
			    fb_190.fb_205 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_190.fb_205 == blr_varying)) &&
			  /*FLD.RDB$FIELD_SUB_TYPE*/
			  fb_190.fb_204 != fb_text_subtype_text))
		{
			char_sets[0] = 0;
			ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
						fb_190.fb_198, /*FLD.RDB$COLLATION_ID*/
  fb_190.fb_196, default_char_set_id, Get::CHARSET_ONLY, false, true, char_sets);
			if (char_sets[0])
				isqlGlob.prints(char_sets);
		}

		if (!/*FLD.RDB$DEFAULT_SOURCE.NULL*/
		     fb_190.fb_195)
		{
			isqlGlob.printf("%s%s ", NEWLINE, TAB_AS_SPACES);
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FLD.RDB$DEFAULT_SOURCE*/
								     fb_190.fb_191, false, true);
		}
		// Validation moved to listDomainConstraints().
		if (/*FLD.RDB$NULL_FLAG*/
		    fb_190.fb_194 == 1)
			isqlGlob.printf(" NOT NULL");

		// Bug 8261:  Now show the collation order information
		/* Show the collation order if one has been specified.  If the collation
		   order is the default for the character set being used, then no collation
		   order will be shown ( because it isn't needed

		   Even if the collation id is 0, it may be non-default for the character.*/

		if (!/*FLD.RDB$COLLATION_ID.NULL*/
		     fb_190.fb_193)
		{
			char_sets[0] = 0;
			ISQL_get_character_sets(/*FLD.RDB$CHARACTER_SET_ID*/
						fb_190.fb_198, /*FLD.RDB$COLLATION_ID*/
  fb_190.fb_196, default_char_set_id, Get::COLLATE_ONLY,
				false, true, char_sets);

			if (char_sets[0])
				isqlGlob.prints(char_sets);
		}

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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
	isqlGlob.printf("COMMIT WORK%s%s", isqlGlob.global_Term, NEWLINE);
}


static void listDomainConstraints()
{
   struct fb_184_struct {
          ISC_QUAD fb_185;	/* RDB$VALIDATION_SOURCE */
          short fb_186;	/* fbUtility */
          char  fb_187 [253];	/* RDB$FIELD_NAME */
   } fb_184;
	bool first = true;

	/*FOR FLD IN RDB$FIELDS WITH
		FLD.RDB$FIELD_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *"
		AND FLD.RDB$SYSTEM_FLAG NE 1
		AND FLD.RDB$VALIDATION_SOURCE NOT MISSING
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
	   if (!fb_182 && fbTrans && DB)
	      fb_182 = DB->compileRequest(fbStatus, sizeof(fb_183), fb_183);
	   if (fbTrans && fb_182)
	      fb_182->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_182->release(); fb_182 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_182->receive (fbStatus, 0, 0, 263, CAST_MSG(&fb_184));
	   if (!fb_184.fb_186 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("%s/* Domain constraints */%s", NEWLINE, NEWLINE);
			first = false;
		}
		fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
				     fb_184.fb_187);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id(/*FLD.RDB$FIELD_NAME*/
					   fb_184.fb_187, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("ALTER DOMAIN %s ADD CONSTRAINT", SQL_identifier);
		}
		else
			isqlGlob.printf("ALTER DOMAIN %s ADD CONSTRAINT", /*FLD.RDB$FIELD_NAME*/
									  fb_184.fb_187);

		isqlGlob.printf("%s%s ", NEWLINE, TAB_AS_SPACES);
		ISQL_print_validation (isqlGlob.Out, &/*FLD.RDB$VALIDATION_SOURCE*/
						      fb_184.fb_185, false, fbTrans);

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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


// Fix computed field expressions.
static void listRelationComputed(LegacyTables flag, SSHORT default_char_set_id)
{
   struct fb_155_struct {
          ISC_QUAD fb_156;	/* RDB$COMPUTED_SOURCE */
          short fb_157;	/* fbUtility */
          short fb_158;	/* gds__null_flag */
          short fb_159;	/* gds__null_flag */
          short fb_160;	/* RDB$COLLATION_ID */
          short fb_161;	/* RDB$SEGMENT_LENGTH */
          short fb_162;	/* gds__null_flag */
          short fb_163;	/* RDB$DIMENSIONS */
          short fb_164;	/* gds__null_flag */
          short fb_165;	/* gds__null_flag */
          short fb_166;	/* RDB$CHARACTER_LENGTH */
          short fb_167;	/* RDB$FIELD_LENGTH */
          short fb_168;	/* RDB$FIELD_SCALE */
          short fb_169;	/* RDB$FIELD_PRECISION */
          short fb_170;	/* RDB$FIELD_SUB_TYPE */
          short fb_171;	/* RDB$CHARACTER_SET_ID */
          short fb_172;	/* gds__null_flag */
          short fb_173;	/* RDB$COLLATION_ID */
          short fb_174;	/* RDB$FIELD_TYPE */
          short fb_175;	/* RDB$SYSTEM_FLAG */
          short fb_176;	/* gds__null_flag */
          short fb_177;	/* RDB$FLAGS */
          char  fb_178 [253];	/* RDB$RELATION_NAME */
          char  fb_179 [253];	/* RDB$SECURITY_CLASS */
          char  fb_180 [253];	/* RDB$FIELD_NAME */
          char  fb_181 [253];	/* RDB$FIELD_NAME */
   } fb_155;
	// This version of cursor gets only sql tables identified by security class
	// and misses views, getting only null view_source

	Firebird::string lastTable;
	SCHAR char_sets[CHARSET_COLLATE_SIZE];

	/*FOR REL IN RDB$RELATIONS CROSS
		RFR IN RDB$RELATION_FIELDS CROSS
		FLD IN RDB$FIELDS
		WITH (REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
			 REL.RDB$VIEW_BLR MISSING AND
			 FLD.RDB$COMPUTED_BLR NOT MISSING AND
			 RFR.RDB$FIELD_SOURCE EQ FLD.RDB$FIELD_NAME AND
			 RFR.RDB$RELATION_NAME EQ REL.RDB$RELATION_NAME
		SORTED BY REL.RDB$RELATION_NAME, RFR.RDB$FIELD_POSITION, RFR.RDB$FIELD_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_153 && fbTrans && DB)
	      fb_153 = DB->compileRequest(fbStatus, sizeof(fb_154), fb_154);
	   if (fbTrans && fb_153)
	      fb_153->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_153->release(); fb_153 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_153->receive (fbStatus, 0, 0, 1062, CAST_MSG(&fb_155));
	   if (!fb_155.fb_157 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		// If this is not an SQL table and we aren't doing ALL objects
		if ((/*REL.RDB$FLAGS.NULL*/
		     fb_155.fb_176 || !(/*REL.RDB$FLAGS*/
      fb_155.fb_177 & REL_sql)) && (flag != ALL_objects) )
			continue;

		SSHORT collation = 0;
		SSHORT char_set_id = 0;

		// Null terminate name string
		fb_utils::exact_name(/*REL.RDB$RELATION_NAME*/
				     fb_155.fb_178);

		if (flag || !strncmp(/*REL.RDB$SECURITY_CLASS*/
				     fb_155.fb_179, "SQL$", 4))
		{
			if (lastTable.isEmpty())
				isqlGlob.printf("%s/* Computed fields */%s%s", NEWLINE, NEWLINE, NEWLINE);

			if (lastTable != /*REL.RDB$RELATION_NAME*/
					 fb_155.fb_178)
			{
				if (!lastTable.isEmpty())
					isqlGlob.printf("%s%s%s", isqlGlob.global_Term, NEWLINE, NEWLINE);

				isqlGlob.printf("ALTER TABLE ");

				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
				{
					IUTILS_copy_SQL_id(/*REL.RDB$RELATION_NAME*/
							   fb_155.fb_178, SQL_identifier, DBL_QUOTE);
					isqlGlob.printf("%s ", SQL_identifier);
				}
				else
					isqlGlob.printf("%s ", /*REL.RDB$RELATION_NAME*/
							       fb_155.fb_178);

				lastTable = /*REL.RDB$RELATION_NAME*/
					    fb_155.fb_178;
			}
			else
				isqlGlob.printf(",");

			isqlGlob.printf("%s%sALTER ", NEWLINE, TAB_AS_SPACES);

			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				IUTILS_copy_SQL_id(fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
									fb_155.fb_180), SQL_identifier, DBL_QUOTE);
				isqlGlob.printf("%s ", SQL_identifier);
			}
			else
				isqlGlob.printf("%s ", fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
									    fb_155.fb_180));

			isqlGlob.printf("TYPE ");

			/*
			** If this is a known domain, then just print the domain rather than type
			** Domains won't have length, array, or blob definitions, but they
			** may have not null, default and check overriding their definitions
			*/

			if (!(fb_utils::implicit_domain(/*FLD.RDB$FIELD_NAME*/
							fb_155.fb_181) && /*FLD.RDB$SYSTEM_FLAG*/
     fb_155.fb_175 != 1))
			{
				fb_utils::exact_name(/*FLD.RDB$FIELD_NAME*/
						     fb_155.fb_181);
				if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
				{
					IUTILS_copy_SQL_id (/*FLD.RDB$FIELD_NAME*/
							    fb_155.fb_181, SQL_identifier, DBL_QUOTE);
					isqlGlob.prints(SQL_identifier);
				}
				else
					isqlGlob.prints(/*FLD.RDB$FIELD_NAME*/
							fb_155.fb_181);

				// International character sets
				// Print only the collation
				if ((/*FLD.RDB$FIELD_TYPE*/
				     fb_155.fb_174 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_155.fb_174 == blr_varying) &&
					!/*RFR.RDB$COLLATION_ID.NULL*/
					 fb_155.fb_172)
				{
					char_sets[0] = '\0';
					collation = /*RFR.RDB$COLLATION_ID*/
						    fb_155.fb_173;
					char_set_id = /*FLD.RDB$CHARACTER_SET_ID*/
						      fb_155.fb_171;
					ISQL_get_character_sets(char_set_id, collation, default_char_set_id, Get::COLLATE_ONLY, false, true, char_sets);

					if (char_sets[0])
						isqlGlob.prints(char_sets);
				}
			}
			else
			{
				if (!ISQL_printNumericType(/*FLD.RDB$FIELD_NAME*/
							   fb_155.fb_181, /*FLD.RDB$FIELD_TYPE*/
  fb_155.fb_174, /*FLD.RDB$FIELD_SUB_TYPE*/
  fb_155.fb_170,
						/*FLD.RDB$FIELD_PRECISION*/
						fb_155.fb_169, /*FLD.RDB$FIELD_SCALE*/
  fb_155.fb_168))
				{
					return;
				}

				if ((/*FLD.RDB$FIELD_TYPE*/
				     fb_155.fb_174 == blr_text) || (/*FLD.RDB$FIELD_TYPE*/
		  fb_155.fb_174 == blr_varying))
				{
					isqlGlob.printf("(%d)",
						ISQL_get_char_length(
							/*FLD.RDB$FIELD_LENGTH*/
							fb_155.fb_167,
							/*FLD.RDB$CHARACTER_LENGTH.NULL*/
							fb_155.fb_165, /*FLD.RDB$CHARACTER_LENGTH*/
  fb_155.fb_166,
							/*FLD.RDB$CHARACTER_SET_ID.NULL*/
							fb_155.fb_164, /*FLD.RDB$CHARACTER_SET_ID*/
  fb_155.fb_171
						)
					);
				}

				// Catch arrays after printing the type

				if (!/*FLD.RDB$DIMENSIONS.NULL*/
				     fb_155.fb_162)
					ISQL_array_dimensions(/*FLD.RDB$FIELD_NAME*/
							      fb_155.fb_181);

				if (/*FLD.RDB$FIELD_TYPE*/
				    fb_155.fb_174 == blr_blob)
				{
					const int subtype = /*FLD.RDB$FIELD_SUB_TYPE*/
							    fb_155.fb_170;
					isqlGlob.printf(" SUB_TYPE ");

					if ((subtype > 0) && (subtype <= MAX_BLOBSUBTYPES))
						isqlGlob.prints(Sub_types[subtype]);
					else
						isqlGlob.printf("%d", subtype);

					isqlGlob.printf(" SEGMENT SIZE %u", (USHORT) /*FLD.RDB$SEGMENT_LENGTH*/
										     fb_155.fb_161);
				}

				// International character sets
				if ((((/*FLD.RDB$FIELD_TYPE*/
				       fb_155.fb_174 == blr_text || /*FLD.RDB$FIELD_TYPE*/
		fb_155.fb_174 == blr_varying) &&
					  /*FLD.RDB$FIELD_SUB_TYPE*/
					  fb_155.fb_170 == fb_text_subtype_text) ||
					 /*FLD.RDB$FIELD_TYPE*/
					 fb_155.fb_174 == blr_blob) &&
					!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
					 fb_155.fb_164)
				{
					char_sets[0] = '\0';

					// Override rdb$fields id with relation_fields if present

					if (!/*RFR.RDB$COLLATION_ID.NULL*/
					     fb_155.fb_172)
						collation = /*RFR.RDB$COLLATION_ID*/
							    fb_155.fb_173;
					else if (!/*FLD.RDB$COLLATION_ID.NULL*/
						  fb_155.fb_159)
						collation = /*FLD.RDB$COLLATION_ID*/
							    fb_155.fb_160;

					if (!/*FLD.RDB$CHARACTER_SET_ID.NULL*/
					     fb_155.fb_164)
						char_set_id = /*FLD.RDB$CHARACTER_SET_ID*/
							      fb_155.fb_171;
					if (char_set_id != default_char_set_id)
					{
						// Currently ALTER TABLE syntax doesn't allow collation here.
						ISQL_get_character_sets(char_set_id, collation, default_char_set_id, Get::CHARSET_ONLY, false, true, char_sets);
					}
					if (char_sets[0])
						isqlGlob.prints(char_sets);
					// CVC: Someone deleted the code that checks intchar when handling collations
					// several lines below, so it didn't have any effect. Commented it.
					//if (!char_set_id)
					//	intchar = true;
				}
			}

			isqlGlob.printf(" COMPUTED BY ");

			if (!/*FLD.RDB$COMPUTED_SOURCE.NULL*/
			     fb_155.fb_158)
				ISQL_print_validation(isqlGlob.Out, &/*FLD.RDB$COMPUTED_SOURCE*/
								     fb_155.fb_156, true, fbTrans);
		}
	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		ISQL_errmsg(fbStatus);
		/*ROLLBACK;*/
		{
		fbTrans->rollback(fbStatus);
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS))
		   fbTrans = 0;
		}
		return;
	/*END_ERROR;*/
	   }
	}

	if (lastTable.hasData())
		isqlGlob.printf("%s%s%s", isqlGlob.global_Term, NEWLINE, NEWLINE);
}


static void list_exceptions()
{
   struct fb_149_struct {
          char  fb_150 [1024];	/* RDB$MESSAGE */
          short fb_151;	/* fbUtility */
          char  fb_152 [253];	/* RDB$EXCEPTION_NAME */
   } fb_149;
/**************************************
 *
 *	l i s t _ e x c e p t i o n s
 *
 **************************************
 *
 * Functional description
 *	List all exceptions defined in the database
 *
 *	Parameters:  none
 *
 **************************************/
	bool first = true;

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
	   if (!fb_147 && fbTrans && DB)
	      fb_147 = DB->compileRequest(fbStatus, sizeof(fb_148), fb_148);
	   if (fbTrans && fb_147)
	      fb_147->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_147->release(); fb_147 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_147->receive (fbStatus, 0, 0, 1279, CAST_MSG(&fb_149));
	   if (!fb_149.fb_151 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("%s/*  Exceptions */%s", NEWLINE, NEWLINE);
		}
		first = false;
		fb_utils::exact_name(/*EXC.RDB$EXCEPTION_NAME*/
				     fb_149.fb_152);

		IUTILS_copy_SQL_id (/*EXC.RDB$MESSAGE*/
				    fb_149.fb_150, SQL_identifier2, SINGLE_QUOTE);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*EXC.RDB$EXCEPTION_NAME*/
					    fb_149.fb_152, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("CREATE EXCEPTION %s %s%s%s",
					 SQL_identifier, SQL_identifier2, isqlGlob.global_Term, NEWLINE);
		}
		else
			isqlGlob.printf("CREATE EXCEPTION %s %s%s%s",
					 /*EXC.RDB$EXCEPTION_NAME*/
					 fb_149.fb_152, SQL_identifier2, isqlGlob.global_Term, NEWLINE);

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


static void list_filters()
{
   struct fb_140_struct {
          char  fb_141 [256];	/* RDB$ENTRYPOINT */
          char  fb_142 [256];	/* RDB$MODULE_NAME */
          short fb_143;	/* fbUtility */
          short fb_144;	/* RDB$OUTPUT_SUB_TYPE */
          short fb_145;	/* RDB$INPUT_SUB_TYPE */
          char  fb_146 [253];	/* RDB$FUNCTION_NAME */
   } fb_140;
/**************************************
 *
 *	l i s t _ f i l t e r s
 *
 **************************************
 *
 * Functional description
 *	List all blob filters
 *
 *	Parameters:  none
 * Results in
 * DECLARE FILTER <fname> INPUT_TYPE <blob_sub_type> OUTPUT_TYPE <blob_subtype>
 *		 ENTRY_POINT <string> MODULE_NAME <string>
 **************************************/
	bool first = true;

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
	   if (!fb_138 && fbTrans && DB)
	      fb_138 = DB->compileRequest(fbStatus, sizeof(fb_139), fb_139);
	   if (fbTrans && fb_138)
	      fb_138->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_138->release(); fb_138 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_138->receive (fbStatus, 0, 0, 771, CAST_MSG(&fb_140));
	   if (!fb_140.fb_143 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*FIL.RDB$FUNCTION_NAME*/
				     fb_140.fb_146);
		fb_utils::exact_name(/*FIL.RDB$MODULE_NAME*/
				     fb_140.fb_142);
		fb_utils::exact_name(/*FIL.RDB$ENTRYPOINT*/
				     fb_140.fb_141);

		if (first)
		{
			isqlGlob.printf("%s/*  BLOB Filter declarations */%s", NEWLINE, NEWLINE);
			first = false;
		}

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id(/*FIL.RDB$FUNCTION_NAME*/
					   fb_140.fb_146, SQL_identifier, DBL_QUOTE);
		else
			strcpy(SQL_identifier, /*FIL.RDB$FUNCTION_NAME*/
					       fb_140.fb_146);

		isqlGlob.printf("DECLARE FILTER %s INPUT_TYPE %d OUTPUT_TYPE %d%s",
			SQL_identifier, /*FIL.RDB$INPUT_SUB_TYPE*/
					fb_140.fb_145, /*FIL.RDB$OUTPUT_SUB_TYPE*/
  fb_140.fb_144, NEWLINE);

		IUTILS_copy_SQL_id(/*FIL.RDB$ENTRYPOINT*/
				   fb_140.fb_141, SQL_identifier, SINGLE_QUOTE);
		IUTILS_copy_SQL_id(/*FIL.RDB$MODULE_NAME*/
				   fb_140.fb_142, SQL_identifier2, SINGLE_QUOTE);

		isqlGlob.printf("%sENTRY_POINT %s MODULE_NAME %s%s%s%s",
			TAB_AS_SPACES, SQL_identifier, SQL_identifier2,
			isqlGlob.global_Term, NEWLINE, NEWLINE);

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


static void list_foreign()
{
   struct fb_126_struct {
          char  fb_127 [12];	/* RDB$DELETE_RULE */
          char  fb_128 [12];	/* RDB$UPDATE_RULE */
          short fb_129;	/* fbUtility */
          short fb_130;	/* gds__null_flag */
          short fb_131;	/* gds__null_flag */
          short fb_132;	/* gds__null_flag */
          char  fb_133 [253];	/* RDB$RELATION_NAME */
          char  fb_134 [253];	/* RDB$RELATION_NAME */
          char  fb_135 [253];	/* RDB$INDEX_NAME */
          char  fb_136 [253];	/* RDB$CONSTRAINT_NAME */
          char  fb_137 [253];	/* RDB$INDEX_NAME */
   } fb_126;
/**************************************
 *
 *	l i s t _ f o r e i g n
 *
 **************************************
 *
 * Functional description
 *	List all foreign key constraints and alter the tables
 *
 **************************************/
	SCHAR collist[BUFFER_LENGTH512 * 2];

	// Static queries for obtaining foreign constraints, where RELC1 is the
	// foreign key constraints, RELC2 is the primary key lookup and REFC is the join table

	/*FOR RELC1 IN RDB$RELATION_CONSTRAINTS CROSS
		RELC2 IN RDB$RELATION_CONSTRAINTS CROSS
		REFC IN RDB$REF_CONSTRAINTS WITH
		RELC1.RDB$CONSTRAINT_TYPE EQ "FOREIGN KEY" AND
		REFC.RDB$CONST_NAME_UQ EQ RELC2.RDB$CONSTRAINT_NAME AND
		REFC.RDB$CONSTRAINT_NAME EQ RELC1.RDB$CONSTRAINT_NAME AND
		(RELC2.RDB$CONSTRAINT_TYPE EQ "UNIQUE" OR
		RELC2.RDB$CONSTRAINT_TYPE EQ "PRIMARY KEY")
		SORTED BY RELC1.RDB$RELATION_NAME, RELC1.RDB$CONSTRAINT_NAME*/
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
	   if (fbTrans && fb_124)
	      fb_124->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_124->release(); fb_124 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_124->receive (fbStatus, 0, 0, 1297, CAST_MSG(&fb_126));
	   if (!fb_126.fb_129 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*RELC1.RDB$RELATION_NAME*/
				     fb_126.fb_133);
		fb_utils::exact_name(/*RELC2.RDB$RELATION_NAME*/
				     fb_126.fb_134);

		ISQL_get_index_segments (collist, sizeof(collist), /*RELC1.RDB$INDEX_NAME*/
								   fb_126.fb_135, true);

		isqlGlob.printf(NEWLINE);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*RELC1.RDB$RELATION_NAME*/
					    fb_126.fb_133, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("ALTER TABLE %s ADD ", SQL_identifier);
		}
		else
			isqlGlob.printf("ALTER TABLE %s ADD ", /*RELC1.RDB$RELATION_NAME*/
							       fb_126.fb_133);

		// If the name of the constraint is not INTEG..., print it.
	    // INTEG_... are internally generated names.
		if (!/*RELC1.RDB$CONSTRAINT_NAME.NULL*/
		     fb_126.fb_132 &&
			!fb_utils::implicit_integrity(/*RELC1.RDB$CONSTRAINT_NAME*/
						      fb_126.fb_136))
		{
			IUTILS_truncate_term (/*RELC1.RDB$CONSTRAINT_NAME*/
					      fb_126.fb_136, static_cast<USHORT>(strlen(/*RELC1.RDB$CONSTRAINT_NAME*/
			     fb_126.fb_136)));
			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			{
				IUTILS_copy_SQL_id (/*RELC1.RDB$CONSTRAINT_NAME*/
						    fb_126.fb_136, SQL_identifier, DBL_QUOTE);
				isqlGlob.printf("CONSTRAINT %s ", SQL_identifier);
			}
			else
				isqlGlob.printf("CONSTRAINT %s ", /*RELC1.RDB$CONSTRAINT_NAME*/
								  fb_126.fb_136);
		}

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*RELC2.RDB$RELATION_NAME*/
					    fb_126.fb_134, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("FOREIGN KEY (%s) REFERENCES %s ", collist, SQL_identifier);
		}
		else
			isqlGlob.printf("FOREIGN KEY (%s) REFERENCES %s ", collist, /*RELC2.RDB$RELATION_NAME*/
										    fb_126.fb_134);

		// Get the column list for the primary key

		ISQL_get_index_segments (collist, sizeof(collist), /*RELC2.RDB$INDEX_NAME*/
								   fb_126.fb_137, true);

		isqlGlob.printf("(%s)", collist);

		// Add the referential actions, if any
		if (!/*REFC.RDB$UPDATE_RULE.NULL*/
		     fb_126.fb_131)
		{
			IUTILS_truncate_term (/*REFC.RDB$UPDATE_RULE*/
					      fb_126.fb_128, static_cast<USHORT>(strlen(/*REFC.RDB$UPDATE_RULE*/
			     fb_126.fb_128)));
			ISQL_ri_action_print (/*REFC.RDB$UPDATE_RULE*/
					      fb_126.fb_128, " ON UPDATE", true);
		}

		if (!/*REFC.RDB$DELETE_RULE.NULL*/
		     fb_126.fb_130)
		{
			IUTILS_truncate_term (/*REFC.RDB$DELETE_RULE*/
					      fb_126.fb_127, static_cast<USHORT>(strlen(/*REFC.RDB$DELETE_RULE*/
			     fb_126.fb_127)));
			ISQL_ri_action_print (/*REFC.RDB$DELETE_RULE*/
					      fb_126.fb_127, " ON DELETE", true);
		}

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);

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


static void list_functions_legacy()
{
   struct fb_97_struct {
          short fb_98;	/* fbUtility */
          short fb_99;	/* RDB$BYTES_PER_CHARACTER */
          char  fb_100 [253];	/* RDB$CHARACTER_SET_NAME */
   } fb_97;
   struct fb_95_struct {
          short fb_96;	/* RDB$CHARACTER_SET_ID */
   } fb_95;
   struct fb_105_struct {
          short fb_106;	/* fbUtility */
          short fb_107;	/* RDB$ARGUMENT_POSITION */
          short fb_108;	/* RDB$MECHANISM */
          short fb_109;	/* RDB$FIELD_SCALE */
          short fb_110;	/* RDB$FIELD_SUB_TYPE */
          short fb_111;	/* gds__null_flag */
          short fb_112;	/* RDB$FIELD_PRECISION */
          short fb_113;	/* RDB$FIELD_LENGTH */
          short fb_114;	/* RDB$CHARACTER_SET_ID */
          short fb_115;	/* RDB$FIELD_TYPE */
   } fb_105;
   struct fb_103_struct {
          char  fb_104 [253];	/* RDB$FUNCTION_NAME */
   } fb_103;
   struct fb_118_struct {
          char  fb_119 [256];	/* RDB$ENTRYPOINT */
          char  fb_120 [256];	/* RDB$MODULE_NAME */
          short fb_121;	/* fbUtility */
          short fb_122;	/* RDB$RETURN_ARGUMENT */
          char  fb_123 [253];	/* RDB$FUNCTION_NAME */
   } fb_118;
/**************************************
 *
 *	l i s t _ f u n c t i o n s _ l e g a c y
 *
 **************************************
 *
 * Functional description
 *	List all external functions
 *
 *	Parameters:  none
 * Results in
 * DECLARE EXTERNAL FUNCTION function_name
 *               CHAR(256) , INTEGER, ....
 *               RETURNS INTEGER BY VALUE
 *               ENTRY_POINT entrypoint MODULE_NAME module;
 **************************************/
	char type_buffer[BUFFER_LENGTH256];
	char return_buffer[BUFFER_LENGTH256];

	bool first = true;

	/*FOR FUN IN RDB$FUNCTIONS
		WITH (FUN.RDB$SYSTEM_FLAG NE 1 OR FUN.RDB$SYSTEM_FLAG MISSING)
		AND FUN.RDB$MODULE_NAME NOT MISSING
		AND FUN.RDB$PACKAGE_NAME MISSING
		SORTED BY FUN.RDB$FUNCTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_116 && fbTrans && DB)
	      fb_116 = DB->compileRequest(fbStatus, sizeof(fb_117), fb_117);
	   if (fbTrans && fb_116)
	      fb_116->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_116->release(); fb_116 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_116->receive (fbStatus, 0, 0, 769, CAST_MSG(&fb_118));
	   if (!fb_118.fb_121 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*FUN.RDB$FUNCTION_NAME*/
				     fb_118.fb_123);
		fb_utils::exact_name(/*FUN.RDB$MODULE_NAME*/
				     fb_118.fb_120);
		fb_utils::exact_name(/*FUN.RDB$ENTRYPOINT*/
				     fb_118.fb_119);
		if (first)
		{
			isqlGlob.printf("%s/*  External Function declarations */%s", NEWLINE, NEWLINE);
			first = false;
		}

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id(/*FUN.RDB$FUNCTION_NAME*/
					   fb_118.fb_123, SQL_identifier, DBL_QUOTE);
		else
			strcpy(SQL_identifier, /*FUN.RDB$FUNCTION_NAME*/
					       fb_118.fb_123);

		// Start new function declaration
		isqlGlob.printf("DECLARE EXTERNAL FUNCTION %s%s", SQL_identifier, NEWLINE);

		bool firstarg = true;

		/*FOR FNA IN RDB$FUNCTION_ARGUMENTS
			WITH FUN.RDB$FUNCTION_NAME EQ FNA.RDB$FUNCTION_NAME
			AND FNA.RDB$PACKAGE_NAME MISSING
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
		   if (!fb_101 && fbTrans && DB)
		      fb_101 = DB->compileRequest(fbStatus, sizeof(fb_102), fb_102);
		   isc_vtov ((const char*) fb_118.fb_123, (char*) fb_103.fb_104, 253);
		   if (fbTrans && fb_101)
		      fb_101->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_103));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_101->release(); fb_101 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_101->receive (fbStatus, 0, 1, 20, CAST_MSG(&fb_105));
		   if (!fb_105.fb_106 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			// Find parameter type
			int i = 0;
			while (/*FNA.RDB$FIELD_TYPE*/
			       fb_105.fb_115 != Column_types[i].type)
				i++;

			fb_assert(Column_types[i].type != 0);

			// Print length where appropriate
			if ((/*FNA.RDB$FIELD_TYPE*/
			     fb_105.fb_115 == blr_text) ||
				(/*FNA.RDB$FIELD_TYPE*/
				 fb_105.fb_115 == blr_varying) ||
				(/*FNA.RDB$FIELD_TYPE*/
				 fb_105.fb_115 == blr_cstring))
			{
				bool did_charset = false;
				/*FOR CHARSET IN RDB$CHARACTER_SETS
					WITH CHARSET.RDB$CHARACTER_SET_ID = FNA.RDB$CHARACTER_SET_ID*/
				{
				for (int retries = 0; retries < 2; ++retries)
				   {
				   {
				   if (!DB)
				      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
				   if (DB && !fbTrans)
				      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
				   }
				   if (!fb_93 && fbTrans && DB)
				      fb_93 = DB->compileRequest(fbStatus, sizeof(fb_94), fb_94);
				   fb_95.fb_96 = fb_105.fb_114;
				   if (fbTrans && fb_93)
				      fb_93->startAndSend(fbStatus, fbTrans, 0, 0, 2, CAST_CONST_MSG(&fb_95));
				   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_93->release(); fb_93 = NULL; }
				   else break;
				   }
				if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
				while (1)
				   {
				   fb_93->receive (fbStatus, 0, 1, 257, CAST_MSG(&fb_97));
				   if (!fb_97.fb_98 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

					did_charset = true;
					fb_utils::exact_name(/*CHARSET.RDB$CHARACTER_SET_NAME*/
							     fb_97.fb_100);
					fb_utils::snprintf(type_buffer, sizeof(type_buffer), "%s(%d) CHARACTER SET %s",
							 Column_types[i].type_name,
							 (/*FNA.RDB$FIELD_LENGTH*/
							  fb_105.fb_113 / MAX (1, /*CHARSET.RDB$BYTES_PER_CHARACTER*/
	   fb_97.fb_99)),
							 /*CHARSET.RDB$CHARACTER_SET_NAME*/
							 fb_97.fb_100);

				/*END_FOR*/
				   }
				   };
				/*ON_ERROR*/
				if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
				   {
					ISQL_errmsg (fbStatus);
					return;
				/*END_ERROR;*/
				   }
				}

				if (!did_charset)
					fb_utils::snprintf(type_buffer, sizeof(type_buffer), "%s(%d)",
							Column_types[i].type_name, /*FNA.RDB$FIELD_LENGTH*/
										   fb_105.fb_113);
			}
			else
			{
				bool precision_known = false;

				if (isqlGlob.major_ods >= ODS_VERSION10 &&
					(/*FNA.RDB$FIELD_TYPE*/
					 fb_105.fb_115 == blr_short ||
					 /*FNA.RDB$FIELD_TYPE*/
					 fb_105.fb_115 == blr_long ||
					 /*FNA.RDB$FIELD_TYPE*/
					 fb_105.fb_115 == blr_int64 ||
					 /*FNA.RDB$FIELD_TYPE*/
					 fb_105.fb_115 == blr_int128))
				{
					// We are ODS >= 10 and could be any Dialect
					// We are Dialect >=3 since FIELD_PRECISION is non-NULL
					if (!/*FNA.RDB$FIELD_PRECISION.NULL*/
					     fb_105.fb_111 &&
						/*FNA.RDB$FIELD_SUB_TYPE*/
						fb_105.fb_110 > 0 &&
						/*FNA.RDB$FIELD_SUB_TYPE*/
						fb_105.fb_110 <= MAX_INTSUBTYPES)
					{
						fb_utils::snprintf(type_buffer, sizeof(type_buffer), "%s(%d, %d)",
								Integral_subtypes[/*FNA.RDB$FIELD_SUB_TYPE*/
										  fb_105.fb_110],
								/*FNA.RDB$FIELD_PRECISION*/
								fb_105.fb_112,
								-/*FNA.RDB$FIELD_SCALE*/
								 fb_105.fb_109);
						precision_known = true;
					}
				} // if isqlGlob.major_ods >= ods_version10 &&

				if (!precision_known)
				{
					// Take a stab at numerics and decimals
					if ((/*FNA.RDB$FIELD_TYPE*/
					     fb_105.fb_115 == blr_short) && (/*FNA.RDB$FIELD_SCALE*/
		   fb_105.fb_109 < 0))
						fb_utils::snprintf(type_buffer, sizeof(type_buffer), "NUMERIC(4, %d)", -/*FNA.RDB$FIELD_SCALE*/
															fb_105.fb_109);
					else if ((/*FNA.RDB$FIELD_TYPE*/
						  fb_105.fb_115 == blr_long) && (/*FNA.RDB$FIELD_SCALE*/
		  fb_105.fb_109 < 0))
						fb_utils::snprintf(type_buffer, sizeof(type_buffer), "NUMERIC(9, %d)", -/*FNA.RDB$FIELD_SCALE*/
															fb_105.fb_109);
					else if ((/*FNA.RDB$FIELD_TYPE*/
						  fb_105.fb_115 == blr_double) && (/*FNA.RDB$FIELD_SCALE*/
		    fb_105.fb_109 < 0))
						fb_utils::snprintf(type_buffer, sizeof(type_buffer), "NUMERIC(15, %d)", -/*FNA.RDB$FIELD_SCALE*/
															 fb_105.fb_109);
					else
						fb_utils::snprintf(type_buffer, sizeof(type_buffer), "%s", Column_types[i].type_name);
				} // if !precision_known
			} // if blr_text or blr_varying or blr_cstring ... else

			// If a return argument, save it for the end, otherwise print

			/*
			 * Changed the following to not return a BLOB by value.
			 * To be sincere, this code doesn't cater for the RETURNS PARAMETER syntax but
			 * it would require more surgery than I'm willing to do, since I'm sick of isql
			 * so I started my own metadata extraction utility based on IBO that does this
			 * trick and others.
			 * Claudio Valderrama (by way of) MOD 23-Apr-2001

			 CVC: Finally enhanced the UDF metadata extraction.
			*/

			int ptype = (int) abs(/*FNA.RDB$MECHANISM*/
					      fb_105.fb_108);
			if (ptype > MAX_UDFPARAM_TYPES)
				ptype = MAX_UDFPARAM_TYPES;

			bool printarg = true;


			if (/*FUN.RDB$RETURN_ARGUMENT*/
			    fb_118.fb_122 == /*FNA.RDB$ARGUMENT_POSITION*/
    fb_105.fb_107)
			{
				if (/*FUN.RDB$RETURN_ARGUMENT*/
				    fb_118.fb_122)
					fb_utils::snprintf(return_buffer, sizeof(return_buffer), "RETURNS PARAMETER %d", /*FUN.RDB$RETURN_ARGUMENT*/
															 fb_118.fb_122);
				else
				{
					fb_utils::snprintf(return_buffer, sizeof(return_buffer), "RETURNS %s%s %s", type_buffer,
							 UDF_param_types[ptype],
							 (/*FNA.RDB$MECHANISM*/
							  fb_105.fb_108 < 0 ? "FREE_IT" : ""));
					printarg = false;
				}

			}

			if (printarg)
			{
				// First arg needs no comma
				isqlGlob.printf("%s%s%s", (firstarg ? "" : ", "), type_buffer, UDF_param_types[ptype]);
				firstarg = false;
			}

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg (fbStatus);
			return;
		/*END_ERROR;*/
		   }
		}

		// Print the return type -- All functions return a type
		isqlGlob.printf("%s%s%s", NEWLINE, return_buffer, NEWLINE);

		// Print out entrypoint information
		IUTILS_copy_SQL_id(/*FUN.RDB$ENTRYPOINT*/
				   fb_118.fb_119, SQL_identifier, SINGLE_QUOTE);
		IUTILS_copy_SQL_id(/*FUN.RDB$MODULE_NAME*/
				   fb_118.fb_120, SQL_identifier2, SINGLE_QUOTE);

		isqlGlob.printf("ENTRY_POINT %s MODULE_NAME %s%s%s%s",
				 SQL_identifier,
				 SQL_identifier2,
				 isqlGlob.global_Term,
				 NEWLINE,
				 NEWLINE);

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

static void list_functions_ods12_headers(SSHORT default_char_set_id)
{
   struct fb_87_struct {
          short fb_88;	/* fbUtility */
          short fb_89;	/* gds__null_flag */
          short fb_90;	/* RDB$DETERMINISTIC_FLAG */
          short fb_91;	/* RDB$RETURN_ARGUMENT */
          char  fb_92 [253];	/* RDB$FUNCTION_NAME */
   } fb_87;
	fb_assert(isqlGlob.major_ods >= ODS_VERSION12);

	bool header = true;
	static const char* const create_function	= "CREATE OR ALTER FUNCTION %s ";
	static const char* const body_function		= "BEGIN END %s%s";

	//  First the dummy functions (without bodies)

	/*FOR FUN IN RDB$FUNCTIONS
		WITH (FUN.RDB$SYSTEM_FLAG NE 1 OR FUN.RDB$SYSTEM_FLAG MISSING)
		AND FUN.RDB$PACKAGE_NAME MISSING
		AND FUN.RDB$MODULE_NAME MISSING
		SORTED BY FUN.RDB$FUNCTION_NAME*/
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
	   if (fbTrans && fb_85)
	      fb_85->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_85->release(); fb_85 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_85->receive (fbStatus, 0, 0, 261, CAST_MSG(&fb_87));
	   if (!fb_87.fb_88 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (header)
		{
			print_proc_prefix(obj_udf, true);
			header = false;
		}

		fb_utils::exact_name(/*FUN.RDB$FUNCTION_NAME*/
				     fb_87.fb_92);
		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id(/*FUN.RDB$FUNCTION_NAME*/
					   fb_87.fb_92, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf(create_function, SQL_identifier);
		}
		else
		{
			isqlGlob.printf(create_function, /*FUN.RDB$FUNCTION_NAME*/
							 fb_87.fb_92);
		}

		get_function_args_ods12(/*FUN.RDB$FUNCTION_NAME*/
					fb_87.fb_92, /*FUN.RDB$RETURN_ARGUMENT*/
  fb_87.fb_91, default_char_set_id);

		if (!/*FUN.RDB$DETERMINISTIC_FLAG.NULL*/
		     fb_87.fb_89 && /*FUN.RDB$DETERMINISTIC_FLAG*/
    fb_87.fb_90)
			isqlGlob.printf("DETERMINISTIC %s", NEWLINE);

		isqlGlob.printf("AS %s", NEWLINE);
		isqlGlob.printf(body_function, Procterm, NEWLINE);

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

	// Only reset the terminators if there were procs to print
	if (!header)
		print_proc_suffix(obj_udf);
}

static void list_functions_ods12_bodies(SSHORT default_char_set_id)
{
   struct fb_71_struct {
          ISC_QUAD fb_72;	/* RDB$FUNCTION_SOURCE */
          char  fb_73 [256];	/* RDB$ENTRYPOINT */
          short fb_74;	/* fbUtility */
          short fb_75;	/* gds__null_flag */
          short fb_76;	/* gds__null_flag */
          short fb_77;	/* gds__null_flag */
          short fb_78;	/* gds__null_flag */
          short fb_79;	/* gds__null_flag */
          short fb_80;	/* RDB$DETERMINISTIC_FLAG */
          short fb_81;	/* RDB$RETURN_ARGUMENT */
          char  fb_82 [253];	/* RDB$FUNCTION_NAME */
          FB_BOOLEAN fb_83;	/* RDB$SQL_SECURITY */
          char  fb_84 [253];	/* RDB$ENGINE_NAME */
   } fb_71;
	fb_assert(isqlGlob.major_ods >= ODS_VERSION12);

	bool header = true;

	// This query gets the function name and the source.  We then nest a query
	// to retrieve the parameters. Alter is used, because the functions are already there
	TEXT msg[MSG_LENGTH];

	/*FOR FUN IN RDB$FUNCTIONS
		WITH (FUN.RDB$SYSTEM_FLAG NE 1 OR FUN.RDB$SYSTEM_FLAG MISSING)
		AND FUN.RDB$PACKAGE_NAME MISSING
		AND FUN.RDB$MODULE_NAME MISSING
		SORTED BY FUN.RDB$FUNCTION_NAME*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_69 && fbTrans && DB)
	      fb_69 = DB->compileRequest(fbStatus, sizeof(fb_70), fb_70);
	   if (fbTrans && fb_69)
	      fb_69->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_69->release(); fb_69 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_69->receive (fbStatus, 0, 0, 787, CAST_MSG(&fb_71));
	   if (!fb_71.fb_74 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
		if (header)
		{
			print_proc_prefix(obj_udf, false);
			header = false;
		}
		fb_utils::exact_name(/*FUN.RDB$FUNCTION_NAME*/
				     fb_71.fb_82);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id(/*FUN.RDB$FUNCTION_NAME*/
					   fb_71.fb_82, SQL_identifier, DBL_QUOTE);
			isqlGlob.printf("%sALTER FUNCTION %s ", NEWLINE, SQL_identifier);
		}
		else
			isqlGlob.printf("%sALTER FUNCTION %s ", NEWLINE, /*FUN.RDB$FUNCTION_NAME*/
									 fb_71.fb_82);

		get_function_args_ods12(/*FUN.RDB$FUNCTION_NAME*/
					fb_71.fb_82, /*FUN.RDB$RETURN_ARGUMENT*/
  fb_71.fb_81, default_char_set_id);

		if (!/*FUN.RDB$DETERMINISTIC_FLAG.NULL*/
		     fb_71.fb_79 && /*FUN.RDB$DETERMINISTIC_FLAG*/
    fb_71.fb_80)
			isqlGlob.printf("DETERMINISTIC %s", NEWLINE);

		// Print the function body

		if (!/*FUN.RDB$ENTRYPOINT.NULL*/
		     fb_71.fb_78)
		{
			fb_utils::exact_name(/*FUN.RDB$ENTRYPOINT*/
					     fb_71.fb_73);
			IUTILS_copy_SQL_id(/*FUN.RDB$ENTRYPOINT*/
					   fb_71.fb_73, SQL_identifier2, SINGLE_QUOTE);
			isqlGlob.printf("EXTERNAL NAME %s%s", SQL_identifier2, NEWLINE);
		}

		if (!/*FUN.RDB$SQL_SECURITY.NULL*/
		     fb_71.fb_77)
		{
			const char* ss = /*FUN.RDB$SQL_SECURITY*/
					 fb_71.fb_83 ? "SQL SECURITY DEFINER" : "SQL SECURITY INVOKER";
			isqlGlob.printf("%s%s", ss, NEWLINE);
		}

		if (!/*FUN.RDB$ENGINE_NAME.NULL*/
		     fb_71.fb_76)
		{
			fb_utils::exact_name(/*FUN.RDB$ENGINE_NAME*/
					     fb_71.fb_84);
			isqlGlob.printf("ENGINE %s", /*FUN.RDB$ENGINE_NAME*/
						     fb_71.fb_84);

			if (!/*FUN.RDB$FUNCTION_SOURCE.NULL*/
			     fb_71.fb_75)
			{
				isqlGlob.printf("%sAS '", NEWLINE);
				SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FUN.RDB$FUNCTION_SOURCE*/
									     fb_71.fb_72, true);
				isqlGlob.printf("'%s", NEWLINE);
			}
		}
		else if (!/*FUN.RDB$FUNCTION_SOURCE.NULL*/
			  fb_71.fb_75)
		{
			isqlGlob.printf("AS %s", NEWLINE);
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*FUN.RDB$FUNCTION_SOURCE*/
								     fb_71.fb_72);
		}

		isqlGlob.printf(" %s%s", Procterm, NEWLINE);

	/*END_FOR*/
	   }
	   };
	/*ON_ERROR*/
	if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
	   {
		IUTILS_msg_get(GEN_ERR, msg, SafeArg() << isc_sqlcode(fbStatus->getErrors()));
		STDERROUT(msg);			// Statement failed, SQLCODE = %d\n\n
		ISQL_errmsg(fbStatus);
		return;
	/*END_ERROR;*/
	   }
	}

	// Only reset the terminators if there were procs to print
	if (!header)
		print_proc_suffix(obj_udf);
}


static void list_generators()
{
   struct fb_59_struct {
          ISC_INT64 fb_60;	/* RDB$INITIAL_VALUE */
          ISC_LONG fb_61;	/* RDB$GENERATOR_INCREMENT */
          short fb_62;	/* fbUtility */
          short fb_63;	/* gds__null_flag */
   } fb_59;
   struct fb_57_struct {
          char  fb_58 [253];	/* RDB$GENERATOR_NAME */
   } fb_57;
   struct fb_66_struct {
          short fb_67;	/* fbUtility */
          char  fb_68 [253];	/* RDB$GENERATOR_NAME */
   } fb_66;
/**************************************
 *
 *	l i s t _ g e n e r a t o r s
 *
 **************************************
 *
 * Functional description
 *	Re create all non-system generators
 *
 **************************************/

	bool first = true;

	/*FOR GEN IN RDB$GENERATORS
		WITH GEN.RDB$GENERATOR_NAME NOT MATCHING "RDB$+" USING "+=[0-9][0-9]* *" AND
			 GEN.RDB$GENERATOR_NAME NOT MATCHING "SQL$+" USING "+=[0-9][0-9]* *" AND
			 (GEN.RDB$SYSTEM_FLAG MISSING OR GEN.RDB$SYSTEM_FLAG NE 1)
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
	   if (!fb_64 && fbTrans && DB)
	      fb_64 = DB->compileRequest(fbStatus, sizeof(fb_65), fb_65);
	   if (fbTrans && fb_64)
	      fb_64->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_64->release(); fb_64 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_64->receive (fbStatus, 0, 0, 255, CAST_MSG(&fb_66));
	   if (!fb_66.fb_67 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;
	{
		if (first)
		{
			isqlGlob.printf("%s/*  Generators or sequences */%s", NEWLINE, NEWLINE);
			first = false;
		}

		fb_utils::exact_name(/*GEN.RDB$GENERATOR_NAME*/
				     fb_66.fb_68);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id(/*GEN.RDB$GENERATOR_NAME*/
					   fb_66.fb_68, SQL_identifier, DBL_QUOTE);
		else
			strcpy(SQL_identifier, /*GEN.RDB$GENERATOR_NAME*/
					       fb_66.fb_68);

		isqlGlob.printf("CREATE GENERATOR %s", SQL_identifier);

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
			   if (!fb_55 && fbTrans && DB)
			      fb_55 = DB->compileRequest(fbStatus, sizeof(fb_56), fb_56);
			   isc_vtov ((const char*) fb_66.fb_68, (char*) fb_57.fb_58, 253);
			   if (fbTrans && fb_55)
			      fb_55->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_57));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_55->release(); fb_55 = NULL; }
			   else break;
			   }
			if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
			while (1)
			   {
			   fb_55->receive (fbStatus, 0, 1, 16, CAST_MSG(&fb_59));
			   if (!fb_59.fb_62 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

				if (!/*G2.RDB$INITIAL_VALUE.NULL*/
				     fb_59.fb_63 && /*G2.RDB$INITIAL_VALUE*/
    fb_59.fb_60 != 0)
					isqlGlob.printf(" START WITH %" SQUADFORMAT, /*G2.RDB$INITIAL_VALUE*/
										     fb_59.fb_60);

				if (/*G2.RDB$GENERATOR_INCREMENT*/
				    fb_59.fb_61 != 1)
					isqlGlob.printf(" INCREMENT %" SLONGFORMAT, /*G2.RDB$GENERATOR_INCREMENT*/
										    fb_59.fb_61);

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

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);
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

	isqlGlob.printf(NEWLINE);
}


static void list_indexes()
{
   struct fb_40_struct {
          ISC_QUAD fb_41;	/* RDB$CONDITION_SOURCE */
          short fb_42;	/* fbUtility */
   } fb_40;
   struct fb_38_struct {
          char  fb_39 [253];	/* RDB$INDEX_NAME */
   } fb_38;
   struct fb_45_struct {
          ISC_QUAD fb_46;	/* RDB$EXPRESSION_SOURCE */
          ISC_QUAD fb_47;	/* RDB$EXPRESSION_BLR */
          short fb_48;	/* fbUtility */
          short fb_49;	/* gds__null_flag */
          short fb_50;	/* gds__null_flag */
          short fb_51;	/* RDB$INDEX_TYPE */
          short fb_52;	/* RDB$UNIQUE_FLAG */
          char  fb_53 [253];	/* RDB$RELATION_NAME */
          char  fb_54 [253];	/* RDB$INDEX_NAME */
   } fb_45;
/**************************************
 *
 *	l i s t _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Define all non-constraint indices
 *	Use a static SQL query to get the info and print it.
 *
 *	Uses get_index_segment to provide a key list for each index
 *
 **************************************/
	char collist[BUFFER_LENGTH512 * 2];

	bool first = true;

	/*FOR IDX IN RDB$INDICES CROSS RELC IN RDB$RELATIONS
		OVER RDB$RELATION_NAME
		WITH (RELC.RDB$SYSTEM_FLAG NE 1 OR RELC.RDB$SYSTEM_FLAG MISSING)
		AND NOT (ANY RC IN RDB$RELATION_CONSTRAINTS
				WITH RC.RDB$INDEX_NAME EQ IDX.RDB$INDEX_NAME)
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
	   if (!fb_43 && fbTrans && DB)
	      fb_43 = DB->compileRequest(fbStatus, sizeof(fb_44), fb_44);
	   if (fbTrans && fb_43)
	      fb_43->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_43->release(); fb_43 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_43->receive (fbStatus, 0, 0, 532, CAST_MSG(&fb_45));
	   if (!fb_45.fb_48 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (first)
		{
			isqlGlob.printf("%s/*  Index definitions for all user tables */%s", NEWLINE, NEWLINE);
			first = false;
		}

		// Strip trailing blanks
		fb_utils::exact_name(/*IDX.RDB$RELATION_NAME*/
				     fb_45.fb_53);
		fb_utils::exact_name(/*IDX.RDB$INDEX_NAME*/
				     fb_45.fb_54);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
		{
			IUTILS_copy_SQL_id (/*IDX.RDB$INDEX_NAME*/
					    fb_45.fb_54,    SQL_identifier,  DBL_QUOTE);
			IUTILS_copy_SQL_id (/*IDX.RDB$RELATION_NAME*/
					    fb_45.fb_53, SQL_identifier2, DBL_QUOTE);
			isqlGlob.printf("CREATE%s%s INDEX %s ON %s",
					(/*IDX.RDB$UNIQUE_FLAG*/
					 fb_45.fb_52 ? " UNIQUE" : ""),
					(/*IDX.RDB$INDEX_TYPE*/
					 fb_45.fb_51 ? " DESCENDING" : ""),
					SQL_identifier,
					SQL_identifier2);
		}
		else
			isqlGlob.printf("CREATE%s%s INDEX %s ON %s",
					(/*IDX.RDB$UNIQUE_FLAG*/
					 fb_45.fb_52 ? " UNIQUE" : ""),
					(/*IDX.RDB$INDEX_TYPE*/
					 fb_45.fb_51 ? " DESCENDING" : ""),
					/*IDX.RDB$INDEX_NAME*/
					fb_45.fb_54,
					/*IDX.RDB$RELATION_NAME*/
					fb_45.fb_53);

		// Get index expression or column names

		if (!/*IDX.RDB$EXPRESSION_BLR.NULL*/
		     fb_45.fb_50)
		{
			isqlGlob.printf(" COMPUTED BY ");
			if (!/*IDX.RDB$EXPRESSION_SOURCE.NULL*/
			     fb_45.fb_49)
				SHOW_print_metadata_text_blob(isqlGlob.Out, &/*IDX.RDB$EXPRESSION_SOURCE*/
									     fb_45.fb_46, false, true);
		}
		else if (ISQL_get_index_segments (collist, sizeof(collist), /*IDX.RDB$INDEX_NAME*/
									    fb_45.fb_54, true))
		{
			isqlGlob.printf(" (%s)", collist);
		}

		// Get index condition, if present

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
			   if (!fb_36 && DB)
			      fb_36 = DB->compileRequest(fbStatus, sizeof(fb_37), fb_37);
			   isc_vtov ((const char*) fb_45.fb_54, (char*) fb_38.fb_39, 253);
			   fb_36->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_38));
			   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_36->release(); fb_36 = NULL; }
			   else break;
			   }
			while (1)
			   {
			   fb_36->receive (fbStatus, 0, 1, 10, CAST_MSG(&fb_40));
			   if (!fb_40.fb_42) break;
			{
				isqlGlob.printf("%s ", NEWLINE);
				SHOW_print_metadata_text_blob(isqlGlob.Out, &/*IDX2.RDB$CONDITION_SOURCE*/
									     fb_40.fb_41, false, true);
			}
			/*END_FOR*/
			   }
			}
		}

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);

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


static void list_package_bodies()
{
   struct fb_29_struct {
          ISC_QUAD fb_30;	/* RDB$PACKAGE_BODY_SOURCE */
          short fb_31;	/* fbUtility */
          short fb_32;	/* gds__null_flag */
          short fb_33;	/* RDB$VALID_BODY_FLAG */
          char  fb_34 [253];	/* RDB$PACKAGE_NAME */
          char  fb_35 [253];	/* RDB$OWNER_NAME */
   } fb_29;
/**************************************
 *
 *	l i s t _ p a c k a g e _ b o d i e s
 *
 **************************************
 *
 * Functional description
 *	Create body of packages
 *	Use a SQL query to get the info and print it.
 *	Ignore the packages whose body has not been defined yet.
 *
 **************************************/

	if (isqlGlob.major_ods < ODS_VERSION12)
		return;

	bool header = true;

	/*FOR PACK IN RDB$PACKAGES WITH
		(PACK.RDB$SYSTEM_FLAG NE 1 OR PACK.RDB$SYSTEM_FLAG MISSING)
		AND PACK.RDB$PACKAGE_BODY_SOURCE NOT MISSING
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
	   if (!fb_27 && fbTrans && DB)
	      fb_27 = DB->compileRequest(fbStatus, sizeof(fb_28), fb_28);
	   if (fbTrans && fb_27)
	      fb_27->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_27->release(); fb_27 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_27->receive (fbStatus, 0, 0, 520, CAST_MSG(&fb_29));
	   if (!fb_29.fb_31 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (header)
		{
			print_proc_prefix(obj_package_body, false);
			header = false;
		}

		fb_utils::exact_name(/*PACK.RDB$PACKAGE_NAME*/
				     fb_29.fb_34);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*PACK.RDB$PACKAGE_NAME*/
					    fb_29.fb_34, SQL_identifier, DBL_QUOTE);
		else
			strcpy(SQL_identifier, /*PACK.RDB$PACKAGE_NAME*/
					       fb_29.fb_34);

		fb_utils::exact_name(/*PACK.RDB$OWNER_NAME*/
				     fb_29.fb_35);

		isqlGlob.printf("%s/* Package body: %s, Owner: %s%s */%s",
				 NEWLINE,
				 /*PACK.RDB$PACKAGE_NAME*/
				 fb_29.fb_34,
				 /*PACK.RDB$OWNER_NAME*/
				 fb_29.fb_35,
				 (!/*PACK.RDB$VALID_BODY_FLAG.NULL*/
				   fb_29.fb_32 && /*PACK.RDB$VALID_BODY_FLAG*/
    fb_29.fb_33 != 0 ? "" : ", Invalid"),
				 NEWLINE);

		if (!/*PACK.RDB$VALID_BODY_FLAG.NULL*/
		     fb_29.fb_32 && /*PACK.RDB$VALID_BODY_FLAG*/
    fb_29.fb_33 != 0)
		{
			isqlGlob.printf("CREATE PACKAGE BODY %s AS%s", SQL_identifier, NEWLINE);
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*PACK.RDB$PACKAGE_BODY_SOURCE*/
								     fb_29.fb_30);
			isqlGlob.printf("%s%s", Procterm, NEWLINE);
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

	if (!header)
		print_proc_suffix(obj_package_body);
}


static void list_package_headers()
{
   struct fb_19_struct {
          ISC_QUAD fb_20;	/* RDB$PACKAGE_HEADER_SOURCE */
          short fb_21;	/* fbUtility */
          short fb_22;	/* gds__null_flag */
          short fb_23;	/* gds__null_flag */
          char  fb_24 [253];	/* RDB$PACKAGE_NAME */
          char  fb_25 [253];	/* RDB$OWNER_NAME */
          FB_BOOLEAN fb_26;	/* RDB$SQL_SECURITY */
   } fb_19;
/**************************************
 *
 *	l i s t _ p a c k a g e _ h e a d e r s
 *
 **************************************
 *
 * Functional description
 *	Show header of packages
 *	Use a SQL query to get the info and print it.
 *
 **************************************/

 	if (isqlGlob.major_ods < ODS_VERSION12)
 		return;

 	bool header = true;

	/*FOR PACK IN RDB$PACKAGES WITH
		(PACK.RDB$SYSTEM_FLAG NE 1 OR PACK.RDB$SYSTEM_FLAG MISSING)
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
	   if (!fb_17 && fbTrans && DB)
	      fb_17 = DB->compileRequest(fbStatus, sizeof(fb_18), fb_18);
	   if (fbTrans && fb_17)
	      fb_17->start(fbStatus, fbTrans, 0);
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_17->release(); fb_17 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_17->receive (fbStatus, 0, 0, 521, CAST_MSG(&fb_19));
	   if (!fb_19.fb_21 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		if (header)
		{
			print_proc_prefix(obj_package_header, false);
			header = false;
		}

		fb_utils::exact_name(/*PACK.RDB$PACKAGE_NAME*/
				     fb_19.fb_24);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*PACK.RDB$PACKAGE_NAME*/
					    fb_19.fb_24, SQL_identifier, DBL_QUOTE);
		else
			strcpy(SQL_identifier, /*PACK.RDB$PACKAGE_NAME*/
					       fb_19.fb_24);

		fb_utils::exact_name(/*PACK.RDB$OWNER_NAME*/
				     fb_19.fb_25);

		isqlGlob.printf("%s/* Package header: %s, Owner: %s */%s",
				 NEWLINE,
				 /*PACK.RDB$PACKAGE_NAME*/
				 fb_19.fb_24,
				 /*PACK.RDB$OWNER_NAME*/
				 fb_19.fb_25,
				 NEWLINE);

		const char* ss = /*PACK.RDB$SQL_SECURITY.NULL*/
				 fb_19.fb_23 ? "" :
						(/*PACK.RDB$SQL_SECURITY*/
						 fb_19.fb_26 ? " SQL SECURITY DEFINER" : " SQL SECURITY INVOKER");

		isqlGlob.printf("CREATE PACKAGE %s%s AS%s", SQL_identifier, ss, NEWLINE);

		if (!/*PACK.RDB$PACKAGE_HEADER_SOURCE.NULL*/
		     fb_19.fb_22)
			SHOW_print_metadata_text_blob(isqlGlob.Out, &/*PACK.RDB$PACKAGE_HEADER_SOURCE*/
								     fb_19.fb_20);
		else
			isqlGlob.printf("BEGIN END /* Missing package header info. */");

		isqlGlob.printf("%s%s", Procterm, NEWLINE);

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

	if (!header)
		print_proc_suffix(obj_package_header);
}


static void list_views()
{
   struct fb_4_struct {
          short fb_5;	/* fbUtility */
          char  fb_6 [253];	/* RDB$FIELD_NAME */
   } fb_4;
   struct fb_2_struct {
          char  fb_3 [253];	/* RDB$RELATION_NAME */
   } fb_2;
   struct fb_11_struct {
          ISC_QUAD fb_12;	/* RDB$VIEW_SOURCE */
          short fb_13;	/* fbUtility */
          short fb_14;	/* gds__null_flag */
          char  fb_15 [253];	/* RDB$RELATION_NAME */
          char  fb_16 [253];	/* RDB$OWNER_NAME */
   } fb_11;
   struct fb_9_struct {
          short fb_10;	/* RDB$FLAGS */
   } fb_9;
/**************************************
 *
 *	l i s t _ v i e w s
 *
 **************************************
 *
 * Functional description
 *	Create text of views.
 *	Use a SQL query to get the info and print it.
 *	Note: This should also contain check option
 *
 **************************************/

	// If this is a view, use print_blob to print the view text

	/*FOR REL IN RDB$RELATIONS WITH
		(REL.RDB$SYSTEM_FLAG NE 1 OR REL.RDB$SYSTEM_FLAG MISSING) AND
		REL.RDB$VIEW_BLR NOT MISSING AND
		REL.RDB$FLAGS = REL_sql
		SORTED BY REL.RDB$RELATION_ID*/
	{
	for (int retries = 0; retries < 2; ++retries)
	   {
	   {
	   if (!DB)
	      DB = fbProvider->attachDatabase(fbStatus, isqlGlob.global_Db_name, 0, NULL);
	   if (DB && !fbTrans)
	      fbTrans = DB->startTransaction(fbStatus, 0, NULL);
	   }
	   if (!fb_7 && fbTrans && DB)
	      fb_7 = DB->compileRequest(fbStatus, sizeof(fb_8), fb_8);
	   fb_9.fb_10 = REL_sql;
	   if (fbTrans && fb_7)
	      fb_7->startAndSend(fbStatus, fbTrans, 0, 0, 2, CAST_CONST_MSG(&fb_9));
	   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_7->release(); fb_7 = NULL; }
	   else break;
	   }
	if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
	while (1)
	   {
	   fb_7->receive (fbStatus, 0, 1, 518, CAST_MSG(&fb_11));
	   if (!fb_11.fb_13 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

		fb_utils::exact_name(/*REL.RDB$RELATION_NAME*/
				     fb_11.fb_15);

		if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
			IUTILS_copy_SQL_id (/*REL.RDB$RELATION_NAME*/
					    fb_11.fb_15, SQL_identifier, DBL_QUOTE);
		else
			strcpy (SQL_identifier, /*REL.RDB$RELATION_NAME*/
						fb_11.fb_15);

		fb_utils::exact_name(/*REL.RDB$OWNER_NAME*/
				     fb_11.fb_16);

		isqlGlob.printf("%s/* View: %s, Owner: %s */%s",
				 NEWLINE,
				 /*REL.RDB$RELATION_NAME*/
				 fb_11.fb_15,
				 /*REL.RDB$OWNER_NAME*/
				 fb_11.fb_16,
				 NEWLINE);
		isqlGlob.printf("CREATE VIEW %s (", SQL_identifier);

		bool first = true;

		// Get column list
		/*FOR RFR IN RDB$RELATION_FIELDS WITH
			RFR.RDB$RELATION_NAME = REL.RDB$RELATION_NAME
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
		   if (!fb_0 && fbTrans && DB)
		      fb_0 = DB->compileRequest(fbStatus, sizeof(fb_1), fb_1);
		   isc_vtov ((const char*) fb_11.fb_15, (char*) fb_2.fb_3, 253);
		   if (fbTrans && fb_0)
		      fb_0->startAndSend(fbStatus, fbTrans, 0, 0, 253, CAST_CONST_MSG(&fb_2));
		   if (fbStatus->getErrors()[1] == isc_bad_req_handle) { fb_0->release(); fb_0 = NULL; }
		   else break;
		   }
		if (!(fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) {
		while (1)
		   {
		   fb_0->receive (fbStatus, 0, 1, 255, CAST_MSG(&fb_4));
		   if (!fb_4.fb_5 || (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)) break;

			fb_utils::exact_name(/*RFR.RDB$FIELD_NAME*/
					     fb_4.fb_6);

			if (isqlGlob.db_SQL_dialect > SQL_DIALECT_V6_TRANSITION)
				IUTILS_copy_SQL_id (/*RFR.RDB$FIELD_NAME*/
						    fb_4.fb_6, SQL_identifier, DBL_QUOTE);
			else
				strcpy (SQL_identifier, /*RFR.RDB$FIELD_NAME*/
							fb_4.fb_6);

			isqlGlob.printf("%s%s", (first ? "" : ", "), SQL_identifier);
			first = false;

		/*END_FOR*/
		   }
		   };
		/*ON_ERROR*/
		if (fbStatus->getState() & Firebird::IStatus::STATE_ERRORS)
		   {
			ISQL_errmsg (fbStatus);
			return;
		/*END_ERROR;*/
		   }
		}

		isqlGlob.printf(") AS%s", NEWLINE);

		if (!/*REL.RDB$VIEW_SOURCE.NULL*/
		     fb_11.fb_14)
			SHOW_print_metadata_text_blob (isqlGlob.Out, &/*REL.RDB$VIEW_SOURCE*/
								      fb_11.fb_12);

		isqlGlob.printf("%s%s", isqlGlob.global_Term, NEWLINE);

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

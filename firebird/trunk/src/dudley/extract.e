/*
 *	PROGRAM:	DUDLEY (ddl compiler)
 *	MODULE:		extract.e
 *	DESCRIPTION:	definition extract routines
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

#include "../jrd/ib_stdio.h"
#include <string.h>
#include "../jrd/gds.h"
#include "../dudley/ddl.h"
#include "../jrd/acl.h"
#include "../jrd/flags.h"
#include "../jrd/obj.h"
#include "../wal/wal.h"
#include "../dudley/ddl_proto.h"
#include "../dudley/extra_proto.h"
#include "../jrd/gds_proto.h"

DATABASE DB = EXTERN FILENAME "yachts.lnk" RUNTIME DB_file_name;

#ifndef FOPEN_WRITE_TYPE
#define FOPEN_WRITE_TYPE	"w"
#endif

#define BLOB_NULL(id)	(!id.gds_quad_low && !id.gds_quad_high)

typedef struct view {
	struct view *view_next;
	TEXT view_name[1];
} *VIEW;

static void binary_to_ascii(SLONG, SSHORT, TEXT *);
static void decompile_blr_literal(SCHAR *);
static void extract_acls(void);
static void extract_computed(TEXT *, BOOLEAN);
static void extract_database(TEXT *);
static void extract_fields(void);
static void extract_filters(void);
static void extract_functions(void);
static void extract_func_args(TEXT *);
static void extract_generators(void);
static void extract_grants(TEXT *);
static void extract_indexes(void);
static void extract_relations(void);
static void extract_rfr(TEXT *);
static void extract_security(void);
static void extract_triggers(void);
static void extract_trig_msgs(TEXT *);
static void extract_view(TEXT *, SLONG *, SLONG *, USHORT, USHORT, USHORT,
						 BOOLEAN *);
static void extract_views(void);
static void field_attributes(SLONG *, TEXT *, SLONG *, TEXT *, SSHORT);
static void format_acl(SCHAR *);
static int name_trunc(TEXT *, TEXT *);
static void print_blob(SLONG *, TEXT);
static void put_comment(USHORT);
static void put_error(USHORT, TEXT *);
static void set_capabilities(void);
static void type_scalar(USHORT, SSHORT, USHORT, SSHORT, USHORT, TEXT *);
static void view_fields(TEXT *);
static void wal_info(UCHAR *, SSHORT *, SLONG *, SLONG *, SLONG *);

static IB_FILE *output_file;
static SCHAR db_items[] = { gds__info_page_size };
static SCHAR db_info[] =
	{ gds__info_num_wal_buffers, gds__info_wal_buffer_size,
	gds__info_wal_ckpt_length, gds__info_wal_grpc_wait_usecs
};
static TEXT acl_privs[] = "?CGDRWP???";
static TEXT *acl_ids[] = {
	"?",
	"group",
	"user",
	"person",
	"project",
	"organization",
	"node",
	"view"
};


static SLONG EXT_capabilities;

#define	EXT_security		1
#define EXT_files		2
#define EXT_external		4
#define EXT_idx_inactive 	8
#define EXT_triggers		16	/* Obsolete - 1996-Aug-05 */
#define EXT_context_name	32
#define EXT_db_description	64
#define EXT_v3			128		/* rdb$dependencies, rdb$functions, rdb$filters, rdb$user_privileges */
					/* rdb$triggers, rdb$types, rdb$indices.rdb$index_type */
					/* rdb$fields.rdb$dimensions, rdb$files.rdb$shadow_number */
					/* rdb$generators */

#define EXT_v4			256

/* table used to determine capabilities, checking for specific 
   fields in system relations */

typedef struct rfr_tab_t {
	TEXT *relation;
	TEXT *field;
	int bit_mask;
} *RFR_TAB;

static struct rfr_tab_t rfr_table[] = {
	"RDB$INDICES", "RDB$INDEX_INACTIVE", EXT_idx_inactive,
/* Obsolete - 1996-Aug-05 David Schnepper 
	"RDB$RELATIONS",	"RDB$STORE_TRIGGER",	EXT_triggers,
*/
	"RDB$RELATIONS", "RDB$EXTERNAL_FILE", EXT_external,
	"RDB$SECURITY_CLASSES", "RDB$SECURITY_CLASS", EXT_security,
	"RDB$FILES", "RDB$FILE_NAME", EXT_files,
	"RDB$VIEW_RELATIONS", "RDB$CONTEXT_NAME", EXT_context_name,
	"RDB$DATABASE", "RDB$DESCRIPTION", EXT_db_description,
	"RDB$FUNCTIONS", "RDB$FUNCTION_NAME", EXT_v3,
	"RDB$LOG_FILES", "RDB$FILE_PARTITIONS", EXT_v4,
	0, 0, 0
};


void DDL_ext(void)
{
/**************************************
 *
 *	D D L _  e x t
 *
 **************************************
 *
 * Functional description
 *	Create an output text file and dump data
 *	definitions into it.
 *
 **************************************/

	output_file = NULL;

	if (*DDL_file_name) {
		if (!(output_file = ib_fopen(DDL_file_name, FOPEN_WRITE_TYPE))) {
			DDL_err(248, DDL_file_name, NULL, NULL, NULL, NULL);	/* msg 248: ddl: can't open %s */
			DDL_exit(FINI_ERROR);
		}
	}
	else
		output_file = ib_stdout;

	if (DDL_default_user || DDL_default_password)
		READY DB USER DDL_default_user PASSWORD DDL_default_password;
	else
		READY DB;

/* the database version number is useful during extract - this goes
   to ib_stdout, not to the output_file */
	if (DDL_version) {
		DDL_msg_put(249, DB_file_name, NULL, NULL, NULL, NULL);	/* msg 249: Version(s) for database \"%s\" */
		gds__version(&DB, NULL, NULL);
	}

	START_TRANSACTION;

	set_capabilities();

	extract_database(DB_file_name);
	if (EXT_capabilities & EXT_v3)
		extract_functions();
	extract_fields();
	extract_relations();
	extract_views();
	extract_indexes();
	if (EXT_capabilities & EXT_security) {
		extract_security();
		extract_acls();
	}
	if (EXT_capabilities & EXT_v3) {
		extract_filters();
		extract_generators();
		extract_triggers();
	}

	ib_fprintf(output_file, "\n");
	ib_fclose(output_file);
	COMMIT;
	FINISH;
}


static void binary_to_ascii(SLONG value, SSHORT scale, TEXT * buffer)
{
/**************************************
 *
 *	b i n a r y _ t o _ a s c i i
 *
 **************************************
 *
 * Functional description
 *	Convert a binary number to ascii and
 *	return it in the user supplied buffer;
 *
 **************************************/
	TEXT *p, *q, temp[32];

	p = buffer;

/* Handle non-negative scale factor in straightforward manner */

	if (scale >= 0) {
		sprintf(p, "%d", value);
		while (*p)
			p++;
		if (scale)
			do
				*p++ = '0';
			while (--scale);
		return;
	}

	if (value < 0) {
		*p++ = '-';
		value = -value;
	}

	for (q = temp; value || scale < 0;) {
		*q++ = value % 10 + '0';
		value /= 10;
		if (!++scale) {
			*q++ = '.';
			if (!value)
				*q++ = '0';
		}
	}

	while (q > temp)
		*p++ = *--q;

	*p = 0;
}


static void decompile_blr_literal(SCHAR * string)
{
/**************************************
 *
 *	d e c o m p i l e _ b l r _ l i t e r a l
 *
 **************************************
 *
 * Functional description
 *	decompile a blr literal
 *	and write it to the output file.
 *
 **************************************/
	SCHAR *p, *q, num_buffer[20], scale;
	TEXT *b, buffer[256];
	SSHORT size;
	SLONG value;

	p = string;

	if ((*p++ != blr_version4) || (*p++ != blr_literal)) {
		put_comment(250);		/* msg 250: **** unable to decompile missing value *** */
		return;
	}

	b = buffer;

	switch (*p++) {
	case (blr_text):
		size = gds__vax_integer(p, 2);
		p += 2;
		*b++ = '"';
		while (size--)
			*b++ = *p++;
		*b++ = '"';
		*b = 0;
		break;

#if (defined JPN_SJIS || defined JPN_EUC)
	case (blr_text2):
		{
			USHORT len;

			/* Convert strings to local encoding */

			scale = gds__vax_integer(p, 2);
			p += 2;
			size = gds__vax_integer(p, 2);
			p += 2;
			*b++ = '"';

			if (DDL_interp == scale) {
				while (size--)
					*b++ = *p++;
			}

			else if (DDL_interp == gds__interp_jpn_euc &&
					 scale == gds__interp_jpn_sjis) {
				if (KANJI_sjis2euc(p, size, b, 253, &len)) {
					put_comment(250);
					/* msg 250: **** unable to decompile missing value *** */
					return;
				}
				b += len;
				p += size;
			}

			else if (DDL_interp == gds__interp_jpn_sjis &&
					 scale == gds__interp_jpn_euc) {
				if (KANJI_euc2sjis(p, size, b, 253, &len)) {
					put_comment(250);
					/* msg 250: **** unable to decompile missing value *** */
					return;
				}
				b += len;
				p += size;
			}

			else {
				put_comment(250);
				/* msg 250: **** unable to decompile missing value *** */
				return;
			}


			*b++ = '"';
			*b = 0;
			break;
		}
#endif

	case (blr_long):
		scale = *p++;
		value = gds__vax_integer(p, 4);
		binary_to_ascii(value, scale, buffer);
		break;

	case (blr_short):
		scale = *p++;
		value = gds__vax_integer(p, 2);
		binary_to_ascii(value, scale, buffer);
		break;

	default:
		gds__msg_format(NULL_PTR, DDL_MSG_FAC, 250, sizeof(buffer), buffer,
						NULL, NULL, NULL, NULL, NULL);
		/* msg 250: **** unable to decompile missing value *** */
	}

	ib_fprintf(output_file, "%s", buffer);
}


static void extract_acls(void)
{
/**************************************
 *
 *	e x t r a c t _ a c l s
 *
 **************************************
 *
 * Functional description
 *	Read security class records and
 *	and write them to the output file.
 *	For each security class record determine if there is
 *	any GRANT user privileges associated with it.  If so,
 *	extract GRANT statement rather than a define security class
 *	statement.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32], *relname;
	USHORT count;

	first = TRUE;
    
    FOR S IN RDB$SECURITY_CLASSES 
        if (first) {
            put_comment(251);       /* msg 251: \n\n\tSecurity Class Definitions / GRANT statements\t */
            first = FALSE;
        } 
    count = 0;
	
    /* if there's any rdb$user_privileges entry then we can use GRANT statement */
    /* resort to parsing rdb$security_class for SQL$<relation-name> instead of
       just checking for rdb$security_classes.rdb$relation_name since it's
       possible that the security class was used on some other relation than
       the one GRANTed ON */
	
    if (EXT_capabilities & EXT_v3) {
        if (strncmp(S.RDB$SECURITY_CLASS, "SQL$", 4) == 0) {
            strcpy(s, S.RDB$SECURITY_CLASS);
            relname = &s[4];
            if (EXT_capabilities & EXT_v4) {
                FOR U IN RDB$USER_PRIVILEGES WITH 
                    U.RDB$RELATION_NAME = relname AND 
                    U.RDB$OBJECT_TYPE = obj_relation 
                    count++;
                END_FOR;
            }
            else {
                FOR U IN RDB$USER_PRIVILEGES WITH U.RDB$RELATION_NAME = relname 
                    count++;
                END_FOR;
            }
            
            /* if there's no rdb$user_privileges entry, but we still find a
               SQL$ security class, that's the relation owner's privilege list.
               Ignore this as the new database created will have a new owner
               and should start fresh with no security class defined for
               owner */
            
            if (!count)
                continue;
        }
    }
    if (count)
        extract_grants(relname);
    else {
        ib_fprintf(output_file, "\ndefine security_class %s",
                   S.RDB$SECURITY_CLASS);
        if (!BLOB_NULL(S.RDB$DESCRIPTION)) {
            ib_fprintf(output_file, "\n\t{");
            print_blob(&S.RDB$DESCRIPTION, 'u');
            ib_fprintf(output_file, "}");
        }
        if (!BLOB_NULL(S.RDB$ACL))
            print_blob(&S.RDB$ACL, 'a');
        ib_fprintf(output_file, ";\n");
    }
    END_FOR;
}


static void extract_computed(TEXT * relation_name, BOOLEAN first)
{
/**************************************
 *
 *	e x t r a c t _ c o m p u t e d
 *
 **************************************
 *
 * Functional description
 *	Read definitions of computed fields
 *	and write them to the output file.
 *
 *	Check on the way by that we're not
 *	trying to expand a computed field
 *	that's been copied whole into a
 *	view.  Meaning that if there is a 
 *	view context, skip this field
 *
 **************************************/
	TEXT s[32];

    FOR RFR IN RDB$RELATION_FIELDS CROSS F IN RDB$FIELDS
        WITH RFR.RDB$RELATION_NAME = relation_name
        AND F.RDB$FIELD_NAME = RFR.RDB$FIELD_SOURCE
        AND F.RDB$COMPUTED_BLR NOT MISSING
        SORTED BY RFR.RDB$FIELD_POSITION 

        if (RFR.RDB$VIEW_CONTEXT) 
            continue;
	
        if (!first)
            ib_fprintf(output_file, ",\n");
        
        first = FALSE;
        name_trunc(RFR.RDB$FIELD_NAME, s);
        if (F.RDB$COMPUTED_SOURCE.NULL) {
            put_error(252, s);      /* msg 252: **** field %s can not be extracted, computed source missing ***  */
            continue;
        }
        ib_fprintf(output_file, "    %s", s);
        
        strcpy(s, F.RDB$FIELD_NAME);
        if (EXT_capabilities & EXT_v3) {
            FOR FNDA IN RDB$FIELDS WITH FNDA.RDB$FIELD_NAME = s
                type_scalar(FNDA.RDB$FIELD_TYPE, FNDA.RDB$FIELD_SCALE,
	                       FNDA.RDB$FIELD_LENGTH,
	                       FNDA.RDB$FIELD_SUB_TYPE, FNDA.RDB$DIMENSIONS, s);
	       END_FOR;
        }
        else {
            FOR FNDA IN RDB$FIELDS WITH FNDA.RDB$FIELD_NAME = s
                type_scalar(FNDA.RDB$FIELD_TYPE, FNDA.RDB$FIELD_SCALE,
                            FNDA.RDB$FIELD_LENGTH,
                            FNDA.RDB$FIELD_SUB_TYPE, 0, NULL);
            END_FOR;
        }
        ib_fprintf(output_file, " computed by (");
        
        print_blob(&F.RDB$COMPUTED_SOURCE, 'u');
        ib_fprintf(output_file, ")");
        if (!RFR.RDB$FIELD_POSITION.NULL)
            ib_fprintf(output_file, "\tposition %d", RFR.RDB$FIELD_POSITION);
        field_attributes(&RFR.RDB$DESCRIPTION, RFR.RDB$QUERY_NAME,
                         &RFR.RDB$QUERY_HEADER, RFR.RDB$EDIT_STRING,
                         RFR.RDB$SYSTEM_FLAG);
        
    END_FOR;
}


static void extract_database(TEXT * DB_file_name)
{
/**************************************
 *
 *	e x t r a c t _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Get the essential database information
 *	and write it to the output file.
 *
 **************************************/
	TEXT s[32], buffer[256], *d;
	SSHORT length, any_shadows;
	SLONG page_size;
	STATUS status_vector[20];
	BOOLEAN first_time;
	SSHORT need_close;
	SCHAR db_info_buffer[64];
	SSHORT num_buf;
	SLONG buf_size;
	SLONG chkpt_len;
	SLONG grp_commit;

	ib_fprintf(output_file, "define database \"%s\"", DB_file_name);

	if (EXT_capabilities & EXT_security) {
	       FOR D IN RDB$DATABASE 
               if (!BLOB_NULL(D.RDB$DESCRIPTION)) {
                   ib_fprintf(output_file, "\n\t{");
                   print_blob(&D.RDB$DESCRIPTION, 'u');
                   ib_fprintf(output_file, "}");
               } 
               if (!D.RDB$SECURITY_CLASS.NULL) {
                   name_trunc(D.RDB$SECURITY_CLASS, s);
                   if (*s)
                       ib_fprintf(output_file, "\n\tsecurity_class %s", s);
               }
	       END_FOR;
	}
	else if (EXT_capabilities & EXT_db_description)
        FOR D IN RDB$DATABASE 
            if (!BLOB_NULL(D.RDB$DESCRIPTION)) {
                ib_fprintf(output_file, "\n\t{");
                print_blob(&D.RDB$DESCRIPTION, 'u');
                ib_fprintf(output_file, "}");
            } 
        END_FOR;

		if (gds__database_info(status_vector,
							   GDS_REF(DB),
							   sizeof(db_items),
							   db_items, sizeof(buffer), buffer)) {
			put_error(254, 0);	/* msg 254: ***gds__database_info failed*** */
			gds__print_status(status_vector);
		}

        d = buffer;
        if (*d++ == gds__info_page_size) {
            length = gds__vax_integer(d, 2);
            d += 2;
            page_size = gds__vax_integer(d, length);
            if (page_size)
                ib_fprintf(output_file, "\n\tpage_size %d", page_size);
        }
        
        if (EXT_capabilities & EXT_files)
            if (EXT_capabilities & EXT_v3) {
                FOR F IN RDB$FILES WITH F.RDB$SHADOW_NUMBER MISSING OR 
                    F.RDB$SHADOW_NUMBER EQ 0 
                    name_trunc(F.RDB$FILE_NAME, buffer);
                    ib_fprintf(output_file, "\n\tfile \"%s\" starting at page %d",
                               buffer, F.RDB$FILE_START);
                END_FOR;
            }
            else
                FOR F IN RDB$FILES name_trunc(F.RDB$FILE_NAME, buffer);
                    ib_fprintf(output_file, "\n\tfile \"%s\" starting at page %d",
                               buffer, F.RDB$FILE_START);
                END_FOR;

/* print log info. */

		if (EXT_capabilities & EXT_v4) {
			first_time = TRUE;
			need_close = 0;

	       FOR F IN RDB$LOG_FILES if (F.RDB$FILE_FLAGS & LOG_overflow)
	             continue;
	
	       if (first_time) {
	           first_time = FALSE;
	           ib_fprintf(output_file, "\n\tlogfile");
	
	           if (F.RDB$FILE_FLAGS & LOG_default)
	               break;
	           else if (F.RDB$FILE_FLAGS & LOG_serial)
	               ib_fprintf(output_file, " base_name");
	           else {
	               need_close = 1;
	               ib_fprintf(output_file, "\n\t\t(");
	           }
	
	           ib_fprintf(output_file, "\n\t\t\"%s\"", F.RDB$FILE_NAME);
	       }
	       else
	           ib_fprintf(output_file, ",\n\t\t\"%s\"", F.RDB$FILE_NAME);
	
	       if (F.RDB$FILE_LENGTH)
	           ib_fprintf(output_file, " size = %d", F.RDB$FILE_LENGTH);
	
	       if (F.RDB$FILE_PARTITIONS)
	           ib_fprintf(output_file, " partitions = %d",
	                      F.RDB$FILE_PARTITIONS);
	       if (F.RDB$FILE_FLAGS & LOG_raw)
	           ib_fprintf(output_file, " raw");
	
	       END_FOR;

			if (need_close)
				ib_fprintf(output_file, "\n\t\t)");

	       FOR F IN RDB$LOG_FILES if (!(F.RDB$FILE_FLAGS & LOG_overflow))
	             continue;
	
	       ib_fprintf(output_file, "\n\toverflow");
	       ib_fprintf(output_file, " \"%s\"", F.RDB$FILE_NAME);
	
	       if (F.RDB$FILE_LENGTH)
	           ib_fprintf(output_file, " size = %d", F.RDB$FILE_LENGTH);
	
	       if (F.RDB$FILE_PARTITIONS)
	           ib_fprintf(output_file, " partitions = %d",
	                      F.RDB$FILE_PARTITIONS);
	       END_FOR;

			if (gds__database_info(gds__status,
								   GDS_REF(DB),
								   sizeof(db_info),
								   db_info,
								   sizeof(db_info_buffer), db_info_buffer)) {
				put_error(254, 0);	/* msg 254: ***gds__database_info failed*** */
				gds__print_status(status_vector);
			}

			buf_size = chkpt_len = grp_commit = 0;
			num_buf = 0;

			wal_info(db_info_buffer, &num_buf, &buf_size, &chkpt_len,
					 &grp_commit);

			if (num_buf)
				ib_fprintf(output_file, "\n\tnum_log_buffers %d", num_buf);
			if (buf_size)
				ib_fprintf(output_file, "\n\tlog_buffer_size %d", buf_size);
			if (chkpt_len)
				ib_fprintf(output_file, "\n\tcheck_point_length %d",
						   chkpt_len / OneK);
			if (grp_commit)
				ib_fprintf(output_file, "\n\tgroup_commit_wait_time %d",
						   grp_commit);
		}

	ib_fprintf(output_file, ";\n");

	if (EXT_capabilities & EXT_v3) {
		any_shadows = FALSE;

	       FOR F IN RDB$FILES WITH F.RDB$SHADOW_NUMBER GT 0
	           SORTED BY F.RDB$SHADOW_NUMBER, F.RDB$FILE_START
	           name_trunc(F.RDB$FILE_NAME, buffer);
	           if (F.RDB$FILE_START)
                   ib_fprintf(output_file, "\n\tfile \"%s\" starting at page %d",
                              buffer, F.RDB$FILE_START);
               else
                   ib_fprintf(output_file, "%s\ndefine shadow %d \"%s\"",
                              (any_shadows) ? ";\n" : "", F.RDB$SHADOW_NUMBER,
                              buffer);
               any_shadows = TRUE;
	       END_FOR;

		if (any_shadows)
			ib_fprintf(output_file, ";\n");
	}
}


static void extract_fields(void)
{
/**************************************
 *
 *	e x t r a c t _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Read the global field definitions
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32];

	first = TRUE;
    FOR F IN RDB$FIELDS WITH
        (F.RDB$SYSTEM_FLAG NE 1 OR F.RDB$SYSTEM_FLAG MISSING) AND
        F.RDB$COMPUTED_BLR MISSING SORTED BY F.RDB$FIELD_NAME 

        if (first) {
            put_comment(255);       /*msg 255: \n\n\tGlobal Field Definitions\t */
            first = FALSE;
        } 
        name_trunc(F.RDB$FIELD_NAME, s);
        ib_fprintf(output_file, "define field %s", s);
        if (F.RDB$FIELD_TYPE == blr_blob) {
            ib_fprintf(output_file, " blob");
            if (F.RDB$SEGMENT_LENGTH)
                ib_fprintf(output_file, " segment_length %u",
                           (USHORT) F.RDB$SEGMENT_LENGTH);
            if (F.RDB$FIELD_SUB_TYPE == 1)
                ib_fprintf(output_file, " sub_type text");
            else if (F.RDB$FIELD_SUB_TYPE == 2)
                ib_fprintf(output_file, " sub_type BLR");
            else if (F.RDB$FIELD_SUB_TYPE == 3)
                ib_fprintf(output_file, " sub_type ACL");
            else if (F.RDB$FIELD_SUB_TYPE)
                ib_fprintf(output_file, " sub_type %d", F.RDB$FIELD_SUB_TYPE);
        }
        else {
            strcpy(s, F.RDB$FIELD_NAME);
            if (EXT_capabilities & EXT_v3) {
                FOR FNDA IN RDB$FIELDS WITH FNDA.RDB$FIELD_NAME = s
                    type_scalar(FNDA.RDB$FIELD_TYPE, FNDA.RDB$FIELD_SCALE,
                                FNDA.RDB$FIELD_LENGTH,
                                FNDA.RDB$FIELD_SUB_TYPE, FNDA.RDB$DIMENSIONS, s);
                END_FOR;
            }
            else {
                FOR FNDA IN RDB$FIELDS WITH FNDA.RDB$FIELD_NAME = s
                    type_scalar(FNDA.RDB$FIELD_TYPE, FNDA.RDB$FIELD_SCALE,
                                FNDA.RDB$FIELD_LENGTH,
                                FNDA.RDB$FIELD_SUB_TYPE, 0, NULL);
                END_FOR;
            }
            
        }
        
        if (!BLOB_NULL(F.RDB$VALIDATION_SOURCE)) {
            ib_fprintf(output_file, "\n\tvalid if (");
            print_blob(&F.RDB$VALIDATION_SOURCE, 'u');
            ib_fprintf(output_file, ")");
        }
        
        if (!BLOB_NULL(F.RDB$MISSING_VALUE)) {
            ib_fprintf(output_file, "\n\tmissing_value is ");
            print_blob(&F.RDB$MISSING_VALUE, 'm');
        }
        
        /*
          if (!BLOB_NULL (F.RDB$DEFAULT_VALUE))
          default_value (&F.RDB$DEFAULT_VALUE);
        */
        
        field_attributes(&F.RDB$DESCRIPTION, F.RDB$QUERY_NAME,
                         &F.RDB$QUERY_HEADER, F.RDB$EDIT_STRING,
                         F.RDB$SYSTEM_FLAG);
        ib_fprintf(output_file, ";\n");
        
    END_FOR;
}


static void extract_filters(void)
{
/**************************************
 *
 *	e x t r a c t _ f i l t e r s 
 *
 **************************************
 *
 * Functional description
 *	Read filter definitions
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32];

	first = TRUE;

	   FOR F IN RDB$FILTERS SORTED BY F.RDB$FUNCTION_NAME 
           if (first) {
               put_comment(256);       /* msg 256: \n\n\tFilter Definitions\t */
               first = FALSE;
           } 
           name_trunc(F.RDB$FUNCTION_NAME, s);
           ib_fprintf(output_file, "\ndefine filter %s", s);
           ib_fprintf(output_file, "\n\tinput_type %d", F.RDB$INPUT_SUB_TYPE);
           ib_fprintf(output_file, "\n\toutput_type %d", F.RDB$OUTPUT_SUB_TYPE);
           if (!F.RDB$MODULE_NAME.NULL && name_trunc(F.RDB$MODULE_NAME, s))
               ib_fprintf(output_file, "\n\tmodule_name '%s'", s);
           name_trunc(F.RDB$ENTRYPOINT, s);
           ib_fprintf(output_file, "\n\tentry_point '%s'", s);
           if (!BLOB_NULL(F.RDB$DESCRIPTION)) {
               ib_fprintf(output_file, "\n\t{");
               print_blob(&F.RDB$DESCRIPTION, 'u');
               ib_fprintf(output_file, "}");
           }
           ib_fprintf(output_file, ";\n");
	   END_FOR;
}


static void extract_functions(void)
{
/**************************************
 *
 *	e x t r a c t _ f u n c t i o n s
 *
 **************************************
 *
 * Functional description
 *	Read function definitions
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32];

	first = TRUE;

	   FOR FUNC IN RDB$FUNCTIONS SORTED BY FUNC.RDB$FUNCTION_NAME 
           if (first) {
               put_comment(257);       /* msg 257: \n\n\tFunction Definitions\t */
               first = FALSE;
           } 
           name_trunc(FUNC.RDB$FUNCTION_NAME, s);
           ib_fprintf(output_file, "\ndefine function %s", s);
           if (!FUNC.RDB$QUERY_NAME.NULL && name_trunc(FUNC.RDB$QUERY_NAME, s))
               ib_fprintf(output_file, "\n\tquery_name %s", s);
           if (!FUNC.RDB$MODULE_NAME.NULL && name_trunc(FUNC.RDB$MODULE_NAME, s))
               ib_fprintf(output_file, "\n\tmodule_name '%s'", s);
           name_trunc(FUNC.RDB$ENTRYPOINT, s);
           ib_fprintf(output_file, "\n\tentry_point '%s'", s);
           if (!BLOB_NULL(FUNC.RDB$DESCRIPTION)) {
               ib_fprintf(output_file, "\n\t{");
               print_blob(&FUNC.RDB$DESCRIPTION, 'u');
               ib_fprintf(output_file, "}");
           }
	
           extract_func_args(FUNC.RDB$FUNCTION_NAME);
	
           ib_fprintf(output_file, ";\n");
	   END_FOR;
}


static void extract_func_args(TEXT * function_name)
{
/**************************************
 *
 *	e x t r a c t _ f u n c _ a r g s
 *
 **************************************
 *
 * Functional description
 *	Read the argument descriptions for a function
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32], *p;

	first = TRUE;
	p = "";

    FOR FARG IN RDB$FUNCTION_ARGUMENTS
        WITH FARG.RDB$FUNCTION_NAME = function_name
        SORTED BY FARG.RDB$ARGUMENT_POSITION
        ib_fprintf(output_file, "%s\n", p);

        if (first) {
            p = ",";
            first = FALSE;
        }
        
        ib_fprintf(output_file, "\t\t");
        if (FARG.RDB$FIELD_TYPE == blr_blob)
            ib_fprintf(output_file, " blob");
        else {
            type_scalar(FARG.RDB$FIELD_TYPE, FARG.RDB$FIELD_SCALE,
                        FARG.RDB$FIELD_LENGTH, FARG.RDB$FIELD_SUB_TYPE, 0, NULL);
        }
        ib_fprintf(output_file, " by ");
        if (FARG.RDB$MECHANISM == FUNCARG_mechanism_value)
            ib_fprintf(output_file, "value");
        else if ((FARG.RDB$MECHANISM == FUNCARG_mechanism_reference) ||
                 (FARG.RDB$MECHANISM == FUNCARG_mechanism_blob_struc))
            ib_fprintf(output_file, "reference");
        else if (FARG.RDB$MECHANISM == FUNCARG_mechanism_sc_array_desc)
            ib_fprintf(output_file, "scalar_array_descriptor");
        
        FOR FUNC IN RDB$FUNCTIONS
            WITH FUNC.RDB$FUNCTION_NAME = function_name
            if (FARG.RDB$ARGUMENT_POSITION == FUNC.RDB$RETURN_ARGUMENT) {
                if (FARG.RDB$ARGUMENT_POSITION)
                ib_fprintf(output_file, " return_argument");
                else
                ib_fprintf(output_file, " return_value");
            } 
        END_FOR;
        
    END_FOR;
}


static void extract_generators(void)
{
/**************************************
 *
 *	e x t r a c t _ g e n e r a t o r s
 *
 **************************************
 *
 * Functional description
 *	Get generator definitions from rdb$generators.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32];

	first = TRUE;
    
    FOR G IN RDB$GENERATORS WITH
        (G.RDB$SYSTEM_FLAG NE 1 OR G.RDB$SYSTEM_FLAG MISSING)
        SORTED BY G.RDB$GENERATOR_NAME 
        if (first) {
            put_comment(290);       /* msg 290: \n\n\tGenerator Definitions\t */
            first = FALSE;
        } 
        name_trunc(G.RDB$GENERATOR_NAME, s);
        ib_fprintf(output_file, "\ndefine generator %s;", s);	
    END_FOR;
}


static void extract_grants(TEXT * relname)
{
/**************************************
 *
 *	e x t r a c t _ g r a n t s
 *
 **************************************
 *
 * Functional description
 *	Build as many GRANT statements as needed
 *	for the relation from reading user privilge records.
 *
 **************************************/
	TEXT relation_name[32], user_name[32], field_name[32], *user_priv;

	if (EXT_capabilities & EXT_v4) {
        FOR U IN RDB$USER_PRIVILEGES WITH U.RDB$RELATION_NAME EQ relname
            AND U.RDB$OBJECT_TYPE EQ obj_relation
            SORTED BY U.RDB$USER, U.RDB$PRIVILEGE 

            switch (U.RDB$PRIVILEGE[0]) {
            case 'S':
                user_priv = "SELECT";
                break;
                
            case 'U':
                user_priv = "UPDATE";
                break;
                
            case 'D':
                user_priv = "DELETE";
                break;
                
            case 'I':
	           user_priv = "INSERT";
	           break;
	
            case 'R':
                /* None of the interfaces knows about the references privilege.
                   Ignore it for now. */
                
                continue;
                
	       default:
	           user_priv = "**unknown**";
	           break;
            }
	
            ib_fprintf(output_file, "\ngrant %s ", user_priv);
            if (!U.RDB$FIELD_NAME.NULL) {
                name_trunc(U.RDB$FIELD_NAME, field_name);
                ib_fprintf(output_file, "(%s) ", field_name);
            }
            name_trunc(U.RDB$RELATION_NAME, relation_name);
            name_trunc(U.RDB$USER, user_name);
            ib_fprintf(output_file, "on %s to %s", relation_name, user_name);
            if (U.RDB$GRANT_OPTION)
                ib_fprintf(output_file, " with grant option");
            ib_fprintf(output_file, ";\n");
            
        END_FOR;
	}
	else {
        FOR U IN RDB$USER_PRIVILEGES WITH U.RDB$RELATION_NAME EQ relname
            SORTED BY U.RDB$USER, U.RDB$PRIVILEGE switch (U.RDB$PRIVILEGE[0]) {
            case 'S':
                user_priv = "SELECT";
                break;
                
            case 'U':
                user_priv = "UPDATE";
                break;
                
            case 'D':
                user_priv = "DELETE";
                break;
                
            case 'I':
                user_priv = "INSERT";
                break;
                
            case 'R':
                /* None of the interfaces knows about the references privilege.
                   Ignore it for now. */
                
                continue;
                
            default:
                user_priv = "**unknown**";
                break;
            }
        
            ib_fprintf(output_file, "\ngrant %s ", user_priv);
            if (!U.RDB$FIELD_NAME.NULL) {
                name_trunc(U.RDB$FIELD_NAME, field_name);
                ib_fprintf(output_file, "(%s) ", field_name);
            }
            name_trunc(U.RDB$RELATION_NAME, relation_name);
            name_trunc(U.RDB$USER, user_name);
            ib_fprintf(output_file, "on %s to %s", relation_name, user_name);
            if (U.RDB$GRANT_OPTION)
                ib_fprintf(output_file, " with grant option");
            ib_fprintf(output_file, ";\n");
            
	    END_FOR;
	}
}


static void extract_indexes(void)
{
/**************************************
 *
 *	e x t r a c t _ i n d e x e s
 *
 **************************************
 *
 * Functional description
 *	Read definitions of indexes
 *	and write them to the output file.
 *
 **************************************/
	TEXT index[32], relation[32], field[32], *p;
	BOOLEAN first;

	first = TRUE;

/* this query gets all IDX which donot have the systemflag set and
   further sorts them by realtion name and index name */
    FOR I IN RDB$INDICES CROSS R IN RDB$RELATIONS
        OVER RDB$RELATION_NAME
        WITH(R.RDB$SYSTEM_FLAG NE 1 OR R.RDB$SYSTEM_FLAG MISSING)
        SORTED BY I.RDB$RELATION_NAME, I.RDB$INDEX_NAME 
        if (first) {
            put_comment(291);       /* msg 291: \n\n\tIndex Definitions\t */
            first = FALSE;
        } 
        if (!I.RDB$UNIQUE_FLAG.NULL && I.RDB$UNIQUE_FLAG == 1)
            p = "unique";
        else
            p = "";
        name_trunc(I.RDB$INDEX_NAME, index);
        name_trunc(I.RDB$RELATION_NAME, relation);
        ib_fprintf(output_file, "\ndefine index %s for %s %s",
                   index, relation, p);
        
        if (!BLOB_NULL(I.RDB$DESCRIPTION)) {
            ib_fprintf(output_file, "\n\t{");
               print_blob(&I.RDB$DESCRIPTION, 'u');
               ib_fprintf(output_file, "}");
        }
        
        if (EXT_capabilities & EXT_idx_inactive)
            FOR ACT IN RDB$INDICES WITH ACT.RDB$INDEX_NAME = index
                if (ACT.RDB$INDEX_INACTIVE) 
                    ib_fprintf(output_file, "\ninactive");
        END_FOR;
        
        if (EXT_capabilities & EXT_v3)
            FOR ACT IN RDB$INDICES WITH ACT.RDB$INDEX_NAME = index
                   if (ACT.RDB$INDEX_TYPE == IDX_type_descend)
                       ib_fprintf(output_file, "\ndescending");
        END_FOR;
        
        
        p = "";
	
        FOR I_S IN RDB$INDEX_SEGMENTS WITH
            I_S.RDB$INDEX_NAME = I.RDB$INDEX_NAME
            SORTED BY I_S.RDB$FIELD_POSITION
            name_trunc(I_S.RDB$FIELD_NAME, field);
            ib_fprintf(output_file, "%s\n\t%s", p, field);
            p = ",";
        
        END_FOR;
	
        ib_fprintf(output_file, ";\n");
    END_FOR;
}


static void extract_relations(void)
{
/**************************************
 *
 *	e x t r a c t _ r e l a t i o n s
 *
 **************************************
 *
 * Functional description
 *	Read relation  definitions
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[256];

	first = TRUE;

	if (EXT_capabilities & EXT_external) {
        FOR R IN RDB$RELATIONS WITH
            (R.RDB$SYSTEM_FLAG NE 1 OR R.RDB$SYSTEM_FLAG MISSING) AND
            R.RDB$VIEW_BLR MISSING SORTED BY R.RDB$RELATION_NAME 
            if (first) {
                put_comment(258);   /* msg 258: \n\n\tRelation Definitions\t */
                first = FALSE;
            } 
            name_trunc(R.RDB$RELATION_NAME, s);
            ib_fprintf(output_file, "\ndefine relation %s", s);
	
            if (!R.RDB$EXTERNAL_FILE.NULL && name_trunc(R.RDB$EXTERNAL_FILE, s))
                ib_fprintf(output_file, "\n\texternal_file \"%s\"", s);
            
            if (R.RDB$SYSTEM_FLAG)
                ib_fprintf(output_file, "\n\tsystem_flag %d", R.RDB$SYSTEM_FLAG);
            if (!BLOB_NULL(R.RDB$DESCRIPTION)) {
                ib_fprintf(output_file, "\n\t{");
                print_blob(&R.RDB$DESCRIPTION, 'u');
                ib_fprintf(output_file, "}");
            }
            extract_rfr(R.RDB$RELATION_NAME);
            ib_fprintf(output_file, ";\n");
        END_FOR;
	}
	else {
        FOR R IN RDB$RELATIONS WITH
            (R.RDB$SYSTEM_FLAG NE 1 OR R.RDB$SYSTEM_FLAG MISSING) AND
            R.RDB$VIEW_BLR MISSING SORTED BY R.RDB$RELATION_NAME 
            if (first) {
                put_comment(258);   /* msg 258: \n\n\tRelation Definitions\t */
                first = FALSE;
            } 
            name_trunc(R.RDB$RELATION_NAME, s);
            ib_fprintf(output_file, "\ndefine relation %s", s);
            
            if (R.RDB$SYSTEM_FLAG)
                ib_fprintf(output_file, "\n\tsystem_flag %d", R.RDB$SYSTEM_FLAG);
            if (!BLOB_NULL(R.RDB$DESCRIPTION)) {
                ib_fprintf(output_file, "\n\t{");
                print_blob(&R.RDB$DESCRIPTION, 'u');
                ib_fprintf(output_file, "}");
            }
            extract_rfr(R.RDB$RELATION_NAME);
            ib_fprintf(output_file, ";\n");
        END_FOR;
	}

}


static void extract_rfr(TEXT * relation_name)
{
/**************************************
 *
 *	e x t r a c t _ r f r
 *
 **************************************
 *
 * Functional description
 *	Read the field descriptions for a relation
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32], *p;

	first = TRUE;
	p = "";

	FOR RFR IN RDB$RELATION_FIELDS CROSS F IN RDB$FIELDS
        WITH RFR.RDB$RELATION_NAME = relation_name
        AND F.RDB$FIELD_NAME = RFR.RDB$FIELD_SOURCE
        AND F.RDB$COMPUTED_BLR MISSING
        SORTED BY RFR.RDB$FIELD_POSITION 

        ib_fprintf(output_file, "%s\n", p);
        if (first) {
            p = ",";
            first = FALSE;
        }
        
        name_trunc(RFR.RDB$FIELD_NAME, s);
        ib_fprintf(output_file, "    %s", s);
        if (strcmp(RFR.RDB$FIELD_NAME, RFR.RDB$FIELD_SOURCE)) {
            name_trunc(RFR.RDB$FIELD_SOURCE, s);
            ib_fprintf(output_file, " based on %s", s);
        }
	
        if (!RFR.RDB$FIELD_POSITION.NULL)
            ib_fprintf(output_file, "\tposition %d", RFR.RDB$FIELD_POSITION);
        
        field_attributes(&RFR.RDB$DESCRIPTION, RFR.RDB$QUERY_NAME,
                         &RFR.RDB$QUERY_HEADER, RFR.RDB$EDIT_STRING,
                         RFR.RDB$SYSTEM_FLAG);
    END_FOR;

	extract_computed(relation_name, first);
}


static void extract_security(void)
{
/**************************************
 *
 *	e x t r a c t _ s e c u r i t y 
 *
 **************************************
 *
 * Functional description
 *	Get security class information for
 *	relations, views, and fields.
 *
 **************************************/
	BOOLEAN first, first_field;
	TEXT r[32], s[32], f[32], *p, rel1[32], rel2[32];
	TEXT modify;
	USHORT count;

	first = TRUE;

    FOR R IN RDB$RELATIONS 
        first_field = TRUE;
        modify = FALSE;
        name_trunc(R.RDB$RELATION_NAME, r);
        p = (R.RDB$VIEW_BLR.NULL) ? "relation" : "view";
        if (!R.RDB$SECURITY_CLASS.NULL && name_trunc(R.RDB$SECURITY_CLASS, s)) {
            /* skip modify relation/view add security class if we have just a stub -
               i.e. security_class is 'SQL$..' and has no rdb$user_privilege entry and
               relation_name matches 'SQL$relation_name'  */
            /* doesn't hurt to extract even if redundant with GRANT statement - i.e. 
               security_class is 'SQL$..' and has rdb$user_privilege entries */
            /* extract if security_class is 'SQL$..' but relation_name doesn't match
               'SQL$relation_name' */
            /* extract if security_class doesn't start with 'SQL$' */
	
            if (EXT_capabilities & EXT_v3) {
                if (strncmp(R.RDB$SECURITY_CLASS, "SQL$", 4) == 0) {
                    count = 0;
	               name_trunc(&R.RDB$SECURITY_CLASS[4], rel1);
	               name_trunc(R.RDB$RELATION_NAME, rel2);
	               if (strcmp(rel1, rel2) == 0) {
	                   FOR U IN RDB$USER_PRIVILEGES
	                       WITH U.RDB$RELATION_NAME EQ R.RDB$RELATION_NAME
	                       count++;
	                   END_FOR;
	               }
	               else
	                   count = 1;
	               if (!count)
	                   continue;
                }
            }
            
            if (first) {
	           put_comment(259);   /* msg 259: \n\tAdd Security Classes to Defined Objects\t\n  */
	           first = FALSE;
            }
            
	       ib_fprintf(output_file, "\nmodify %s %s\n\tsecurity_class %s", p, r,
	                  s);
	       modify = TRUE;
        }
        
        FOR RFR IN RDB$RELATION_FIELDS
            WITH RFR.RDB$RELATION_NAME EQ R.RDB$RELATION_NAME AND
            RFR.RDB$SECURITY_CLASS NOT MISSING 
            name_trunc(RFR.RDB$FIELD_NAME, f);
            if (name_trunc(RFR.RDB$SECURITY_CLASS, s)) {
                if (first) {
                    put_comment(259);   /* msg 259: \n\tAdd Security Classes to Defined Objects\t\n  */
                    first = FALSE;
                }
                
                if (!modify) {
	           ib_fprintf(output_file, "\nmodify %s %s", p, r);
	           modify = TRUE;
                }
	
                if (!first_field)
                    ib_fprintf(output_file, ",");
                first_field = FALSE;
                ib_fprintf(output_file, "\n    modify field %s security_class %s", f,
                           s);
            }
            
        END_FOR;
        if (modify)
            ib_fprintf(output_file, ";\n");	
    END_FOR;
}


static void extract_triggers(void)
{
/**************************************
 *
 *	e x t r a c t _ t r i g g e r s
 *
 **************************************
 *
 * Functional description
 *	Get trigger definitions from
 *	rdb$triggers for each relation.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32], st[32];

	first = TRUE;

    FOR T IN RDB$TRIGGERS WITH
        (T.RDB$SYSTEM_FLAG NE 1 OR T.RDB$SYSTEM_FLAG MISSING)
        SORTED BY T.RDB$TRIGGER_NAME 
        if (first) {
            put_comment(260);       /* msg 260: \n\n\tTrigger Definitions\t */
            first = FALSE;
        } 
        name_trunc(T.RDB$TRIGGER_NAME, st);
        ib_fprintf(output_file, "\n\ndefine trigger %s", st);
        name_trunc(T.RDB$RELATION_NAME, s);
        ib_fprintf(output_file, " for %s\n", s);
        
        if (T.RDB$TRIGGER_INACTIVE)
            ib_fprintf(output_file, "\tinactive\n");
        
        switch (T.RDB$TRIGGER_TYPE) {
        case (SSHORT) trg_store:
            ib_fprintf(output_file, "\tpre store");
            break;
            
        case (SSHORT) trg_post_store:
            ib_fprintf(output_file, "\tpost store");
            break;
            
        case (SSHORT) trg_modify:
	       ib_fprintf(output_file, "\tpre modify");
	       break;
           
        case (SSHORT) trg_post_modify:
	       ib_fprintf(output_file, "\tpost modify");
	       break;
           
        case (SSHORT) trg_pre_erase:
            ib_fprintf(output_file, "\tpre erase");
            break;
            
        case (SSHORT) trg_erase:
            ib_fprintf(output_file, "\tpost erase");
            break;
	
        default:
            put_comment(261);       /* msg 261: ***** trigger type not understood **** */
	   }
        
        ib_fprintf(output_file, " %d:\n", T.RDB$TRIGGER_SEQUENCE);
        if (T.RDB$TRIGGER_SOURCE.NULL) {
            put_error(262, st);
            /* msg 262: **** trigger source for trigger %s must be recreated **** */
        }
        else {
            print_blob(&T.RDB$TRIGGER_SOURCE, 'u');
        }
        ib_fprintf(output_file, "\n\tend_trigger");
        if (!BLOB_NULL(T.RDB$DESCRIPTION)) {
            ib_fprintf(output_file, "\n\t{");
            print_blob(&T.RDB$DESCRIPTION, 'u');
            ib_fprintf(output_file, "}");
        }
        extract_trig_msgs(T.RDB$TRIGGER_NAME);
        
        ib_fprintf(output_file, ";");
    END_FOR;
}


static void extract_trig_msgs(TEXT * trigger_name)
{
/**************************************
 *
 *	e x t r a c t _ t r i g _ m s g s
 *
 **************************************
 *
 * Functional description
 *	Read the trigger messages as part of the trigger definition
 *	and write them to the output file.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32], *p, quote_char;

	first = TRUE;
	p = " ";

/* delimit message with single quote by default */

	quote_char = 39;

    FOR TM IN RDB$TRIGGER_MESSAGES
        WITH TM.RDB$TRIGGER_NAME = trigger_name
        SORTED BY TM.RDB$MESSAGE_NUMBER
        /* If message has a ' in it, delimit with " instead */
        if (strchr(TM.RDB$MESSAGE, 39) != NULL)
            quote_char = 34;
        ib_fprintf(output_file, "%s\n", p);
        if (first) {
            p = ",";
            first = FALSE;
        }
        ib_fprintf(output_file, "\tmessage ");
        ib_fprintf(output_file, " %d: ", TM.RDB$MESSAGE_NUMBER);
        ib_fprintf(output_file, " %c%s%c", quote_char, TM.RDB$MESSAGE,
	              quote_char);
    END_FOR;
} 


static void extract_view(TEXT * rel_name,
						 SLONG * source,
						 SLONG * description,
						 USHORT source_null,
                         USHORT desc_null, USHORT system_flag, BOOLEAN * first)
{
/**************************************
 *
 *	e x t r a c t _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Get definitions of a view and its fields
 *	and write it to the output file.
 *
 **************************************/
	TEXT s[32];

	if (*first) {
		*first = FALSE;
		put_comment(266);		/* msg 266: \n\n\tView Definitions\t */
	}

	name_trunc(rel_name, s);
	ib_fprintf(output_file, "\ndefine view %s of ", s);
	if (source_null) {
		put_error(267, s);		/* msg 267: **** view definition %s must be recreated **** */
		return;
	}

	print_blob(source, 'u');

	if (system_flag)
		ib_fprintf(output_file, "\n\tsystem_flag %d", system_flag);

	if (!desc_null) {
		ib_fprintf(output_file, "\t{");
		print_blob(description, 'u');
		ib_fprintf(output_file, "}");
	}

	view_fields(rel_name);
	ib_fprintf(output_file, ";\n");
}


static void extract_views(void)
{
/**************************************
 *
 *	e x t r a c t _ v i e w s
 *
 **************************************
 *
 * Functional description
 *	Get definitions of views and their fields
 *	and write it to the output file.
 *
 **************************************/
	BOOLEAN first;
	USHORT source_null, desc_null, did_any, do_this_one;
	VIEW view_list, view, *view_ptr;
	TEXT s[32], *view_name;

	first = TRUE;

	if (!(EXT_capabilities & EXT_v3)) {
		/* In V2 we have no choice but to output views in the order they appear */

        FOR V IN RDB$RELATIONS WITH
            (V.RDB$SYSTEM_FLAG NE 1 OR V.RDB$SYSTEM_FLAG MISSING) AND
            V.RDB$VIEW_BLR NOT MISSING
            SORTED BY V.RDB$RELATION_NAME
            source_null = V.RDB$VIEW_SOURCE.NULL ? TRUE : FALSE;
            desc_null = BLOB_NULL(V.RDB$DESCRIPTION) ? TRUE : FALSE;
	        extract_view(V.RDB$RELATION_NAME, &V.RDB$VIEW_SOURCE,
                         &V.RDB$DESCRIPTION, source_null, desc_null,
                         V.RDB$SYSTEM_FLAG, &first);
	
	    END_FOR;
		return;
	}

/* In V3 there are dependencies that can help us to output the views in
   the correct order.  But first let's just save the names of the views. */

	view_list = NULL;

    FOR V IN RDB$RELATIONS WITH
        (V.RDB$SYSTEM_FLAG NE 1 OR V.RDB$SYSTEM_FLAG MISSING) AND
        V.RDB$VIEW_BLR NOT MISSING
        SORTED BY DESC V.RDB$RELATION_NAME name_trunc(V.RDB$RELATION_NAME, s);
        view = (VIEW) DDL_alloc(sizeof(struct view) + strlen(s));
        strcpy(view->view_name, s);
        view->view_next = view_list;
        view_list = view;
        
	END_FOR;

	if (!view_list)
		return;

    /* For each view that was found, look for any views in the list that it is
   dependent on.  When a view is found that isn't dependent on any views
   still in the list, dump it out and remove it from the list.  Continue
   going through the list until nothing more can be done. */

	do {
		for (did_any = FALSE, view_ptr = &view_list; *view_ptr;) {
			do_this_one = TRUE;
			view_name = (*view_ptr)->view_name;
            
			/* fields of rdb$dependencies have been renamed for V4.0
			   This means if we are extracting a pre V3 definition, views
			   will not be spit out in the optimal order. */

			if (EXT_capabilities & EXT_v4) {
                FOR D IN RDB$DEPENDENCIES WITH
                    D.RDB$DEPENDENT_NAME EQ view_name AND
                    D.RDB$DEPENDENT_TYPE EQ 0 AND
                    D.RDB$DEPENDED_ON_TYPE EQ 0
                    name_trunc(D.RDB$DEPENDED_ON_NAME, s);
                    for (view = view_list; view; view = view->view_next)
                        if (!strcmp(s, view->view_name)) {
                            do_this_one = FALSE;
                            break;
                        }
                END_FOR;
			}
			if (do_this_one) {
                FOR V IN RDB$RELATIONS WITH
                    V.RDB$RELATION_NAME EQ view_name
                    source_null = V.RDB$VIEW_SOURCE.NULL ? TRUE : FALSE;
                    desc_null = BLOB_NULL(V.RDB$DESCRIPTION) ? TRUE : FALSE;
                    extract_view(V.RDB$RELATION_NAME, &V.RDB$VIEW_SOURCE,
                                 &V.RDB$DESCRIPTION, source_null, desc_null,
                                 V.RDB$SYSTEM_FLAG, &first);
	
	            END_FOR;
				did_any = TRUE;
				*view_ptr = (*view_ptr)->view_next;
			}
			else
				view_ptr = &(*view_ptr)->view_next;
		}
	} while (did_any);
    
    /* If any views remain, we can't figure out an order so just dump them */
    
	for (view = view_list; view; view = view->view_next) {
        FOR V IN RDB$RELATIONS WITH
            V.RDB$RELATION_NAME EQ view->view_name
            source_null = V.RDB$VIEW_SOURCE.NULL ? TRUE : FALSE;
            desc_null = BLOB_NULL(V.RDB$DESCRIPTION) ? TRUE : FALSE;
	         extract_view(V.RDB$RELATION_NAME, &V.RDB$VIEW_SOURCE,
                          &V.RDB$DESCRIPTION, source_null, desc_null,
                          V.RDB$SYSTEM_FLAG, &first);
	
        END_FOR;
	}
}


static void field_attributes(SLONG * descr,
							 TEXT * query_name,
							 SLONG * query_header,
							 TEXT * edit_string, SSHORT system_flag)
{
/**************************************
 *
 *	f i e l d _ a t t r i b u t e s
 *
 **************************************
 *
 * Functional description
 *	Print miscellaneous field attributes.
 *
 **************************************/
	TEXT s[256];

	if (descr && (descr[0] || descr[1])) {
		ib_fprintf(output_file, "\n\t{");
		print_blob(descr, 'u');
		ib_fprintf(output_file, "}");
	}

	if (system_flag)
		ib_fprintf(output_file, "\n\tsystem_flag %d", system_flag);

	if (query_name && name_trunc(query_name, s))
		ib_fprintf(output_file, "\n\tquery_name %s", s);

	if (query_header && (query_header[0] || query_header[1])) {
		ib_fprintf(output_file, "\n\tquery_header ");
		print_blob(query_header, 'h');
	}

	if (edit_string && name_trunc(edit_string, s))
		if (strchr(s, '"'))
			ib_fprintf(output_file, "\n\tedit_string \'%s\'", s);
		else
			ib_fprintf(output_file, "\n\tedit_string \"%s\"", s);
}


static void format_acl(SCHAR * string)
{
/**************************************
 *
 *	f o r m a t _ a c l
 *
 **************************************
 *
 * Functional description
 * 	Format an access control list.
 *
 **************************************/
	SCHAR *p, c;
	TEXT view[32], group[32], user[32], person[41],
		project[41], organization[41], node[41], temp[40];
	BOOLEAN first;
	int l;

	p = string;
	first = TRUE;

	if (*p++ != ACL_version)
		put_comment(268);		/* msg 268: ***** ACL not understood ***** */
	else
		ib_fprintf(output_file, "\n");

	while (*p)
		switch (*p++) {
		case ACL_id_list:
			*view = *user = *group = FALSE;
			*person = *project = *organization = *node = FALSE;
			if (!first)
				ib_fprintf(output_file, ",\n");
			else
				first = FALSE;

			ib_fputc('\t', output_file);
			while (c = *p++) {
				switch (c) {
				case id_view:
					l = *p++;
					strncpy(view, p, l);
					view[l] = 0;
					break;

				case id_group:
					l = *p++;
					strncpy(group, p, l);
					group[l] = 0;
					break;

				case id_user:
					l = *p++;
					strncpy(user, p, l);
					user[l] = 0;
					break;

				case id_person:
					l = *p++;
					strncpy(person, p, l);
					person[l] = 0;
					break;

				case id_project:
					l = *p++;
					strncpy(temp, p, l);
					temp[l] = 0;
					sprintf(project, ".%s", temp);
					break;

				case id_organization:
					l = *p++;
					strncpy(temp, p, l);
					temp[l] = 0;
					sprintf(organization, ".%s", temp);
					break;

				case id_node:
					l = *p++;
					strncpy(temp, p, l);
					temp[l] = 0;
					sprintf(node, ".%s", temp);
					break;

				}
				p += l;
			}

/* 
   Here we try to decompose the identifier.  Views are easy, they're just views.
   If its not a view, it may be a user/group pair or just the user or just the group.
   Those are operating system dependant.  Otherwise, its an Apollo identifier.
*/

			if (*view)
				ib_fprintf(output_file, "view %s ", view);
			else
				ib_fprintf(output_file, "<%s,%s> ",
						   (*group) ? group : (TEXT *) "*",
						   (*user) ? user : (TEXT *) "*");
			break;

		case ACL_priv_list:
			while (c = *p++)
				ib_fputc(acl_privs[c], output_file);
			break;
		}
}


static int name_trunc(TEXT * in, TEXT * out)
{
/**************************************
 *
 *	n a m e _ t r u n c
 *
 **************************************
 *
 * Functional description
 * 	Copy a name (or other single word)
 *	to the output string, elmininating
 *	trailing blanks.
 *
 **************************************/
	TEXT *start;

	if (!in || *in == ' ')
		return NULL;

	start = out;

	while (*in && *in != ' ')
		*out++ = *in++;

	*out = 0;

	return *start;
}


static void print_blob(SLONG * blob_id, TEXT type)
{
/**************************************
 *
 *	p r i n t _ b l o b
 *
 **************************************
 *
 * Functional description
 *	Read the contents of an blob
 *	and write them to the output file.
 *	Blobs are characterized as "u" for
 *	unspecified, "m" for missing values
 *	to decode, "h" meaning query
 *	headers that need formatting and "a"
 *	for acls. 
 *
 **************************************/
	BOOLEAN first;
	TEXT buffer[4096];
	SCHAR blr_buffer[2048], *p, *q;
	SSHORT length;
	STATUS status_vector[20], status;
	SLONG *blob;
	UCHAR bpb_buffer[20], *bpb, *r;

	blob = NULL;
	bpb = r = bpb_buffer;

#if (defined JPN_EUC || defined JPN_SJIS)
	if ((type == 'u') || (type == 'h')) {
		/* create bpb for description blob and query header blob */

		*r++ = gds__bpb_version1;
		*r++ = gds__bpb_target_interp;
		*r++ = 2;
		*r++ = DDL_interp;
		*r++ = DDL_interp >> 8;
	}
#endif /* (defined JPN_EUC || defined JPN_SJIS) */

	if (gds__open_blob2(status_vector,
						GDS_REF(DB),
						GDS_REF(gds__trans),
						GDS_REF(blob),
						GDS_VAL(blob_id), (USHORT) (r - bpb), bpb)) {
		gds__print_status(status_vector);
		DDL_err(269, NULL, NULL, NULL, NULL, NULL);	/* msg 269: gds__open_blob failed */
		return;
	}

	first = TRUE;
	if (type == 'm' || type == 'a') {
		p = blr_buffer;
		*blr_buffer = 0;
	}

	while (!(status = gds__get_segment(status_vector,
									   GDS_REF(blob),
									   GDS_REF(length),
									   (USHORT) (sizeof(buffer) - 1),
									   buffer)) || status == gds__segment) {
		buffer[length] = 0;
		switch (type) {
		case 'h':
			if (q = (SCHAR *) strchr(buffer, '\n')) {
				p = buffer;
				do {
					*q = 0;
					ib_fprintf(output_file, "\"%s\" / ", p);
					p = q + 1;
				} while (q = (SCHAR *) strchr(p, '\n'));
				ib_fprintf(output_file, "\"%s\"", p);
				first = FALSE;
				break;
			}
			if (!first)
				ib_fprintf(output_file, " / ");
			first = FALSE;
			ib_fprintf(output_file, "\"%s\"", buffer);
			break;

		case 'a':
		case 'm':
			q = (SCHAR *) buffer;
			while (length--)
				*p++ = *q++;
			break;

		case 'u':
			for (p = (SCHAR *) buffer; *p; p++)
				ib_putc(*p, output_file);
			break;

		default:
			put_comment(270);	/* msg 270: ***** blob option not understood **** */
		}
	}

	if (status_vector[1] != gds__segstr_eof) {
		gds__print_status(status_vector);
		DDL_err(271, NULL, NULL, NULL, NULL, NULL);	/* msg 271: gds__get_segment failed  */
		return;
	}

	if (gds__close_blob(status_vector, GDS_REF(blob))) {
		gds__print_status(status_vector);
		DDL_err(272, NULL, NULL, NULL, NULL, NULL);	/* msg 272: gds__close_blob failed */
		return;
	}

	if (type == 'm')
		decompile_blr_literal(blr_buffer);

	if (type == 'a')
		format_acl(blr_buffer);
}


static void put_comment(USHORT number)
{
/**************************************
 *
 *	p u t _ c o m m e n t
 *
 **************************************
 *
 * Functional description
 *	Retrieve a message from the error file, 
 *	format it, and print it in the output file
 *	but not on the screen
 *
 **************************************/
	TEXT message[256];

	gds__msg_format(NULL_PTR, DDL_MSG_FAC, number, sizeof(message), message,
					NULL, NULL, NULL, NULL, NULL);
	ib_fprintf(output_file, "%s\n\n", message);
}


static void put_error(USHORT number, TEXT * arg1)
{
/**************************************
 *
 *	p u t _ e r r o r
 *
 **************************************
 *
 * Functional description
 *	Retrieve a message from the error file, 
 *	format it, and print it in the output file
 *	and on the screen
 *
 **************************************/
	TEXT message[256];

	gds__msg_format(NULL_PTR, DDL_MSG_FAC, number, sizeof(message), message,
					arg1, NULL, NULL, NULL, NULL);
	ib_fprintf(output_file, "%s\n\n", message);
	ib_printf("%s\n", message);
}


static void set_capabilities(void)
{
/**************************************
 *
 *	s e t _ c a p a b i l i t i e s
 *
 **************************************
 *
 * Functional description
 *	set the capabilities bits for the
 *	database being extracted to avoid
 *	unpleasantness later.
 *
 **************************************/
	ULONG *req;
	STATUS status_vector[20];
	RFR_TAB rel_field_table;

	req = NULL;

/* Look for desireable fields in system relations */

	for (rel_field_table = rfr_table; rel_field_table->relation;
		 rel_field_table++) {
	       FOR(REQUEST_HANDLE req) x IN RDB$RELATION_FIELDS WITH x.
	           RDB$RELATION_NAME =
	           rel_field_table->relation AND x.RDB$FIELD_NAME =
	           rel_field_table->field EXT_capabilities |=
	           rel_field_table->bit_mask;
	       END_FOR;
	}

	gds__release_request(gds__status, GDS_REF(req));
}


static void type_scalar(USHORT dtype,
						SSHORT scale,
						USHORT length,
						SSHORT subtype, USHORT dim, TEXT * field_name)
{
/**************************************
 *
 *	t y p e _ s c a l a r
 *
 **************************************
 *
 * Functional description
 *	Finish out the kernel field definition of a
 *	non-blob type field.
 *
 **************************************/
	TEXT c;

	switch (dtype) {
	case blr_cstring:
		ib_fprintf(output_file, " cstring [%d]", length);
		break;

	case blr_text:
		ib_fprintf(output_file, " char [%d]", length);
		break;

	case blr_varying:
		ib_fprintf(output_file, " varying [%d]", length);
		break;

	case blr_short:
		ib_fprintf(output_file, " short");
		break;

	case blr_long:
		ib_fprintf(output_file, " long");
		break;

	case blr_quad:
		ib_fprintf(output_file, " quad");
		break;

	case blr_timestamp:
		ib_fprintf(output_file, " date");
		break;

	case blr_sql_date:
		ib_fprintf(output_file, " date sub_type sql_date");
		break;

	case blr_sql_time:
		ib_fprintf(output_file, " date sub_type sql_time");
		break;

	case blr_float:
		ib_fprintf(output_file, " float");
		break;

	case blr_double:
		ib_fprintf(output_file, " double");
		break;

	case blr_int64:
		ib_fprintf(output_file, " integer64");
		break;

	default:
		ib_fprintf(output_file, " UNKNOWN");
		break;
	}

	if (dim) {
		c = '(';
        FOR D IN RDB$FIELD_DIMENSIONS WITH 
            D.RDB$FIELD_NAME EQ field_name 
            SORTED BY D.RDB$DIMENSION 
            if (D.RDB$LOWER_BOUND  == 1)
                ib_fprintf(output_file, "%c%d", c, D.RDB$UPPER_BOUND);
            else
                ib_fprintf(output_file, "%c%d:%d", c, D.RDB$LOWER_BOUND,
                           D.RDB$UPPER_BOUND);
            c = ',';
	    END_FOR;
        ib_fprintf(output_file, ")");
	}

	if (scale)
#if (defined JPN_SJIS || defined JPN_EUC)

		if (dtype != blr_text && dtype != blr_cstring && dtype != blr_varying)
#endif
			ib_fprintf(output_file, " scale %d", scale);

	if (subtype
		&& (dtype == blr_text || dtype == blr_varying
			|| dtype == blr_cstring)) if (subtype == 1)
			ib_fprintf(output_file, " sub_type fixed");
		else
			ib_fprintf(output_file, " sub_type %d", subtype);
}


static void view_fields(TEXT * view_name)
{
/**************************************
 *
 *	v i e w _ f i e l d s
 *
 **************************************
 *
 * Functional description
 *	Write out the view fields, including
 *	such oddities as context variables.
 *
 **************************************/
	BOOLEAN first;
	TEXT s[32], context[32], temp[32];

	first = TRUE;
	context[0] = 0;

    FOR RFR IN RDB$RELATION_FIELDS CROSS
        VR IN RDB$VIEW_RELATIONS
        WITH RFR.RDB$RELATION_NAME = view_name
        AND RFR.RDB$VIEW_CONTEXT != 0 AND
        VR.RDB$VIEW_NAME = RFR.RDB$RELATION_NAME AND
        VR.RDB$VIEW_CONTEXT = RFR.RDB$VIEW_CONTEXT
        SORTED BY RFR.RDB$FIELD_POSITION 
        if (!first)
            ib_fprintf(output_file, ",\n");
        else
            first = FALSE;
        name_trunc(RFR.RDB$FIELD_NAME, s);
        if (EXT_capabilities & EXT_context_name) {
            FOR VR2 IN RDB$VIEW_RELATIONS WITH
                VR2.RDB$VIEW_NAME = VR.RDB$VIEW_NAME AND
                VR2.RDB$VIEW_CONTEXT = VR.RDB$VIEW_CONTEXT
                name_trunc(VR2.RDB$CONTEXT_NAME, context);
            END_FOR;
            if (!(strcmp(RFR.RDB$FIELD_NAME, RFR.RDB$BASE_FIELD)))
                ib_fprintf(output_file, "    %s.%s", context, s);
            else {
                name_trunc(RFR.RDB$BASE_FIELD, temp);
                ib_fprintf(output_file, "    %s FROM %s.%s", s, context, temp);
            }
            if (!RFR.RDB$FIELD_POSITION.NULL)
                ib_fprintf(output_file, "\tposition %d", RFR.RDB$FIELD_POSITION);
            
            field_attributes(&RFR.RDB$DESCRIPTION, RFR.RDB$QUERY_NAME,
                             &RFR.RDB$QUERY_HEADER, RFR.RDB$EDIT_STRING,
                             RFR.RDB$SYSTEM_FLAG);
        }            
	END_FOR;

	extract_computed(view_name, first);
}


static void wal_info(UCHAR * db_info_buffer,
					 SSHORT * num_buf,
					 SLONG * buf_size, SLONG * chkpt_len, SLONG * grp_commit)
{
/**************************************
 *
 *	w a l _ i n f o
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	UCHAR item;
	UCHAR *d, *p;
	SLONG length;

	p = db_info_buffer;

	while ((item = *p++) != gds__info_end) {
		length = gds__vax_integer(p, 2);
		p += 2;
		switch (item) {
		case gds__info_num_wal_buffers:
			*num_buf = gds__vax_integer(p, length);
			break;

		case gds__info_wal_buffer_size:
			*buf_size = gds__vax_integer(p, length);
			break;

		case gds__info_wal_ckpt_length:
			*chkpt_len = gds__vax_integer(p, length);
			break;

		case gds__info_wal_grpc_wait_usecs:
			*grp_commit = gds__vax_integer(p, length);
			break;

		default:
			;
		}
		p += length;
	}
}

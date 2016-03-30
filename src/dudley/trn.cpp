/*
 *	PROGRAM:	JRD Data Definition Utility
 *	MODULE:		trn.cpp
 *	DESCRIPTION:	Translate meta-data change to dynamic meta data
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

#include "firebird.h"
#include <stdio.h>
#include <string.h>
#include "../jrd/ibase.h"
#include "../dudley/ddl.h"
//#include "../jrd/license.h"
#include "../dudley/gener_proto.h"
#include "../dudley/lex_proto.h"
#include "../dudley/trn_proto.h"
#include "../gpre/prett_proto.h"
#include "../jrd/gds_proto.h"
#include "../jrd/gdsassert.h"

using MsgFormat::SafeArg;


static void add_dimensions(STR, DUDLEY_FLD);
static void add_field(STR, DUDLEY_FLD, DUDLEY_REL);
static void add_files(STR, FIL, DBB);
static void add_filter(STR, FILTER);
static void add_function(STR, FUNC);
static void add_function_arg(STR, FUNCARG);
static void add_generator(STR, SYM);
static void add_global_field(STR, DUDLEY_FLD);
static void add_index(STR, DUDLEY_IDX);
static void add_relation(STR, DUDLEY_REL);
static void add_security_class(STR, SCL);
static void add_trigger(STR, DUDLEY_TRG);
static void add_trigger_msg(STR, TRGMSG);
static void add_view(STR, DUDLEY_REL);
static void drop_field(STR, DUDLEY_FLD);
static void drop_filter(STR, FILTER);
static void drop_function(STR, FUNC);
static void drop_global_field(STR, DUDLEY_FLD);
static void drop_index(STR, DUDLEY_IDX);
static void drop_relation(STR, DUDLEY_REL);
static void drop_security_class(STR, SCL);
static void drop_shadow(STR, SLONG);
static void drop_trigger(STR, DUDLEY_TRG);
static void drop_trigger_msg(STR, TRGMSG);
static void gen_dyn_c(void*, SSHORT, const char*);
static void gen_dyn_cxx(void*, SSHORT, const char*);
static void gen_dyn_pas(void*, SSHORT, const char*);
static void modify_database(STR, DBB);
static void modify_field(STR, DUDLEY_FLD, DUDLEY_REL);
static void modify_global_field(STR, DUDLEY_FLD);
static void modify_index(STR, DUDLEY_IDX);
static void modify_relation(STR, DUDLEY_REL);
static void modify_trigger(STR, DUDLEY_TRG);
static void modify_trigger_msg(STR, TRGMSG);
static void put_acl(STR, UCHAR, SCL);
static void put_blr(STR, UCHAR, DUDLEY_REL, DUDLEY_NOD);
static void put_number(STR, TEXT, SSHORT);
static void put_query_header(STR, TEXT, DUDLEY_NOD);
static void put_symbol(STR, TEXT, SYM);
static void put_text(STR, UCHAR, TXT);
static void raw_ada(STR);
static void raw_cobol(STR);
static void raw_ftn(STR);

static FILE* output_file;

static inline void check_dyn(str* dyn, const int l)
{
	if (dyn->str_current - dyn->str_start + l > dyn->str_length && !TRN_get_buffer(dyn, l))
	{
		DDL_error_abort( NULL, 320);
	}
}

// CVC: These two overloaded functions made to solve ugly warnings after Blas
//  dropped gdsold.cpp and the typed isc_dyn_ constants disappeared.
static inline void put_symbol(STR dyn, int attribute, SYM symbol)
{
	put_symbol(dyn, static_cast<TEXT>(attribute), symbol);
}

static inline void put_number( STR dyn, int attribute, SSHORT number)
{
	put_number(dyn, static_cast<TEXT>(attribute), number);
}


static const char* const FOPEN_WRITE_TYPE	= "w";


void TRN_translate()
{
/**************************************
 *
 *	T R N _ t r a n s l a t e
 *
 **************************************
 *
 * Functional description
 *	Translate from internal data structures into dynamic DDL.
 *
 **************************************/

/* Start by reversing the set of actions */
	str d;
	str* dyn = &d;
	dyn->str_current = dyn->str_start = static_cast<UCHAR*>(gds__alloc(8192));
	if (!dyn->str_current)
		DDL_error_abort(NULL, 14);	/* msg 14: memory exhausted */
	dyn->str_length = 8192;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
	gds_alloc_flag_unfreed(dyn->str_start);
#endif

	if (!dudleyGlob.DYN_file_name[0])
		output_file = stdout;
	else if (!(output_file = fopen(dudleyGlob.DYN_file_name, FOPEN_WRITE_TYPE))) {
		DDL_msg_put(281, SafeArg() << dudleyGlob.DYN_file_name);
		/* msg 281: gdef: can't open DYN output file: %s */
		DDL_exit(FINI_ERROR);
	}

	check_dyn(dyn, 2);
	dyn->add_byte(isc_dyn_version_1);
	dyn->add_byte(isc_dyn_begin);

	for (ACT action = dudleyGlob.DDL_actions; action; action = action->act_next)
		if (!(action->act_flags & ACT_ignore))
			switch (action->act_type) {
			case act_c_database:
			case act_m_database:
				modify_database(dyn, action->act_object);
				break;

			case act_a_relation:
				add_relation(dyn, (DUDLEY_REL) action->act_object);
				break;

			case act_m_relation:
				modify_relation(dyn, (DUDLEY_REL) action->act_object);
				break;

			case act_d_relation:
				drop_relation(dyn, (DUDLEY_REL) action->act_object);
				break;

			case act_a_field:
				add_field(dyn, (DUDLEY_FLD) action->act_object, NULL);
				break;

			case act_m_field:
				modify_field(dyn, (DUDLEY_FLD) action->act_object, NULL);
				break;

			case act_d_field:
				drop_field(dyn, (DUDLEY_FLD) action->act_object);
				break;

			case act_a_filter:
				add_filter(dyn, (FILTER) action->act_object);
				break;

			case act_d_filter:
				drop_filter(dyn, (FILTER) action->act_object);
				break;

			case act_a_function:
				add_function(dyn, (FUNC) action->act_object);
				break;

			case act_a_function_arg:
				add_function_arg(dyn, (FUNCARG) action->act_object);
				break;

			case act_d_function:
				drop_function(dyn, (FUNC) action->act_object);
				break;

			case act_a_generator:
				add_generator(dyn, (SYM) action->act_object);
				break;

			case act_a_gfield:
				add_global_field(dyn, (DUDLEY_FLD) action->act_object);
				break;

			case act_m_gfield:
				modify_global_field(dyn, (DUDLEY_FLD) action->act_object);
				break;

			case act_d_gfield:
				drop_global_field(dyn, (DUDLEY_FLD) action->act_object);
				break;

			case act_a_index:
				add_index(dyn, (DUDLEY_IDX)action->act_object);
				break;

			case act_m_index:
				modify_index(dyn, (DUDLEY_IDX)action->act_object);
				break;

			case act_d_index:
				drop_index(dyn, (DUDLEY_IDX)action->act_object);
				break;

			case act_a_security:
				add_security_class(dyn, (SCL) action->act_object);
				break;

			case act_d_security:
				drop_security_class(dyn, (SCL) action->act_object);
				break;

			case act_a_trigger:
				add_trigger(dyn, (DUDLEY_TRG) action->act_object);
				break;

			case act_d_trigger:
				drop_trigger(dyn, (DUDLEY_TRG) action->act_object);
				break;

			case act_m_trigger:
				modify_trigger(dyn, (DUDLEY_TRG) action->act_object);
				break;

			case act_a_trigger_msg:
				add_trigger_msg(dyn, (TRGMSG) action->act_object);
				break;

			case act_d_trigger_msg:
				drop_trigger_msg(dyn, (TRGMSG) action->act_object);
				break;

			case act_a_shadow:
				add_files(dyn, (FIL) action->act_object, NULL);
				break;

			case act_d_shadow:
				drop_shadow(dyn, (IPTR) (action->act_object));
				break;

			case act_m_trigger_msg:
				modify_trigger_msg(dyn, (TRGMSG) action->act_object);
				break;

			default:
				DDL_err(282);	/* msg 282: action not implemented yet */
			}

	check_dyn(dyn, 2);
	dyn->add_byte(isc_dyn_end);
	dyn->add_byte(isc_dyn_eoc);

	USHORT length;

	switch (dudleyGlob.language) {
	case lan_undef:
	case lan_c:
		if (PRETTY_print_dyn(dyn->str_start, gen_dyn_c, 0, 0))
			DDL_err(283);	/*msg 283: internal error during DYN pretty print */
		break;

	case lan_cxx:
		if (PRETTY_print_dyn(dyn->str_start, gen_dyn_cxx, 0, 0))
			DDL_err(283);	/*msg 283: internal error during DYN pretty print */
		break;

	case lan_pascal:
		length = dyn->str_current - dyn->str_start;
		fprintf(output_file, "   isc_dyn_length	: gds__short := %d;\n",
				   length);
		fprintf(output_file,
				   "   isc_dyn	: packed array [1..%d] of char := (\n",
				   length);
		if (PRETTY_print_dyn(dyn->str_start, gen_dyn_pas, 0, 1))
			DDL_err(285);	/*msg 285: internal error during DYN pretty print */
		fprintf(output_file, "   );	(* end of DYN string *)\n");
		break;

	case lan_fortran:
		length = dyn->str_current - dyn->str_start;
		fprintf(output_file, "       INTEGER*2 GDS__DYN_LENGTH\n");
		fprintf(output_file, "       INTEGER*4 GDS__DYN(%d)\n",
				   (int) ((length + 3) / 4));
		fprintf(output_file, "       DATA GDS__DYN_LENGTH  /%d/\n",
				   length);
		fprintf(output_file, "       DATA (GDS__DYN(I), I=1,%d)  /\n",
				   (int) ((length + 3) / 4));
		raw_ftn(dyn);
		break;
	case lan_ada:
		length = dyn->str_current - dyn->str_start;
		fprintf(output_file,
				   "isc_dyn_length: short_integer := %d;\n", length);
		fprintf(output_file,
				   "isc_dyn	: CONSTANT firebird.blr (1..%d) := (\n", length);
		raw_ada(dyn);
		break;

	case lan_ansi_cobol:
		length = dyn->str_current - dyn->str_start;
		fprintf(output_file,
				   "       01  GDS-DYN-LENGTH PIC S9(4) USAGE COMP VALUE IS %d.\n",
				   length);
		fprintf(output_file, "       01  GDS-DYN.\n");
		raw_cobol(dyn);
		break;
	case lan_cobol:
		length = dyn->str_current - dyn->str_start;
		fprintf(output_file,
				   "       01  GDS__DYN_LENGTH PIC S9(4) USAGE COMP VALUE IS %d.\n",
				   length);
		fprintf(output_file, "       01  GDS__DYN.\n");
		raw_cobol(dyn);
		break;
	}

	if (output_file != stdout)
		fclose(output_file);

	gds__free(dyn->str_start);
}


static void add_dimensions( STR dyn, DUDLEY_FLD field)
{
/**************************************
 *
 *	a d d _ d i m e n s i o n s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create dimensions.
 *	First get rid of any old ones.
 *
 **************************************/
	put_symbol(dyn, isc_dyn_delete_dimensions, field->fld_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);

	USHORT n = 0;
	for (const SLONG* range = field->fld_ranges; n < field->fld_dimension;
		 range += 2, ++n)
	{
		put_number(dyn, isc_dyn_def_dimension, n);
		put_symbol(dyn, isc_dyn_fld_name, field->fld_name);
		put_number(dyn, isc_dyn_dim_lower, (SSHORT) range[0]);
		put_number(dyn, isc_dyn_dim_upper, (SSHORT) range[1]);
		check_dyn(dyn, 1);
		dyn->add_byte(isc_dyn_end);
	}
}


static void add_field( STR dyn, DUDLEY_FLD field, DUDLEY_REL view)
{
/**************************************
 *
 *	a d d _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a
 *	local field.
 *
 **************************************/
	DUDLEY_REL relation;
	DUDLEY_FLD source_field;
	int n;

	SYM name = field->fld_name;
	if (view)
		relation = view;
	else
		relation = field->fld_relation;

	put_symbol(dyn, isc_dyn_def_local_fld, name);
	put_symbol(dyn, isc_dyn_rel_name, relation->rel_name);

	SYM symbol = field->fld_source;
	if (symbol &&
		strcmp(symbol->sym_string, name->sym_string) && !field->fld_computed)
	{
		put_symbol(dyn, isc_dyn_fld_source, symbol);
	}

	put_symbol(dyn, isc_dyn_security_class, field->fld_security_class);
	put_symbol(dyn, isc_dyn_fld_edit_string, field->fld_edit_string);
	put_symbol(dyn, isc_dyn_fld_query_name, field->fld_query_name);
	put_query_header(dyn, isc_dyn_fld_query_header, field->fld_query_header);

	if (field->fld_system)
		put_number(dyn, isc_dyn_system_flag, field->fld_system);

	put_symbol(dyn, isc_dyn_fld_base_fld, field->fld_base);

	if (field->fld_context)
		put_number(dyn, isc_dyn_view_context,
				   field->fld_context->ctx_context_id);

	if (field->fld_computed) {
		if (!field->fld_context && view)
			put_number(dyn, isc_dyn_view_context, 0);
		put_blr(dyn, isc_dyn_fld_computed_blr, NULL, field->fld_computed);
		source_field = field->fld_source_field;
		put_number(dyn, isc_dyn_fld_type, source_field->fld_dtype);
		put_number(dyn, isc_dyn_fld_length, source_field->fld_length);
		put_number(dyn, isc_dyn_fld_scale, source_field->fld_scale);
		put_number(dyn, isc_dyn_fld_sub_type, source_field->fld_sub_type);
		if (n = source_field->fld_segment_length)
			put_number(dyn, isc_dyn_fld_segment_length, n);
	}

	put_text(dyn, isc_dyn_fld_computed_source, field->fld_compute_src);

	if (field->fld_flags & fld_explicit_position)
		put_number(dyn, isc_dyn_fld_position, field->fld_position);

	put_text(dyn, isc_dyn_description, field->fld_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_files( STR dyn, FIL files, DBB databaseL)
{
/**************************************
 *
 *	a d d _ f i l e s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add database
 *	or shadow files.
 *
 **************************************/
	FIL file, next;

/* Reverse the order of files (parser left them backwards) */

	for (file = files, files = NULL; file; file = next) {
		next = file->fil_next;
		file->fil_next = files;
		files = file;
	}

	if (!databaseL && files)
		put_number(dyn, isc_dyn_def_shadow, files->fil_shadow_number);

	for (file = files; file; file = file->fil_next) {
		put_symbol(dyn, isc_dyn_def_file, file->fil_name);
		put_number(dyn, isc_dyn_file_start, (SSHORT) (file->fil_start));
		put_number(dyn, isc_dyn_file_length, (SSHORT) (file->fil_length));
		put_number(dyn, isc_dyn_shadow_man_auto, file->fil_manual);
		put_number(dyn, isc_dyn_shadow_conditional, file->fil_conditional);
		check_dyn(dyn, 1);
		dyn->add_byte(isc_dyn_end);
	}
	if (!databaseL && files) {
		check_dyn(dyn, 1);
		dyn->add_byte(isc_dyn_end);
	}
}


static void add_filter( STR dyn, FILTER filter)
{
/**************************************
 *
 *	a d d _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a
 *	blob filter.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_def_filter, filter->filter_name);
	put_number(dyn, isc_dyn_filter_in_subtype,
			   filter->filter_input_sub_type);
	put_number(dyn, isc_dyn_filter_out_subtype,
			   filter->filter_output_sub_type);
	put_symbol(dyn, isc_dyn_func_entry_point, filter->filter_entry_point);
	put_symbol(dyn, isc_dyn_func_module_name, filter->filter_module_name);
	put_text(dyn, isc_dyn_description, filter->filter_description);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_function( STR dyn, FUNC function)
{
/**************************************
 *
 *	a d d _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a
 *	user define function.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_def_function, function->func_name);
	put_symbol(dyn, isc_dyn_fld_query_name, function->func_query_name);
	put_symbol(dyn, isc_dyn_func_entry_point, function->func_entry_point);
	put_number(dyn, isc_dyn_func_return_argument, function->func_return_arg);
	put_symbol(dyn, isc_dyn_func_module_name, function->func_module_name);
	put_text(dyn, isc_dyn_description, function->func_description);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_function_arg( STR dyn, FUNCARG func_arg)
{
/**************************************
 *
 *	a d d _ f u n c t i o n _ a r g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a
 *	user defined function.
 *
 **************************************/

	put_number(dyn, isc_dyn_def_function_arg, func_arg->funcarg_position);
	put_symbol(dyn, isc_dyn_function_name, func_arg->funcarg_funcname);
	put_number(dyn, isc_dyn_func_mechanism, func_arg->funcarg_mechanism);
	put_number(dyn, isc_dyn_fld_type, func_arg->funcarg_dtype);
	put_number(dyn, isc_dyn_fld_scale, func_arg->funcarg_scale);
	if (func_arg->funcarg_has_sub_type)
		put_number(dyn, isc_dyn_fld_sub_type, func_arg->funcarg_sub_type);
	put_number(dyn, isc_dyn_fld_length, func_arg->funcarg_length);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_generator( STR dyn, SYM symbol)
{
/**************************************
 *
 *	a d d _ g e n e r a t o r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a
 *	generator.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_def_generator, symbol);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_global_field( STR dyn, DUDLEY_FLD field)
{
/**************************************
 *
 *	a d d _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a relation.
 *
 **************************************/
	if (field->fld_computed)
		return;

	SYM name = field->fld_name;

	put_symbol(dyn, isc_dyn_def_global_fld, name);
	put_symbol(dyn, isc_dyn_fld_edit_string, field->fld_edit_string);
	put_symbol(dyn, isc_dyn_fld_query_name, field->fld_query_name);
	put_query_header(dyn, isc_dyn_fld_query_header, field->fld_query_header);

	if (field->fld_system)
		put_number(dyn, isc_dyn_system_flag, field->fld_system);

	put_number(dyn, isc_dyn_fld_type, field->fld_dtype);
	put_number(dyn, isc_dyn_fld_length, field->fld_length);
	put_number(dyn, isc_dyn_fld_scale, field->fld_scale);
	put_number(dyn, isc_dyn_fld_sub_type, field->fld_sub_type);

	const int n = field->fld_segment_length;
	if (n)
		put_number(dyn, isc_dyn_fld_segment_length, n);

	put_blr(dyn, isc_dyn_fld_missing_value, NULL, field->fld_missing);

	put_blr(dyn, isc_dyn_fld_validation_blr, NULL, field->fld_validation);
	put_text(dyn, isc_dyn_fld_validation_source, field->fld_valid_src);
	put_text(dyn, isc_dyn_description, field->fld_description);

	if (field->fld_dimension) {
		put_number(dyn, isc_dyn_fld_dimensions, field->fld_dimension);
		add_dimensions(dyn, field);
	}

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_index( STR dyn, DUDLEY_IDX index)
{
/**************************************
 *
 *	a d d _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create an index.
 *
 **************************************/
	put_symbol(dyn, isc_dyn_def_idx, index->idx_name);
	put_symbol(dyn, isc_dyn_rel_name, index->idx_relation);
	put_number(dyn, isc_dyn_idx_unique, (index->idx_unique) ? TRUE : FALSE);
	put_number(dyn, isc_dyn_idx_inactive, (index->idx_inactive) ? TRUE : FALSE);
	if (index->idx_type)
		put_number(dyn, isc_dyn_idx_type, index->idx_type);

	put_text(dyn, isc_dyn_description, index->idx_description);

	for (int i = 0; i < index->idx_count; i++)
		put_symbol(dyn, isc_dyn_fld_name, index->idx_field[i]);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_relation( STR dyn, DUDLEY_REL relation)
{
/**************************************
 *
 *	a d d _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a relation.
 *
 **************************************/

	if (relation->rel_rse) {
		add_view(dyn, relation);
		return;
	}

	put_symbol(dyn, isc_dyn_def_rel, relation->rel_name);
	put_symbol(dyn, isc_dyn_security_class, relation->rel_security_class);
	put_symbol(dyn, isc_dyn_rel_ext_file, relation->rel_filename);
	if (relation->rel_system)
		put_number(dyn, isc_dyn_system_flag, relation->rel_system);
	put_text(dyn, isc_dyn_description, relation->rel_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_security_class( STR dyn, SCL sec_class)
{
/**************************************
 *
 *	a d d _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add a security class.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_def_security_class, sec_class->scl_name);

	put_acl(dyn, isc_dyn_scl_acl, sec_class);

	put_text(dyn, isc_dyn_description, sec_class->scl_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_trigger( STR dyn, DUDLEY_TRG trigger)
{
/**************************************
 *
 *	a d d _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add a trigger for a relation.
 *
 **************************************/
	DUDLEY_REL relation = trigger->trg_relation;
	SYM name = trigger->trg_name;

	put_symbol(dyn, isc_dyn_def_trigger, name);
	put_symbol(dyn, isc_dyn_rel_name, relation->rel_name);
	put_number(dyn, isc_dyn_trg_type, trigger->trg_type);
	put_number(dyn, isc_dyn_trg_sequence, trigger->trg_sequence);
	put_number(dyn, isc_dyn_trg_inactive, trigger->trg_inactive);
	put_blr(dyn, isc_dyn_trg_blr, relation, trigger->trg_statement);
	put_text(dyn, isc_dyn_trg_source, trigger->trg_source);
	put_text(dyn, isc_dyn_description, trigger->trg_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_trigger_msg( STR dyn, TRGMSG trigmsg)
{
/**************************************
 *
 *	a d d _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to add a trigger message.
 *
 **************************************/

	put_number(dyn, isc_dyn_def_trigger_msg, trigmsg->trgmsg_number);
	put_symbol(dyn, isc_dyn_trg_name, trigmsg->trgmsg_trg_name);
	put_symbol(dyn, isc_dyn_trg_msg, trigmsg->trgmsg_text);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void add_view( STR dyn, DUDLEY_REL relation)
{
/**************************************
 *
 *	a d d _ v i e w
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to create a view.
 *
 **************************************/
	put_symbol(dyn, isc_dyn_def_view, relation->rel_name);
	put_symbol(dyn, isc_dyn_security_class, relation->rel_security_class);
	if (relation->rel_system)
		put_number(dyn, isc_dyn_system_flag, relation->rel_system);
	put_text(dyn, isc_dyn_description, relation->rel_description);
	put_blr(dyn, isc_dyn_view_blr, relation, relation->rel_rse);
	put_text(dyn, isc_dyn_view_source, relation->rel_view_source);

	DUDLEY_NOD contexts = relation->rel_rse->nod_arg[s_rse_contexts];
	for (SSHORT i = 0; i < contexts->nod_count; i++) {
		DUDLEY_CTX context = (DUDLEY_CTX) contexts->nod_arg[i]->nod_arg[0];
		put_symbol(dyn, isc_dyn_view_relation,
				   context->ctx_relation->rel_name);
		put_number(dyn, isc_dyn_view_context, context->ctx_context_id);
		put_symbol(dyn, isc_dyn_view_context_name, context->ctx_name);
		check_dyn(dyn, 1);
		dyn->add_byte(isc_dyn_end);
	}

	for (DUDLEY_FLD field = relation->rel_fields; field; field = field->fld_next)
		add_field(dyn, field, relation);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


bool TRN_get_buffer(STR dyn, USHORT length)
{
/**************************************
 *
 *	g e t _ b u f f e r
 *
 **************************************
 *
 * Functional description
 *	Generated DYN string is about to over the memory allocated for it.
 *	So, allocate an extra memory.
 *
 **************************************/
	const USHORT len = dyn->str_current - dyn->str_start;

/* Compute the next incremental string len */

	const USHORT n = MIN(dyn->str_length * 2, 65536 - 4);

/* If we're in danger of blowing the limit, stop right here */

	if (n < len + length)
		return false;

	const UCHAR* q;
	UCHAR *p, *old;
	q = old = dyn->str_start;
	dyn->str_start = p = static_cast<UCHAR*>(gds__alloc(n));

	if (!p)
		return false;

	dyn->str_length = n;
	dyn->str_current = dyn->str_start + len;

#ifdef DEBUG_GDS_ALLOC
/* For V4.0 we don't care about Gdef specific memory leaks */
	gds_alloc_flag_unfreed(dyn->str_start);
#endif

	memmove(p, q, len);
	gds__free(old);
	return true;
}


static void drop_field( STR dyn, DUDLEY_FLD field)
{
/**************************************
 *
 *	d r o p _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to eliminate
 *	a local field.
 *
 **************************************/
	SYM name = field->fld_name;
	DUDLEY_REL relation = field->fld_relation;

	put_symbol(dyn, isc_dyn_delete_local_fld, name);
	put_symbol(dyn, isc_dyn_rel_name, relation->rel_name);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}




static void drop_filter( STR dyn, FILTER filter)
{
/**************************************
 *
 *	d r o p _ f i l t e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete a
 *	blob filter.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_delete_filter, filter->filter_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_function( STR dyn, FUNC function)
{
/**************************************
 *
 *	d r o p _ f u n c t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete a
 *	user defined function.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_delete_function, function->func_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_global_field( STR dyn, DUDLEY_FLD field)
{
/**************************************
 *
 *	d r o p _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to eliminate
 *	a global field.
 *
 **************************************/
	SYM name = field->fld_name;

	put_symbol(dyn, isc_dyn_delete_global_fld, name);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_index( STR dyn, DUDLEY_IDX index)
{
/**************************************
 *
 *	d r o p  _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete an index.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_delete_idx, index->idx_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_relation( STR dyn, DUDLEY_REL relation)
{
/**************************************
 *
 *	d r o p  _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete an relation.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_delete_rel, relation->rel_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_security_class( STR dyn, SCL sec_class)
{
/**************************************
 *
 *	d r o p  _ s e c u r i t y _ c l a s s
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to delete an securtiy_class.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_delete_security_class, sec_class->scl_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_shadow( STR dyn, SLONG shadow_number)
{
/**************************************
 *
 *	d r o p _ s h a d o w
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to drop a shadow.
 *
 **************************************/

	put_number(dyn, isc_dyn_delete_shadow, (SSHORT) shadow_number);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_trigger( STR dyn, DUDLEY_TRG trigger)
{
/**************************************
 *
 *	d r o p _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to delete a trigger.
 *
 **************************************/
	SYM name = trigger->trg_name;

	put_symbol(dyn, isc_dyn_delete_trigger, name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void drop_trigger_msg( STR dyn, TRGMSG trigmsg)
{
/**************************************
 *
 *	d r o p _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to delete a trigger message.
 *
 **************************************/

	put_number(dyn, isc_dyn_delete_trigger_msg, trigmsg->trgmsg_number);
	put_symbol(dyn, isc_dyn_trg_name, trigmsg->trgmsg_trg_name);
	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void gen_dyn_c(void *user_arg, SSHORT offset, const char* string)
{
/**************************************
 *
 *	g e n _ d y n _ c
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/

	fprintf(output_file, "    %s\n", string);
}


static void gen_dyn_cxx(void *user_arg, SSHORT offset, const char* string)
{
/**************************************
 *
 *	g e n _ d y n _ c x x
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/
	char temp[1024]; // This buffer size should be more than enough
	fb_assert(strlen(string) < sizeof(temp));
	strcpy(temp, string);

	char* q = temp;
	const char* p = q;

	fprintf(output_file, "    ");
	for (; *q; *q++) {
		if ((*q == '$') || (*q == '_')) {
			const char* r = q;
			if ((*--r == '_') && (*--r == 's') && (*--r == 'd') && (*--r == 'g'))
			{
				*q = 0;
				fprintf(output_file, "%s", p);
				p = ++q;
			}
		}
	}

	fprintf(output_file, "%s\n", p);
}


static void gen_dyn_pas(void *user_arg, SSHORT offset, const char* string)
{
/**************************************
 *
 *	g e n _ d y n _ p a s
 *
 **************************************
 *
 * Functional description
 *	Callback routine for BLR pretty printer.
 *
 **************************************/
	fprintf(output_file, "      %s\n", string);
}


static void modify_database( STR dyn, DBB databaseL)
{
/**************************************
 *
 *	m o d i f y _ d a t a b a s e
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify a database.
 *
 **************************************/

	if (!(databaseL->dbb_files ||
		  databaseL->dbb_security_class ||
		  databaseL->dbb_description ||
		  (databaseL->
		   dbb_flags & (DBB_null_security_class | DBB_null_description))))
	{
		return;
	}

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_mod_database);

	add_files(dyn, databaseL->dbb_files, databaseL);
	if (databaseL->dbb_flags & DBB_null_security_class) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_security_class);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_security_class,
				   databaseL->dbb_security_class);
	if (databaseL->dbb_flags & DBB_null_description) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_description);
		dyn->add_word(0);
	}
	else if (databaseL->dbb_description)
		put_text(dyn, isc_dyn_description, databaseL->dbb_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void modify_field( STR dyn, DUDLEY_FLD field, DUDLEY_REL view)
{
/**************************************
 *
 *	m o d i f y _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify a
 *	local field.
 *
 **************************************/
	DUDLEY_REL relation;

	SYM name = field->fld_name;
	if (view)
		relation = view;
	else
		relation = field->fld_relation;

	put_symbol(dyn, isc_dyn_mod_local_fld, name);
	put_symbol(dyn, isc_dyn_rel_name, relation->rel_name);

	SYM symbol = field->fld_source;
	if (symbol &&
		strcmp(symbol->sym_string, name->sym_string) && !field->fld_computed)
	{
		put_symbol(dyn, isc_dyn_fld_source, symbol);
	}

	if (field->fld_flags & fld_null_security_class) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_security_class);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_security_class, field->fld_security_class);
	if (field->fld_flags & fld_null_edit_string) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_edit_string);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_fld_edit_string, field->fld_edit_string);
	if (field->fld_flags & fld_null_query_name) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_query_name);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_fld_query_name, field->fld_query_name);
	if (field->fld_flags & fld_null_query_header) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_query_header);
		dyn->add_word(0);
	}
	else
		put_query_header(dyn, isc_dyn_fld_query_header,
						 field->fld_query_header);

	if (field->fld_system)
		put_number(dyn, isc_dyn_system_flag, field->fld_system);

	if (field->fld_flags & fld_explicit_position)
		put_number(dyn, isc_dyn_fld_position, field->fld_position);

	if (field->fld_flags & fld_null_description) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_description);
		dyn->add_word(0);
	}
	else if (field->fld_description)
		put_text(dyn, isc_dyn_description, field->fld_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void modify_global_field( STR dyn, DUDLEY_FLD field)
{
/**************************************
 *
 *	m o d i f y _ g l o b a l _ f i e l d
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to
 *	modify a global field.  This gets
 *	more and more tiresome.
 *
 **************************************/
	if (field->fld_computed)
		return;

	SYM name = field->fld_name;

	put_symbol(dyn, isc_dyn_mod_global_fld, name);
	if (field->fld_flags & fld_null_edit_string) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_edit_string);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_fld_edit_string, field->fld_edit_string);
	if (field->fld_flags & fld_null_query_name) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_query_name);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_fld_query_name, field->fld_query_name);

	if (field->fld_flags & fld_null_query_header) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_query_header);
		dyn->add_word(0);
	}
	else
		put_query_header(dyn, isc_dyn_fld_query_header,
						 field->fld_query_header);

	if (field->fld_system & fld_explicit_system)
		put_number(dyn, isc_dyn_system_flag, field->fld_system);

	if (field->fld_dtype) {
		put_number(dyn, isc_dyn_fld_type, field->fld_dtype);
		put_number(dyn, isc_dyn_fld_length, field->fld_length);
		put_number(dyn, isc_dyn_fld_scale, field->fld_scale);
	}

	if (field->fld_has_sub_type)
		put_number(dyn, isc_dyn_fld_sub_type, field->fld_sub_type);

	const int n = field->fld_segment_length;
	if (n)
		put_number(dyn, isc_dyn_fld_segment_length, n);

	if (field->fld_flags & fld_null_missing_value) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_fld_missing_value);
		dyn->add_word(0);
	}
	else if (field->fld_missing)
		put_blr(dyn, isc_dyn_fld_missing_value, NULL, field->fld_missing);

	if (field->fld_flags & fld_null_validation) {
		check_dyn(dyn, 6);
		dyn->add_byte(isc_dyn_fld_validation_blr);
		dyn->add_word(0);
		dyn->add_byte(isc_dyn_fld_validation_source);
		dyn->add_word(0);
	}
	else if (field->fld_validation) {
		put_blr(dyn, isc_dyn_fld_validation_blr, NULL,
				field->fld_validation);
		put_text(dyn, isc_dyn_fld_validation_source, field->fld_valid_src);
	}

	if (field->fld_flags & fld_null_description) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_description);
		dyn->add_word(0);
	}
	else if (field->fld_description)
		put_text(dyn, isc_dyn_description, field->fld_description);

	if (field->fld_dimension) {
		put_number(dyn, isc_dyn_fld_dimensions, field->fld_dimension);
		add_dimensions(dyn, field);
	}

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void modify_index( STR dyn, DUDLEY_IDX index)
{
/**************************************
 *
 *	m o d i f y _ i n d e x
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify an index.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_mod_idx, index->idx_name);

	if (index->idx_flags & IDX_unique_flag)
		put_number(dyn, isc_dyn_idx_unique, (index->idx_unique) ? TRUE : FALSE);

	if (index->idx_flags & IDX_active_flag)
		put_number(dyn, isc_dyn_idx_inactive,
				   (index->idx_inactive) ? TRUE : FALSE);

	if (index->idx_flags & IDX_type_flag)
		put_number(dyn, isc_dyn_idx_type, index->idx_type);

	if (index->idx_flags & IDX_null_description) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_description);
		dyn->add_word(0);
	}
	else if (index->idx_description)
		put_text(dyn, isc_dyn_description, index->idx_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void modify_relation( STR dyn, DUDLEY_REL relation)
{
/**************************************
 *
 *	m o d i f y _ r e l a t i o n
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic DDL to modify a relation.
 *
 **************************************/

	put_symbol(dyn, isc_dyn_mod_rel, relation->rel_name);
	if (relation->rel_flags & rel_null_security_class) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_security_class);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_security_class,
				   relation->rel_security_class);
	if (relation->rel_system)
		put_number(dyn, isc_dyn_system_flag, relation->rel_system);
	if (relation->rel_flags & rel_null_description) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_description);
		dyn->add_word(0);
	}
	else if (relation->rel_description)
		put_text(dyn, isc_dyn_description, relation->rel_description);
	if (relation->rel_flags & rel_null_ext_file) {
		check_dyn(dyn, 3);
		dyn->add_byte(isc_dyn_rel_ext_file);
		dyn->add_word(0);
	}
	else
		put_symbol(dyn, isc_dyn_rel_ext_file, relation->rel_filename);


	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void modify_trigger( STR dyn, DUDLEY_TRG trigger)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to modify a trigger for a relation.
 *
 **************************************/
	DUDLEY_REL relation = trigger->trg_relation;
	SYM name = trigger->trg_name;

	put_symbol(dyn, isc_dyn_mod_trigger, name);
	put_symbol(dyn, isc_dyn_rel_name, relation->rel_name);
	if (trigger->trg_mflag & trg_mflag_onoff)
		put_number(dyn, isc_dyn_trg_inactive, trigger->trg_inactive);
	if (trigger->trg_mflag & trg_mflag_type)
		put_number(dyn, isc_dyn_trg_type, trigger->trg_type);
	if (trigger->trg_mflag & trg_mflag_seqnum)
		put_number(dyn, isc_dyn_trg_sequence, trigger->trg_sequence);
	if (trigger->trg_statement)
		put_blr(dyn, isc_dyn_trg_blr, relation, trigger->trg_statement);
	if (trigger->trg_source)
		put_text(dyn, isc_dyn_trg_source, trigger->trg_source);
	if (trigger->trg_description)
		put_text(dyn, isc_dyn_description, trigger->trg_description);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void modify_trigger_msg( STR dyn, TRGMSG trigmsg)
{
/**************************************
 *
 *	m o d i f y _ t r i g g e r _ m s g
 *
 **************************************
 *
 * Functional description
 *	Generate dynamic ddl to modify a trigger message.
 *
 **************************************/

	put_number(dyn, isc_dyn_mod_trigger_msg, trigmsg->trgmsg_number);
	put_symbol(dyn, isc_dyn_trg_name, trigmsg->trgmsg_trg_name);
	put_symbol(dyn, isc_dyn_trg_msg, trigmsg->trgmsg_text);

	check_dyn(dyn, 1);
	dyn->add_byte(isc_dyn_end);
}


static void put_acl( STR dyn, UCHAR attribute, SCL sec_class)
{
/**************************************
 *
 *	p u t _ a c l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	TEXT buffer[4096];

	if (!sec_class)
		return;

	USHORT length = GENERATE_acl(sec_class, (UCHAR*) buffer);
	fb_assert(length <= 4096);		/* to make sure buffer is big enough */

	check_dyn(dyn, 3 + length);
	dyn->add_byte(attribute);
	dyn->add_word(length);
	const TEXT* p = buffer;
	while (length--)
		dyn->add_byte(*p++);
}


static void put_blr( STR dyn, UCHAR attribute, DUDLEY_REL relation, DUDLEY_NOD node)
{
/**************************************
 *
 *	p u t _ b l r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	if (!node)
		return;

	check_dyn(dyn, 3);
	dyn->add_byte(attribute);

/* Skip over space to put count later, generate blr, then
   go bad and fix up length */

	const USHORT offset = dyn->str_current - dyn->str_start;
	dyn->str_current = dyn->str_current + 2;
	GENERATE_blr(dyn, node);
	UCHAR* p = dyn->str_start + offset;
	const USHORT length = (dyn->str_current - p) - 2;
	*p++ = length;
	*p = (length >> 8);
}


static void put_number( STR dyn, TEXT attribute, SSHORT number)
{
/**************************************
 *
 *	p u t _ n u m b e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/

	check_dyn(dyn, 5);
	dyn->add_byte(attribute);

/* Assume number can be expressed in two bytes */

	dyn->add_word(2);
	dyn->add_word(number);
}


static void put_query_header( STR dyn, TEXT attribute, DUDLEY_NOD node)
{
/**************************************
 *
 *	p u t _ q u e r y _ h e a d e r
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	if (!node)
		return;

	check_dyn(dyn, 3);
	dyn->add_byte(attribute);

	const USHORT offset = dyn->str_current - dyn->str_start;
	dyn->str_current = dyn->str_current + 2;
	for (int pos = 0; pos < node->nod_count; pos++) {
		SYM symbol = (SYM) node->nod_arg[pos];
		const UCHAR* s = (UCHAR *) symbol->sym_string;
		check_dyn(dyn, symbol->sym_length);
		while (*s)
			dyn->add_byte(*s++);
	}
	UCHAR* s = dyn->str_start + offset;
	const USHORT length = (dyn->str_current - s) - 2;
	*s++ = length;
	*s = (length >> 8);
}


static void put_symbol( STR dyn, TEXT attribute, SYM symbol)
{
/**************************************
 *
 *	p u t _ s y m b o l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	if (!symbol)
		return;

	const USHORT l = symbol->sym_length;

	check_dyn(dyn, l + 5);
	dyn->add_byte(attribute);
	dyn->add_word(l);

	for (const TEXT* string = symbol->sym_string; *string;)
		dyn->add_byte(*string++);
}


static void put_text( STR dyn, UCHAR attribute, TXT text)
{
/**************************************
 *
 *	p u t _ t e x t
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	if (!text)
		return;

	const USHORT length = text->txt_length;

	if (!length)
		return;

	check_dyn(dyn, length + 5);
	dyn->add_byte(attribute);

	dyn->add_word(length);
	LEX_get_text(dyn->str_current, text);
	dyn->str_current += length;
}


static void raw_ada( STR dyn)
{
/**************************************
 *
 *	r a w _ a d a
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	USHORT n = 0;
	for (const UCHAR* p = dyn->str_start; p < dyn->str_current; p++) {
		if (p < (dyn->str_current - 1)) {
			fprintf(output_file, "%d,", *p);
			n += 4;
		}
		else {
			fprintf(output_file, "%d", *p);
			n += 3;
		}
		if (n > 60) {
			fprintf(output_file, "\n");
			n = 0;
		}
	}
	fprintf(output_file, ");\n");
}

static void raw_cobol( STR dyn)
{
/**************************************
 *
 *	r a w _ c o b o l
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	union {
		UCHAR bytewise_blr[4];
		SLONG longword_blr;
	} blr_hunk;

	int length = 1;
	UCHAR* blr = dyn->str_start;
	int blr_length = dyn->str_current - dyn->str_start;

	while (blr_length) {
		for (UCHAR* c = blr_hunk.bytewise_blr;
			 c < blr_hunk.bytewise_blr + sizeof(SLONG); c++)
		{
			*c = *blr++;
			if (!--blr_length)
				break;
		}
		if (dudleyGlob.language == lan_ansi_cobol)
			fprintf(output_file,
					   "           03  GDS-DYN-%d PIC S9(10) USAGE COMP VALUE IS %"
					   SLONGFORMAT".\n",
					   length++, blr_hunk.longword_blr);
		else
			fprintf(output_file,
					   "           03  GDS__DYN_%d PIC S9(10) USAGE COMP VALUE IS %"
					   SLONGFORMAT".\n",
					   length++, blr_hunk.longword_blr);
	}
}


static void raw_ftn( STR dyn)
{
/**************************************
 *
 *	r a w _ f t n
 *
 **************************************
 *
 * Functional description
 *
 **************************************/
	TEXT buffer[80];
	int blr_length;
	union {
		UCHAR bytewise_blr[4];
		SLONG longword_blr;
	} blr_hunk;

	UCHAR* blr = (UCHAR *) dyn->str_start;
	blr_length = dyn->str_current - dyn->str_start;
	TEXT* p = buffer;

	while (blr_length) {
		UCHAR* c;
		// Do not change these assignments order: they target the same union.
		for (c = blr_hunk.bytewise_blr, blr_hunk.longword_blr = 0;
			 c < blr_hunk.bytewise_blr + sizeof(SLONG); c++)
		{
			*c = *blr++;
			if (!--blr_length)
				break;
		}
		if (blr_length)
			sprintf(p, "%" SLONGFORMAT",", blr_hunk.longword_blr);
		else
			sprintf(p, "%" SLONGFORMAT"/", blr_hunk.longword_blr);
		while (*p)
			p++;
		if (p - buffer > 50) {
			fprintf(output_file, "%s%s\n", "     +   ", buffer);
			p = buffer;
			*p = 0;
		}
	}

	if (p != buffer)
		fprintf(output_file, "%s%s\n", "     +   ", buffer);
}


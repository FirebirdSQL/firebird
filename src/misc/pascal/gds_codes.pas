(*
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
 *)

const
	isc_facility		= 20;
	isc_err_base		= 335544320;
	isc_err_factor		= 1;
	gds_facility		= 20;
	gds_err_base		= 335544320;
	gds_err_factor		= 1;

	isc_arg_end		= 0;	(* end of argument list *)
	isc_arg_gds		= 1;	(* generic DSRI status value *)
	isc_arg_string		= 2;	(* string argument *)
	isc_arg_cstring		= 3;	(* count & string argument *)
	isc_arg_number		= 4;	(* numeric argument (long) *)
	isc_arg_interpreted	= 5;	(* interpreted status code (string) *)
	isc_arg_vms		= 6;	(* VAX/VMS status code (long) *)
	isc_arg_unix		= 7;	(* UNIX error code *)
	isc_arg_domain		= 8;	(* Apollo/Domain error code *)
	isc_arg_dos		= 9;	(* MSDOS/OS2 error code *)
	gds_arg_end		= 0;	(* end of argument list *)
	gds_arg_gds		= 1;	(* generic DSRI status value *)
	gds_arg_string		= 2;	(* string argument *)
	gds_arg_cstring		= 3;	(* count & string argument *)
	gds_arg_number		= 4;	(* numeric argument (long) *)
	gds_arg_interpreted	= 5;	(* interpreted status code (string) *)
	gds_arg_vms		= 6;	(* VAX/VMS status code (long) *)
	gds_arg_unix		= 7;	(* UNIX error code *)
	gds_arg_domain		= 8;	(* Apollo/Domain error code *)
	gds_arg_dos		= 9;	(* MSDOS/OS2 error code *)


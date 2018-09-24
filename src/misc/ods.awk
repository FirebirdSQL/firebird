#
#	PROGRAM:	ODS sizes and offsets validation
#	MODULE:		ods.awk
#	DESCRIPTION:	Generates c++ code printing reference sample for ODS strctures
#
#  The contents of this file are subject to the Initial
#  Developer's Public License Version 1.0 (the "License");
#  you may not use this file except in compliance with the
#  License. You may obtain a copy of the License at
#  http://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
#
#  Software distributed under the License is distributed AS IS,
#  WITHOUT WARRANTY OF ANY KIND, either express or implied.
#  See the License for the specific language governing rights
#  and limitations under the License.
#
#  The Original Code was created by Alexander Peshkoff
#  for the Firebird Open Source RDBMS project.
#
#  Copyright (c) 2018 Alexander Peshkoff <peshkoff@mail.ru>
#  and all contributors signed below.
#
#  All Rights Reserved.
#  Contributor(s): ______________________________________.
#

BEGIN {
	st = 0;
	nm = "";
	nm2 = "";
	v2 = "";
	skip = 0;

	print "#include <stdio.h>"
	print "#ifdef _MSC_VER"
	print "#include \"gen/autoconfig_msvc.h\""
	print "#else"
	print "#include \"gen/autoconfig.h\""
	print "#endif"
	print ""
	print "#if SIZEOF_VOID_P == 8"
	print "#define FMT \"l\""
	print "#else"
	print "#define FMT \"\""
	print "#endif"
	print ""
	print "#include \"fb_types.h\""
	print "#define ODS_TESTING"
	print "#include \"../jrd/ods.h\""
	print ""
	print "using namespace Ods;"
	print ""
	print "int main() {"
}

($1 == "struct") {
	++st;
	if (st <= 2)
	{
		if (st == 1)
		{
			nm = $2;
			nm2 = nm;
		}
		else
		{
			nm2 = nm "::" $2;
		}
		v2 = $2 "_";

		print "\tprintf(\"\\n *** " $0 " %\" FMT \"u\\n\", sizeof(" nm2 "));";
		print "\t" nm2 " " v2 ";";
	}
}


($1 == "};") {
	if (st > 1)
	{
		v2 = nm "_";
	}
	st--;
}

($1 == "}") {
	if (st > 1)
	{
		st--;
		v2 = nm "_";
	}
}

($2 == "ODS_TESTING") { skip = 1; }

(st > 0 && substr($1, 1, 1) != "#" && $1 != "struct" && skip == 0 && $1 != "//") {
	m = $2;
	i = index(m, "[");
	if (i == 0)
		i = index(m, ";");
	if (i > 0)
		m = substr(m, 1, i - 1);

	if (length(m) > 0)
		print "\tprintf(\"" m " %\" FMT \"d\\n\", (char*)&" v2 "." m " - (char*)&" v2 ");"
}

($2 == "//ODS_TESTING") { skip = 0; }

END {
	print "}"
}


/*
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
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2015 Adriano dos Santos Fernandes <adrianosf@gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef DSQL_NODE_PRINTER_H
#define DSQL_NODE_PRINTER_H

#include <optional>
#include "../dsql/Nodes.h"
#include "../common/classes/TriState.h"

#define NODE_PRINT(var, property)	var.print(STRINGIZE(property), property)
#define NODE_PRINT_ENUM(var, property)	var.print(STRINGIZE(property), (int) property)

//#define TRIVIAL_NODE_PRINTER

namespace Firebird::Jrd {


#ifndef TRIVIAL_NODE_PRINTER

class NodePrinter
{
public:
	NodePrinter(unsigned aIndent = 0)
		: indent(aIndent)
	{
	}

public:
	void begin(const string& s)
	{
		printIndent();
		text += "<";
		text += s;
		text += ">\n";

		++indent;
		stack.push(s);
	}

	void end()
	{
		string s = stack.pop();

		--indent;

		printIndent();
		text += "</";
		text += s;
		text += ">\n";
	}

	void print(const string& s, const MetaName& value)
	{
		printIndent();

		text += "<";
		text += s;
		text += ">";
		text += value.c_str();
		text += "</";
		text += s;
		text += ">\n";
	}

	void print(const string& s, const QualifiedName& value)
	{
		printIndent();

		text += "<";
		text += s;
		text += ">";
		text += value.toQuotedString();
		text += "</";
		text += s;
		text += ">\n";
	}

	void print(const string&, const IntlString&)
	{
		//// FIXME-PRINT:
	}

	void print(const string& s, const string& value)
	{
		printIndent();

		text += "<";
		text += s;
		text += ">";
		text += value;
		text += "</";
		text += s;
		text += ">\n";
	}

	void print(const string& s, bool value)
	{
		printIndent();

		text += "<";
		text += s;
		text += ">";
		text += value ? "true" : "false";
		text += "</";
		text += s;
		text += ">\n";
	}

	void print(const string& s, SINT64 value)
	{
		printIndent();

		string temp;
		temp.printf("<%s>%" QUADFORMAT"d</%s>\n", s.c_str(), value, s.c_str());
		text += temp;
	}

	void print(const string& s, long value)
	{
		print(s, (SINT64) value);
	}

	void print(const string& s, unsigned long value)
	{
		print(s, (SINT64) value);
	}

	void print(const string& s, int value)
	{
		print(s, (SINT64) value);
	}

	void print(const string& s, unsigned int value)
	{
		print(s, (SINT64) value);
	}

	void print(const string& s, USHORT value)
	{
		print(s, (SINT64) value);
	}

	void print(const string& s, SSHORT value)
	{
		print(s, (SINT64) value);
	}

	void print(const string&, const UCharBuffer&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const dsql_ctx&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const dsql_var&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const dsql_fld&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const dsql_map&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const dsql_par&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const ImplicitJoin&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const dsc&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const Format&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const GeneratorItem&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const RecordSource& recordSource)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const SubQuery&)
	{
		//// FIXME-PRINT:
	}

	void print(const string&, const Cursor&)
	{
		//// FIXME-PRINT:
	}

	template <typename T>
	void print(const string& s, const Array<T>& array)
	{
		begin(s);

		for (const T* i = array.begin(); i != array.end(); ++i)
		{
			string s2;
			s2.printf("%d", i - array.begin());
			print(s2, *i);
		}

		end();
	}

	template <typename T>
	void print(const string& s, const ObjectsArray<T>& array)
	{
		begin(s);

		unsigned n = 0;

		for (typename ObjectsArray<T>::const_iterator i = array.begin();
			 i != array.end();
			 ++i, ++n)
		{
			string s2;
			s2.printf("%d", n);
			print(s2, *i);
		}

		end();
	}

	template <typename T>
	void print(const string& s, const T* array)
	{
		if (array)
			print(s, *array);
	}

	void print(const string& s, const TriState& triState)
	{
		if (triState.isAssigned())
			print(s, triState.asBool());
	}

	template <typename T>
	void print(const string& s, const std::optional<T>& optional)
	{
		if (optional.has_value())
			print(s, optional.value());
	}

	template <typename T>
	void print(const string&, const Pair<T>&)
	{
		//// FIXME-PRINT:
	}

	template <typename T>
	void print(const string& s, const NestConst<T>& ptr)
	{
		print(s, ptr.getObject());
	}

	void print(const string& s, const Printable* printable)
	{
		printIndent();
		text += "<";
		text += s;

		if (!printable)
		{
			text += " />\n";
			return;
		}

		text += ">\n";

		++indent;
		printable->print(*this);
		--indent;

		printIndent();
		text += "</";
		text += s;
		text += ">\n";
	}

	void print(const string& s, const Printable& printable)
	{
		print(s, &printable);
	}

	void append(const NodePrinter& subPrinter)
	{
		text += subPrinter.text;
	}

	unsigned getIndent() const
	{
		return indent;
	}

	const string& getText() const
	{
		return text;
	}

private:
	void printIndent()
	{
		for (unsigned i = 0; i < indent; ++i)
			text += "\t";
	}

private:
	unsigned indent;

private:
	ObjectsArray<string> stack;
	string text;
};

#else // TRIVIAL_NODE_PRINTER

// trivial NodePrinter class - to print only node name
class NodePrinter
{
public:
	NodePrinter(unsigned aIndent = 0)
	{
	}

public:
	void begin(const string& s)
	{
	}

	void end()
	{
	}

	template <typename T>
	void print(const string& s, const T& value)
	{
	}

	void append(const NodePrinter& subPrinter)
	{
	}

	unsigned getIndent() const
	{
		return 0;
	}

	const string getText() const
	{
		return "";
	}
};

#endif // TRIVIAL_NODE_PRINTER


}	// namespace Firebird::Jrd

#endif	// DSQL_NODE_PRINTER_H

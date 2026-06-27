/*
 *	PROGRAM:		Firebird JSON logic.
 *	MODULE:			JsonConsts.h
 *	DESCRIPTION:	JSON constants.
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
 *  The Original Code was created by Artyom Abakumov
 *  <artyom.abakumov (at) red-soft.ru> for Red Soft Corporation.
 *
 *  Copyright (c) 2023 Red Soft Corporation <info (at) red-soft.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#ifndef JSON_CONSTS_H
#define JSON_CONSTS_H

#include "firebird.h"

#include "../common/classes/fb_string.h"
#include "../common/StatusArg.h"

#include "../jrd/intl.h"
#include "../common/TextType.h"


// Tokens
namespace FBJSON
{
	// usings

	inline constexpr TTypeId JSON_TTYPE = ttype_utf8;
	inline constexpr USHORT JSON_TTYPE_BYTES_BET_CHAR = 4;
	inline constexpr USHORT JSON_DEFAULT_SIZE = 6000 * JSON_TTYPE_BYTES_BET_CHAR;

	using TextPos = FB_UINT64;
	using TextLength = FB_UINT64;

	inline constexpr TextPos INVALID_TEXT_POS = MAX_UINT64;
	inline constexpr TextLength JSON_MAX_REPORT_SIZE = 50;

	using SmallString = Firebird::string;
	using JsonToken = std::string_view;
	using JsonArrayIndex = USHORT;

	enum PlanType : UCHAR // Ordered by priority
	{
		DEFAULT = 0,
		INNER, // one to one from nested
		OUTER, // one to many with nested
		UNION, // 2 columns: print full one, then print full the second:<a, null>, <null, a>
		CROSS // 2 columns: print columns at the same moment: <a,b>
	};

	enum class CompareResult : int
	{
		EQUALS = 0,
		LESS = -1,
		MORE = 1
	};


	// Basically the inversion
	inline constexpr CompareResult operator*(CompareResult lhs, const bool invert)
	{
		if (!invert)
			return lhs;

		return static_cast<CompareResult>(-static_cast<int>(lhs));
	}

	inline CompareResult castCompareResult(const int r)
	{
		if (r < 0)
			return CompareResult::LESS;
		else if (r > 0)
			return CompareResult::MORE;
		else
			return CompareResult::EQUALS;
	}

	namespace Tokens
	{
		namespace TypeName
		{
			inline constexpr JsonToken EMPTY = "empty";
			inline constexpr JsonToken STRING = "string";
			inline constexpr JsonToken NUMBER = "number";
			inline constexpr JsonToken BOOLEAN = "boolean";
			inline constexpr JsonToken JNULL = "null";
			inline constexpr JsonToken ARRAY = "array";
			inline constexpr JsonToken OBJECT = "object";
			inline constexpr JsonToken DATETIME = "datetime";
			inline constexpr JsonToken UNKNOWN = "unknown";
			inline constexpr JsonToken TIME_WITH_TZ = "time with time zone";
			inline constexpr JsonToken TIME_WITHOUT_TZ = "time without time zone";
			inline constexpr JsonToken TIMESTAMT_WITH_TZ = "timestamp with time zone";
			inline constexpr JsonToken TIMESTAMT_WITHOUT_TZ = "timestamp without time zone";
			inline constexpr JsonToken DATE = "date";
		}

		const JsonToken ANY_FIELD("*");
	}
}


// using JsonIscStatus = ISC_STATUS;
// using JsonStatusVector = Firebird::Arg::StatusVector;
// using JsonStatusMsg = Firebird::Arg::Gds;
// using JsonStatusMsgIntArg = Firebird::Arg::Num;
// using JsonStatusMsgStrArg = Firebird::Arg::Str;

// Temporary classes to avoid constantly moving codes msg/jrd.h;
// It will be replaced by the real jrd codes
struct JsonStatusMsgWrapper;
using JsonIscStatus = const char*;
using JsonStatusVector = JsonStatusMsgWrapper;
using JsonStatusMsg = JsonStatusMsgWrapper;
using JsonStatusMsgIntArg = JsonStatusMsgWrapper;
using JsonStatusMsgStrArg = JsonStatusMsgWrapper;

struct JsonStatusMsgWrapper
{
	Firebird::ObjectsArray<Firebird::string> impl;
	mutable Firebird::Arg::StatusVector status;

	JsonStatusMsgWrapper() noexcept : impl(*getDefaultMemoryPool()), status()
	{ }

	JsonStatusMsgWrapper(SINT64 value) : impl(*getDefaultMemoryPool()), status()
	{
		Firebird::string str;
		str.printf("%" SQUADFORMAT, value);
		impl.push(str);
	}


	JsonStatusMsgWrapper(const Firebird::string& se) : impl(*getDefaultMemoryPool()), status()
	{
		impl.push(se);
	}

	JsonStatusMsgWrapper(const std::string_view sv) : impl(*getDefaultMemoryPool()), status()
	{
		impl.push(Firebird::string(sv.data(), static_cast<Firebird::AbstractString::size_type>(sv.length())));
	}

	JsonStatusMsgWrapper(const char* rawStr) : impl(*getDefaultMemoryPool()), status()
	{
		Firebird::string str(rawStr);
		impl.push(str);
	}

	JsonStatusMsgWrapper(const char* rawStr, ULONG length) : impl(*getDefaultMemoryPool()), status()
	{
		Firebird::string str(rawStr, length);
		impl.push(str);
	}

	// StatusVector case - append multiple args
	JsonStatusMsgWrapper& operator<<(const JsonStatusMsgWrapper& arg)
	{
		for (FB_SIZE_T i = 0; i < arg.impl.getCount(); i++)
		{
			impl.push(arg.impl[i]);
		}

		return *this;
	}

	bool isDirty() const
	{
		return impl.hasData();
	}

	const ISC_STATUS* value() const
	{
		status.assign(Firebird::Arg::Gds(isc_random) << Firebird::Arg::Str(getMessage()));
		return status.value();
	}

	Firebird::string getMessage() const
	{
		FB_SIZE_T msgNum = 0;
		Firebird::string outMsg;
		// Append non args to the end of the message
		for (; msgNum < impl.getCount(); msgNum++)
		{
			FB_SIZE_T maxUsedMsg = 0;
			const Firebird::string& mainMsg = impl[msgNum];
			for (FB_SIZE_T i = 0; i < mainMsg.length(); i++)
			{
				const char curChar = mainMsg[i];
				if (i < mainMsg.length() - 1 && curChar == '@' &&
					(mainMsg[i + 1] >= '0' && mainMsg[i + 1] <= '9'))
				{
					const FB_SIZE_T digit = mainMsg[i + 1] - '0';
					if (msgNum + digit >= impl.getCount())
					{
						fb_assert(false);
						return "<Argument mismatched>";
					}
					else
					{
						maxUsedMsg = maxUsedMsg > digit ? maxUsedMsg : digit;
						outMsg += impl[msgNum + digit];
						i++;
					}
				}
				else
					outMsg += curChar;
			}

			msgNum += maxUsedMsg; // Skip args
		}

		return outMsg;
	}
};

#define isc_jlexer_invalid_syntax "Invalid input syntax"
#define isc_jlexer_time_precision_syntax "Time precision should be positive"
#define isc_jlexer_end "Unexpected end of command at position @1"
#define isc_jlexer_end_with_line "Unexpected end of command at position @1 (line @2)"
#define isc_jlexer_passing_not_defined "Passing clause has not been set"
#define isc_jlexer_passing_var_not_defined "Variable '@1' has not been passed. Note that the passing name is case-sensitive"
#define isc_jpath_common "JSON Path: "
#define isc_jpath_invalid_token "Invalid input syntax\nToken \"@1\" is invalid"
#define isc_jpath_problem_place "\n-At position @1\n->@2"
#define isc_jpath_problem_place_with_line "\n-At position @1 (line @2)\n->@3"
#define isc_jpath_multiple_roots "Usage of multiple root accessors is not allowed for current JSON function"
#define isc_jpath_math_is_forbidden "Using methods or arithmetics is not allown in for current function"
#define isc_jpath_illegal_item_variable "Using @ variable is allowed only in a filter expression"
#define isc_jpath_missing_root "The root $ expression is missing"
#define isc_jpath_invalid_range "Range value should be an INTEGRAL value in range [@1 to @2]"
#define isc_jstrict_common "strict mode: "
#define isc_jstrict_out_of_range "Range [@1 to @2] is out of array bounds (0..@3)"
#define isc_jstrict_invalid_range "Range [@1 to @2] is invalid"
#define isc_jstrict_filed_name_mismatched "Object filed \"@1\" has not been found"
#define isc_jstrict_path_mismatched "Json Path does not match JSON structure"
#define isc_jstrict_non_array_size "Cannot calculate size for a non-array token"
#define isc_jfilter_common "JSON Path Filter: "
#define isc_jword_string "string"
#define isc_jword_left "left"
#define isc_jword_right "right"
#define isc_jword_unary "unary"
#define isc_jword_unary_minus "unary minus"
#define isc_jword_unary_plus "unary plus"
#define isc_jword_bool_or_unknown "Boolean or Unknown"
#define isc_jparser_common "JSON parser: invalid input syntax for JSON.\n"
#define isc_jparser_input "JSON data: @1"
#define isc_jparser_end_unexpectedly "Expected @1, but encountered the end of the input\n"
#define isc_jparser_expected_token "Expected @1, but found \"@2\".\n"
#define isc_jparser_string "string"
#define isc_jparser_object_end "'}'"
#define isc_jparser_object_colon "':'"
#define isc_jparser_array_end "']'"
#define isc_jparser_value "value"
#define isc_jparser_end "end of the input"
#define isc_jparser_unescaped_character "Character with value 0x@1 must be escaped\n"
#define isc_jparser_un_incorrect_code "The unicode sequence '\\u@1' does not consist of 4 hexadecimal digits\n"
#define isc_jparser_un_double_high_surrogate "Unicode lead surrogate '\\u@1' must not follow a lead surrogate (U+D800 to U+DBFF)\n"
#define isc_jparser_un_missing_high_surrogate "The unicode tail surrogate '\\u@1' must follow a lead surrogate (U+D800 to U+DBFF)\n"
#define isc_jparser_un_conv_error "The Unicode sequence '\\u0000' cannot be converted to text\n"
#define isc_jparser_invalid_sequence "Escape sequence '\\@1' is invalid\n"
#define isc_jparser_invalid_token "Token \"@1\" is invalid\n"
#define isc_jparser_invalid_number_parse "The value \"@1\" cannot be converted to a number\n"
#define isc_jparser_empty_input "Empty value cannot be converted to a number\n"
#define isc_jparser_invalid_quoted_string "Invalid quoted string: the closing quote is missing\n"
#define isc_jparser_number_overflow "Data overflow for number @1\n"
#define isc_jdyn_common "JSON processor\n"
#define isc_jdyn_get_double_invalid_type_error "Number expected, but invalid type encountered"
#define isc_jdyn_get_double_string_type_error "Number expected, but string '@1' encountered"
#define isc_jdyn_internal_error "internal error (bug)"
#define isc_jdyn_to_double_error "The statement '@1' cannot be converted to a number"
#define isc_jdyn_invalid_expression_before_is_unknown "Invalid expression before 'is unknown' predicate"
#define isc_jdyn_operand_error "@1 operand of operator @2 must be @3"
#define isc_jdyn_number_operand_error "@1 operand of operator @2 must be a number" // , but a @3 was provided
#define isc_jdyn_both_not_the_type "Both operations of component @1 must be @2"
#define isc_jdyn_missing_bool_op "A value without a condition cannot produce a boolean result"
#define isc_jdyn_missing_value_for_op "Missing value before or after an operand"
#define isc_jpath_operations_limit "The operations limit has been exceeded (@1, max allowed @2)"
#define isc_jdyn_json_wrong_type "The value in the JSON representation is expected as a string"
#define isc_jdyn_invalid_statement "Invalid statement"
#define isc_jdyn_returns_multiple_values "@1 path expression returns more than one value"
#define isc_jdyn_returns_non_scalar "@1 path expression returned a non-scalar value"
#define isc_jdyn_missing_wrapper_scalar "@1 should be used with the ARRAY WRAPPER clause because the result has a scalar value"
#define isc_jdyn_missing_wrapper_several "@1 should be used with the ARRAY WRAPPER clause because the result consists of several values"
#define isc_jdyn_incorrect_filter_result "The result of the filter calculation is not a boolean or unknown value"
#define isc_jdyn_type_unknown_token "type(): An unknown token has been passed"
#define isc_jdyn_regexflags_not_string "like_regex: The flags clause type is not a string"
#define isc_jdyn_unknown_regexflag "like_regex: Unknown flag '@1'"
#define isc_jdyn_flags_illegal_usage "like_regex: Illegal usage of a regex flags clause"
#define isc_jdyn_token_parsing_error "@1: Error while parsing the input string\n"
#define isc_jdyn_to_number_error "@1: A non-string or non-number value has been passed"
#define isc_jdyn_path_empty_result "Cannot operate with empty value/sequence in the current expression"
#define isc_jdyn_function_empty_result "@1 returned an empty value"
#define isc_jdyn_key_is_null "A JSON key cannot be NULL"
#define isc_jdyn_invalid_key_type "Invalid key: Only a character type is allowed"
#define isc_jdyn_nonunique_keys "There is a non-unique key with the name '@1' at indexes @2 and @3"
#define isc_jdyn_missing_passing_id "Passing name \"@1\" is not defined"
#define isc_jmodify_insert_missing_index "Modify error: INSERT mode requires an index range, not an array itself; Use APPEND mode in this case"
#define isc_jmodify_insert_incompatible "Modify error: INSERT mode is incompatible with the matched JSON token"
#define isc_jmodify_insert_duplicate "Modify error: attempt to INSERT duplicate key. Use UPDATE mode"
#define isc_jmodify_mode_vs_object_incompatible "Modify error: Current mode is incompatible with an object"
#define isc_jmodify_mode_vs_scalar_incompatible "Modify error: Current mode is incompatible with a scalar"
#define isc_jmodify_missing_item "Item to modify has not been found"
#define isc_jdyn_not_json "The JSON is not in a proper format"
#define isc_jdyn_duplicate_fields "The JSON contains duplicate fields"
#define isc_jdyn_keyvalue_with_scalar "scalar is incomputable with keyvalue()"
#define isc_jdyn_keyvalue_with_array "array is incomputable with keyvalue()"
#define isc_jdyn_keyvalue_with_null "null is incomputable with keyvalue()"
#define isc_jdyn_keyvalue_with_invalid_type "Invalid input type for keyvalue()"
#define isc_jdyn_size_invalid_type "Invalid input type for size()"
#define isc_jdyn_datetime_invalid_type "Invalid input type for datetime()"
#define isc_jdyn_number_limit "Input value is out of JSON number limit"
#define isc_jdyn_keyvalue_returned_sequence "keyvalue() returned several values an empty sequence"
#define isc_jdyn_incorrect_path_if_result "The result of the path filter calculation is not a Boolean or Unknown value"
#define isc_jdyn_datetime_incompatible "Current type is incompatible with the datetime() method"
#define isc_jdyn_boolean_cast_type "@1: Invalid input type for boolean conversion"
#define isc_jdyn_boolean_cast_value "@1: The input text cannot be parsed as boolean"
#define isc_jdyn_json_to_string_prefix "@1: "
#define isc_jdyn_json_to_string_cast_error "Cannot convert JSON Object or JSON Array to string"
#define isc_jdyn_to_int_error "@1: Invalid input type for number conversion\n"
#define isc_jout_enum_with_multiple_columns "A query expression argument in the JSON_ARRAY function must return exactly one column"
#define isc_jdsc_invalid_subtype "Invalid or not specified JSON subtype"
#define isc_jdsc_invalid_charset "JSON should be in UTF8 representation"
#define isc_jdsc_concatenate_unsupported "Concatenation of JSON type is unsupported"
#define isc_jdsc_non_scalar "The input value is not a scalar"
#define isc_jdsc_cannot_convert_json_to_dsc "Cannot convert JSON value to a scalar"
#define isc_jdsc_too_big "The JSON string is too big (the length is @1)"


#endif // JSON_CONSTS_H

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
 *  The Original Code was created by Adriano dos Santos Fernandes.
 *
 *  Copyright (c) 2015 Adriano dos Santos Fernandes <adrianosf at gmail.com>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

package com.github.asfernandes.cloop.tests.test1;


public class CalcException extends Exception
{
	private int code;

	public CalcException(int code)
	{
		this.code = code;
	}

	public int getCode()
	{
		return code;
	}

	public static void catchException(ICalc.IStatus status, Throwable t)
	{
		status.setCode(ICalc.IStatus.ERROR_1);
	}

	public static void checkException(ICalc.IStatus status) throws CalcException
	{
		int code = status.getCode();

		if (code != 0)
			throw new CalcException(code);
	}
}

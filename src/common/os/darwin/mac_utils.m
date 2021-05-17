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
 *  The Original Code was created by Alexander Peshkoff
 *  for the Firebird Open Source RDBMS project.
 *
 *  Copyright (c) 2020 Alexander Peshkoff <peshkoff@mail.ru>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 */


#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <os/log.h>

Boolean isSandboxed()
{
	SecTaskRef task = SecTaskCreateFromSelf(nil);
	CFTypeRef value = SecTaskCopyValueForEntitlement(task,
		CFStringCreateWithCString(0, "com.apple.security.app-sandbox", kCFStringEncodingUTF8), nil);

    return value != nil;
}

const char* getTemporaryFolder()
{
	if (!isSandboxed())
		return NULL;

	NSString* tempDir = NSTemporaryDirectory();
	if (tempDir == nil)
		return NULL;

	return tempDir.UTF8String;
}

void osLog(const char* msg)
{
	os_log_with_type(OS_LOG_DEFAULT, OS_LOG_TYPE_DEFAULT, "%{public}s", msg);
}


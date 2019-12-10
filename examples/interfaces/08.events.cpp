/*
 *	PROGRAM:	Object oriented API samples.
 *	MODULE:		08.events.cpp
 *	DESCRIPTION:	A sample of working with events.
 *
 *					Example for the following interfaces:
 *					IEvents - returned by queEvents(), used to cancel events monitoring
 *					IEventCallback - it's callback is invoked when event happens
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Adriano dos Santos Fernandes.
 * Portions created by the Initial Developer are Copyright (C) 2011 the Initial Developer.
 * All Rights Reserved.
 *
 * Contributor(s):
 *	Alexander Peshkov
 */

#include "ifaceExamples.h"

#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

static IMaster* master = fb_get_master_interface();

namespace
{
	// This class encapsulates single event handling
	class Event : public IEventCallbackImpl<Event, ThrowStatusWrapper>
	{
	public:
		Event(IAttachment* aAttachment, const char* name)
			: refCounter(0),
			  attachment(aAttachment),
			  counter(0),
			  status(master->getStatus()),
			  events(NULL),
			  first(true)
		{
			eveLen = isc_event_block(&eveBuffer, &eveResult, 1, name);
			events = attachment->queEvents(&status, this, eveLen, eveBuffer);
		}

		void process(int pass)
		{
			if (!events)
				return;

			ISC_ULONG tot = 0;
			if (counter)
			{
				isc_event_counts(&tot, eveLen, eveBuffer, eveResult);

				events->release();
				events = NULL;
				counter = 0;
				events = attachment->queEvents(&status, this, eveLen, eveBuffer);
			}

			if (tot && !first)
				printf("Event count on pass %d is %d\n", pass, tot);
			else
				printf("Pass %d - no events\n", pass);

			first = false;
		}

		// IEventCallback implementation
		void eventCallbackFunction(unsigned int length, const ISC_UCHAR* data)
		{
			memcpy(eveResult, data, length);
			++counter;
			if (!first)
				printf("AST called\n");
		}

		// refCounted implementation
		virtual void addRef()
		{
			++refCounter;
		}

		virtual int release()
		{
			if (--refCounter == 0)
			{
				delete this;
				return 0;
			}
			else
				return 1;
		}

	private:
		~Event()
		{
			if (events)
				events->release();
			if (eveBuffer)
				isc_free((char*) eveBuffer);
			if (eveResult)
				isc_free((char*) eveResult);
			status.dispose();
		}

		FbSampleAtomic refCounter;
		IAttachment* attachment;
		volatile int counter;
		ThrowStatusWrapper status;
		IEvents* events;
		unsigned char* eveBuffer;
		unsigned char* eveResult;
		unsigned eveLen;
		bool first;
	};
}

int main()
{
	int rc = 0;

	// set default password if none specified in environment
	setenv("ISC_USER", "sysdba", 0);
	setenv("ISC_PASSWORD", "masterkey", 0);

	// With ThrowStatusWrapper passed as status interface FbException will be thrown on error
	ThrowStatusWrapper status(master->getStatus());

	// Declare pointers to required interfaces
	IProvider* prov = master->getDispatcher();
	IAttachment* att = NULL;
	ITransaction* tra = NULL;
	Event* event = NULL;

	try
	{
		// attach database
		att = prov->attachDatabase(&status, "employee", 0, NULL);

		// register an event
		event = new Event(att, "EVENT1");
		event->addRef();

		const char cmdBlock[] = "execute block as begin post_event 'EVENT1'; end";

		for (int i = 0; i < 3; ++i)
		{
#ifndef WIN32
			sleep(1);		// sec
#else
			Sleep(1000);	// msec
#endif
			event->process(i);

			tra = att->startTransaction(&status, 0, NULL);
			att->execute(&status, tra, 0, cmdBlock, SAMPLES_DIALECT,
				NULL, NULL, NULL, NULL);
			tra->commit(&status);
			tra = NULL;
		}

		// cleanup
		event->release();
		event = NULL;
		att->detach(&status);
		att = NULL;
	}
	catch (const FbException& error)
	{
		// handle error
		rc = 1;

		char buf[256];
		master->getUtilInterface()->formatStatus(buf, sizeof(buf), error.getStatus());
		fprintf(stderr, "%s\n", buf);
	}

	// release interfaces after error caught
	if (event)
		event->release();
	if (tra)
		tra->release();
	if (att)
		att->release();

	// generic cleanup
	prov->release();
	status.dispose();

	return rc;
}

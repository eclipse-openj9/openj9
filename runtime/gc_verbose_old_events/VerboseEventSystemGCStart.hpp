
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(EVENT_SYSTEM_GC_START_HPP_)
#define EVENT_SYSTEM_GC_START_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "mmhook.h"

#include "VerboseEventGCStart.hpp"

/**
 * Stores the data relating to the start of a system garbage collection.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventSystemGCStart : public MM_VerboseEventGCStart
{
private:
	/**
	 * Passed Data 
	 * @{
	 */
	/** @} */
	
	/**
	 * External Data 
	 * @{
	 */
	U_64 _lastSysTime; /**< the timestamp of the last system GC */
	UDATA _sysCollectionCount; /**< the count of system GCs */
	/** @} */

public:
	static MM_VerboseEvent *newInstance(MM_SystemGCStartEvent *event, J9HookInterface** hookInterface);

	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MM_VerboseEventSystemGCStart(MM_SystemGCStartEvent *event, J9HookInterface** hookInterface) :
		MM_VerboseEventGCStart(event->currentThread, event->timestamp, event->eventid, event->gcStartData, hookInterface)
	{};
};

#endif /* EVENT_SYSTEM_GC_START_HPP_ */

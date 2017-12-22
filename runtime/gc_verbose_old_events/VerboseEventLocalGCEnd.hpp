
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

#if !defined(EVENT_LOCAL_GC_END_HPP_)
#define EVENT_LOCAL_GC_END_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"
#include "GCExtensions.hpp"

#if defined(J9VM_GC_MODRON_SCAVENGER)

#include "VerboseEvent.hpp"

/**
 * Stores the data relating to the end of a local garbage collection.
 * @ingroup GC_verbose_events
 */
class MM_VerboseEventLocalGCEnd : public MM_VerboseEvent
{
private:
	/* Passed Data */
	UDATA	_globalGCCount;	/**< the number of global gc's */
	UDATA	_localGCCount; /**< the number oflocal gc's */
	UDATA	_rememberedSetOverflowed; /**< flag to indicate whether the remembered set overflowed */
	UDATA	_causedRememberedSetOverflow; /**< flag to indicate whether the remembered set overflow was caused by this collection */
	UDATA	_scanCacheOverflow; /**< flag to indicate whether the scan cache has overflowed */
	UDATA	_failedFlipCount; /**< the number of objects that could not be flipped */
	UDATA	_failedFlipBytes; /**< the number of bytes that could not be flipped */
	UDATA	_failedTenureCount; /**< the number of objects that could not be tenured */
	UDATA	_failedTenureBytes; /**< the number of bytes that could not be tenured */
	UDATA	_backout; /**< flag to indicate whether the local collection was aborted */
	UDATA	_flipCount; /**< the number of objects that were flipped */
	UDATA	_flipBytes; /**< the number of bytes that were flipped */
	UDATA	_tenureCount; /**< the number of objects that were tenured */
	UDATA	_tenureBytes; /**< the number of bytes that were tenured */
	UDATA	_tilted; /**< flag to indicate whether we have a tilted scavenger */
	UDATA	_nurseryFreeBytes;/**< number of bytes free in the nursery */
	UDATA	_nurseryTotalBytes; /**< total number of bytes in the nursery */
	UDATA	_tenureFreeBytes; /**< number of bytes free in the tenure area */
	UDATA	_tenureTotalBytes; /**< total number of bytes in the tenure area */
	UDATA   _loaEnabled; /**< flag to indicate whether or not the LOA is enabled */
	UDATA	_tenureLOAFreeBytes; /**< number of bytes free in the LOA */
	UDATA	_tenureLOATotalBytes; /**< total number of bytes in the LOA */
	UDATA	_tenureAge; /**< the age at which objects currently get tenured */
	UDATA	_totalMemorySize; /**< the total size of all memory spaces */

	/* Java Fields  */
	MM_GCExtensions * _extensions; /**<  VM State. Used to populate java fields. */
	UDATA	_finalizerCount; /**< number of finalizers */
	UDATA	_weakReferenceClearCount; /**< number of weak references cleared */
	UDATA	_softReferenceClearCount; /**< number of soft references cleared */
	UDATA	_dynamicSoftReferenceThreshold; /**< the dynamic soft reference threshold */
	UDATA	_softReferenceThreshold; /**< soft reference threshold */
	UDATA	_phantomReferenceClearCount; /**< number of phantom references cleared */
	
	/* Consumed Data */
	U_64	_localGCStartTime;
	
public:

	static MM_VerboseEvent *newInstance(MM_LocalGCEndEvent *event, J9HookInterface** hookInterface);

	virtual void consumeEvents();
	virtual void formattedOutput(MM_VerboseOutputAgent *agent);

	MMINLINE virtual bool definesOutputRoutine() { return true; };
	MMINLINE virtual bool endsEventChain() { return false; };
		
	MM_VerboseEventLocalGCEnd(MM_LocalGCEndEvent *event, J9HookInterface** hookInterface) :
	MM_VerboseEvent(event->currentThread, event->timestamp, event->eventid, hookInterface),
	_globalGCCount(event->globalGCCount),
	_localGCCount(event->localGCCount),
	_rememberedSetOverflowed(event->rememberedSetOverflowed),
	_causedRememberedSetOverflow(event->causedRememberedSetOverflow),
	_scanCacheOverflow(event->scanCacheOverflow),
	_failedFlipCount(event->failedFlipCount),
	_failedFlipBytes(event->failedFlipBytes),
	_failedTenureCount(event->failedTenureCount),
	_failedTenureBytes(event->failedTenureBytes),
	_backout(event->backout),
	_flipCount(event->flipCount),
	_flipBytes(event->flipBytes),
	_tenureCount(event->tenureCount),
	_tenureBytes(event->tenureBytes),
	_tilted(event->tilted),
	_nurseryFreeBytes(event->nurseryFreeBytes),
	_nurseryTotalBytes(event->nurseryTotalBytes),
	_tenureFreeBytes(event->tenureFreeBytes),
	_tenureTotalBytes(event->tenureTotalBytes),
	_loaEnabled(event->loaEnabled),
	_tenureLOAFreeBytes(event->tenureLOAFreeBytes),
	_tenureLOATotalBytes(event->tenureLOATotalBytes),
	_tenureAge(event->tenureAge),
	_totalMemorySize(event->totalMemorySize),
	/* Java Properties */
	_extensions(MM_GCExtensions::getExtensions(event->currentThread)),
	_finalizerCount(_extensions->scavengerJavaStats._unfinalizedEnqueued),
	_weakReferenceClearCount(_extensions->scavengerJavaStats._weakReferenceStats._cleared),
	_softReferenceClearCount(_extensions->scavengerJavaStats._softReferenceStats._cleared),
	_dynamicSoftReferenceThreshold(_extensions->getDynamicMaxSoftReferenceAge()),
	_softReferenceThreshold(_extensions->getMaxSoftReferenceAge()),
	_phantomReferenceClearCount(_extensions->scavengerJavaStats._phantomReferenceStats._cleared)
	{};
};

#endif /* J9VM_GC_MODRON_SCAVENGER */
#endif /* EVENT_LOCAL_GC_END_HPP_ */

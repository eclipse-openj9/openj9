
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

#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "modronopt.h"

#if defined(J9VM_PROF_EVENT_REPORTING)

#include "mmhook_internal.h"
#include "mmomrhook_internal.h"

#include "EnvironmentBase.hpp"
#include "HeapMap.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "ObjectHeapBufferedIterator.hpp"
#include "ObjectModel.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionManager.hpp"
#include "ScavengerForwardedHeader.hpp"

void
globalGCReportObjectEvents(MM_EnvironmentBase *env, MM_HeapMap *markMap)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	MM_HeapRegionManager *regionManager = extensions->heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;
	OMR_VMThread *vmThread = env->getOmrVMThread();
	while((region = regionIterator.nextRegion()) != NULL) {
		/* Iterate over live objects only */
		MM_MemorySubSpace *memorySubSpace = region->getSubSpace();

		GC_ObjectHeapBufferedIterator objectHeapIterator(extensions, region);

		J9Object *objectPtr = NULL;
		while( NULL != (objectPtr = objectHeapIterator.nextObject()) ) {
			/* Check the mark state of each object. If it isn't marked, build a dead object. */
			if(!markMap->isBitSet(objectPtr)) {

				UDATA deadObjectByteSize = extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
				memorySubSpace->abandonHeapChunk(objectPtr, ((U_8*)objectPtr) + deadObjectByteSize);

				TRIGGER_J9HOOK_MM_OMR_OBJECT_DELETE(env->getExtensions()->omrHookInterface, vmThread, objectPtr, memorySubSpace);
			}
		}
	}
}

void
localGCReportObjectEvents(MM_EnvironmentBase *env, MM_MemorySubSpaceSemiSpace *memorySubSpaceNew)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	OMR_VMThread *vmThread = env->getOmrVMThread();

	/* Find the region associated with the evacuate allocate profile */
	GC_MemorySubSpaceRegionIterator regionIterator(memorySubSpaceNew);
	MM_HeapRegionDescriptor *evacuateRegion = NULL;
	while ((evacuateRegion = regionIterator.nextRegion()) != NULL) {
		J9Object *objectPtr = (J9Object *)evacuateRegion->getLowAddress();
		/* skip survivor regions */
		if (memorySubSpaceNew->isObjectInEvacuateMemory(objectPtr)) {
			MM_MemorySubSpace *evacuateMemorySubSpace = evacuateRegion->getSubSpace();
			/* Use the object model helper to test for holes,
			 * otherwise use ScavengerForwardedHeader to test for forwarded objects.
			 */
			while(objectPtr < (J9Object *)evacuateRegion->getHighAddress()) {
				if (extensions->objectModel.isDeadObject(objectPtr)) {
					objectPtr = (J9Object *)((U_8 *)objectPtr + extensions->objectModel.getSizeInBytesDeadObject(objectPtr));
				} else {
					MM_ScavengerForwardedHeader forwardHeader(objectPtr);
					if (forwardHeader.isForwardedPointer()) {
						J9Object *forwardPtr = forwardHeader.getForwardedObject();
						Assert_MM_true(NULL != forwardPtr);
						TRIGGER_J9HOOK_MM_OMR_OBJECT_RENAME(env->getExtensions()->omrHookInterface, vmThread, objectPtr, forwardPtr);
						objectPtr = (J9Object *)((U_8 *)objectPtr + extensions->objectModel.getConsumedSizeInBytesWithHeaderBeforeMove(forwardPtr));
					} else {
						TRIGGER_J9HOOK_MM_OMR_OBJECT_DELETE(env->getExtensions()->omrHookInterface, vmThread, objectPtr, evacuateMemorySubSpace);
						objectPtr = (J9Object *)((U_8 *)objectPtr + extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr));
					}
				}
			}
		}
	}
}

#endif /* J9VM_PROF_EVENT_REPORTING */

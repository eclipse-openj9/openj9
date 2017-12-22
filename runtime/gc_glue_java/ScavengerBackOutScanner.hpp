
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef SCAVENGERBACKOUTSCANNER_HPP_
#define SCAVENGERBACKOUTSCANNER_HPP_

#include "j9cfg.h"
#include "ModronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)
#include "EnvironmentStandard.hpp"
#include "RootScanner.hpp"
#include "Scavenger.hpp"

/**
 * The backout slot scanner for MM_Scavenger.
 * @copydoc MM_RootScanner
 * @ingroup GC_Modron_Standard
 */
class MM_ScavengerBackOutScanner : public MM_RootScanner
{
private:
	MM_Scavenger *_scavenger;

#if defined(J9VM_GC_FINALIZATION)
	void backoutUnfinalizedObjects(MM_EnvironmentStandard *env);
	void backoutFinalizableObjects(MM_EnvironmentStandard *env);
#endif

public:
	MM_ScavengerBackOutScanner(MM_EnvironmentBase *env, bool singleThread, MM_Scavenger *scavenger)
		: MM_RootScanner(env, singleThread)
		, _scavenger(scavenger)
	{
		_typeId = __FUNCTION__;
		setNurseryReferencesPossibly(true);
	}

	virtual void scanAllSlots(MM_EnvironmentBase *env);

	virtual void
	doSlot(omrobjectptr_t *slotPtr) {
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		// todo: create MM_ConcurrentScavengerBackOutScanner
		if (_extensions->concurrentScavenger) {
			_scavenger->fixupSlotWithoutCompression(slotPtr);
		} else 
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
		{		
			_scavenger->backOutFixSlotWithoutCompression(slotPtr);
		}
	}

	virtual void
	doClass(J9Class *clazz)
	{
		/* we do not process classes in the scavenger */
		assume0(0);
	}

	/* Do nothing since the lists were set to NULL already */
	virtual void scanWeakReferenceObjects(MM_EnvironmentBase *env) {}

	/* Do nothing since the lists were set to NULL already */
	virtual void scanSoftReferenceObjects(MM_EnvironmentBase *env) {}

	/* Do nothing since the lists were set to NULL already */
	virtual void scanPhantomReferenceObjects(MM_EnvironmentBase *env) {}

#if defined(J9VM_GC_FINALIZATION)
	virtual void
	scanUnfinalizedObjects(MM_EnvironmentBase *env)
	{
		/* allow the scavenger to handle this */
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
		backoutUnfinalizedObjects(MM_EnvironmentStandard::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}
#endif /* J9VM_GC_FINALIZATION */

#if defined(J9VM_GC_FINALIZATION)
	virtual void
	doFinalizableObject(omrobjectptr_t object)
	{
		Assert_MM_unreachable();
	}

	virtual void
	scanFinalizableObjects(MM_EnvironmentBase *env)
	{
		/* allow the scavenger to handle this */
		reportScanningStarted(RootScannerEntity_FinalizableObjects);
		backoutFinalizableObjects(MM_EnvironmentStandard::getEnvironment(env));
		reportScanningEnded(RootScannerEntity_FinalizableObjects);
	}
#endif /* J9VM_GC_FINALIZATION */

	/* empty, move ownable synchronizer backout processing in scanAllSlots() */
	virtual void scanOwnableSynchronizerObjects(MM_EnvironmentBase *env) {}
};
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

#endif /* SCAVENGERBACKOUTSCANNER_HPP_ */

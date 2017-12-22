
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

#ifndef FIXUPROOTS_HPP_
#define FIXUPROOTS_HPP_

#include "CollectorLanguageInterfaceImpl.hpp"
#include "CompactScheme.hpp"
#include "CompactSchemeFixupObject.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentStandard.hpp"
#include "RootScanner.hpp"

class MM_CompactSchemeFixupRoots : public MM_RootScanner {
private:
	MM_CompactScheme* _compactScheme;

public:
	MM_CompactSchemeFixupRoots(MM_EnvironmentBase *env, MM_CompactScheme* compactScheme) :
		MM_RootScanner(env),
		_compactScheme(compactScheme)
	{
		setIncludeStackFrameClassReferences(false);
	}

	virtual void doSlot(omrobjectptr_t* slot)
	{
		*slot = _compactScheme->getForwardingPtr(*slot);
	}

	virtual void doClass(J9Class *clazz)
	{
		/* We MUST NOT store the forwarded class object back into the clazz since forwarding can
		 * only happen once and will be taken care of by the code below. ie: the classObject slot
		 * is part of the GC_ClassIterator.
		 */
		GC_ClassIterator classIterator(_env, clazz);
		while (volatile omrobjectptr_t *slot = classIterator.nextSlot()) {
			/* discard volatile since we must be in stop-the-world mode */
			doSlot((omrobjectptr_t*)slot);
		}
	}

	virtual void doClassLoader(J9ClassLoader *classLoader)
	{
		if (J9_GC_CLASS_LOADER_DEAD != (classLoader->gcFlags & J9_GC_CLASS_LOADER_DEAD)) {
			doSlot(&classLoader->classLoaderObject);
			scanModularityObjects(classLoader);
		}
	}

#if defined(J9VM_GC_FINALIZATION)
	virtual void scanUnfinalizedObjects(MM_EnvironmentBase *env) {
		/* allow the compact scheme to handle this */
		reportScanningStarted(RootScannerEntity_UnfinalizedObjects);
		MM_CompactSchemeFixupObject fixupObject(env, _compactScheme);
		fixupUnfinalizedObjects(env);
		reportScanningEnded(RootScannerEntity_UnfinalizedObjects);
	}
#endif

#if defined(J9VM_GC_FINALIZATION)
	virtual void doFinalizableObject(omrobjectptr_t object) {
		Assert_MM_unreachable();
	}

	virtual void scanFinalizableObjects(MM_EnvironmentBase *env) {
		if (_singleThread || J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			reportScanningStarted(RootScannerEntity_FinalizableObjects);
			MM_CompactSchemeFixupObject fixupObject(env, _compactScheme);
			fixupFinalizableObjects(env);
			reportScanningEnded(RootScannerEntity_FinalizableObjects);
		}
	}
#endif /* J9VM_GC_FINALIZATION */

	virtual void scanOwnableSynchronizerObjects(MM_EnvironmentBase *env) {
		/* empty, move ownable synchronizer processing in fixupObject */
	}

private:
#if defined(J9VM_GC_FINALIZATION)
	void fixupFinalizableObjects(MM_EnvironmentBase *env);
	void fixupUnfinalizedObjects(MM_EnvironmentBase *env);
#endif
};
#endif /* FIXUPROOTS_HPP_ */

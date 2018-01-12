
/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#ifndef CHECKMARKROOTS_HPP_
#define CHECKMARKROOTS_HPP_

#include "RootScanner.hpp"

class MM_CompactSchemeCheckMarkRoots : public MM_RootScanner {
private:
public:
	MM_CompactSchemeCheckMarkRoots(MM_EnvironmentStandard *env)
		: MM_RootScanner(env, true)
	{}

	virtual void doSlot(omrobjectptr_t* slot)
	{}

	virtual void doClass(J9Class *clazz)
	{
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
	virtual void doFinalizableObject(omrobjectptr_t object) {
		omrobjectptr_t objectPtr = object;
		doSlot(&objectPtr);
	}
#endif /* J9VM_GC_FINALIZATION */
};

#endif /* CHECKMARKROOTS_HPP_ */

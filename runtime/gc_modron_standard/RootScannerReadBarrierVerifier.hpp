/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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


#if !defined(RootScannerReadBarrierVerifier_HPP_)
#define RootScannerReadBarrierVerifier_HPP_

#include "j9.h"
#include "RootScanner.hpp"

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)

class MM_RootScannerReadBarrierVerifier : public MM_RootScanner
{
	private:
		bool _poison;
	protected:

	public:
		MM_RootScannerReadBarrierVerifier(MM_EnvironmentBase *env, bool singleThread = false, bool poison = false) : 
			MM_RootScanner(env, singleThread)
		{
			_poison = poison; /* Toggle between poisoning and healing */
		}

		virtual void doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator);
		virtual void doJNIWeakGlobalReference(omrobjectptr_t *slotPtr);
		
		virtual void scanConstantPoolObjectSlots(MM_EnvironmentBase *env);
		virtual void doConstantPoolObjectSlots(omrobjectptr_t *slotPtr);

		virtual void scanClassStaticSlots(MM_EnvironmentBase *env);
		virtual void doClassStaticSlots(omrobjectptr_t *slotPtr);

		virtual void doSlot(J9Object** slotPtr) {};
		virtual void doClass(J9Class *clazz) {};
		virtual void doFinalizableObject(J9Object *objectPtr) {};

};

#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

#endif /* RootScannerReadBarrierVerifier_HPP_ */


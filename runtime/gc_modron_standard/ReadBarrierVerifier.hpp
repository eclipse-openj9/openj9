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


/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(ReadBarrierVerifier_HPP_)
#define ReadBarrierVerifier_HPP_


#include "j9.h"
#include "j9cfg.h"

#include "StandardAccessBarrier.hpp"

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)

/**
 * Access barrier for Modron collector.
 */
 
 class MM_ReadBarrierVerifier : public MM_StandardAccessBarrier
{

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
public:
	static MM_ReadBarrierVerifier *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	MM_ReadBarrierVerifier(MM_EnvironmentBase *env) :
		MM_StandardAccessBarrier(env)
	{
		_typeId = __FUNCTION__;
	}

	virtual bool preObjectRead(J9VMThread *vmThread, J9Object *srcObject, fj9object_t *srcAddress);
	virtual bool preObjectRead(J9VMThread *vmThread, J9Class *srcClass, j9object_t *srcAddress);
	virtual bool preMonitorTableSlotRead(J9VMThread *vmThread, j9object_t *srcAddress);
	virtual bool preMonitorTableSlotRead(J9JavaVM *vm, j9object_t *srcAddress);

	void poisonSlot(MM_GCExtensionsBase *extensions, omrobjectptr_t *slot);
	void poisonJniWeakReferenceSlots(MM_EnvironmentBase *env);
	void poisonMonitorReferenceSlots(MM_EnvironmentBase *env);
	void poisonStaticClassSlots(MM_EnvironmentBase *env);
	void poisonConstantPoolObjects(MM_EnvironmentBase *env);
	virtual void poisonSlots(MM_EnvironmentBase *env);

	void healSlot(MM_GCExtensionsBase *extensions, fomrobject_t *srcAddress);
	void healSlot(MM_GCExtensionsBase *extensions, omrobjectptr_t *slot);
	void healJniWeakReferenceSlots(MM_EnvironmentBase *env);
	void healMonitorReferenceSlots(MM_EnvironmentBase *env);
	void healStaticClassSlots(MM_EnvironmentBase *env);
	void healConstantPoolObjects(MM_EnvironmentBase *env);
	virtual void healSlots(MM_EnvironmentBase *env);


};
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

#endif /* ReadBarrierVerifier_HPP_ */


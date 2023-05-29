
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(MARKINGSCHEMEROOTCLEARER_HPP_)
#define MARKINGSCHEMEROOTCLEARER_HPP_

#include "j9.h"

#include "ModronTypes.hpp"
#include "RootScanner.hpp"

class MM_EnvironmentBase;
class MM_MarkingScheme;
class MM_MarkingDelegate;
class MM_ReferenceStats;

class MM_MarkingSchemeRootClearer : public MM_RootScanner
{
/* Data members & types */
public:
protected:
private:
	MM_MarkingScheme *_markingScheme;
	MM_MarkingDelegate *_markingDelegate;

/* Methods */
public:
	MM_MarkingSchemeRootClearer(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme, MM_MarkingDelegate *markingDelegate) :
		  MM_RootScanner(env)
		, _markingScheme(markingScheme)
		, _markingDelegate(markingDelegate)
	{
		_typeId = __FUNCTION__;
	};

	virtual void doSlot(omrobjectptr_t *slotPtr);
	virtual void doClass(J9Class *clazz);
	virtual void scanWeakReferenceObjects(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanWeakReferencesComplete(MM_EnvironmentBase *env);
	virtual void scanSoftReferenceObjects(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanSoftReferencesComplete(MM_EnvironmentBase *env);
	virtual void scanPhantomReferenceObjects(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanPhantomReferencesComplete(MM_EnvironmentBase *env);
	virtual void scanUnfinalizedObjects(MM_EnvironmentBase *env);
	virtual CompletePhaseCode scanUnfinalizedObjectsComplete(MM_EnvironmentBase *env);
	virtual void scanOwnableSynchronizerObjects(MM_EnvironmentBase *env);
	virtual void scanContinuationObjects(MM_EnvironmentBase *env);
	virtual void iterateAllContinuationObjects(MM_EnvironmentBase *env);
	virtual void doMonitorReference(J9ObjectMonitor *objectMonitor, GC_HashTableIterator *monitorReferenceIterator);
	virtual CompletePhaseCode scanMonitorReferencesComplete(MM_EnvironmentBase *envBase);
	virtual void doJNIWeakGlobalReference(omrobjectptr_t *slotPtr);
	virtual void doRememberedSetSlot(omrobjectptr_t *slotPtr, GC_RememberedSetSlotIterator *rememberedSetSlotIterator);
	virtual void doStringTableSlot(omrobjectptr_t *slotPtr, GC_StringTableIterator *stringTableIterator);
	virtual void doStringCacheTableSlot(omrobjectptr_t *slotPtr);
	virtual void doJVMTIObjectTagSlot(omrobjectptr_t *slotPtr, GC_JVMTIObjectTagTableIterator *objectTagTableIterator);
	virtual void doFinalizableObject(omrobjectptr_t object);

protected:
private:
};

#endif /* MARKINGSCHEMEROOTCLEARER_HPP_ */

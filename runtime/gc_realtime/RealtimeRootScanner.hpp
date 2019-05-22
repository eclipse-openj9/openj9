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
 * @ingroup GC_Metronome
 */

#if !defined(REALTIMEROOTSCANNER_HPP_)
#define REALTIMEROOTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "EnvironmentRealtime.hpp"
#include "GlobalCollector.hpp"
#include "GCExtensions.hpp"
#include "Scheduler.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceMetronome.hpp"
#include "MemoryPoolSegregated.hpp"
#include "ObjectAllocationInterface.hpp"
#include "RootScanner.hpp"
#include "RealtimeGC.hpp"
#include "RealtimeMarkingScheme.hpp"
#include "StackSlotValidator.hpp"

class MM_Dispatcher;
class MM_MemorySubSpace;
class MM_MemorySubSpaceMetronome;

class MM_RealtimeRootScanner : public MM_RootScanner
{
	/*
	 * Data members
	 */
private:
protected:
	MM_RealtimeGC *_realtimeGC;
	MM_RealtimeMarkingScheme *_markingScheme;
	MM_EnvironmentRealtime *_env;

public:
	UDATA _threadCount;
	I_32 _yieldCount;

	/*
	 * Function members
	 */
private:
protected:
public:
	virtual void doClass(J9Class* clazz);

#if defined(J9VM_GC_REALTIME) 
	void doStringTableSlot(J9Object **slotPtr, GC_StringTableIterator *stringTableIterator);
	virtual void doStringCacheTableSlot(J9Object **slotPtr);
#endif /* J9VM_GC_REALTIME */

	virtual void scanThreads(MM_EnvironmentBase *env);
	virtual bool scanOneThread(MM_EnvironmentBase *env, J9VMThread* walkThread, void* localData);
	virtual void scanOneThreadImpl(MM_EnvironmentRealtime *env, J9VMThread* walkThread, void* localData);
	void reportThreadCount(MM_EnvironmentBase *env);
	void scanAtomicRoots(MM_EnvironmentRealtime *env);

	/* The yield family of methods override the RootScanner to do actual yielding.
	 * Their implementations basically use the scheduler API */
	virtual bool shouldYieldFromClassScan(UDATA timeSlackNanoSec = 0);
	virtual bool shouldYieldFromStringScan();
	virtual bool shouldYieldFromMonitorScan();
	virtual bool shouldYield();
	virtual void yield();
	virtual bool condYield(U_64 timeSlackNanoSec = 0);

	CompletePhaseCode scanClassesComplete(MM_EnvironmentBase *env);

	virtual const char* scannerName() = 0;
	
	virtual void doClassSlot(J9Class **clazzPtr);

	virtual void scanMonitorLookupCaches(MM_EnvironmentBase *env);

	virtual void scanStringTable(MM_EnvironmentBase *env);

	MM_RealtimeRootScanner(MM_EnvironmentRealtime *env, MM_RealtimeGC *realtimeGC)
		: MM_RootScanner(env)
		, _realtimeGC(realtimeGC)
		, _markingScheme(realtimeGC->getMarkingScheme())
		, _env(env)
		, _threadCount(0)
		, _yieldCount(0)
	{
		_typeId = __FUNCTION__;
#if defined(J9VM_GC_REALTIME) 
		/* String table should be clearable, not a hard root */
		setStringTableAsRoot(false);
#endif /* defined(J9VM_GC_REALTIME) */
	}
};

#endif /* REALTIMEROOTSCANNER_HPP_ */


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

#if !defined(ENVIRONMENTREALTIME_HPP_)
#define ENVIRONMENTREALTIME_HPP_

#include "Base.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "OSInterface.hpp"
#include "Scheduler.hpp"

class MM_AllocationContextRealtime;
class MM_HeapRegionDescriptorRealtime;
class MM_RealtimeRootScanner;
class MM_Timer;

class MM_EnvironmentRealtime : public MM_EnvironmentBase
{
/* Data section */
public:
	volatile U_32 _monitorCacheCleared; /**< Flag field to indicate whether the monitor lookup cache has been cleared on the thread */

protected:
private:
	MM_Scheduler *_scheduler;
	MM_RealtimeRootScanner *_rootScanner;
	
	MM_OSInterface *_osInterface;
	
	I_32 _yieldDisableDepth;

	uintptr_t _scannedBytes;         /**< Number of bytes, objects, pointer fields scanned in current major GC */
	uintptr_t _scannedObjects;
	uintptr_t _scannedPointerFields;
	
	MM_HeapRegionDescriptorRealtime **_overflowCache; /**< Local cache of overflowed regions.  Can only be manipulated by IncrementalOverflow */
	uintptr_t _overflowCacheCount; /**< Count of used elements in the _overflowCache array. Can on be manipulated by IncrementalOverflow */
	MM_Timer *_timer;
	
	U_32 _distanceToYieldTimeCheck; /**< Number of condYield that can be skipped before actual checking for yield, when the quanta time has been relaxed */
	U_32 _currentDistanceToYieldTimeCheck; /**< The current remaining number of condYield calls to be skipped before the next actual yield check */

/* Functionality Section */
	
public:
	static MM_EnvironmentRealtime *newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread);
	virtual void kill();

	MMINLINE static MM_EnvironmentRealtime *getEnvironment(OMR_VMThread *omrVMThread) { return static_cast<MM_EnvironmentRealtime*>(omrVMThread->_gcOmrVMThreadExtensions); }
	MMINLINE static MM_EnvironmentRealtime *getEnvironment(MM_EnvironmentBase *ptr) { return (MM_EnvironmentRealtime *)ptr; }
	
	MM_Scheduler *getScheduler() const { return _scheduler; }
	
	MM_AllocationContextRealtime *getAllocationContext() const { return (MM_AllocationContextRealtime *)MM_EnvironmentBase::getAllocationContext(); }
	void setAllocationContext(MM_AllocationContextRealtime *allocContext) { MM_EnvironmentBase::setAllocationContext((MM_AllocationContext *)allocContext); }

	U_32 getMonitorCacheCleared() const { return _monitorCacheCleared; }
	void setMonitorCacheCleared(U_32 monitorCacheCleared) { _monitorCacheCleared = monitorCacheCleared; }
	
	/* Maintains a recursive count of _not_ being able to yield.  
	 * Initially the depth is zero indicating that it can yield.
	 */
	void disableYield();  
	void enableYield();
	I_32 getYieldDisableDepth() const { return _yieldDisableDepth; }

	MMINLINE U_32 getCurrentDistanceToYieldTimeCheck() { return _currentDistanceToYieldTimeCheck; }
	MMINLINE void resetCurrentDistanceToYieldTimeCheck()
	{
		if(0 != _distanceToYieldTimeCheck) {
			_currentDistanceToYieldTimeCheck = _distanceToYieldTimeCheck;
		}
	}
	MMINLINE bool hasDistanceToYieldTimeCheck()
	{
		bool shouldSkipTimeCheck = (0 != _currentDistanceToYieldTimeCheck);
		if (shouldSkipTimeCheck) {
			--_currentDistanceToYieldTimeCheck;
		}
		return shouldSkipTimeCheck;
	}
	
	void reportScanningSuspended();
	void reportScanningResumed();
	
	void setRootScanner(MM_RealtimeRootScanner *rootScanner) { _rootScanner = rootScanner; }
	
	uintptr_t getScannedBytes() const { return _scannedBytes; }
	void addScannedBytes(uintptr_t scannedBytes) { _scannedBytes += scannedBytes; }
	uintptr_t getScannedObjects() const { return _scannedObjects; }
	void incScannedObjects() { _scannedObjects++; }
	uintptr_t getScannedPointerFields() const { return _scannedPointerFields; }
	void addScannedPointerFields(uintptr_t scannedPointerFields) { _scannedPointerFields += scannedPointerFields; }
	void resetScannedCounters() 
	{
		_scannedBytes = 0;
		_scannedObjects = 0;
		_scannedPointerFields = 0;
	}
	MM_Timer *getTimer() {return _timer;}
	
	MMINLINE uintptr_t getOverflowCacheUsedCount() {return _overflowCacheCount;}
	MMINLINE void incrementOverflowCacheUsedCount() {_overflowCacheCount += 1;}
	MMINLINE void resetOverflowCacheUsedCount() {_overflowCacheCount = 0;}
	MMINLINE MM_HeapRegionDescriptorRealtime **getOverflowCache() {return _overflowCache;}
	
	MM_EnvironmentRealtime(OMR_VMThread *omrVMThread) :
		MM_EnvironmentBase(omrVMThread),
		_scheduler((MM_Scheduler *)MM_GCExtensionsBase::getExtensions(omrVMThread->_vm)->dispatcher),
		_rootScanner(NULL),
		_osInterface(_scheduler->_osInterface),
		_overflowCache(NULL),
		_overflowCacheCount(0),
		_timer(NULL),
		_distanceToYieldTimeCheck(0),
		_currentDistanceToYieldTimeCheck(0)
	{ 
		_typeId = __FUNCTION__;
	}
	
	MM_EnvironmentRealtime(OMR_VM *vm) :
		MM_EnvironmentBase(vm),
		_scheduler((MM_Scheduler *)MM_GCExtensionsBase::getExtensions(vm)->dispatcher),
		_rootScanner(NULL),
		_osInterface(_scheduler->_osInterface),
		_overflowCache(NULL),
		_overflowCacheCount(0),
		_timer(NULL),
		_distanceToYieldTimeCheck(0),
		_currentDistanceToYieldTimeCheck(0)
	{ 
		_typeId = __FUNCTION__;
	}
	
protected:
	virtual bool initialize(MM_GCExtensionsBase *extensions);
	virtual void tearDown(MM_GCExtensionsBase *extensions);
	
private:

};

#endif /* ENVIRONMENTREALTIME_HPP_ */


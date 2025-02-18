
/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(CONTINUATIONSLOTITERATOR_HPP_)
#define CONTINUATIONSLOTITERATOR_HPP_
#if JAVA_SPEC_VERSION >= 24

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

/**
 * Iterate over monitor records slots and vthread slot in a J9VMContinuation.
 * Used by ScanContinuationNativeSlots() (JAVA_SPEC_VERSION >= 24 only).
 * @ingroup GC_Structs
 */
class GC_ContinuationSlotIterator
{
public:
	/**
	 * State constants representing the current stage of the iteration process
	 */
	enum State {
		state_start = 0,
		state_monitor_records,
		state_vthread,
		state_end
	};

protected:
	J9VMThread *_vmThread;
	State _state;

	j9object_t *_vthread;
	J9MonitorEnterRecord *_monitorRecord;
	J9MonitorEnterRecord *_jniMonitorRecord;

public:
	GC_ContinuationSlotIterator(J9VMThread *vmThread, J9VMContinuation *continuation)
		: _vmThread(vmThread)
		, _state(state_start)
		, _vthread(&continuation->vthread)
		, _monitorRecord(continuation->monitorEnterRecords)
		, _jniMonitorRecord(continuation->jniMonitorEnterRecords)
	{};

	/**
	 * @return @ref ContinuationSlotIteratorState representing the current state (stage
	 * of the iteration process)
	 */
	MMINLINE int
	getState()
	{
		return _state;
	}

	/**
	 * Get the J9VMThread * for the thread being iterated
	 * @return _vmThread - the J9VMThread being iterated.
	 */
	MMINLINE J9VMThread *
	getVMThread()
	{
		return _vmThread;
	}

	j9object_t *nextSlot();
};
#endif /* JAVA_SPEC_VERSION >= 24 */

#endif /* CONTINUATIONSLOTITERATOR_HPP_ */

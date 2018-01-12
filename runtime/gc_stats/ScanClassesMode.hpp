
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


#if !defined(SCANCLASSESMODE_HPP_)
#define SCANCLASSESMODE_HPP_

#include "j9cfg.h"
#include "j9comp.h"
#include "omrgcconsts.h"
#include "AtomicOperations.hpp"

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)

/**
 * Concurrent status symbols extending ConcurrentKickoffReason enumeration.
 * Explain why kickoff triggered by Java.
 */
enum {
	FORCED_UNLOADING_CLASSES = (uintptr_t)((uintptr_t)NO_LANGUAGE_KICKOFF_REASON + 1)
};

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_ScanClassesMode : public MM_Base
{
private:
	volatile uintptr_t _scanClassesMode;

public:
	typedef enum {
		SCAN_CLASSES_NEED_TO_BE_EXECUTED = 1,
		SCAN_CLASSES_CURRENTLY_ACTIVE,
		SCAN_CLASSES_COMPLETE,
		SCAN_CLASSES_DISABLED
	} ScanClassesMode;

public:
	MMINLINE ScanClassesMode getScanClassesMode() { return (ScanClassesMode)_scanClassesMode; }

	MMINLINE bool
	switchScanClassesMode(ScanClassesMode oldMode, ScanClassesMode newMode)
	{
		uintptr_t result = MM_AtomicOperations::lockCompareExchange(&_scanClassesMode, (uintptr_t)oldMode, (uintptr_t)newMode);
		return result == (uintptr_t)oldMode;
		}
	
	MMINLINE void
	setScanClassesMode(ScanClassesMode mode)
	{
		MM_AtomicOperations::set((uintptr_t *)&_scanClassesMode, (uintptr_t)mode);
	}
	
	MMINLINE bool
	isPendingOrActiveMode()
	{
		uintptr_t mode = _scanClassesMode;
		return (mode == SCAN_CLASSES_NEED_TO_BE_EXECUTED) || (mode == SCAN_CLASSES_CURRENTLY_ACTIVE);
	}

	MMINLINE const char *
	getScanClassesModeAsString()
	{
		switch (getScanClassesMode()) {
		case SCAN_CLASSES_NEED_TO_BE_EXECUTED:
			return "pending";

		case SCAN_CLASSES_CURRENTLY_ACTIVE:
			return "active";

		case SCAN_CLASSES_COMPLETE:
			return "complete";

		case SCAN_CLASSES_DISABLED:
			return "disabled";

		default:
			return "unknown";
		}
	}
	
	/**
	 * Create a ScanClassesMode object.
	 */
	MM_ScanClassesMode() :
		MM_Base(),
		_scanClassesMode((uintptr_t)SCAN_CLASSES_DISABLED)
	{}

};

#endif	/* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

#endif /* SCANCLASSESMODE_HPP_ */

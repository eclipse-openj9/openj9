/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
 * @ingroup GC_Base
 */

#if !defined(HOTFIELDUTIL_HPP_)
#define HOTFIELDUTIL_HPP_

#include "j9.h"
#include "j9cfg.h"

#if defined(J9VM_GC_MODRON_SCAVENGER) || defined(J9VM_GC_VLHGC)

#include "BaseNonVirtual.hpp"
#include "GCExtensions.hpp"


/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_HotFieldUtil : public MM_BaseNonVirtual
{
public:
    /**
	 * Sort all hot fields for all classes.
	 * Used when scavenger dynamicBreadthFirstScanOrdering is enabled
	 * 
	 * @param javaVM[in] pointer to the J9JavaVM
	 * @param gcCount[in] current PGC/scavenge count
	 */
	static void sortAllHotFieldData(J9JavaVM *javaVM, uintptr_t gcCount);

	/**
	 * Sort all hot fields for a single class.
	 * Used when scavenger dynamicBreadthFirstScanOrdering is enabled
	 * 
	 * @param javaVM[in] pointer to the J9JavaVM
	 * @param hotFieldClassInfo[in] the hot field information of the class that will be sorted
	 */
	MMINLINE static void sortClassHotFieldList(J9JavaVM *javaVM, J9ClassHotFieldsInfo* hotFieldClassInfo);

	/**
	 * Reset all hot fields for all classes.
	 * Used when scavenger dynamicBreadthFirstScanOrdering is enabled and hotFieldResettingEnabled is true.
	 *
	 * @param javaVM[in] pointer to the J9JavaVM
	 */
	MMINLINE static void resetAllHotFieldData(J9JavaVM *javaVM);
};

#endif /* J9VM_GC_MODRON_SCAVENGER || J9VM_GC_VLHGC */
#endif /* HOTFIELDUTIL_HPP_ */

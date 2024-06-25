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
#if !defined(CONTINUATIONSTATS_HPP_)
#define CONTINUATIONSTATS_HPP_
#include "j9port.h"
#include "modronopt.h"

#include "Base.hpp"
#include "AtomicOperations.hpp"

/**
 * Storage for statistics relevant to the continuation Objects
 * @ingroup GC_Stats
 */
class MM_ContinuationStats : public MM_Base {
private:
protected:
public:
	uintptr_t _total;	/**< number of alive continuation objects on heap at the end of cycle */
	uintptr_t _started;	/**< number of continuation objects has started at the end of cycle */

	/* function members */
private:
protected:
public:
	void clear()
	{
		_total = 0;
		_started = 0;
	}

	void merge(MM_ContinuationStats* statsToMerge)
	{
		_total += statsToMerge->_total;
		_started += statsToMerge->_started;
	}

	MM_ContinuationStats() :
		MM_Base()
		, _total(0)
		, _started(0)
	{
		clear();
	}
};
#endif /* CONTINUATIONSTATS_HPP_ */

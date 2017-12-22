
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(INCREMENTALCARDTABLE_HPP_)
#define INCREMENTALCARDTABLE_HPP_


#include "j9.h"
#include "j9cfg.h"

#include "CardTable.hpp"

class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @warn All card table functions assume EXCLUSIVE ranges, ie they take a base and top card where base card
 * is first card in range and top card is card immediately AFTER the end of the range.
 * @ingroup GC_Modron_Standard
 */
class MM_IncrementalCardTable : public MM_CardTable 
{
public:
protected:
private:
	UDATA _cardTableSize;	/**< The size, in bytes, of the card table (cached since it is dangerous to try to derive it during shutdown) */

public:
	static MM_IncrementalCardTable *newInstance(MM_EnvironmentBase *env, MM_Heap *heap);
protected:
	bool initialize(MM_EnvironmentBase *env, MM_Heap *heap);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	/**
	 * Create a CardTable object.
	 */
	MM_IncrementalCardTable()
		: MM_CardTable()
		, _cardTableSize(0)
	{
		_typeId = __FUNCTION__;
	}

private:
};

#endif /* INCREMENTALCARDTABLE_HPP_ */

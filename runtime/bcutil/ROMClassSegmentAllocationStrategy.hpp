/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

/*
 * ROMClassSegmentAllocationStrategy.hpp
 */

#ifndef ROMCLASSSEGMENTALLOCATIONSTRATEGY_HPP_
#define ROMCLASSSEGMENTALLOCATIONSTRATEGY_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"
#include "j9.h"

#include "AllocationStrategy.hpp"

class ROMClassSegmentAllocationStrategy : public AllocationStrategy
{
public:
	ROMClassSegmentAllocationStrategy(J9JavaVM* javaVM, J9ClassLoader* classLoader) :
		_javaVM(javaVM),
		_classLoader(classLoader),
		_segment(NULL),
		_bytesRequested(0)
	{
	}

	U_8* allocate(UDATA byteAmount);
	void updateFinalROMSize(UDATA finalSize);
	UDATA getSegmentSize() { return _segment->size;}

private:
	J9JavaVM* _javaVM;
	J9ClassLoader* _classLoader;
	J9MemorySegment* _segment;
	UDATA _bytesRequested;
};

#endif /* ROMCLASSSEGMENTALLOCATIONSTRATEGY_HPP_ */

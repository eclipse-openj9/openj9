
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

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "HeapRegionDataForCompactVLHGC.hpp"

MM_HeapRegionDataForCompactVLHGC::MM_HeapRegionDataForCompactVLHGC(MM_EnvironmentVLHGC *env)
	: MM_BaseNonVirtual()
	, _shouldCompact(false)
	, _shouldFixup(false)
	, _compactDestination(NULL)
	, _nextInWorkList(NULL)
	, _blockedList(NULL)
	, _nextEvacuationCandidate(NULL)
	, _nextRebuildCandidate(NULL)
	, _nextMoveEventCandidate(NULL)
	, _isCompactDestination(false)
	, _vineDepth(0)
	, _previousContext(NULL)
{
	_typeId = __FUNCTION__;
}

bool 
MM_HeapRegionDataForCompactVLHGC::initialize(MM_EnvironmentVLHGC *env, MM_HeapRegionManager *regionManager, MM_HeapRegionDescriptor *descriptor)
{
	return true;
}

void 
MM_HeapRegionDataForCompactVLHGC::tearDown(MM_EnvironmentVLHGC *env)
{
	
}

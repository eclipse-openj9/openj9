/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
 * @ingroup Shared_Common
 */

#include "Managers.hpp"
#include "ut_j9shr.h"
#include "j9shrnls.h"

SH_Managers*
SH_Managers::newInstance(J9JavaVM* vm, SH_Managers* memForConstructor)
{
	SH_Managers* retval = memForConstructor;
	new(retval) SH_Managers();
	retval->initialize();
	return retval;
}

UDATA
SH_Managers::getRequiredConstrBytes()
{
	UDATA retval = 0;
	retval += sizeof(SH_Managers);
	return retval;
}

SH_Manager*
SH_Managers::getManager(UDATA index)
{
	SH_Manager* retval = NULL;
	retval = _initializedManagers[index];
	return retval;
}

SH_Manager*
SH_Managers::addManager(SH_Manager* manager)
{
	SH_Manager* retval = NULL;
	UDATA index = _initializedManagersCntr;

	_initializedManagersCntr+=1;

	Trc_SHR_Assert_True(_initializedManagersCntr <= NUM_MANAGERS);

	_initializedManagers[index] = manager;
	retval = _initializedManagers[index];
	return retval;
}

SH_Manager*
SH_Managers::startDo(J9VMThread* vmthread, UDATA limitState, ManagerWalkState* walkState)
{
	SH_Manager* retval = NULL;
	walkState->index = 0;
	walkState->limitState = limitState;
	retval = nextDo(walkState);
	return retval;
}

SH_Manager*
SH_Managers::nextDo(ManagerWalkState* walkState)
{
	SH_Manager* returnVal;

	if (walkState->index == NUM_MANAGERS) {
		return NULL;
	}

	do {
		returnVal = _initializedManagers[walkState->index++];
		
		if (returnVal != NULL) {
			if ((walkState->limitState == 0) || (returnVal->getState() == walkState->limitState)) {
				return returnVal;
			}
		}
	} while (walkState->index < NUM_MANAGERS);

	return NULL;
}


SH_Manager*
SH_Managers::getManagerForDataType(UDATA dataType)
{
	SH_Manager* retval = NULL;
	UDATA i;
	/* TODO: Optimize */
	for (i=0; i<NUM_MANAGERS; i++) {
		if (_initializedManagers[i]->isDataTypeRepresended(dataType) == true) {
			retval = _initializedManagers[i];
			goto done;
		}
	}
	done:
	return retval;
}

void
SH_Managers::initialize()
{
	IDATA i;

	_initializedManagersCntr = 0;
	for (i = 0; i<NUM_MANAGERS; i++) {
		_initializedManagers[i] = 0;
	}
	return;
}


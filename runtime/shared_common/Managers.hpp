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

/**
 * @file
 * @ingroup Shared_Common
 */

#if !defined(MANAGERS_HPP_INCLUDED)
#define MANAGERS_HPP_INCLUDED

/* @ddr_namespace: default */
#include "Manager.hpp"
#include "j9.h"

#define NUM_MANAGERS 6

class SH_Managers
{
public:
	class ManagerWalkState
	{
	public :
		J9VMThread* currentThread;
		UDATA limitState;
		UDATA index;
	};

	static SH_Managers* newInstance(J9JavaVM* vm, SH_Managers* memForConstructor);
	static UDATA getRequiredConstrBytes();

	SH_Manager* getManager(UDATA index);
	SH_Manager* addManager(SH_Manager* manager);
	SH_Manager* startDo(J9VMThread* vmthread, UDATA limitState, ManagerWalkState* walkState);
	SH_Manager* nextDo(ManagerWalkState* walkState);
	SH_Manager* getManagerForDataType(UDATA dataType);


protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

private:
	SH_Manager* _initializedManagers[NUM_MANAGERS];
	UDATA _initializedManagersCntr;

	void initialize();
};

#endif /* !defined(MANAGERS_HPP_INCLUDED) */

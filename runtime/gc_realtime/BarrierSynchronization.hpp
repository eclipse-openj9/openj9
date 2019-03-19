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

#if !defined(BARRIERSYNCHRONIZATION_HPP_)
#define BARRIERSYNCHRONIZATION_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"

/**
 * This class provides thread synchronization between the GC thread and the mutator threads.
 * Its API is wide enough to support both the class Metronome gang scheduling as well as the
 * Staccato ragged barrier model.
 *
 */
class MM_BarrierSynchronization : public MM_BaseVirtual
{
/* Data members */
public:
protected:
private:
	J9JavaVM *_vm;                                 /**< Retained to access VM functions. */
	UDATA _vmResponsesRequiredForExclusiveVMAccess;  /**< Used to support the (request/wait)ExclusiveVMAccess methods. */
	UDATA _jniResponsesRequiredForExclusiveVMAccess;  /**< Used to support the (request/wait)ExclusiveVMAccess methods. */

/* Methods */
public:

	/* The usual initialization and teardown methods.
	 */
	static MM_BarrierSynchronization *newInstance(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
	MM_BarrierSynchronization(MM_EnvironmentBase *env) :
		_vm((J9JavaVM *)env->getOmrVM()->_language_vm),
		_vmResponsesRequiredForExclusiveVMAccess(0),
		_jniResponsesRequiredForExclusiveVMAccess(0)
	{
		_typeId = __FUNCTION__;
	}

	/*
	 * These functions are used by classic Metronome gang-scheduling and
	 * also transitionally used for the parts of Staccato that have not
	 * been made concurrent.
	 */
	void preRequestExclusiveVMAccess(OMR_VMThread *threadRequestingExclusive);
	void postRequestExclusiveVMAccess(OMR_VMThread *threadRequestingExclusive);
	UDATA requestExclusiveVMAccess(MM_EnvironmentBase *env, UDATA block, UDATA *gcPriority);
	void waitForExclusiveVMAccess(MM_EnvironmentBase *env, bool waitRequired);
	void acquireExclusiveVMAccess(MM_EnvironmentBase *env, bool waitRequired);
	void releaseExclusiveVMAccess(MM_EnvironmentBase *env, bool releaseRequired);

};

#endif /* BARRIERSYNCHRONIZATION_HPP_ */

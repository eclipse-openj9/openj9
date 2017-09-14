
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if !defined(STACCATOGC_HPP_)
#define STACCATOGC_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modronopt.h"

#include "RealtimeGC.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Staccato
 */
class MM_StaccatoGC : public MM_RealtimeGC
{
/* Data members & types */
public:
protected:
private:
	bool _moreTracingRequired; /**< Is used to decide if there needs to be another pass of the tracing loop. */

/* Methods */
public:
	/* Constructors & destructors */
	static MM_StaccatoGC *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	MM_StaccatoGC(MM_EnvironmentBase *env) :
		MM_RealtimeGC(env)
	{
		_typeId = __FUNCTION__;
	}
	
	/* Inherited from MM_RealtimeGC */
	virtual MM_RealtimeAccessBarrier* allocateAccessBarrier(MM_EnvironmentBase *env);
	virtual void doTracing(MM_EnvironmentRealtime* env);
	virtual void enableWriteBarrier(MM_EnvironmentBase* env);
	virtual void disableWriteBarrier(MM_EnvironmentBase* env);
	virtual void enableDoubleBarrier(MM_EnvironmentBase* env);
	virtual void disableDoubleBarrierOnThread(MM_EnvironmentBase* env, J9VMThread* vmThread);
	virtual void disableDoubleBarrier(MM_EnvironmentBase* env);
	
	/* New methods */
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	bool doClassTracing(MM_EnvironmentRealtime* env);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	void flushRememberedSet(MM_EnvironmentRealtime *env);
	
protected:
private:
};

#endif /* STACCATOGC_HPP_ */
